#include "ui.h"
#include "export.h"
#include "lucide_icons.h"
#include "minecraft_icons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <random>
#include <unordered_map>

namespace seedlens {
namespace {
constexpr ImU32 background = IM_COL32(245, 247, 250, 255), panel = IM_COL32(255, 255, 255, 255),
                line = IM_COL32(220, 227, 234, 255);
constexpr ImU32 text = IM_COL32(23, 32, 42, 255), muted = IM_COL32(100, 116, 139, 255),
                accent = IM_COL32(37, 99, 235, 255);
ImVec4 color4(ImU32 c) {
    return ImGui::ColorConvertU32ToFloat4(c);
}
void icon(ImDrawList* draw, const char* name, ImVec2 pos, float size, ImU32 color, float weight = 1.65f) {
    const auto* shape = lucide::find(name);
    float f = size / 24;
    for (int p = 0; p < shape->count; ++p) {
        const auto& path = shape->paths[p];
        std::vector<ImVec2> points;
        points.reserve(path.count);
        for (int i = 0; i < path.count; ++i)
            points.emplace_back(pos.x + path.xy[i * 2] * f, pos.y + path.xy[i * 2 + 1] * f);
        if (points.size() > 1)
            draw->AddPolyline(points.data(), int(points.size()), color, 0, weight * size / 20);
    }
}
void hint(const char* label) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
        ImGui::TextUnformatted(label);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
bool icon_button(const char* id, const char* shape, const char* tooltip, float size, bool selected = false) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(239, 246, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(147, 197, 253, 255));
    }
    auto p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::Button(id, {size, size});
    icon(ImGui::GetWindowDrawList(), shape, {p.x + size * 0.25f, p.y + size * 0.25f}, size * 0.5f,
         selected ? accent : text);
    hint(tooltip);
    if (selected)
        ImGui::PopStyleColor(2);
    return pressed;
}
bool segment_button(const char* label, ImVec2 size, bool selected) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(239, 246, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, color4(accent));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(191, 219, 254, 255));
    }
    bool strong = selected && ImGui::GetIO().Fonts->Fonts.Size > 1;
    if (strong)
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1], 0);
    bool clicked = ImGui::Button(label, size);
    if (strong)
        ImGui::PopFont();
    if (selected)
        ImGui::PopStyleColor(3);
    return clicked;
}
bool action_button(const char* id, const char* shape, const char* label, ImVec2 size, bool selected = false) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(239, 246, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(147, 197, 253, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, color4(accent));
    }
    auto p = ImGui::GetCursorScreenPos();
    std::string caption = "     ";
    caption += label;
    caption += "##";
    caption += id;
    bool clicked = ImGui::Button(caption.c_str(), size);
    float h = ImGui::GetItemRectSize().y;
    icon(ImGui::GetWindowDrawList(), shape, {p.x + 11 * h / 34, p.y + (h - 17 * h / 34) / 2}, 17 * h / 34,
         selected ? accent : text);
    if (selected)
        ImGui::PopStyleColor(3);
    return clicked;
}
std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
    return s;
}
bool matches(const char* filter, const std::string& a, const std::string& b = {}) {
    if (!filter[0])
        return true;
    auto f = lower(filter);
    return lower(a).find(f) != std::string::npos || lower(b).find(f) != std::string::npos;
}
std::string coordinates(int x, int z) {
    return std::to_string(x) + ", " + std::to_string(z);
}
const char* dimension_name(int dim, bool ja) {
    if (dim == -1)
        return ja ? "ネザー" : "Nether";
    if (dim == 1)
        return ja ? "ジ・エンド" : "The End";
    return ja ? "オーバーワールド" : "Overworld";
}
void copy_position(const World& w, const SLMarker& m, bool teleport) {
    std::string s;
    if (teleport) {
        const char* dim = w.dimension == -1 ? "the_nether" : w.dimension == 1 ? "the_end" : "overworld";
        s = std::string("/execute in minecraft:") + dim + " run tp @s " + std::to_string(m.x) + " " +
            (m.y == std::numeric_limits<int>::min() ? "~" : std::to_string(m.y)) + " " + std::to_string(m.z);
    } else
        s = coordinates(m.x, m.z);
    ImGui::SetClipboardText(s.c_str());
}
} // namespace

struct App::Impl {
    Platform platform;
    Session s;
    std::filesystem::path settings;
    TilePool tiles;
    SearchWorker map_search, finder;
    std::unique_ptr<Context> probe;
    struct GpuTile {
        TextureHandle view = 0;
        std::vector<int> ids;
        uint64_t touched = 0;
        bool ok = true;
    };
    std::unordered_map<TileKey, GpuTile, TileHash> cache;
    std::optional<SearchResult> map_result, find_result;
    std::optional<SLMarker> selected;
    std::optional<std::filesystem::path> png;
    std::optional<Session> saved_session;
    PixelRect map_rect{};
    ImVec2 map_pos{}, map_size{};
    float u = 1;
    bool sidebar_open = true;
    bool compact=false,dismiss_popups=false,layout_ready=false;
    std::optional<ImVec2> pending_context;
    bool zooming = false;
    double target_bpp = 4, anchor_x = 0, anchor_z = 0;
    ImVec2 zoom_anchor{};
    TextureHandle game_atlas = 0;
    uint64_t frame_count = 0, map_request = 0, find_request = 0;
    double last_view_change = 0, last_search = -1, last_save = 0, toast_time = 0;
    bool view_dirty = true, find_open = false, bookmarks_open = false, help_open = false, add_open = false,
         focus_seed = false;
    bool dragging = false, find_biome = false;
    int layers_tab = 0, find_kind = SL_VILLAGE, find_biome_id = 1, find_radius = 2048;
    int jump_x = 0, jump_z = 0, highlight = -1;
    char seed_input[512]{}, layer_filter[128]{}, find_filter[128]{}, bookmark_name[512]{};
    std::string toast, error;
    std::string license_text;
    SLMarker bookmark_target{};
    World bookmark_world;

