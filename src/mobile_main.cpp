#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "ui.h"
#include "export.h"
#include "mobile_platform.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>

int seedlens_core_tests_main();
namespace seedlens {
struct Finger {float x=0,y=0,start_x=0,start_y=0;Uint64 start=0;bool map=false,scroll=false,context=false;};
struct MobileHost {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* target=nullptr;
    SDL_Rect safe{};
    int target_w=0,target_h=0;
    float pixel_ratio=1,ui_scale=1;
    bool dirty=true,background=false,mobile=true;
    Uint64 next_frame=0;
    Uint32 wake_event=0;
    std::filesystem::path data_dir,settings;
    std::unique_ptr<App> app;
    std::map<SDL_FingerID,Finger> fingers;
    float pinch_distance=0,pinch_x=0,pinch_y=0;

    void update_safe(){
        int width=0,height=0,out_w=0,out_h=0;
        SDL_GetWindowSize(window,&width,&height);
        SDL_GetRenderOutputSize(renderer,&out_w,&out_h);
        if(!SDL_GetWindowSafeArea(window,&safe)||safe.w<=0||safe.h<=0)safe={0,0,width,height};
        int keyboard=mobile_keyboard_height();
        if(keyboard>0)safe.h=std::max(1,std::min(safe.h,height-keyboard-safe.y));
        pixel_ratio=width>0?float(out_w)/width:1;
        ui_scale=std::max(0.75f,SDL_GetWindowDisplayScale(window)/std::max(pixel_ratio,0.1f));
        if(target&&(target_w!=int(safe.w*pixel_ratio)||target_h!=int(safe.h*pixel_ratio))){
            SDL_DestroyTexture(target);target=nullptr;
        }
        target_w=std::max(1,int(safe.w*pixel_ratio));target_h=std::max(1,int(safe.h*pixel_ratio));
        if(!target)target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,target_w,target_h);
    }
    Platform platform(){
        Platform p;p.touch=mobile;
        p.wake=[event=wake_event]{SDL_Event message{};message.type=event;SDL_PushEvent(&message);};
        p.create_texture=[this](const uint32_t* pixels,int w,int h)->TextureHandle{
            auto* texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,w,h);
            if(!texture)return 0;
            SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
            if(!SDL_UpdateTexture(texture,nullptr,pixels,w*4)){SDL_DestroyTexture(texture);return 0;}
            return reinterpret_cast<TextureHandle>(texture);
        };
        p.release_texture=[](TextureHandle handle){if(handle)SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(handle));};
        p.image_resource=[](int id){ImageData image;read_png_resource(id,image.pixels,image.width,image.height);return image;};
        p.resource_data=[](int id){auto bytes=embedded_resource(id);return std::vector<uint8_t>(bytes.begin(),bytes.end());};
        p.choose_save=[this](const wchar_t* extension,const std::wstring& name){
            (void)extension;return data_dir/utf8(name);
        };
        p.finish_export=[](const std::filesystem::path& file,const char* mime){mobile_export_file(file,mime);};
        return p;
    }
    void fonts(){
        auto& io=ImGui::GetIO();float size=mobile?16.0f:15.0f;
        auto regular=embedded_resource(401),strong=embedded_resource(402);
        ImFont* body=io.Fonts->AddFontFromMemoryCompressedTTF(regular.data(),int(regular.size()),size);
        io.Fonts->AddFontFromMemoryCompressedTTF(strong.data(),int(strong.size()),size);
        io.FontDefault=body;
    }
    bool capture(const std::filesystem::path& path){
        PixelRect rect=app->export_rect();
        SDL_Rect crop{int(rect.left*pixel_ratio),int(rect.top*pixel_ratio),
            int((rect.right-rect.left)*pixel_ratio),int((rect.bottom-rect.top)*pixel_ratio)};
        crop.x=std::clamp(crop.x,0,target_w-1);crop.y=std::clamp(crop.y,0,target_h-1);
        crop.w=std::clamp(crop.w,1,target_w-crop.x);crop.h=std::clamp(crop.h,1,target_h-crop.y);
        auto* surface=SDL_RenderReadPixels(renderer,&crop);
        if(!surface)return false;
        auto* rgba=SDL_ConvertSurface(surface,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(surface);
        if(!rgba)return false;
        std::vector<uint32_t> pixels(size_t(rgba->w)*rgba->h);
        for(int y=0;y<rgba->h;++y)memcpy(pixels.data()+size_t(y)*rgba->w,static_cast<uint8_t*>(rgba->pixels)+size_t(y)*rgba->pitch,size_t(rgba->w)*4);
        std::string error;bool ok=write_png(path,rgba->w,rgba->h,pixels.data(),error);SDL_DestroySurface(rgba);
        if(ok){mobile_export_file(path,"image/png");app->notification("保存先を選択してください / Choose where to save");}
        else app->notification(error);
        return ok;
    }
    void touch(SDL_Event& event){
        auto& f=event.tfinger;float x=0,y=0;int w=0,h=0;SDL_GetWindowSize(window,&w,&h);
        x=f.x*w-safe.x;y=f.y*h-safe.y;
        if(event.type==SDL_EVENT_FINGER_DOWN){
            Finger finger;finger.x=finger.start_x=x;finger.y=finger.start_y=y;finger.start=SDL_GetTicks();
            finger.map=app->map_input_at(x,y);fingers[f.fingerID]=finger;
        }else if(event.type==SDL_EVENT_FINGER_MOTION){
            auto it=fingers.find(f.fingerID);if(it==fingers.end())return;
            float dy=y-it->second.y;it->second.x=x;it->second.y=y;
            if(fingers.size()==1&&!it->second.map&&std::hypot(x-it->second.start_x,y-it->second.start_y)>8*ui_scale){
                if(!it->second.scroll){it->second.scroll=true;ImGui::GetIO().AddMouseButtonEvent(0,false);ImGui::ClearActiveID();}
                ImGui::GetIO().AddMousePosEvent(x,y);ImGui::GetIO().AddMouseWheelEvent(0,dy/(44*ui_scale));dirty=true;
            }
        }else{
            fingers.erase(f.fingerID);
        }
        if(fingers.size()==2){
            auto first=fingers.begin(),second=std::next(first);
            float dx=second->second.x-first->second.x,dy=second->second.y-first->second.y;
            float distance=std::hypot(dx,dy),cx=(first->second.x+second->second.x)/2,cy=(first->second.y+second->second.y)/2;
            ImGui::GetIO().AddMouseButtonEvent(0,false);ImGui::ClearActiveID();
            if(pinch_distance>0&&distance>4)app->gesture(pinch_distance/distance,cx,cy,cx-pinch_x,cy-pinch_y);
            pinch_distance=distance;pinch_x=cx;pinch_y=cy;
        }else pinch_distance=0;
    }
};
}

