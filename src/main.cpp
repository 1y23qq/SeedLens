#include "ui.h"
#include "export.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
namespace {
ID3D11Device* device = nullptr;
ID3D11DeviceContext* device_context = nullptr;
IDXGISwapChain* swap_chain = nullptr;
ID3D11RenderTargetView* target = nullptr;
UINT resize_width = 0, resize_height = 0;
void destroy_target() {
    if (target) {
        target->Release();
        target = nullptr;
    }
}
void create_target() {
    ID3D11Texture2D* buffer = nullptr;
    if (SUCCEEDED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) {
        device->CreateRenderTargetView(buffer, nullptr, &target);
        buffer->Release();
    }
}
bool create_device(HWND window) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0}, level;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                               D3D11_SDK_VERSION, &desc, &swap_chain, &device, &level,
                                               &device_context);
    if (FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                           D3D11_SDK_VERSION, &desc, &swap_chain, &device, &level,
                                           &device_context);
    if (FAILED(hr))
        return false;
    create_target();
    return target != nullptr;
}
void cleanup() {
    destroy_target();
    if (swap_chain)
        swap_chain->Release();
    if (device_context)
        device_context->Release();
    if (device)
        device->Release();
}
LRESULT WINAPI window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(window, msg, wp, lp))
        return TRUE;
    switch (msg) {
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) {
            resize_width = LOWORD(lp);
            resize_height = HIWORD(lp);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DPICHANGED: {
        auto* r = reinterpret_cast<RECT*>(lp);
        SetWindowPos(window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lp);
        float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(window);
        info->ptMinTrackSize = {LONG(1040 * dpi), LONG(680 * dpi)};
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
bool capture_map(const std::filesystem::path& path, RECT crop, std::string& error) {
    ID3D11Texture2D *buffer = nullptr, *staging = nullptr;
    if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&buffer))))
        return false;
    D3D11_TEXTURE2D_DESC desc;
    buffer->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &staging);
    if (FAILED(hr)) {
        buffer->Release();
        return false;
    }
    device_context->CopyResource(staging, buffer);
    buffer->Release();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = device_context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        staging->Release();
        return false;
    }
    crop.left = std::clamp(crop.left, 0L, LONG(desc.Width) - 1);
    crop.top = std::clamp(crop.top, 0L, LONG(desc.Height) - 1);
    crop.right = std::clamp(crop.right, crop.left + 1, LONG(desc.Width));
    crop.bottom = std::clamp(crop.bottom, crop.top + 1, LONG(desc.Height));
    int w = crop.right - crop.left, h = crop.bottom - crop.top;
    std::vector<uint32_t> pixels(size_t(w) * h);
    for (int y = 0; y < h; ++y)
        memcpy(pixels.data() + size_t(y) * w,
               static_cast<BYTE*>(mapped.pData) + size_t(y + crop.top) * mapped.RowPitch + crop.left * 4,
               size_t(w) * 4);
    device_context->Unmap(staging, 0);
    staging->Release();
    return seedlens::write_png(path, w, h, pixels.data(), error);
}