    Impl(Platform services,Session session,std::filesystem::path path)
        :platform(std::move(services)),s(std::move(session)),settings(std::move(path)),
         tiles(platform.wake),map_search(platform.wake),finder(platform.wake){
        sidebar_open=!platform.touch;
        snprintf(seed_input, sizeof(seed_input), "%s", s.seed_text.c_str());
        jump_x = int(s.x);
        jump_z = int(s.z);
        sync_world();
        load_game_sprites();
        if(platform.resource_data){auto bytes=platform.resource_data(200);license_text.assign(bytes.begin(),bytes.end());}
    }
    ~Impl() {
        clear_tiles();
        if (game_atlas)
            platform.release_texture(game_atlas);
    }
    const char* t(const char* ja, const char* en) const {
        return s.japanese ? ja : en;
    }
    void tell(std::string message) {
        toast = std::move(message);
        toast_time = ImGui::GetTime();
    }
    void clear_tiles() {
        for (auto& [key, tile] : cache)
            if (tile.view)
                platform.release_texture(tile.view);
        cache.clear();
    }
    void mark_view() {
        s.x = std::clamp(s.x, -double(world_limit), double(world_limit));
        s.z = std::clamp(s.z, -double(world_limit), double(world_limit));
        view_dirty = true;
        last_view_change = ImGui::GetTime();
    }
    void center(int x, int z) {
        zooming = false;
        target_bpp = s.blocks_per_pixel;
        s.x = x;
        s.z = z;
        jump_x = x;
        jump_z = z;
        mark_view();
    }
    void sync_world() {
        if (!dimension_available(s.world, s.world.dimension))
            s.world.dimension = 0;
        if (probe && probe->world == s.world)
            return;
        zooming = false;
        target_bpp = s.blocks_per_pixel;
        probe = std::make_unique<Context>(s.world);
        tiles.reset();
        clear_tiles();
        map_search.cancel();
        finder.cancel();
        map_result.reset();
        find_result.reset();
        selected.reset();
        highlight = -1;
        view_dirty = true;
        last_view_change = 0;
        last_search = -1;
        error.clear();
        if (s.world.edition)
            s.bedrock_version = s.world.version;
        else
            s.java_version = s.world.version;
    }
    void apply_seed() {
        uint64_t seed;
        std::string message;
        if (!parse_seed(seed_input, seed, message)) {
            error = message;
            return;
        }
        s.world.seed = seed;
        s.seed_text = seed_input;
        error.clear();
        sync_world();
        mark_view();
    }
    void switch_edition(int edition) {
        if (s.world.edition == edition)
            return;
        s.world.edition = edition;
        s.world.version = edition ? s.bedrock_version : s.java_version;
        bool valid = false;
        const auto& api = backend(edition);
        for (int i = 0; i < api.version_count; ++i)
            valid |= api.versions[i].id == s.world.version;
        if (!valid)
            s.world.version = api.versions[0].id;
        if (edition)
            s.world.large = false;
        sync_world();
    }
    void switch_dimension(int dim) {
        if (dim == s.world.dimension)
            return;
        int old = s.world.dimension;
        s.world.dimension = dim;
        if (old == 0 && dim == -1) {
            s.x /= 8;
            s.z /= 8;
        } else if (old == -1 && dim == 0) {
            s.x *= 8;
            s.z *= 8;
        } else {
            s.x = s.z = 0;
        }
        jump_x = int(s.x);
        jump_z = int(s.z);
        sync_world();
        mark_view();
    }
    void zoom(double factor, ImVec2 anchor) {
        if (!zooming)
            target_bpp = s.blocks_per_pixel;
        target_bpp = std::clamp(target_bpp * factor, 0.25, 512.0);
        anchor_x = s.x + (anchor.x - map_size.x / 2) * s.blocks_per_pixel;
        anchor_z = s.z + (anchor.y - map_size.y / 2) * s.blocks_per_pixel;
        zoom_anchor = anchor;
        zooming = true;
        mark_view();
    }
    void animate_zoom() {
        if (!zooming)
            return;
        double amount = 1 - std::exp(-16 * std::min(double(ImGui::GetIO().DeltaTime), 1.0 / 30));
        s.blocks_per_pixel += (target_bpp - s.blocks_per_pixel) * amount;
        if (std::abs(target_bpp - s.blocks_per_pixel) < 0.003 * target_bpp) {
            s.blocks_per_pixel = target_bpp;
            zooming = false;
        }
        s.x = anchor_x - (zoom_anchor.x - map_size.x / 2) * s.blocks_per_pixel;
        s.z = anchor_z - (zoom_anchor.y - map_size.y / 2) * s.blocks_per_pixel;
        mark_view();
    }
    void set_texture(GpuTile& tile) {
        if (tile.view) {
            platform.release_texture(tile.view);
            tile.view = 0;
        }
        std::vector<uint32_t> pixels(tile.ids.size());
        for (size_t i = 0; i < pixels.size(); ++i) {
            uint32_t c = biome_color(tile.ids[i], legacy_palette(s.world));
            if (highlight >= 0 && tile.ids[i] != highlight) {
                unsigned r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255;
                c = 0xff000000 | ((b * 22 / 100 + 191) << 16) | ((g * 22 / 100 + 191) << 8) |
                    (r * 22 / 100 + 191);
            }
            pixels[i] = c;
        }
        tile.view=platform.create_texture(pixels.data(),tile_size,tile_size);
        if(!tile.view)tile.ok=false;
    }
    void collect() {
        for (auto& tile : tiles.collect()) {
            if (tile.key.epoch != tiles.epoch())
                continue;
            GpuTile gpu;
            gpu.ids = std::move(tile.biomes);
            gpu.touched = frame_count;
            gpu.ok = tile.ok;
            if (tile.ok)
                set_texture(gpu);
            auto it = cache.find(tile.key);
            if (it != cache.end() && it->second.view)
                platform.release_texture(it->second.view);
            cache[tile.key] = std::move(gpu);
        }
        if (cache.size() > 144) {
            std::vector<std::pair<uint64_t, TileKey>> old;
            for (auto& [key, tile] : cache)
                if (frame_count - tile.touched > 2)
                    old.emplace_back(tile.touched, key);
            std::sort(old.begin(), old.end(), [](auto& a, auto& b) { return a.first < b.first; });
            for (auto& [stamp, key] : old) {
                if (cache.size() <= 112)
                    break;
                auto it = cache.find(key);
                if (it->second.view)
                    platform.release_texture(it->second.view);
                cache.erase(it);
            }
        }
        if (auto result = map_search.collect())
            if (result->id == map_request && result->request.world == s.world)
                map_result = std::move(*result);
        if (auto result = finder.collect())
            if (result->id == find_request) {
                if (result->request.spawn && !result->markers.empty() &&
                    result->request.world.seed == s.world.seed &&
                    result->request.world.edition == s.world.edition &&
                    result->request.world.version == s.world.version) {
                    auto m = result->markers[0];
                    if (s.world.dimension != 0)
                        switch_dimension(0);
                    center(m.x, m.z);
                    selected = m;
                    tell(t("推定スポーン地点に移動しました", "Moved to estimated world spawn"));
                } else if (result->request.world == s.world)
                    find_result = std::move(*result);
            }
    }
    void style() {
        auto& st = ImGui::GetStyle();
        st.WindowPadding = {16 * u, 14 * u};
        st.FramePadding = {10 * u, 7 * u};
        st.ItemSpacing = {8 * u, 8 * u};
        st.ItemInnerSpacing = {7 * u, 5 * u};
        st.WindowRounding = 10 * u;
        st.ChildRounding = 0;
        st.FrameRounding = 6 * u;
        st.PopupRounding = 9 * u;
        st.ScrollbarRounding = 8 * u;
        st.ScrollbarSize = 8 * u;
        st.GrabRounding = 5 * u;
        st.WindowBorderSize = 1;
        st.FrameBorderSize = 1;
        st.PopupBorderSize = 1;
        auto& c = st.Colors;
        c[ImGuiCol_Text] = color4(text);
        c[ImGuiCol_TextDisabled] = color4(muted);
        c[ImGuiCol_WindowBg] = color4(panel);
        c[ImGuiCol_ChildBg] = color4(panel);
        c[ImGuiCol_PopupBg] = color4(panel);
        c[ImGuiCol_Border] = color4(line);
        c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
        c[ImGuiCol_FrameBg] = color4(IM_COL32(248, 250, 252, 255));
        c[ImGuiCol_FrameBgHovered] = color4(IM_COL32(241, 245, 249, 255));
        c[ImGuiCol_FrameBgActive] = color4(IM_COL32(239, 246, 255, 255));
        c[ImGuiCol_Button] = color4(IM_COL32(255, 255, 255, 255));
        c[ImGuiCol_ButtonHovered] = color4(IM_COL32(239, 246, 255, 255));
        c[ImGuiCol_ButtonActive] = color4(IM_COL32(219, 234, 254, 255));
        c[ImGuiCol_Header] = color4(IM_COL32(231, 240, 255, 255));
        c[ImGuiCol_HeaderHovered] = color4(IM_COL32(241, 246, 252, 255));
        c[ImGuiCol_HeaderActive] = color4(IM_COL32(219, 234, 254, 255));
        c[ImGuiCol_CheckMark] = color4(accent);
        c[ImGuiCol_SliderGrab] = color4(accent);
        c[ImGuiCol_SliderGrabActive] = color4(accent);
        c[ImGuiCol_Separator] = color4(line);
        c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
        c[ImGuiCol_ScrollbarGrab] = color4(IM_COL32(203, 213, 225, 255));
        c[ImGuiCol_ScrollbarGrabHovered] = color4(IM_COL32(148, 163, 184, 255));
        c[ImGuiCol_TextSelectedBg] = color4(IM_COL32(191, 219, 254, 170));
        c[ImGuiCol_NavCursor] = color4(accent);
        c[ImGuiCol_TableHeaderBg] = color4(IM_COL32(245, 248, 252, 255));
        c[ImGuiCol_TableBorderLight] = color4(line);
        c[ImGuiCol_TableBorderStrong] = color4(line);
        c[ImGuiCol_TitleBg] = color4(background);
        c[ImGuiCol_TitleBgActive] = color4(panel);
        c[ImGuiCol_TitleBgCollapsed] = color4(background);
        c[ImGuiCol_ModalWindowDimBg] = {0.12f, 0.18f, 0.25f, 0.24f};
        c[ImGuiCol_Tab] = color4(IM_COL32(248, 250, 252, 255));
        c[ImGuiCol_TabHovered] = color4(IM_COL32(219, 234, 254, 255));
        c[ImGuiCol_TabSelected] = color4(IM_COL32(231, 240, 255, 255));
        c[ImGuiCol_TabSelectedOverline] = color4(accent);
        c[ImGuiCol_ResizeGrip] = color4(IM_COL32(203, 213, 225, 100));
        c[ImGuiCol_ResizeGripHovered] = color4(accent);
    }
    void export_csv(bool search_results) {
        auto* result =
            search_results ? (find_result ? &*find_result : nullptr) : (map_result ? &*map_result : nullptr);
        if (!result)
            return;
        auto path = platform.choose_save( L"csv", L"SeedLens-structures.csv");
        if (path.empty())
            return;
        std::string message;
        if (write_csv(path, result->request.world, result->markers, message)){
            if(platform.finish_export)platform.finish_export(path,"text/csv");
            tell(platform.touch?t("保存先を選択してください","Choose where to save"):t("CSVを保存しました","CSV saved"));
        }
        else
            tell(message);
    }
#include "ui_layout.h"
#include "ui_mobile.h"