SDL_AppResult SDL_AppInit(void** state,int argc,char** argv){
    using namespace seedlens;
    SDL_SetAppMetadata("SeedLens","1.1.0","com.seedlens.app");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON,"1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,"1");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE,"60");
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS))return SDL_APP_FAILURE;
    auto host=std::make_unique<MobileHost>();
#if !defined(SDL_PLATFORM_ANDROID) && !defined(SDL_PLATFORM_IOS)
    host->mobile=false;
#endif
    bool self_test=false;int width=390,height=844;std::string scene;
    for(int i=1;i<argc;++i){
        std::string arg=argv[i];
        if(arg=="--mobile-preview")host->mobile=true;
        else if(arg=="--self-test")self_test=true;
        else if(arg=="--scene"&&i+1<argc)scene=argv[++i];
        else if(arg=="--wide"){width=1180;height=820;}
    }
    char* preference=SDL_GetPrefPath("1y23qq","SeedLens");
    host->data_dir=preference?preference:".";SDL_free(preference);host->settings=host->data_dir/"session.ini";
    std::error_code ec;std::filesystem::create_directories(host->data_dir,ec);
#ifdef _WIN32
    _putenv_s("TMP",host->data_dir.string().c_str());
#else
    setenv("TMPDIR",host->data_dir.string().c_str(),1);
