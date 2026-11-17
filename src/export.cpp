#include "export.h"
#include <wincodec.h>
#include <commdlg.h>
#include <fstream>
#include <limits>

namespace seedlens {
std::string utf8(const std::wstring& s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
    std::string v(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), v.data(), n, nullptr, nullptr);
    return v;
}
std::wstring wide(const std::string& s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring v(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), v.data(), n);
    return v;
}
template <class T> struct Com {
    T* p = nullptr;
    ~Com() {
        if (p)
            p->Release();
    }
    T** put() {
        return &p;
    }
    T* operator->() {
        return p;
    }
};
bool read_png_resource(int id, std::vector<uint32_t>& rgba, int& width, int& height) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!resource)
        return false;
    HGLOBAL data = LoadResource(module, resource);
    if (!data)
        return false;
    Com<IWICImagingFactory> factory;
    Com<IWICStream> stream;
    Com<IWICBitmapDecoder> decoder;
    Com<IWICBitmapFrameDecode> frame;
    Com<IWICFormatConverter> converter;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IWICImagingFactory, reinterpret_cast<void**>(factory.put()));
    if (SUCCEEDED(hr))
        hr = factory->CreateStream(stream.put());
    if (SUCCEEDED(hr))
        hr = stream->InitializeFromMemory(static_cast<BYTE*>(LockResource(data)),
                                          SizeofResource(module, resource));
    if (SUCCEEDED(hr))
        hr = factory->CreateDecoderFromStream(stream.p, nullptr, WICDecodeMetadataCacheOnLoad, decoder.put());
    if (SUCCEEDED(hr))
        hr = decoder->GetFrame(0, frame.put());
    UINT w = 0, h = 0;
    if (SUCCEEDED(hr))
        hr = frame->GetSize(&w, &h);
    if (FAILED(hr) || w > 2048 || h > 2048 || !w || !h)
        return false;
    if (SUCCEEDED(hr))
        hr = factory->CreateFormatConverter(converter.put());
    if (SUCCEEDED(hr))
        hr = converter->Initialize(frame.p, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                   WICBitmapPaletteTypeCustom);
    rgba.resize(size_t(w) * h);
    if (SUCCEEDED(hr))
        hr = converter->CopyPixels(nullptr, w * 4, UINT(rgba.size() * 4),
                                   reinterpret_cast<BYTE*>(rgba.data()));
    if (FAILED(hr))
        return false;
    width = int(w);
    height = int(h);
    return true;
}
bool write_png(const std::filesystem::path& path, int width, int height, const uint32_t* rgba,
               std::string& error) {
    error.clear();
    if (width < 1 || height < 1 || width > 8192 || height > 8192 || !rgba) {
        error = "Invalid image dimensions.";
        return false;
    }
    Com<IWICImagingFactory> factory;
    Com<IWICStream> stream;
    Com<IWICBitmapEncoder> encoder;
    Com<IWICBitmapFrameEncode> frame;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IWICImagingFactory, reinterpret_cast<void**>(factory.put()));
    if (SUCCEEDED(hr))
        hr = factory->CreateStream(stream.put());
    if (SUCCEEDED(hr))
        hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr))
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put());
    if (SUCCEEDED(hr))
        hr = encoder->Initialize(stream.p, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr))
        hr = encoder->CreateNewFrame(frame.put(), nullptr);
    if (SUCCEEDED(hr))
        hr = frame->Initialize(nullptr);
    if (SUCCEEDED(hr))
        hr = frame->SetSize(UINT(width), UINT(height));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(hr))
        hr = frame->SetPixelFormat(&format);
    if (SUCCEEDED(hr) && !IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA))
        hr = E_FAIL;
    std::vector<uint32_t> bgra(size_t(width) * height);
    for (size_t i = 0; i < bgra.size(); ++i) {
        uint32_t c = rgba[i];
        bgra[i] = (c & 0xff00ff00u) | ((c & 0xffu) << 16) | ((c >> 16) & 0xffu);
    }
    if (SUCCEEDED(hr))
        hr = frame->WritePixels(UINT(height), UINT(width * 4), UINT(width * height * 4),
                                reinterpret_cast<BYTE*>(bgra.data()));
    if (SUCCEEDED(hr))
        hr = frame->Commit();
    if (SUCCEEDED(hr))
        hr = encoder->Commit();
    if (FAILED(hr)) {
        error = "PNG export failed (" + std::to_string(uint32_t(hr)) + ").";
        return false;
    }
    return true;
}
static std::string csv_field(std::string s) {
    size_t i = 0;
    while ((i = s.find('"', i)) != std::string::npos) {
        s.insert(i, 1, '"');
        i += 2;
    }
    return '"' + s + '"';
}
bool write_csv(const std::filesystem::path& path, const World& w, const std::vector<SLMarker>& markers,
               std::string& error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "Cannot create CSV file.";
        return false;
    }
    out << "\xef\xbb\xbf" << "edition,version,seed,dimension,type,x,y,z,biome,flags,variant\n";
    const auto& api = backend(w.edition);
    std::string version;
    for (int i = 0; i < api.version_count; ++i)
        if (api.versions[i].id == w.version)
            version = api.versions[i].name;
    for (auto& m : markers) {
        std::string name = m.kind == -1 ? "Estimated spawn" : m.kind == -2 ? "Biome" : kinds()[m.kind].en;
        out << (w.edition ? "Bedrock" : "Java") << ',' << csv_field(version) << ',' << signed_seed(w.seed)
            << ',' << w.dimension << ',' << csv_field(name) << ',' << m.x << ',';
        if (m.y != std::numeric_limits<int>::min())
            out << m.y;
        out << ',' << m.z << ',' << csv_field(biome_label(w, m.biome, false)) << ',' << m.flags << ','
            << m.variant << '\n';
    }
    out.flush();
    if (!out) {
        error = "CSV write failed.";
        return false;
    }
    error.clear();
    return true;
}
std::filesystem::path save_dialog(HWND owner, const wchar_t* extension, const std::wstring& suggested) {
    wchar_t path[32768]{};
    wcsncpy_s(path, suggested.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = path;
    ofn.nMaxFile = 32768;
    ofn.lpstrDefExt = extension;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    ofn.lpstrFilter = wcscmp(extension, L"png") == 0 ? L"PNG image\0*.png\0\0" : L"CSV file\0*.csv\0\0";
    return GetSaveFileNameW(&ofn) ? std::filesystem::path(path) : std::filesystem::path();
}
} // namespace seedlens