    ImVec2 screen(int x, int z) const {
        return {map_pos.x + map_size.x / 2 + float((x - s.x) / s.blocks_per_pixel),
                map_pos.y + map_size.y / 2 + float((z - s.z) / s.blocks_per_pixel)};
    }
    std::pair<int, int> world_at(ImVec2 p) const {
        return {std::clamp(int(std::floor(s.x + (p.x - map_pos.x - map_size.x / 2) * s.blocks_per_pixel)),
                           -world_limit, world_limit),
                std::clamp(int(std::floor(s.z + (p.y - map_pos.y - map_size.y / 2) * s.blocks_per_pixel)),
                           -world_limit, world_limit)};
    }
    void request_map() {
        if (!view_dirty || ImGui::GetTime() - last_view_change < 0.16)
            return;
        double hw = map_size.x * s.blocks_per_pixel / 2, hh = map_size.y * s.blocks_per_pixel / 2;
        SearchRequest req;
        req.world = s.world;
        req.x0 = int(std::clamp(s.x - hw, -double(world_limit), double(world_limit)));
        req.x1 = int(std::clamp(s.x + hw, -double(world_limit), double(world_limit)));
        req.z0 = int(std::clamp(s.z - hh, -double(world_limit), double(world_limit)));
        req.z1 = int(std::clamp(s.z + hh, -double(world_limit), double(world_limit)));
        req.layers = s.layers;
        if (std::max(req.x1 - req.x0, req.z1 - req.z0) > 65536) {
            map_search.cancel();
            map_result.reset();
            view_dirty = false;
            return;
        }
        map_request = map_search.submit(req);
        view_dirty = false;
        last_search = ImGui::GetTime();
    }
    void draw_tiles(ImDrawList* draw) {
        int scale = sample_scale(s.blocks_per_pixel);
        int span = scale * tile_size;
        auto [x0, z0] = world_at(map_pos);
        auto [x1, z1] = world_at({map_pos.x + map_size.x, map_pos.y + map_size.y});
        int tx0 = floor_div(x0, span), tz0 = floor_div(z0, span), tx1 = floor_div(x1, span),
            tz1 = floor_div(z1, span);
        draw->AddRectFilled(map_pos, {map_pos.x + map_size.x, map_pos.y + map_size.y},
                            IM_COL32(245, 247, 250, 255));
        int proxy_span = 256 * tile_size;
        if (scale < 256) {
            for (int z = floor_div(z0, proxy_span); z <= floor_div(z1, proxy_span); ++z)
                for (int x = floor_div(x0, proxy_span); x <= floor_div(x1, proxy_span); ++x) {
                    TileKey key{tiles.epoch(), 256, x, z};
                    if (!cache.contains(key))
                        tiles.request(key, s.world);
                }
        }
        std::vector<std::pair<TileKey, GpuTile*>> fallback;
        for (auto& [key, tile] : cache)
            if (key.epoch == tiles.epoch() && tile.view && key.scale != scale)
                fallback.emplace_back(key, &tile);
        std::sort(fallback.begin(), fallback.end(),
                  [](auto& a, auto& b) { return a.first.scale > b.first.scale; });
        for (const auto& [key, tile] : fallback) {
            int size = key.scale * tile_size;
            auto a = screen(key.x * size, key.z * size), b = screen((key.x + 1) * size, (key.z + 1) * size);
            if (b.x < map_pos.x || b.y < map_pos.y || a.x > map_pos.x + map_size.x ||
                a.y > map_pos.y + map_size.y)
                continue;
            if (key.scale == 256)
                tile->touched = frame_count;
            draw->AddImage(ImTextureRef(ImTextureID(tile->view)), a, b);
        }
        std::vector<TileKey> missing;
        for (int z = tz0; z <= tz1; ++z)
            for (int x = tx0; x <= tx1; ++x) {
                TileKey key{tiles.epoch(), scale, x, z};
                auto a = screen(x * span, z * span), b = screen((x + 1) * span, (z + 1) * span);
                auto it = cache.find(key);
                if (it != cache.end()) {
                    it->second.touched = frame_count;
                    if (it->second.view)
                        draw->AddImage(ImTextureRef(ImTextureID(it->second.view)), a, b);
                    else
                        draw->AddRectFilled(a, b, IM_COL32(54, 36, 37, 255));
                } else {
                    missing.push_back(key);
                }
            }
        std::sort(missing.begin(), missing.end(), [&](auto& a, auto& b) {
            return std::hypot((a.x + 0.5) * span - s.x, (a.z + 0.5) * span - s.z) <
                   std::hypot((b.x + 0.5) * span - s.x, (b.z + 0.5) * span - s.z);
        });
        for (auto& key : missing)
            tiles.request(key, s.world);
        if (s.grid && 16 / s.blocks_per_pixel >= 6 * u) {
            for (int x = floor_div(x0, 16) * 16; x <= x1; x += 16) {
                float sx = screen(x, 0).x;
                draw->AddLine({sx, map_pos.y}, {sx, map_pos.y + map_size.y},
                              x % 512 == 0 ? IM_COL32(230, 242, 232, 85) : IM_COL32(15, 22, 24, 60), 1);
            }
            for (int z = floor_div(z0, 16) * 16; z <= z1; z += 16) {
                float sy = screen(0, z).y;
                draw->AddLine({map_pos.x, sy}, {map_pos.x + map_size.x, sy},
                              z % 512 == 0 ? IM_COL32(230, 242, 232, 85) : IM_COL32(15, 22, 24, 60), 1);
            }
        }
        if (s.slime && s.world.dimension == 0 && s.blocks_per_pixel <= 2) {
            for (int z = floor_div(z0, 16); z <= floor_div(z1, 16); ++z)
                for (int x = floor_div(x0, 16); x <= floor_div(x1, 16); ++x)
                    if (probe->slime(x, z)) {
                        auto a = screen(x * 16, z * 16), b = screen(x * 16 + 16, z * 16 + 16);
                        draw->AddRectFilled(a, b, IM_COL32(157, 246, 113, 77));
                        draw->AddRect(a, b, IM_COL32(180, 255, 137, 130));
                    }
        }
        auto origin = screen(0, 0);
        if (origin.x >= map_pos.x && origin.x <= map_pos.x + map_size.x && origin.y >= map_pos.y &&
            origin.y <= map_pos.y + map_size.y) {
            draw->AddLine({origin.x - 7 * u, origin.y}, {origin.x + 7 * u, origin.y},
                          IM_COL32(255, 255, 240, 185), 1.2f);
            draw->AddLine({origin.x, origin.y - 7 * u}, {origin.x, origin.y + 7 * u},
                          IM_COL32(255, 255, 240, 185), 1.2f);
        }
    }
    void begin_bookmark(SLMarker m) {
        bookmark_target = m;
        bookmark_world = s.world;
        std::string name = m.kind >= 0    ? t(kinds()[m.kind].ja, kinds()[m.kind].en)
                           : m.kind == -1 ? t("スポーン", "Spawn")
                                          : t("マーカー", "Marker");
        name += " " + coordinates(m.x, m.z);
        snprintf(bookmark_name, sizeof(bookmark_name), "%s", name.c_str());
        add_open = true;
    }
    void draw_markers(ImDrawList* draw, bool hovered) {
        const SLMarker* hit = nullptr;
        float hit_distance = 18 * u;
        std::map<std::pair<int, int>, std::pair<ImVec2, int>> occupied;
        if (map_result)
            for (auto& m : map_result->markers) {
                if (m.kind < 0 || !(s.layers & (uint64_t(1) << m.kind)))
                    continue;
                auto p = screen(m.x, m.z);
                if (p.x < map_pos.x - 14 * u || p.y < map_pos.y - 14 * u ||
                    p.x > map_pos.x + map_size.x + 14 * u || p.y > map_pos.y + map_size.y + 14 * u)
                    continue;
                float distance = std::hypot(ImGui::GetIO().MousePos.x - p.x, ImGui::GetIO().MousePos.y - p.y);
                if (hovered && distance < hit_distance) {
                    hit_distance = distance;
                    hit = &m;
                }
                auto cell =
                    std::make_pair(int((p.x - map_pos.x) / (29 * u)), int((p.y - map_pos.y) / (29 * u)));
                auto found = occupied.find(cell);
                if (found != occupied.end()) {
                    found->second.second++;
                    continue;
                }
                occupied[cell] = {p, 1};
                draw->AddRectFilled({p.x - 13 * u, p.y - 13 * u}, {p.x + 13 * u, p.y + 13 * u},
                                    IM_COL32(20, 24, 28, 110), 1 * u);
                draw->AddRect({p.x - 13 * u, p.y - 13 * u}, {p.x + 13 * u, p.y + 13 * u},
                              IM_COL32(228, 232, 236, 185), 1 * u, 0, 1 * u);
                game_sprite(draw, m.kind, {p.x - 12 * u, p.y - 12 * u}, 24 * u, m.flags);
            }
        for (auto& [cell, data] : occupied)
            if (data.second > 1) {
                auto p = data.first;
                std::string count = "+" + std::to_string(data.second - 1);
                ImVec2 sz = ImGui::CalcTextSize(count.c_str());
                draw->AddRectFilled({p.x + 5 * u, p.y + 4 * u}, {p.x + sz.x + 11 * u, p.y + sz.y + 7 * u},
                                    panel, 4 * u);
                draw->AddText({p.x + 8 * u, p.y + 5 * u}, accent, count.c_str());
            }
        for (auto& b : s.bookmarks) {
            if (b.world.seed != s.world.seed || b.world.edition != s.world.edition ||
                b.world.version != s.world.version || b.world.dimension != s.world.dimension)
                continue;
            auto p = screen(b.x, b.z);
            draw->AddCircle({p.x, p.y}, 15 * u, IM_COL32(240, 228, 168, 230), 0, 2 * u);
            icon(draw, "bookmark", {p.x - 7 * u, p.y - 7 * u}, 14 * u, IM_COL32(255, 245, 191, 255));
        }
        if (selected) {
            auto p = screen(selected->x, selected->z);
            draw->AddCircle(p, 20 * u, IM_COL32(245, 250, 240, 235), 40, 2 * u);
            draw->AddCircle(p, 24 * u, IM_COL32(10, 15, 18, 160), 40, 1 * u);
            draw->AddCircleFilled(p, 3 * u, IM_COL32(255, 255, 248, 255));
        }
        if (hit) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            bool showing =
                selected && selected->kind == hit->kind && selected->x == hit->x && selected->z == hit->z;
            if (!showing && !png) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(t(kinds()[hit->kind].ja, kinds()[hit->kind].en));
                ImGui::TextColored(color4(muted), "X %d   Z %d", hit->x, hit->z);
                ImGui::EndTooltip();
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !dragging)
                selected = *hit;
        }
        if(pending_context){
            auto [x,z]=world_at(*pending_context);SLMarker m{};m.kind=-2;m.x=x;m.z=z;m.y=s.world.y;m.biome=probe->biome(x,s.world.y,z);
            selected=m;pending_context.reset();ImGui::OpenPopup("map_context");
        }
        if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            auto [x, z] = world_at(ImGui::GetIO().MousePos);
            SLMarker m{};
            m.kind = -2;
            m.x = x;
            m.z = z;
            m.y = s.world.y;
            m.biome = probe->biome(x, s.world.y, z);
            selected = hit ? *hit : m;
            ImGui::OpenPopup("map_context");
        }
        if (ImGui::BeginPopup("map_context")) {
            if (selected) {
                ImGui::TextColored(color4(muted), "X %d   Z %d", selected->x, selected->z);
                ImGui::Separator();
                if (ImGui::MenuItem(t("ブックマークに追加", "Add bookmark")))
                    begin_bookmark(*selected);
                if (ImGui::MenuItem(t("座標をコピー", "Copy coordinates")))
                    copy_position(s.world, *selected, false);
                if (ImGui::MenuItem(t("テレポートコマンドをコピー", "Copy teleport command")))
                    copy_position(s.world, *selected, true);
                if (ImGui::MenuItem(t("ここを中心にする", "Center here")))
                    center(selected->x, selected->z);
                if (ImGui::MenuItem(t("マーカーを解除", "Clear selection")))
                    selected.reset();
            }
            ImGui::EndPopup();
        }
    }
    void inspector() {
        if (!selected)
            return;
        auto m = *selected;
        float w = std::min(310*u,map_size.x-24*u), h=204*u;
        auto anchor=screen(m.x,m.z);
        float lo_x=map_pos.x+12*u,hi_x=std::max(lo_x,map_pos.x+map_size.x-w-58*u);
        float lo_y=map_pos.y+12*u,hi_y=std::max(lo_y,map_pos.y+map_size.y-h-12*u);
        ImGui::SetNextWindowPos(compact?ImVec2{map_pos.x+(map_size.x-w)/2,hi_y}:ImVec2{std::clamp(anchor.x+22*u,lo_x,hi_x),std::clamp(anchor.y+20*u,lo_y,hi_y)});
        ImGui::SetNextWindowSize({w, h});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16 * u, 13 * u});
        ImGui::SetNextWindowBgAlpha(0.97f);
        ImGui::Begin("##inspector", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar();
        if (m.kind >= 0) {
            game_sprite(ImGui::GetWindowDrawList(), m.kind, ImGui::GetCursorScreenPos(), 24 * u, m.flags);
            ImGui::Dummy({29 * u, 24 * u});
            ImGui::SameLine(0, 3 * u);
            ImGui::TextUnformatted(t(kinds()[m.kind].ja, kinds()[m.kind].en));
        } else
            ImGui::TextUnformatted(m.kind == -1 ? t("推定スポーン地点", "Estimated spawn")
                                                : t("選択した地点", "Selected location"));
        ImGui::SameLine(ImGui::GetWindowWidth() - 46 * u);
        if (icon_button("##closeinspector", "x", t("閉じる", "Close"), 24 * u))
            selected.reset();
        ImGui::Spacing();
        ImGui::Text("X  %d      Z  %d", m.x, m.z);
        if (m.y != std::numeric_limits<int>::min())
            ImGui::TextColored(color4(muted), "Y  %d", m.y);
        else
            ImGui::TextDisabled("%s", t("Y: 未確定", "Y: not determined"));
        int biome = m.biome < 0 ? probe->biome(m.x, s.world.y, m.z) : m.biome;
        std::string biome_text = biome_label(s.world, biome, s.japanese);
        ImGui::TextColored(color4(muted), "%s", biome_text.c_str());
        if (m.flags & SL_END_SHIP)
            ImGui::TextColored(color4(accent), "%s", t("エンドシップあり", "End ship present"));
        if (m.flags & SL_DRIED_GHAST)
            ImGui::TextColored(color4(accent), "%s", t("乾燥したガストあり", "Dried ghast present"));
        if (m.kind == SL_BASTION) {
            const char* en[] = {"Housing units", "Hoglin stables", "Treasure room", "Bridge"};
            const char* ja[] = {"住居", "ホグリンの小屋", "宝物部屋", "橋"};
            ImGui::TextColored(color4(accent), "%s",
                               t(ja[std::clamp(m.variant, 0, 3)], en[std::clamp(m.variant, 0, 3)]));
        }
        if (m.flags & SL_ABANDONED) {
            ImGui::SameLine();
            ImGui::TextColored(color4(accent), "%s", t("ゾンビ村", "Abandoned"));
        }
        if (m.flags & SL_BASEMENT) {
            ImGui::SameLine();
            ImGui::TextColored(color4(accent), "%s", t("地下室あり", "Basement"));
        }
        if (action_button("copypos", "copy", t("座標", "Copy"), {104 * u, 30 * u})) {
            copy_position(s.world, m, false);
            tell(t("座標をコピーしました", "Coordinates copied"));
        }
        ImGui::SameLine(0, 6 * u);
        if (icon_button("##tp", "door-open",
                        t("テレポートコマンドをコピー。Y未確定の場合は現在のYを使います。",
                          "Copy teleport command. Unknown Y keeps your current height."),
                        30 * u)) {
            copy_position(s.world, m, true);
            tell(t("コマンドをコピーしました", "Teleport command copied"));
        }
        ImGui::SameLine(0, 6 * u);
        if (icon_button("##addbookmark", "bookmark", t("ブックマークに追加", "Add bookmark"), 30 * u))
            begin_bookmark(m);
        ImGui::SameLine(0, 6 * u);
        if (icon_button("##centerselection", "locate-fixed", t("中心に移動", "Center on location"), 30 * u))
            center(m.x, m.z);
        ImGui::End();
    }

    void search_dialog() {
        if (find_open) {
            ImGui::OpenPopup("Find locations##findmodal");
            find_open = false;
        }
        ImGui::SetNextWindowSize(modal_size(680,620), ImGuiCond_Appearing);
        bool open = true;
        if (!ImGui::BeginPopupModal("Find locations##findmodal", &open, ImGuiWindowFlags_NoSavedSettings))
            return;
        ImGui::TextUnformatted(t("場所を探す", "Find locations"));
        ImGui::SameLine();
        ImGui::TextDisabled("X %d  Z %d", int(s.x), int(s.z));
        ImGui::Separator();
        if (ImGui::RadioButton(t("構造物", "Structure"), !find_biome))
            find_biome = false;
        ImGui::SameLine();
        if (ImGui::RadioButton(t("バイオーム", "Biome"), find_biome))
            find_biome = true;
        const auto& api = backend(s.world.edition);
        if (!find_biome) {
            if (!probe->supports(find_kind)) {
                for (auto& k : kinds())
                    if (probe->supports(k.kind)) {
                        find_kind = k.kind;
                        break;
                    }
            }
            ImGui::SetNextItemWidth(std::min(350*u,ImGui::GetContentRegionAvail().x));
            if (ImGui::BeginCombo("##findkind", t(kinds()[find_kind].ja, kinds()[find_kind].en))) {
                for (auto& k : kinds())
                    if (probe->supports(k.kind) && ImGui::Selectable(t(k.ja, k.en), find_kind == k.kind))
                        find_kind = k.kind;
                ImGui::EndCombo();
            }
        } else {
            if (!api.biome_exists(s.world.version, find_biome_id, s.world.dimension))
                for (int id = 0; id < 256; ++id)
                    if (api.biome_exists(s.world.version, id, s.world.dimension)) {
                        find_biome_id = id;
                        break;
                    }
            ImGui::SetNextItemWidth(std::min(350*u,ImGui::GetContentRegionAvail().x));
            std::string name = biome_label(s.world, find_biome_id, s.japanese);
            if (ImGui::BeginCombo("##findbiome", name.c_str())) {
                for (int id = 0; id < 256; ++id)
                    if (api.biome_exists(s.world.version, id, s.world.dimension)) {
                        std::string label = biome_label(s.world, id, s.japanese);
                        if (ImGui::Selectable(label.c_str(), find_biome_id == id))
                            find_biome_id = id;
                    }
                ImGui::EndCombo();
            }
        }
        if(!compact||ImGui::GetWindowWidth()/u>=620)ImGui::SameLine();
        ImGui::SetNextItemWidth(std::min(180*u,ImGui::GetContentRegionAvail().x));
        if (!find_biome && find_kind == SL_DUNGEON)
            find_radius = 128;
        std::string radius = std::to_string(find_radius) + t(" ブロック", " blocks");
        if (ImGui::BeginCombo("##radius", radius.c_str())) {
            for (int r : {128, 512, 1024, 2048, 4096, 8192, 16384, 32768})
                if (ImGui::Selectable((std::to_string(r) + " blocks").c_str(), find_radius == r))
                    find_radius = r;
            ImGui::EndCombo();
        }
        if (find_biome)
            ImGui::TextDisabled(
                "%s", t("Y高さの地点を32ブロック間隔で探索。結果は見つかった地点です。",
                        "Searches a 32-block sample grid at the selected Y; results are sampled matches."));
        else if (find_kind == SL_DUNGEON)
            ImGui::TextDisabled("%s", t("地形を評価するため低速です。半径128ブロック以内。",
                                        "Terrain checks are slow. Maximum search radius: 128 blocks."));
        if (!finder.busy()) {
            if (action_button("startfind", "search", t("検索", "Search"), {125 * u, 34 * u})) {
                SearchRequest req;
                req.world = s.world;
                int r = !find_biome && find_kind == SL_DUNGEON ? 128 : find_radius;
                req.x0 = std::max(-world_limit, int(s.x) - r);
                req.x1 = std::min(world_limit, int(s.x) + r);
                req.z0 = std::max(-world_limit, int(s.z) - r);
                req.z1 = std::min(world_limit, int(s.z) + r);
                if (find_biome)
                    req.biome = find_biome_id;
                else
                    req.layers = uint64_t(1) << find_kind;
                find_request = finder.submit(req);
                find_result.reset();
            }
        } else {
            ImGui::ProgressBar(finder.progress(), {std::min(250*u,ImGui::GetContentRegionAvail().x-90*u), 25 * u});
            ImGui::SameLine();
            if (ImGui::Button(t("中止", "Cancel"))) {
                finder.cancel();
                find_result.reset();
            }
        }
        if (find_result) {
            ImGui::SameLine();
            ImGui::TextDisabled("%zu  /  %.2f s", find_result->markers.size(),
                                find_result->milliseconds / 1000);
            ImGui::SameLine();
            if (icon_button("##csvresults", "download", "CSV", 30 * u))
                export_csv(true);
            if (find_result->truncated)
                ImGui::TextColored(color4(accent), "%s",
                                   t("結果上限に到達しました。範囲を狭めてください。",
                                     "Result limit reached. Reduce the search radius."));
            if (find_result->omitted)
                ImGui::TextColored(color4(accent), "%s",
                                   t("範囲が広すぎます。半径4096ブロック以下で検索してください。",
                                     "Area too large for this layer. Use a radius of 4096 blocks or less."));
            if (!find_result->error.empty())
                ImGui::TextWrapped("%s", find_result->error.c_str());
        }
        ImGui::Separator();
        ImGui::InputTextWithHint("##findfilter", t("結果を絞り込み…", "Filter results…"), find_filter,
                                 sizeof(find_filter));
        float remaining = ImGui::GetContentRegionAvail().y - 44 * u;
        if(compact)mobile_search_results(remaining);
        else if (ImGui::BeginTable("results", 4,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH,
                              {0, remaining})) {
            ImGui::TableSetupColumn(t("種類", "Type"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 100 * u);
            ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 100 * u);
            ImGui::TableSetupColumn(t("距離", "Distance"), ImGuiTableColumnFlags_WidthFixed, 90 * u);
            ImGui::TableHeadersRow();
            if (find_result)
                for (size_t i = 0; i < find_result->markers.size(); ++i) {
                    const auto& m = find_result->markers[i];
                    std::string label = m.kind == -2 ? biome_label(s.world, m.biome, s.japanese)
                                                     : t(kinds()[m.kind].ja, kinds()[m.kind].en);
                    if (m.flags & SL_END_SHIP)
                        label += t(" (シップ)", " (ship)");
                    if (!matches(find_filter, label, coordinates(m.x, m.z)))
                        continue;
                    ImGui::PushID(int(i));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto row_pos = ImGui::GetCursorScreenPos();
                    std::string row_label = "          " + label;
                    if (ImGui::Selectable(row_label.c_str(), false,
                                          ImGuiSelectableFlags_SpanAllColumns |
                                              ImGuiSelectableFlags_AllowDoubleClick,
                                          {0, 28 * u})) {
                        selected = m;
                        center(m.x, m.z);
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            ImGui::CloseCurrentPopup();
                    }
                    if (m.kind >= 0)
                        game_sprite(ImGui::GetWindowDrawList(), m.kind,
                                    {row_pos.x + 2 * u, row_pos.y + 2 * u}, 24 * u, m.flags);
                    else
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            {row_pos.x + 5 * u, row_pos.y + 7 * u}, {row_pos.x + 21 * u, row_pos.y + 23 * u},
                            biome_color(m.biome, legacy_palette(s.world)), 2 * u);
                    hint(t("ダブルクリックで地図を開く", "Double-click to open on map"));
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", m.x);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", m.z);
                    ImGui::TableNextColumn();
                    double cx = (find_result->request.x0 + find_result->request.x1) / 2.0,
                           cz = (find_result->request.z0 + find_result->request.z1) / 2.0;
                    ImGui::Text("%.0f", std::hypot(m.x - cx, m.z - cz));
                    ImGui::PopID();
                }
            ImGui::EndTable();
        }
        if (ImGui::Button(t("閉じる", "Close"), {95 * u, 30 * u}))
            ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if (selected && ImGui::Button(t("選択地点を地図で開く", "Open selected location"), {230 * u, 30 * u}))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    void bookmark_dialog() {
        if (add_open) {
            ImGui::OpenPopup("Add bookmark##add");
            add_open = false;
        }
        ImGui::SetNextWindowSize(modal_size(430,190), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Add bookmark##add", nullptr,
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("X %d   Z %d", bookmark_target.x, bookmark_target.z);
            ImGui::SetNextItemWidth(-1);
            bool add = ImGui::InputText("##bookmarkname", bookmark_name, sizeof(bookmark_name),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
            add |= action_button("savebookmark", "bookmark", t("保存", "Save"), {130 * u, 34 * u});
            ImGui::SameLine();
            if (ImGui::Button(t("キャンセル", "Cancel"), {100 * u, 34 * u}))
                ImGui::CloseCurrentPopup();
            if (add && bookmark_name[0]) {
                s.bookmarks.push_back({bookmark_world, bookmark_target.x, bookmark_target.z, bookmark_name});
                save_session(settings, s);
                tell(t("ブックマークを保存しました", "Bookmark saved"));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (bookmarks_open) {
            ImGui::OpenPopup("Bookmarks##bookmarksmodal");
            bookmarks_open = false;
        }
        ImGui::SetNextWindowSize(modal_size(660,520), ImGuiCond_Appearing);
        bool open = true;
        if (!ImGui::BeginPopupModal("Bookmarks##bookmarksmodal", &open, ImGuiWindowFlags_NoSavedSettings))
            return;
        ImGui::TextUnformatted(t("ブックマーク", "Bookmarks"));
        ImGui::SameLine();
        ImGui::TextDisabled("%zu", s.bookmarks.size());
        ImGui::Separator();
        ImGui::BeginChild("bookmarklist", {0, -48 * u});
        int remove = -1;
        for (size_t i = 0; i < s.bookmarks.size(); ++i) {
            auto& b = s.bookmarks[i];
            ImGui::PushID(int(i));
            float w = ImGui::GetContentRegionAvail().x;
            if (action_button("visit", "map-pin", b.name.c_str(), {w - 40 * u, 34 * u})) {
                s.world = b.world;
                s.seed_text = signed_seed(s.world.seed);
                snprintf(seed_input, sizeof(seed_input), "%s", s.seed_text.c_str());
                sync_world();
                center(b.x, b.z);
                SLMarker m{};
                m.kind = -2;
                m.x = b.x;
                m.z = b.z;
                m.y = b.world.y;
                m.biome = probe->biome(b.x, b.world.y, b.z);
                selected = m;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(0, 6 * u);
            if (icon_button("##remove", "x", t("削除", "Remove"), 34 * u))
                remove = int(i);
            ImGui::TextDisabled("%s  ·  %s  ·  %s", b.world.edition ? "Bedrock" : "Java",
                                dimension_name(b.world.dimension, s.japanese),
                                signed_seed(b.world.seed).c_str());
            ImGui::Spacing();
            ImGui::PopID();
        }
        if (remove >= 0) {
            s.bookmarks.erase(s.bookmarks.begin() + remove);
            save_session(settings, s);
        }
        if (s.bookmarks.empty())
            ImGui::TextDisabled("%s", t("地図上を右クリックして保存できます。",
                                        "Right-click on the map to save a location."));
        ImGui::EndChild();
        if (action_button("addcenter", "plus", t("現在の中心を追加", "Bookmark map center"),
                          {230 * u, 32 * u})) {
            SLMarker m{};
            m.kind = -2;
            m.x = int(s.x);
            m.z = int(s.z);
            m.y = s.world.y;
            begin_bookmark(m);
        }
        ImGui::SameLine();
        if (ImGui::Button(t("閉じる", "Close"), {95 * u, 32 * u}))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    void help_dialog() {
        if (help_open) {
            ImGui::OpenPopup("Help##about");
            help_open = false;
        }
        ImGui::SetNextWindowSize(modal_size(720,640), ImGuiCond_Appearing);
        bool open = true;
        if (!ImGui::BeginPopupModal("Help##about", &open, ImGuiWindowFlags_NoSavedSettings))
            return;
        ImGui::TextUnformatted(t("使い方・対応範囲", "Help & coverage"));
        ImGui::Separator();
        if (ImGui::BeginTabBar("help_tabs")) {
            if (ImGui::BeginTabItem(t("使い方", "Guide"))) {
                ImGui::Spacing();
                ImGui::TextWrapped(
                    "%s", t("シード・エディション・バージョンを選び、矢印ボタンかEnterで適用します。ドラッグ"
                            "で移動、ホイールでズームします。地図を右クリックすると座標のコピーやブックマーク"
                            "保存ができます。",
                            "Select edition, version and seed. Apply with the arrow or Enter. Drag to pan, "
                            "scroll to zoom. Right-click the map to copy coordinates or save a bookmark."));
                ImGui::Spacing();
                ImGui::TextWrapped("%s",
                                   t("Yはバイオームを調べる高さです。地上は320、地下は0や-"
                                     "32を選べます。バイオーム一覧をクリックするとその種類が強調されます。",
                                     "Y selects the biome slice height. Use 320 for surface biomes, or 0 and "
                                     "-32 for caves. Click a biome in the list to highlight it."));
                ImGui::Spacing();
                ImGui::TextWrapped(
                    "%s", t("ネザーと地上の切替では座標を8:"
                            "1で変換します。スライムチャンクは1ピクセルあたり2ブロック以下まで拡大すると表示"
                            "されます。密集レイヤーは表示幅8192ブロック以内で表示されます。広域は65536ブロッ"
                            "ク以内で構造物を検索します。",
                            "Switching between Nether and Overworld converts coordinates 8:1. Slime chunks "
                            "appear at 2 blocks per pixel or closer. Dense layers require a view width of "
                            "8192 blocks or less. Structure overlays scan views up to 65536 blocks wide."));
                ImGui::Spacing();
                ImGui::TextColored(color4(accent), "%s", t("ショートカット", "Keyboard"));
                ImGui::TextUnformatted("Ctrl+L   Seed\nCtrl+F   Find locations\nCtrl+B   Bookmarks\nCtrl+S   "
                                       "Export PNG\nCtrl+G   Go to X / Z\nHome     Origin\n+ / -    Zoom\nG  "
                                       "      Chunk grid\nF1       Help\nEscape   Close / clear selection");
                ImGui::Spacing();
                ImGui::TextWrapped(
                    "%s",
                    t("設定とブックマークはLocalAppDataのSeedLensフォルダーに保存します。--"
                      "portableで起動するとexeの横に保存します。配布物はexeだけです。",
                      "Settings and bookmarks are saved under LocalAppData/SeedLens. Start with --portable "
                      "to store them beside the executable. Only the EXE is required for distribution."));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(t("対応範囲・精度", "Coverage"))) {
                ImGui::Spacing();
                ImGui::TextWrapped(
                    "%s",
                    t("Java: 1.0～26.3の代表リリース。Betaは実験的です。Bedrock: "
                      "1.0～26.60系の代表リリース。全パッチ・スナップショットの個別再現ではありません。",
                      "Java: representative releases from 1.0 through 26.3; beta is experimental. Bedrock: "
                      "representative releases from 1.0 through the 26.60 family. This does not individually "
                      "emulate every patch or snapshot."));
                ImGui::Spacing();
                ImGui::TextWrapped(
                    "%s",
                    t("Bedrockの26.30 / 26.40 / "
                      "26.60はプレビュー由来の生成コードです。26.60～26."
                      "63の正式版との完全一致は未検証です。互換性のあるホットフィックスを同じ項目にまとめてい"
                      "ます。",
                      "Bedrock 26.30 / 26.40 / 26.60 use generation code derived from previews. Exact parity "
                      "with final 26.60–26.63 is unverified. Compatible hotfixes are grouped in one entry."));
                ImGui::Spacing();
                ImGui::TextWrapped(
                    "%s",
                    t("全ディメンションのバイオームと主要な構造物を予測します。ブロック単位でゲーム全体を生成"
                      "するものではありません。地形に依存する寺院・洋館、スポーン地点、一部の装飾物は誤差や偽"
                      "陽性がありえます。既存ワールドの変更済みチャンク、MOD、データパックには対応しません。",
                      "Predicts biomes in all dimensions and the main structure types. It does not simulate "
                      "the entire game at block level. Terrain-dependent temples, mansions, spawn and some "
                      "decorations can be approximate or produce false positives. Modified chunks, mods and "
                      "datapacks are not modeled."));
                ImGui::Spacing();
                ImGui::TextWrapped(
                    "%s",
                    t("未対応: "
                      "オーバーワールドの化石、Javaのモンスタールーム、Bedrockのネザーの化石。一部の旧版機能"
                      "にも制限があります。モンスタールーム検索はBedrock 1.18以降で半径128ブロック以内です。",
                      "Unsupported: Overworld fossils, Java monster rooms, and Bedrock Nether fossils. Some "
                      "older features have additional limits. Bedrock monster rooms require 1.18+ and a "
                      "radius no larger than 128 blocks."));
                ImGui::Separator();
                ImGui::Text("%s / %s", s.world.edition ? "Bedrock" : "Java",
                            dimension_name(s.world.dimension, s.japanese));
                ImGui::BeginChild("coverage_list", {0, 170 * u});
                for (auto& k : kinds())
                    if (k.dimension == s.world.dimension) {
                        bool supported = probe->supports(k.kind);
                        ImGui::TextColored(color4(supported ? accent : muted), "%s  %s",
                                           supported ? "+" : "-", t(k.ja, k.en));
                    }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(t("ライセンス", "Licenses"))) {
                ImGui::Spacing();
                ImGui::BeginChild("license_text", {0, 470 * u});
                if (!license_text.empty()) {
                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted(license_text.c_str());
                    ImGui::PopTextWrapPos();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 48 * u);
        if (ImGui::Button(t("閉じる", "Close"), {110 * u, 32 * u}))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    void shortcuts() {
        auto& io = ImGui::GetIO();
        if (io.WantTextInput)
            return;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L))
            focus_seed = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F))
            find_open = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B))
            bookmarks_open = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G))
            center(jump_x, jump_z);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
            auto p = platform.choose_save( L"png", L"SeedLens.png");
            if (!p.empty())
                png = p;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F1))
            help_open = true;
        if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
            return;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) {
            std::random_device random;
            uint64_t seed = (uint64_t(random()) << 32) ^ random();
            snprintf(seed_input, sizeof(seed_input), "%s", signed_seed(seed).c_str());
            apply_seed();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Home) || ImGui::IsKeyPressed(ImGuiKey_Keypad7))
            center(0, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))
            zoom(0.75, {map_size.x / 2, map_size.y / 2});
        if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))
            zoom(1 / 0.75, {map_size.x / 2, map_size.y / 2});
        if (ImGui::IsKeyPressed(ImGuiKey_G) && !io.KeyCtrl)
            s.grid = !s.grid;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            selected.reset();
        float movement =
            600 * std::min(io.DeltaTime, 1.0f / 30) * float(s.blocks_per_pixel) * (io.KeyShift ? 2 : 1);
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
            s.x -= movement;
            mark_view();
        }
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
            s.x += movement;
            mark_view();
        }
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
            s.z -= movement;
            mark_view();
        }
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
            s.z += movement;
            mark_view();
        }
    }
    void frame(float dpi) {
        u = dpi;
        compact=platform.touch&&ImGui::GetIO().DisplaySize.x/u<1000;
        if(!layout_ready){sidebar_open=!platform.touch||!compact;layout_ready=true;}
        if(dismiss_popups){if(ImGui::GetCurrentContext()->OpenPopupStack.Size)ImGui::ClosePopupToLevel(0,true);dismiss_popups=false;}
        ++frame_count;
        style();
        sync_world();
        collect();
        shortcuts();
        animate_zoom();
        auto display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(display);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, color4(background));
        ImGui::Begin("SeedLensRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        topbar(display.x);
        sidebar(display.y);
        featurebar(display.x);
        map(display.x, display.y);
        status(display.x, display.y);
        ImGui::End();
        inspector();
        search_dialog();
        bookmark_dialog();
        help_dialog();
        if (!toast.empty() && ImGui::GetTime() - toast_time < 4) {
            ImGui::SetNextWindowPos({map_pos.x + map_size.x / 2, header_height()+16*u}, ImGuiCond_Always, {0.5f, 0});
            ImGui::SetNextWindowBgAlpha(0.98f);
            ImGui::Begin("##notification", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
            ImGui::TextColored(color4(accent), "%s", toast.c_str());
            ImGui::End();
        }
        if (ImGui::GetTime() - last_save > 10) {
            if (!saved_session || *saved_session != s) {
                if (save_session(settings, s))
                    saved_session = s;
            }
            last_save = ImGui::GetTime();
        }
    }
};
App::App(Platform services,Session s,std::filesystem::path p)
    :impl_(std::make_unique<Impl>(std::move(services),std::move(s),std::move(p))){}