#endif
    if(self_test){
        int code=seedlens_core_tests_main();
        std::ofstream report(host->data_dir/"self-test.json");
        report<<"{\"ok\":"<<(code==0?"true":"false")<<",\"java_profiles\":"<<backend(0).version_count<<",\"bedrock_profiles\":"<<backend(1).version_count<<"}\n";
        report.close();SDL_Log("SeedLens self-test: %s",code==0?"PASS":"FAIL");
        return code==0?SDL_APP_SUCCESS:SDL_APP_FAILURE;
    }
    host->window=SDL_CreateWindow("SeedLens",width,height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(!host->window)return SDL_APP_FAILURE;
    host->renderer=SDL_CreateRenderer(host->window,nullptr);
    if(!host->renderer)return SDL_APP_FAILURE;
    SDL_SetRenderVSync(host->renderer,1);host->wake_event=SDL_RegisterEvents(1);
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    host->fonts();ImGui_ImplSDL3_InitForSDLRenderer(host->window,host->renderer);
    ImGui_ImplSDLRenderer3_Init(host->renderer);
    Session session;load_session(host->settings,session);
    if(scene=="nether")session.world.dimension=-1;
    if(scene=="end")session.world.dimension=1;
    host->app=std::make_unique<App>(host->platform(),std::move(session),host->settings);
    mobile_initialize(host->window);host->update_safe();
    *state=host.release();return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppEvent(void* state,SDL_Event* source){
    using namespace seedlens;auto& h=*static_cast<MobileHost*>(state);SDL_Event event=*source;h.dirty=true;
    if(event.type==SDL_EVENT_QUIT)return SDL_APP_SUCCESS;
    if(event.type==SDL_EVENT_WILL_ENTER_BACKGROUND){h.background=true;save_session(h.settings,h.app->session());}
    if(event.type==SDL_EVENT_DID_ENTER_FOREGROUND){h.background=false;h.next_frame=0;}
    if(event.type==SDL_EVENT_KEY_DOWN&&event.key.key==SDLK_AC_BACK)return h.app->back()?SDL_APP_CONTINUE:SDL_APP_SUCCESS;
    if(event.type==SDL_EVENT_FINGER_DOWN||event.type==SDL_EVENT_FINGER_MOTION||event.type==SDL_EVENT_FINGER_UP||event.type==SDL_EVENT_FINGER_CANCELED)h.touch(event);
    bool touch_drag=h.fingers.size()>1||(!h.fingers.empty()&&(h.fingers.begin()->second.scroll||h.fingers.begin()->second.context));
    if(event.type==SDL_EVENT_MOUSE_MOTION){
        if(event.motion.which==SDL_TOUCH_MOUSEID&&touch_drag)return SDL_APP_CONTINUE;
        event.motion.x-=h.safe.x;event.motion.y-=h.safe.y;
    }
    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_UP){
        if(event.button.which==SDL_TOUCH_MOUSEID&&touch_drag)return SDL_APP_CONTINUE;
        event.button.x-=h.safe.x;event.button.y-=h.safe.y;
    }
    ImGui_ImplSDL3_ProcessEvent(&event);return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppIterate(void* state){
    using namespace seedlens;auto& h=*static_cast<MobileHost*>(state);
    if(h.background)return SDL_APP_CONTINUE;
    Uint64 now=SDL_GetTicks();
    if(!h.dirty&&!h.app->busy()&&now<h.next_frame)return SDL_APP_CONTINUE;
    h.dirty=false;h.update_safe();if(!h.target)return SDL_APP_FAILURE;
    if(h.fingers.size()==1){
        auto& f=h.fingers.begin()->second;
        if(f.map&&!f.context&&now-f.start>600&&std::hypot(f.x-f.start_x,f.y-f.start_y)<8*h.ui_scale){
            f.context=h.app->context_at(f.x,f.y);if(f.context){ImGui::GetIO().AddMouseButtonEvent(0,false);ImGui::ClearActiveID();}
        }
    }
    ImGui_ImplSDLRenderer3_NewFrame();ImGui_ImplSDL3_NewFrame();
    auto& io=ImGui::GetIO();io.DisplaySize={float(h.safe.w),float(h.safe.h)};io.DisplayFramebufferScale={h.pixel_ratio,h.pixel_ratio};
    ImGui::GetStyle().FontScaleDpi=h.ui_scale;
    ImGui::NewFrame();h.app->frame(h.ui_scale);ImGui::Render();
    SDL_SetRenderTarget(h.renderer,h.target);SDL_SetRenderDrawColor(h.renderer,245,247,250,255);SDL_RenderClear(h.renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),h.renderer);
    if(auto file=h.app->take_export())h.capture(*file);
    std::string notice;mobile_notifications(notice);if(!notice.empty())h.app->notification(notice);
    SDL_SetRenderTarget(h.renderer,nullptr);SDL_SetRenderDrawColor(h.renderer,255,255,255,255);SDL_RenderClear(h.renderer);
    SDL_FRect destination{h.safe.x*h.pixel_ratio,h.safe.y*h.pixel_ratio,h.safe.w*h.pixel_ratio,h.safe.h*h.pixel_ratio};
    SDL_RenderTexture(h.renderer,h.target,nullptr,&destination);SDL_RenderPresent(h.renderer);
    h.next_frame=now+(h.app->busy()||!h.fingers.empty()?16:180);
    return SDL_APP_CONTINUE;
}
void SDL_AppQuit(void* state,SDL_AppResult){
    using namespace seedlens;auto* h=static_cast<MobileHost*>(state);if(!h){SDL_Quit();return;}
    if(h->app){save_session(h->settings,h->app->session());h->app.reset();}
    if(ImGui::GetCurrentContext()){ImGui_ImplSDLRenderer3_Shutdown();ImGui_ImplSDL3_Shutdown();ImGui::DestroyContext();}
    if(h->target)SDL_DestroyTexture(h->target);
    if(h->renderer)SDL_DestroyRenderer(h->renderer);
    if(h->window)SDL_DestroyWindow(h->window);
    delete h;SDL_Quit();
}
