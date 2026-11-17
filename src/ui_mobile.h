float header_height() const {return compact?(ImGui::GetIO().DisplaySize.x/u>=680?64*u:112*u):56*u;}
float footer_height() const {return compact&&ImGui::GetIO().DisplaySize.x/u<600?112*u:66*u;}
ImVec2 modal_size(float width,float height) const{
    auto display=ImGui::GetIO().DisplaySize;
    return {std::min(width*u,display.x-20*u),std::min(height*u,display.y-20*u)};
}
void version_picker(float width){
    const auto& api=backend(s.world.edition);
    std::string value=s.world.edition?"BE ":"Java ";
    for(int i=0;i<api.version_count;++i)if(api.versions[i].id==s.world.version){
        std::string name=api.versions[i].name;value+=name.substr(0,name.find(' '));if(name.find('*')!=std::string::npos)value+=" *";
    }
    ImGui::SetNextItemWidth(width);
    if(ImGui::BeginCombo("##mobile_version",value.c_str())){
        if(segment_button("Java",{95*u,40*u},s.world.edition==0))switch_edition(0);
        ImGui::SameLine();
        if(segment_button("Bedrock",{105*u,40*u},s.world.edition==1))switch_edition(1);
        ImGui::Separator();const auto& choices=backend(s.world.edition);
        for(int i=0;i<choices.version_count;++i){
            bool active=choices.versions[i].id==s.world.version;
            if(ImGui::Selectable(choices.versions[i].name,active,0,{0,36*u})){s.world.version=choices.versions[i].id;sync_world();}
            if(active)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}
void mobile_topbar(float width){
    bool horizontal=width/u>=680;
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled({0,0},{width,header_height()},panel);
    draw->AddLine({0,header_height()},{width,header_height()},line);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10*u,12*u});
    ImGui::SetCursorPos({12*u,8*u});
    if(focus_seed){ImGui::SetKeyboardFocusHere();focus_seed=false;}
    ImGui::SetNextItemWidth(horizontal?width-516*u:width-124*u);
    if(ImGui::InputTextWithHint("##mobile_seed",t("シード","Seed"),seed_input,sizeof(seed_input),ImGuiInputTextFlags_EnterReturnsTrue))apply_seed();
    ImGui::SameLine(0,8*u);
    if(icon_button("##mobile_apply","arrow-right",t("適用","Apply"),44*u))apply_seed();
    ImGui::SameLine(0,8*u);
    if(icon_button("##mobile_random","shuffle",t("ランダム","Random"),44*u)){
        std::random_device random;snprintf(seed_input,sizeof(seed_input),"%s",signed_seed((uint64_t(random())<<32)^random()).c_str());apply_seed();
    }
    if(horizontal)ImGui::SameLine(0,8*u);else ImGui::SetCursorPos({12*u,60*u});
    float field=horizontal?180*u:(width-84*u)/2;
    version_picker(field);ImGui::SameLine(0,8*u);ImGui::SetNextItemWidth(horizontal?140*u:field);
    const char* name=s.world.dimension==0?t("地上","Overworld"):s.world.dimension==-1?t("ネザー","Nether"):t("エンド","The End");
    if(ImGui::BeginCombo("##mobile_dim",name)){
        for(int dim:{0,-1,1}){
            ImGui::BeginDisabled(!dimension_available(s.world,dim));
            if(ImGui::Selectable(dimension_name(dim,s.japanese),s.world.dimension==dim,0,{0,36*u}))switch_dimension(dim);
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0,8*u);
    if(icon_button("##mobile_menu","settings-2",t("メニュー","Menu"),44*u))ImGui::OpenPopup("mobile_menu");
    if(ImGui::BeginPopup("mobile_menu")){
        if(ImGui::MenuItem(t("ブックマーク","Bookmarks")))bookmarks_open=true;
        if(ImGui::MenuItem(t("地図をPNGで保存","Save map as PNG"))){
            auto p=platform.choose_save(L"png",L"SeedLens-"+wide(signed_seed(s.world.seed))+L".png");if(!p.empty())png=p;
        }
        if(ImGui::MenuItem(t("構造物をCSVで保存","Export structures as CSV"),nullptr,false,map_result.has_value()))export_csv(false);
        if(ImGui::MenuItem(t("シードを貼り付け","Paste seed"))){
            const char* value=ImGui::GetClipboardText();if(value){snprintf(seed_input,sizeof(seed_input),"%s",value);apply_seed();}
        }
        if(ImGui::MenuItem("日本語 / English"))s.japanese=!s.japanese;
        ImGui::BeginDisabled(s.world.edition!=0);
        if(ImGui::Checkbox(t("巨大バイオーム","Large biomes"),&s.world.large))sync_world();
        ImGui::EndDisabled();
        if(ImGui::MenuItem(t("使い方・対応範囲","Help & coverage")))help_open=true;
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}
void mobile_status(float width,float height){
    float top=height-footer_height();bool narrow=width/u<600;
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled({0,top},{width,height},panel);draw->AddLine({0,top},{width,top},line);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{7*u,11*u});
    ImGui::SetCursorPos({12*u,top+6*u});
    float input=narrow?(width-104*u)/2:115*u;
    ImGui::SetNextItemWidth(input);
    bool go=ImGui::InputInt("##mobile_x",&jump_x,0,0,ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0,8*u);ImGui::SetNextItemWidth(input);
    go|=ImGui::InputInt("##mobile_z",&jump_z,0,0,ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0,8*u);
    go|=icon_button("##mobile_goto","locate-fixed",t("座標へ移動","Go to coordinates"),44*u);
    if(go)go_to_inputs();
    if(narrow)ImGui::SetCursorPos({12*u,top+54*u});else ImGui::SameLine(0,14*u);
    if(icon_button("##mobile_layers","layers",t("表示項目","Features"),40*u,sidebar_open))sidebar_open=!sidebar_open;
    ImGui::SameLine(0,8*u);
    if(icon_button("##mobile_search","search",t("場所を探す","Find locations"),40*u))find_open=true;
    ImGui::SameLine(0,8*u);ImGui::SetNextItemWidth(narrow?96*u:120*u);
    std::string y=s.world.y==320?t("地上","Surface"):"Y "+std::to_string(s.world.y);
    if(ImGui::BeginCombo("##mobile_y",y.c_str())){
        for(int value:{320,0,-32,-51}){std::string label=value==320?t("地上","Surface"):"Y "+std::to_string(value);if(ImGui::Selectable(label.c_str(),s.world.y==value,0,{0,36*u})){s.world.y=value;sync_world();}}
        ImGui::Separator();int edit=s.world.y;ImGui::SetNextItemWidth(100*u);
        if(ImGui::InputInt("Y##mobile_yinput",&edit,0,0,ImGuiInputTextFlags_EnterReturnsTrue)){s.world.y=std::clamp(edit,-64,320);sync_world();}
        ImGui::EndCombo();
    }
    ImGui::SameLine(0,8*u);
    if(icon_button("##mobile_grid","grid-2x2",t("境界・スライム","Grid / slime"),40*u,s.grid||s.slime))ImGui::OpenPopup("mobile_grid");
    if(ImGui::BeginPopup("mobile_grid")){
        ImGui::Checkbox(t("チャンク境界","Chunk grid"),&s.grid);
        ImGui::BeginDisabled(s.world.dimension!=0);ImGui::Checkbox(t("スライムチャンク","Slime chunks"),&s.slime);ImGui::EndDisabled();
        if(ImGui::MenuItem(t("推定スポーン地点へ","Estimated world spawn"))){SearchRequest req;req.world=s.world;req.world.dimension=0;req.spawn=true;find_request=finder.submit(req);}
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    if(narrow){ImGui::SetCursorPos({width-86*u,top+68*u});ImGui::TextDisabled("X / Z");}
    else{
        ImGui::SetCursorPos({12*u,top+47*u});
        std::string label=biome_label(s.world,probe->biome(int(s.x),s.world.y,int(s.z)),s.japanese);
        ImGui::TextColored(color4(muted),"%s   X %d  Z %d",label.c_str(),int(s.x),int(s.z));
    }
}
void mobile_search_results(float height){
    ImGui::BeginChild("mobile_results",{0,std::max(height,70*u)});
    if(find_result)for(size_t i=0;i<find_result->markers.size();++i){
        const auto& m=find_result->markers[i];
        std::string label=m.kind==-2?biome_label(s.world,m.biome,s.japanese):t(kinds()[m.kind].ja,kinds()[m.kind].en);
        if(m.flags&SL_END_SHIP)label+=t(" (シップ)"," (ship)");
        if(!matches(find_filter,label,coordinates(m.x,m.z)))continue;
        ImGui::PushID(int(i));auto p=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;
        if(ImGui::Selectable("##result",false,0,{width,60*u})){
            selected=m;center(m.x,m.z);ImGui::ClearActiveID();ImGui::CloseCurrentPopup();
        }
        auto* draw=ImGui::GetWindowDrawList();
        if(m.kind>=0)game_sprite(draw,m.kind,{p.x+4*u,p.y+14*u},28*u,m.flags);
        else draw->AddRectFilled({p.x+7*u,p.y+16*u},{p.x+29*u,p.y+38*u},biome_color(m.biome,legacy_palette(s.world)),3*u);
        draw->AddText({p.x+42*u,p.y+7*u},text,label.c_str());
        std::string position="X "+std::to_string(m.x)+"   Z "+std::to_string(m.z);
        draw->AddText({p.x+42*u,p.y+33*u},muted,position.c_str());
        draw->AddLine({p.x,p.y+60*u},{p.x+width,p.y+60*u},line);
        ImGui::PopID();
    }
    ImGui::EndChild();
}