App::~App() = default;
void App::frame(float dpi) {
    impl_->frame(dpi);
}
bool App::busy() const {
    return impl_->zooming || impl_->tiles.pending() || impl_->map_search.busy() || impl_->finder.busy() ||
           ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_RightArrow) ||
           ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_DownArrow) ||
           ImGui::IsMouseDragging(0) || ImGui::IsAnyItemActive();
}
const Session& App::session() const {
    return impl_->s;
}
std::optional<std::filesystem::path> App::take_export() {
    auto path = std::move(impl_->png);
    impl_->png.reset();
    return path;
}
PixelRect App::export_rect() const {
    return impl_->map_rect;
}
void App::notification(std::string message) {
    impl_->tell(std::move(message));
}
bool App::map_input_at(float x,float y) const{
    const auto& p=*impl_;
    return !(p.compact&&p.sidebar_open)&&ImGui::GetCurrentContext()->OpenPopupStack.empty()&&
        x>=p.map_pos.x&&y>=p.map_pos.y&&x<p.map_pos.x+p.map_size.x&&y<p.map_pos.y+p.map_size.y;
}
bool App::gesture(double factor,float x,float y,float dx,float dy){
    if(!map_input_at(x,y))return false;
    impl_->s.x-=dx*impl_->s.blocks_per_pixel;impl_->s.z-=dy*impl_->s.blocks_per_pixel;
    if(std::abs(factor-1)>0.0001)impl_->zoom(factor,{x-impl_->map_pos.x,y-impl_->map_pos.y});
    else impl_->mark_view();
    return true;
}
bool App::context_at(float x,float y){
    if(!map_input_at(x,y))return false;
    impl_->pending_context=ImVec2{x,y};return true;
}
bool App::back(){
    if(!ImGui::GetCurrentContext()->OpenPopupStack.empty()){impl_->dismiss_popups=true;return true;}
    if(impl_->compact&&impl_->sidebar_open){impl_->sidebar_open=false;return true;}
    if(impl_->selected){impl_->selected.reset();return true;}
    return false;
}
} // namespace seedlens