int command_line(const std::vector<std::wstring>& args, bool& handled, bool& portable) {
    using namespace seedlens;
    handled = false;
    portable = false;
    World w = default_world();
    int cx = 0, cz = 0, width = 1024, height = 768, scale = 4, radius = 2048, kind = SL_VILLAGE;
    std::filesystem::path png, csv, output;
    bool query = false, versions = false, self_test = false;
    int qx = 0, qy = 64, qz = 0;
    std::string version;
    auto number = [&](size_t& i) {
        if (++i >= args.size())
            throw std::runtime_error("Missing argument.");
        size_t end;
        int value = std::stoi(args[i], &end);
        if (end != args[i].size())
            throw std::runtime_error("Invalid integer.");
        return value;
    };
    for (size_t i = 1; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == L"--portable")
            portable = true;
        else if (a == L"--edition") {
            if (++i >= args.size())
                throw std::runtime_error("Missing edition.");
            if (args[i] != L"java" && args[i] != L"bedrock")
                throw std::runtime_error("Edition must be java or bedrock.");
            w.edition = args[i] == L"bedrock";
            w.version = backend(w.edition).versions[0].id;
        } else if (a == L"--seed") {
            if (++i >= args.size())
                throw std::runtime_error("Missing seed.");
            std::string error;
            if (!parse_seed(utf8(args[i]), w.seed, error))
                throw std::runtime_error(error);
        } else if (a == L"--version") {
            if (++i >= args.size())
                throw std::runtime_error("Missing version.");
            version = utf8(args[i]);
        } else if (a == L"--dimension") {
            if (++i >= args.size())
                throw std::runtime_error("Missing dimension.");
            if (args[i] == L"overworld")
                w.dimension = 0;
            else if (args[i] == L"nether")
                w.dimension = -1;
            else if (args[i] == L"end")
                w.dimension = 1;
            else
                throw std::runtime_error("Invalid dimension.");
        } else if (a == L"--at") {
            cx = number(i);
            cz = number(i);
        } else if (a == L"--y")
            w.y = number(i);
        else if (a == L"--large-biomes")
            w.large = true;
        else if (a == L"--width")
            width = number(i);
        else if (a == L"--height")
            height = number(i);
        else if (a == L"--scale")
            scale = number(i);
        else if (a == L"--radius")
            radius = number(i);
        else if (a == L"--query") {
            handled = true;
            query = true;
            qx = number(i);
            qy = number(i);
            qz = number(i);
        } else if (a == L"--export") {
            handled = true;
            if (++i >= args.size())
                throw std::runtime_error("Missing image path.");
            png = args[i];
        } else if (a == L"--scan") {
            handled = true;
            if (++i >= args.size())
                throw std::runtime_error("Missing CSV path.");
            csv = args[i];
        } else if (a == L"--output") {
            if (++i >= args.size())
                throw std::runtime_error("Missing output path.");
            output = args[i];
        } else if (a == L"--kind") {
            if (++i >= args.size())
                throw std::runtime_error("Missing structure kind.");
            std::string name = utf8(args[i]);
            kind = -1;
            for (auto& k : kinds()) {
                std::string key = k.en;
                for (char& c : key) {
                    if (c == ' ')
                        c = '-';
                    else if (c >= 'A' && c <= 'Z')
                        c += 'a' - 'A';
                }
                if (name == key)
                    kind = k.kind;
            }
            if (kind < 0)
                throw std::runtime_error(
                    "Unknown structure kind. Use hyphenated English names, e.g. village, trial-chambers.");
        } else if (a == L"--versions") {
            handled = true;
            versions = true;
        } else if (a == L"--self-test") {
            handled = true;
            self_test = true;
        } else if (a == L"--help") {
            handled = true;
            std::puts(
                "SeedLens [--edition java|bedrock] [--version VERSION] [--seed SEED] [--dimension "
                "overworld|nether|end] [--at X Z] [--y Y]\n  --query X Y Z [--output FILE]\n  --export "
                "FILE.png [--width N --height N --scale 1|4|16|64|256]\n  --scan FILE.csv [--kind village "
                "--radius 2048]\n  --versions [--output FILE]\n  --self-test [--output FILE]\n  --portable");
            return 0;
        } else
            throw std::runtime_error("Unknown argument: " + utf8(a));
    }
    if (!handled)
        return 0;
    if (width < 1 || width > 4096 || height < 1 || height > 4096 || radius < 1 || radius > 32768 ||
        w.y < -64 || w.y > 320 || std::abs(double(cx)) > world_limit || std::abs(double(cz)) > world_limit ||
        std::abs(double(qx)) > world_limit || std::abs(double(qz)) > world_limit)
        throw std::runtime_error("Coordinates or image/search dimensions are outside supported bounds.");
    if (scale != 1 && scale != 4 && scale != 16 && scale != 64 && scale != 256)
        throw std::runtime_error("Invalid scale.");
    const auto& api = backend(w.edition);
    if (!version.empty()) {
        bool found = false;
        for (int i = 0; i < api.version_count; ++i) {
            std::string name = api.versions[i].name;
            if (name == version || name.substr(0, name.find(' ')) == version) {
                w.version = api.versions[i].id;
                found = true;
                break;
            }
        }
        if (!found)
            throw std::runtime_error("Unsupported version. Use --versions.");
    }
    if (!dimension_available(w, w.dimension))
        throw std::runtime_error("The End does not exist in this version.");
    if (w.edition && w.large)
        throw std::runtime_error("Large biomes are only available for Java.");
    std::string result;
    if (versions) {
        for (int i = 0; i < api.version_count; ++i)
            result += std::string(api.versions[i].name) + "\n";
    }
    if (self_test) {
        World test = default_world();
        test.seed = static_cast<uint64_t>(INT64_C(-3829811542736183482));
        Context ctx(test);
        if (ctx.biome(68148, 77, 80990) != 188)
            throw std::runtime_error("Java reference biome check failed.");
        World be = default_world(1);
        be.dimension = -1;
        Context bed(be);
        if (bed.biome(0, 64, 0) < 0)
            throw std::runtime_error("Bedrock generator check failed.");
        std::vector<uint32_t> marker_pixels;
        int marker_width = 0, marker_height = 0;
        if (!read_png_resource(300, marker_pixels, marker_width, marker_height) || marker_width != 256 ||
            marker_height != 160)
            throw std::runtime_error("Embedded Minecraft textures failed to decode.");
        result = "{\"ok\":true,\"java_reference\":\"Dappled Forest\",\"bedrock_nether\":true}\n";
    }
    if (query) {
        Context ctx(w);
        int b = ctx.biome(qx, qy, qz);
        result = "{\"seed\":\"" + signed_seed(w.seed) + "\",\"edition\":\"" +
                 (w.edition ? "bedrock" : "java") + "\",\"dimension\":" + std::to_string(w.dimension) +
                 ",\"x\":" + std::to_string(qx) + ",\"y\":" + std::to_string(qy) +
                 ",\"z\":" + std::to_string(qz) + ",\"biome_id\":" + std::to_string(b) + ",\"biome\":\"" +
                 biome_label(w, b, false) + "\"}\n";
    }
    if (!png.empty()) {
        Context ctx(w);
        std::vector<int> ids(size_t(width) * height);
        if (!ctx.generate(scale, floor_div(cx, scale) - width / 2, floor_div(cz, scale) - height / 2, width,
                          height, w.y, ids.data()))
            throw std::runtime_error("Map generation failed.");
        std::vector<uint32_t> pixels(ids.size());
        for (size_t i = 0; i < ids.size(); ++i)
            pixels[i] = biome_color(ids[i], legacy_palette(w));
        std::string error;
        if (!write_png(png, width, height, pixels.data(), error))
            throw std::runtime_error(error);
        result = "PNG exported.\n";
    }
    if (!csv.empty()) {
        Context ctx(w);
        if (!ctx.supports(kind))
            throw std::runtime_error(
                "This structure is unavailable for the selected edition, version or dimension.");
        if (kind == SL_DUNGEON && radius > 128)
            throw std::runtime_error("Monster room search radius must be <= 128.");
        std::vector<SLMarker> markers(10000);
        int n = ctx.scan(kind, std::max(-world_limit, cx - radius), std::max(-world_limit, cz - radius),
                         std::min(world_limit, cx + radius), std::min(world_limit, cz + radius),
                         markers.data(), int(markers.size()), nullptr, nullptr);
        if (n < 0)
            throw std::runtime_error("Structure search failed.");
        markers.resize(n);
        std::string error;
        if (!write_csv(csv, w, markers, error))
            throw std::runtime_error(error);
        result = std::to_string(n) + " structures exported (square search area).\n";
    }
    if (!output.empty()) {
        std::ofstream file(output, std::ios::binary | std::ios::trunc);
        file << result;
        if (!file)
            throw std::runtime_error("Cannot write output file.");
    } else
        std::fwrite(result.data(), 1, result.size(), stdout);
    return 0;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::vector<std::wstring> args;
        for (int i = 0; i < argc; ++i)
            args.emplace_back(argv[i]);
        LocalFree(argv);
        bool handled = false, portable = false;
        int rc = command_line(args, handled, portable);
        if (handled) {
            CoUninitialize();
            return rc;
        }
        ImGui_ImplWin32_EnableDpiAwareness();
        WNDCLASSEXW wc{sizeof(wc),
                       CS_CLASSDC,
                       window_proc,
                       0,
                       0,
                       instance,
                       LoadIconW(instance, MAKEINTRESOURCEW(1)),
                       LoadCursor(nullptr, IDC_ARROW),
                       nullptr,
                       nullptr,
                       L"SeedLens",
                       LoadIconW(instance, MAKEINTRESOURCEW(1))};
        RegisterClassExW(&wc);
        RECT work{0, 0, 1440, 920};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        int initial_width = std::min(1440, int(work.right - work.left) - 32);
        int initial_height = std::min(920, int(work.bottom - work.top) - 32);
        HWND window = CreateWindowW(wc.lpszClassName, L"SeedLens", WS_OVERLAPPEDWINDOW,
                                    work.left + (work.right - work.left - initial_width) / 2,
                                    work.top + (work.bottom - work.top - initial_height) / 2, initial_width,
                                    initial_height, nullptr, nullptr, instance, nullptr);
        if (!create_device(window)) {
            cleanup();
            throw std::runtime_error("DirectX 11 could not be initialized.");
        }
        BOOL dark = FALSE;
        DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
        COLORREF caption = RGB(255, 255, 255), caption_text = RGB(23, 32, 42);
        DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
        DwmSetWindowAttribute(window, 36, &caption_text, sizeof(caption_text));
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        wchar_t windows[32768];
        GetWindowsDirectoryW(windows, 32768);
        std::filesystem::path fonts = std::filesystem::path(windows) / L"Fonts";
        auto font_resource = [&](int id, ImFontConfig* config) -> ImFont* {
            HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(id), RT_RCDATA);
            if (!resource)
                return nullptr;
            HGLOBAL bytes = LoadResource(instance, resource);
            if (!bytes)
                return nullptr;
            return io.Fonts->AddFontFromMemoryCompressedTTF(
                LockResource(bytes), int(SizeofResource(instance, resource)), 15, config);
        };
        ImFontConfig regular_config;
        regular_config.SizePixels = 15;
        ImFont* regular = font_resource(401, &regular_config);
        if (!regular)
            throw std::runtime_error("Embedded Noto Sans JP font is unavailable.");
        std::filesystem::path jp = fonts / L"YuGothM.ttc";
        if (!std::filesystem::exists(jp))
            jp = fonts / L"meiryo.ttc";
        if (std::filesystem::exists(jp)) {
            ImFontConfig fallback;
            fallback.MergeMode = true;
            fallback.SizePixels = 15;
            io.Fonts->AddFontFromFileTTF(seedlens::utf8(jp.wstring()).c_str(), 15, &fallback);
        }
        ImFontConfig strong_config;
        strong_config.SizePixels = 15;
        font_resource(402, &strong_config);
        io.FontDefault = regular;
        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX11_Init(device, device_context);
        seedlens::Session session;
        auto path = seedlens::settings_path(portable);
        seedlens::load_session(path, session);
        ShowWindow(window, SW_SHOWDEFAULT);
        UpdateWindow(window);
        {
            seedlens::App app(window, device, std::move(session), path);
            bool done = false;
            float old_dpi = 0;
            while (!done) {
                MSG msg;
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                    if (msg.message == WM_QUIT)
                        done = true;
                }
                if (done)
                    break;
                if (IsIconic(window)) {
                    MsgWaitForMultipleObjectsEx(0, nullptr, 100, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                    continue;
                }
                if (resize_width && resize_height) {
                    destroy_target();
                    HRESULT hr =
                        swap_chain->ResizeBuffers(0, resize_width, resize_height, DXGI_FORMAT_UNKNOWN, 0);
                    resize_width = resize_height = 0;
                    if (FAILED(hr))
                        throw std::runtime_error("Resizing the graphics buffer failed.");
                    create_target();
                }
                float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(window);
                if (dpi != old_dpi) {
                    ImGui::GetStyle().FontScaleDpi = dpi;
                    old_dpi = dpi;
                }
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
                app.frame(dpi);
                ImGui::Render();
                const float clear[] = {0.96f, 0.97f, 0.98f, 1};
                device_context->OMSetRenderTargets(1, &target, nullptr);
                device_context->ClearRenderTargetView(target, clear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                if (auto export_path = app.take_export()) {
                    std::string error;
                    if (capture_map(*export_path, app.export_rect(), error))
                        app.notification("保存しました / Saved: " +
                                         seedlens::utf8(export_path->filename().wstring()));
                    else
                        app.notification(error.empty() ? "PNG export failed." : error);
                }
                HRESULT presented = swap_chain->Present(1, 0);
                if (presented == DXGI_ERROR_DEVICE_REMOVED || presented == DXGI_ERROR_DEVICE_RESET)
                    throw std::runtime_error("The graphics device was reset. Please restart SeedLens.");
                MsgWaitForMultipleObjectsEx(0, nullptr, app.busy() ? 8 : 220, QS_ALLINPUT,
                                            MWMO_INPUTAVAILABLE);
            }
            seedlens::save_session(path, app.session());
        }
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        cleanup();
        DestroyWindow(window);
        UnregisterClassW(wc.lpszClassName, instance);
        CoUninitialize();
        return 0;
    } catch (const std::exception& error) {
        std::string text = error.what();
        std::fwrite(text.data(), 1, text.size(), stderr);
        if (__argc <= 1)
            MessageBoxW(nullptr, seedlens::wide(text).c_str(), L"SeedLens", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
}
