float sidebar_width() const {
    return sidebar_open&&!compact ? 252 * u : 0;
}

void load_game_sprites(){
    auto image=platform.image_resource(300);
    if(!image.pixels.empty())game_atlas=platform.create_texture(image.pixels.data(),image.width,image.height);
}

void game_sprite(ImDrawList* draw, int kind, ImVec2 p, float size, unsigned flags = 0,
                 float opacity = 1) const {
    if (!game_atlas) {
        icon(draw, kind >= 0 ? kinds()[kind].icon : "compass", p, size, text);
        return;
    }
    int index = minecraft::sprite_index(kind, flags);
    float x = float(index % minecraft::columns * minecraft::cell + minecraft::inset);
    float y = float(index / minecraft::columns * minecraft::cell + minecraft::inset);
    ImVec2 uv0{x / minecraft::width, y / minecraft::height};
    ImVec2 uv1{(x + minecraft::content) / minecraft::width, (y + minecraft::content) / minecraft::height};
    draw->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
    draw->AddImage(ImTextureRef(ImTextureID(game_atlas)), p, {p.x + size, p.y + size}, uv0, uv1,
                   IM_COL32(255, 255, 255, int(255 * opacity)));
    draw->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void go_to_inputs() {
    jump_x = std::clamp(jump_x, -world_limit, world_limit);
    jump_z = std::clamp(jump_z, -world_limit, world_limit);
    center(jump_x, jump_z);
    SLMarker m{};
    m.kind = -2;
    m.x = jump_x;
    m.z = jump_z;
    m.y = s.world.y;
    m.biome = probe->biome(m.x, m.y, m.z);
    selected = m;
}

void topbar(float width) {
    if(compact){mobile_topbar(width);return;}
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled({0, 0}, {width, 56 * u}, panel);
    draw->AddLine({0, 55 * u}, {width, 55 * u}, line);
    ImGui::SetCursorPos({16 * u, 11 * u});
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(color4(muted), "%s", t("シード", "Seed"));
    ImGui::SameLine(0, 12 * u);
    if (focus_seed) {
        ImGui::SetKeyboardFocusHere();
        focus_seed = false;
    }
    ImGui::SetNextItemWidth(std::clamp(width / u - 770.0f, 210.0f, 340.0f) * u);
    bool invalid_seed = !error.empty();
    if (invalid_seed)
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(219, 117, 105, 255));
    if (ImGui::InputText("##seed", seed_input, sizeof(seed_input), ImGuiInputTextFlags_EnterReturnsTrue))
        apply_seed();
    if (invalid_seed) {
        ImGui::PopStyleColor();
        if (!error.empty())
            hint(error.c_str());
    }
    ImGui::SameLine(0, 6 * u);
    if (icon_button("##apply", "arrow-right", t("シードを適用  Enter", "Apply seed  Enter"), 32 * u))
        apply_seed();
    ImGui::SameLine(0, 5 * u);
    if (icon_button("##random", "shuffle", t("ランダムなシード  R", "Random seed  R"), 32 * u)) {
        std::random_device random;
        uint64_t seed = (uint64_t(random()) << 32) ^ random();
        snprintf(seed_input, sizeof(seed_input), "%s", signed_seed(seed).c_str());
        apply_seed();
    }
    ImGui::SameLine(0, 18 * u);
    const auto& api = backend(s.world.edition);
    std::string version = s.world.edition ? "Bedrock " : "Java ";
    for (int i = 0; i < api.version_count; ++i)
        if (api.versions[i].id == s.world.version)
            version += api.versions[i].name;
    ImGui::SetNextItemWidth(212 * u);
    if (ImGui::BeginCombo("##editionversion", version.c_str())) {
        if (ImGui::Button("Java", {91 * u, 28 * u}))
            switch_edition(0);
        ImGui::SameLine(0, 6 * u);
        if (ImGui::Button("Bedrock", {91 * u, 28 * u}))
            switch_edition(1);
        ImGui::Separator();
        const auto& choices = backend(s.world.edition);
        for (int i = 0; i < choices.version_count; ++i) {
            bool active = s.world.version == choices.versions[i].id;
            if (ImGui::Selectable(choices.versions[i].name, active)) {
                s.world.version = choices.versions[i].id;
                sync_world();
            }
            if (active)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    hint(t("Java / Bedrock と生成バージョン。* はプレビュー由来の実装です。",
           "Edition and generation version. * indicates an engine derived from a preview."));
    ImGui::SameLine(0, 12 * u);
    ImGui::SetNextItemWidth(173 * u);
    if (ImGui::BeginCombo("##dimension", dimension_name(s.world.dimension, s.japanese))) {
        for (int dim : {0, -1, 1}) {
            ImGui::BeginDisabled(!dimension_available(s.world, dim));
            if (ImGui::Selectable(dimension_name(dim, s.japanese), s.world.dimension == dim))
                switch_dimension(dim);
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::SetCursorPos({width - 166 * u, 11 * u});
    if (icon_button("##bookmarks", "bookmark", t("ブックマーク  Ctrl+B", "Bookmarks  Ctrl+B"), 32 * u))
        bookmarks_open = true;
    ImGui::SameLine(0, 6 * u);
    if (icon_button("##exports", "download", t("PNG / CSVを保存", "Export PNG / CSV"), 32 * u))
        ImGui::OpenPopup("exports");
    ImGui::SameLine(0, 6 * u);
    if (ImGui::Button(s.japanese ? "EN##lang" : "JP##lang", {32 * u, 32 * u}))
        s.japanese = !s.japanese;
    ImGui::SameLine(0, 6 * u);
    if (icon_button("##help", "circle-help", t("使い方・対応範囲  F1", "Help & coverage  F1"), 32 * u))
        help_open = true;
    if (ImGui::BeginPopup("exports")) {
        if (ImGui::MenuItem(t("地図をPNGで保存", "Save map as PNG"), "Ctrl+S")) {
            auto path = platform.choose_save( L"png", L"SeedLens-" + wide(signed_seed(s.world.seed)) + L".png");
            if (!path.empty())
                png = path;
        }
        if (ImGui::MenuItem(t("構造物をCSVで保存", "Export structures as CSV"), nullptr, false,
                            map_result.has_value()))
            export_csv(false);
        ImGui::EndPopup();
    }
}

void sidebar(float height) {
    if (!sidebar_open)
        return;
    if(compact){
        ImGui::SetNextWindowPos({12*u,12*u});
        ImGui::SetNextWindowSize({std::min(340*u,ImGui::GetIO().DisplaySize.x-24*u),std::max(160*u,height-24*u)});
        ImGui::Begin("##mobile_features",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove);
        ImGui::TextUnformatted(t("表示項目","Features"));ImGui::SameLine(ImGui::GetWindowWidth()-52*u);
        if(icon_button("##close_features","x",t("閉じる","Close"),32*u))sidebar_open=false;
        ImGui::Separator();
    }else{
    ImGui::SetCursorPos({0, 56 * u});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12 * u, 12 * u});
    ImGui::BeginChild("sidebar", {252 * u, height - 122 * u}, ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    }
    float width = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {7 * u, 6 * u});
    if (segment_button(t("構造物##layerstab", "Structures##layerstab"), {(width - 5 * u) / 2, 30 * u},
                       layers_tab == 0))
        layers_tab = 0;
    ImGui::SameLine(0, 5 * u);
    if (segment_button(t("バイオーム##biomestab", "Biomes##biomestab"), {(width - 5 * u) / 2, 30 * u},
                       layers_tab == 1))
        layers_tab = 1;
    ImGui::PopStyleVar();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##layerfilter", t("表示項目を検索…", "Filter features…"), layer_filter,
                             sizeof(layer_filter));
    if (layers_tab == 0) {
        if (ImGui::SmallButton(t("すべて", "All"))) {
            for (auto& k : kinds())
                if (probe->supports(k.kind))
                    s.layers |= uint64_t(1) << k.kind;
            mark_view();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(t("解除", "None"))) {
            for (auto& k : kinds())
                if (k.dimension == s.world.dimension)
                    s.layers &= ~(uint64_t(1) << k.kind);
            mark_view();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(t("標準", "Default"))) {
            s.layers = default_layers();
            mark_view();
        }
    } else if (ImGui::SmallButton(t("ハイライト解除", "Clear highlight"))) {
        highlight = -1;
        for (auto& [key, tile] : cache)
            set_texture(tile);
    }
    ImGui::Separator();
    float list_height = std::max(80 * u, ImGui::GetContentRegionAvail().y - 48 * u);
    ImGui::BeginChild("featurelist", {0, list_height}, 0, ImGuiWindowFlags_NoBackground);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {5 * u, 2 * u});
    if (layers_tab == 0) {
        for (auto& k : kinds()) {
            if (!probe->supports(k.kind) || !matches(layer_filter, k.en, k.ja))
                continue;
            ImGui::PushID(k.kind);
            auto p = ImGui::GetCursorScreenPos();
            float row_width = ImGui::GetContentRegionAvail().x;
            bool enabled = (s.layers & (uint64_t(1) << k.kind)) != 0;
            if (ImGui::Selectable("##feature", false, 0, {row_width, 32 * u})) {
                s.layers ^= uint64_t(1) << k.kind;
                mark_view();
            }
            auto* draw = ImGui::GetWindowDrawList();
            game_sprite(draw, k.kind, {p.x + 3 * u, p.y + 4 * u}, 24 * u, 0, enabled ? 1.0f : 0.45f);
            draw->AddText({p.x + 36 * u, p.y + 7 * u}, enabled ? text : muted, t(k.ja, k.en));
            ImVec2 check{p.x + row_width - 19 * u, p.y + 9 * u};
            draw->AddRectFilled(check, {check.x + 14 * u, check.y + 14 * u},
                                enabled ? IM_COL32(239, 246, 255, 255) : IM_COL32(255, 255, 255, 255), 3 * u);
            draw->AddRect(check, {check.x + 14 * u, check.y + 14 * u}, enabled ? accent : line, 3 * u);
            if (enabled)
                icon(draw, "check", {check.x + 1 * u, check.y + 1 * u}, 12 * u, accent);
            if (ImGui::IsItemHovered()&&ImGui::GetIO().MouseSource!=ImGuiMouseSource_TouchScreen) {
                ImGui::BeginTooltip();
                game_sprite(ImGui::GetWindowDrawList(), k.kind, ImGui::GetCursorScreenPos(), 28 * u);
                ImGui::Dummy({34 * u, 28 * u});
                ImGui::SameLine();
                ImGui::TextUnformatted(t(k.ja, k.en));
                if (k.dense)
                    ImGui::TextDisabled("%s",
                                        k.kind == SL_DUNGEON
                                            ? t("検索半径128ブロック以内", "Search radius up to 128 blocks")
                                            : t("広域では拡大して表示", "Zoom in to show this layer"));
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
    } else {
        const auto& api = backend(s.world.edition);
        for (int id = 0; id < 256; ++id) {
            if (!api.biome_exists(s.world.version, id, s.world.dimension))
                continue;
            std::string label = biome_label(s.world, id, s.japanese);
            if (!matches(layer_filter, label, biome_label(s.world, id, false)))
                continue;
            ImGui::PushID(id);
            auto p = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##biome", highlight == id, 0,
                                  {ImGui::GetContentRegionAvail().x, 30 * u})) {
                highlight = highlight == id ? -1 : id;
                for (auto& [key, tile] : cache)
                    set_texture(tile);
            }
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled({p.x + 6 * u, p.y + 8 * u}, {p.x + 23 * u, p.y + 25 * u},
                                biome_color(id, legacy_palette(s.world)), 2 * u);
            draw->AddText({p.x + 35 * u, p.y + 6 * u}, text, label.c_str());
            ImGui::PopID();
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();
    if (action_button("find", "search", t("場所を探す", "Find locations"), {width, 32 * u})) {
        find_open = true;
        find_biome = layers_tab == 1;
    }
    if(compact)ImGui::End();else ImGui::EndChild();
}

void featurebar(float width) {
    if (sidebar_open||compact)
        return;
    float left = sidebar_width();
    ImGui::SetCursorPos({left, 56 * u});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10 * u, 5 * u});
    ImGui::BeginChild("featurestrip", {width - left, 44 * u}, ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (icon_button("##togglepanel", "layers", t("表示項目パネル", "Features panel"), 30 * u, sidebar_open))
        sidebar_open = !sidebar_open;
    for (auto& k : kinds()) {
        if (!probe->supports(k.kind))
            continue;
        ImGui::SameLine(0, 5 * u);
        ImGui::PushID(k.kind);
        bool enabled = (s.layers & (uint64_t(1) << k.kind)) != 0;
        auto p = ImGui::GetCursorScreenPos();
        if (enabled) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(231, 240, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(147, 197, 253, 255));
        }
        if (ImGui::Button("##sprite", {30 * u, 30 * u})) {
            s.layers ^= uint64_t(1) << k.kind;
            mark_view();
        }
        if (enabled)
            ImGui::PopStyleColor(2);
        game_sprite(ImGui::GetWindowDrawList(), k.kind, {p.x + 3 * u, p.y + 3 * u}, 24 * u, 0,
                    enabled ? 1.0f : 0.45f);
        hint(t(k.ja, k.en));
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void draw_axes(ImDrawList* draw) {
    int spacing = 16;
    while (spacing / s.blocks_per_pixel < 150 * u)
        spacing *= 2;
    auto [x0, z0] = world_at(map_pos);
    auto [x1, z1] = world_at({map_pos.x + map_size.x, map_pos.y + map_size.y});
    auto badge = [&](const std::string& value, ImVec2 p) {
        ImVec2 sz = ImGui::CalcTextSize(value.c_str());
        float pad = 5 * u;
        draw->AddRectFilled({p.x - pad, p.y - 1 * u}, {p.x + sz.x + pad, p.y + sz.y + 2 * u},
                            IM_COL32(28, 33, 39, 210), 5 * u);
        draw->AddText(p, IM_COL32(239, 242, 246, 255), value.c_str());
    };
    for (int x = floor_div(x0, spacing) * spacing; x <= x1; x += spacing) {
        auto p = screen(x, 0);
        std::string label = std::to_string(x);
        auto sz = ImGui::CalcTextSize(label.c_str());
        if (p.x > map_pos.x + 50 * u && p.x < map_pos.x + map_size.x - 55 * u)
            badge(label, {p.x - sz.x / 2, map_pos.y + 7 * u});
    }
    for (int z = floor_div(z0, spacing) * spacing; z <= z1; z += spacing) {
        auto p = screen(0, z);
        if (p.y > map_pos.y + 35 * u && p.y < map_pos.y + map_size.y - 55 * u)
            badge(std::to_string(z), {map_pos.x + 7 * u, p.y - 7 * u});
    }
}

void status(float width, float height) {
    if(compact){mobile_status(width,height);return;}
    float top = height - 66 * u;
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled({0, top}, {width, height}, panel);
    draw->AddLine({0, top}, {width, top}, line);
    ImGui::SetCursorPos({16 * u, top + 7 * u});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8 * u, 5 * u});
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("X");
    ImGui::SameLine(0, 6 * u);
    ImGui::SetNextItemWidth(104 * u);
    bool go = ImGui::InputInt("##jumpx", &jump_x, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0, 12 * u);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Z");
    ImGui::SameLine(0, 6 * u);
    ImGui::SetNextItemWidth(104 * u);
    go |= ImGui::InputInt("##jumpz", &jump_z, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0, 6 * u);
    go |= icon_button("##goto", "arrow-right", t("座標へ移動", "Go to coordinates"), 28 * u);
    if (go)
        go_to_inputs();
    ImGui::SameLine(0, 10 * u);
    if (icon_button("##spawn", "crosshair", t("推定スポーン地点へ", "Estimated world spawn"), 28 * u)) {
        SearchRequest req;
        req.world = s.world;
        req.world.dimension = 0;
        req.spawn = true;
        find_request = finder.submit(req);
        tell(t("スポーン地点を計算しています…", "Calculating estimated world spawn…"));
    }
    ImGui::SameLine(0, 18 * u);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Y");
    ImGui::SameLine(0, 6 * u);
    ImGui::SetNextItemWidth(151 * u);
    std::string slice = s.world.y == 320   ? t("地上", "Surface")
                        : s.world.y == 0   ? t("地下 (Y 0)", "Underground")
                        : s.world.y == -51 ? t("最下層 (Y -51)", "Bottom (Y -51)")
                                           : "Y " + std::to_string(s.world.y);
    if (ImGui::BeginCombo("##slice", slice.c_str())) {
        for (int y : {320, 0, -32, -51}) {
            std::string label = y == 320 ? t("地上", "Surface") : "Y " + std::to_string(y);
            if (ImGui::Selectable(label.c_str(), s.world.y == y)) {
                s.world.y = y;
                sync_world();
            }
        }
        ImGui::Separator();
        ImGui::SetNextItemWidth(130 * u);
        int custom_y = s.world.y;
        if (ImGui::InputInt("Y##custom", &custom_y, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
            s.world.y = std::clamp(custom_y, -64, 320);
            sync_world();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, 18 * u);
    ImGui::Checkbox(t("チャンク境界", "Grid"), &s.grid);
    ImGui::SameLine(0, 15 * u);
    ImGui::BeginDisabled(s.world.dimension != 0);
    ImGui::Checkbox(t("スライム", "Slime"), &s.slime);
    ImGui::EndDisabled();
    ImGui::SameLine(0, 12 * u);
    if (icon_button("##worldsettings", "settings-2", t("ワールド設定", "World options"), 28 * u))
        ImGui::OpenPopup("world_options");
    if (ImGui::BeginPopup("world_options")) {
        ImGui::BeginDisabled(s.world.edition != 0);
        if (ImGui::Checkbox(t("巨大バイオーム", "Large biomes"), &s.world.large))
            sync_world();
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    bool hover = ImGui::GetIO().MousePos.x >= map_pos.x && ImGui::GetIO().MousePos.y >= map_pos.y &&
                 ImGui::GetIO().MousePos.y < map_pos.y + map_size.y;
    auto [x, z] = hover ? world_at(ImGui::GetIO().MousePos) : std::pair<int, int>{int(s.x), int(s.z)};
    ImGui::SetCursorPos({16 * u, top + 43 * u});
    ImGui::TextColored(color4(muted), "X %d  Z %d", x, z);
    ImGui::SameLine(227 * u);
    ImGui::TextColored(color4(muted), "%s %d, %d", t("チャンク", "Chunk"), floor_div(x, 16),
                       floor_div(z, 16));
    ImGui::SameLine(425 * u);
    int biome = probe->biome(x, s.world.y, z);
    ImGui::TextColored(color4(muted), "%s", biome_label(s.world, biome, s.japanese).c_str());
    std::string progress;
    if (tiles.pending() || zooming || view_dirty)
        progress = t("地図を生成中…", "Generating map…");
    else if (map_search.busy())
        progress = t("構造物を検索中…", "Scanning structures…");
    else if (map_result) {
        progress = std::to_string(map_result->markers.size()) + t(" か所", " locations");
        if (map_result->truncated)
            progress += " +";
        if (map_result->omitted)
            progress += t("  ·  拡大で追加表示", "  ·  zoom for more");
    } else
        progress = t("構造物は拡大すると表示", "Zoom in for structures");
    float length = ImGui::CalcTextSize(progress.c_str()).x;
    ImGui::SameLine(std::max(680 * u, width - length - 16 * u));
    ImGui::TextColored(color4(tiles.pending() || map_search.busy() ? accent : muted), "%s", progress.c_str());
}

void map(float width, float height) {
    float left = sidebar_width() + 1 * u;
    float top = compact?header_height():(sidebar_open ? 56.0f : 100.0f) * u;
    ImGui::SetCursorPos({left, top});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::BeginChild("map", {width - left, std::max(80*u,height - top - footer_height())}, 0,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    auto old_size = map_size;
    map_pos = ImGui::GetCursorScreenPos();
    map_size = ImGui::GetContentRegionAvail();
    if (map_size.x != old_size.x || map_size.y != old_size.y)
        mark_view();
    map_rect = {int(map_pos.x), int(map_pos.y), int(map_pos.x + map_size.x), int(map_pos.y + map_size.y)};
    auto* draw = ImGui::GetWindowDrawList();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    float right = map_pos.x + map_size.x, bottom = map_pos.y + map_size.y;
    bool top_tools=!png&&map_size.y>=250*u,bottom_tools=!png&&map_size.y>=150*u;
    bool over_tools = mouse.x >= right - 51 * u && mouse.x <= right - 10 * u &&
                      ((top_tools&&mouse.y >= map_pos.y + 10 * u && mouse.y <= map_pos.y + 85 * u) ||
                       (bottom_tools&&mouse.y >= bottom - 132 * u && mouse.y <= bottom - 19 * u));
    bool hovered = false, active = false, was_dragging = dragging;
    if (over_tools && !dragging)
        ImGui::Dummy(map_size);
    else {
        ImGui::InvisibleButton("##canvas", map_size,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        hovered = ImGui::IsItemHovered();
        active = ImGui::IsItemActive();
    }
    if (active && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3 * u) ||
                   ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 3 * u))) {
        zooming = false;
        target_bpp = s.blocks_per_pixel;
        dragging = true;
        s.x -= ImGui::GetIO().MouseDelta.x * s.blocks_per_pixel;
        s.z -= ImGui::GetIO().MouseDelta.y * s.blocks_per_pixel;
        mark_view();
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        zoom(std::pow(0.75, ImGui::GetIO().MouseWheel), {mouse.x - map_pos.x, mouse.y - map_pos.y});
    }
    if (!platform.touch&&hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        zoom(ImGui::GetIO().KeyShift ? 2.0 : 0.5, {mouse.x - map_pos.x, mouse.y - map_pos.y});
    }
    draw->PushClipRect(map_pos, {map_pos.x + map_size.x, map_pos.y + map_size.y}, true);
    draw->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
    draw_tiles(draw);
    draw->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    draw_markers(draw, hovered);
    draw_axes(draw);
    draw->PopClipRect();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        if (was_dragging)
            last_view_change = ImGui::GetTime();
        dragging = false;
    }
    request_map();
    float tx = map_pos.x + map_size.x - 47 * u;
    if(top_tools){
    ImGui::SetCursorScreenPos({tx, map_pos.y + 13 * u});
    if (icon_button("##mapfind", "search", t("場所を探す  Ctrl+F", "Find locations  Ctrl+F"), 32 * u))
        find_open = true;
    ImGui::SetCursorScreenPos({tx, map_pos.y + 51 * u});
    if (icon_button("##expandmap", "maximize", t("地図を広く表示", "Expand map"), 32 * u, !sidebar_open))
        sidebar_open = !sidebar_open;
    }
    float by = map_pos.y + map_size.y;
    if(bottom_tools){
    ImGui::SetCursorScreenPos({tx, by - 129 * u});
    if (icon_button("##zoomplus", "plus", t("拡大  +", "Zoom in  +"), 32 * u))
        zoom(0.75, {map_size.x / 2, map_size.y / 2});
    ImGui::SetCursorScreenPos({tx, by - 91 * u});
    if (icon_button("##zoomminus", "minus", t("縮小  -", "Zoom out  -"), 32 * u))
        zoom(1 / 0.75, {map_size.x / 2, map_size.y / 2});
    ImGui::SetCursorScreenPos({tx, by - 53 * u});
    if (icon_button("##origin", "locate-fixed", t("原点へ  Home", "Origin  Home"), 32 * u))
        center(0, 0);
    }
    double desired = s.blocks_per_pixel * 110 * u, power = std::pow(10, std::floor(std::log10(desired))),
           distance = power;
    for (double m : {1.0, 2.0, 5.0})
        if (m * power <= desired)
            distance = m * power;
    float length = float(distance / s.blocks_per_pixel), bx = map_pos.x + 19 * u, sy = by - 22 * u;
    draw->AddRectFilled({bx - 7 * u, sy - 24 * u}, {bx + std::max(length, 80 * u) + 8 * u, sy + 8 * u},
                        IM_COL32(255, 255, 255, 238), 4 * u);
    draw->AddLine({bx, sy}, {bx + length, sy}, text, 2 * u);
    draw->AddLine({bx, sy - 5 * u}, {bx, sy + 1 * u}, text, 2 * u);
    draw->AddLine({bx + length, sy - 5 * u}, {bx + length, sy + 1 * u}, text, 2 * u);
    char label[80];
    snprintf(label, sizeof(label), "%.0f blocks", distance);
    draw->AddText({bx, sy - 21 * u}, text, label);
    if (s.slime && s.blocks_per_pixel > 2) {
        const char* message = t("スライムチャンクは拡大すると表示", "Zoom in to see slime chunks");
        auto sz = ImGui::CalcTextSize(message);
        draw->AddRectFilled({tx - sz.x - 18 * u, by - 40 * u}, {tx - 7 * u, by - 12 * u}, panel, 4 * u);
        draw->AddText({tx - sz.x - 13 * u, by - 35 * u}, muted, message);
    }
    ImGui::EndChild();
}
