#include "mobile_platform.h"
#include <SDL3/SDL.h>
#include <mutex>
#include <utility>
#include <memory>
#if defined(SDL_PLATFORM_ANDROID)
#include <jni.h>
#endif

namespace seedlens {
static std::mutex notice_mutex;
static std::string pending_notice;
void mobile_set_notice(std::string notice){std::lock_guard lock(notice_mutex);pending_notice=std::move(notice);}
void mobile_notifications(std::string& value){std::lock_guard lock(notice_mutex);value=std::exchange(pending_notice,{});}
#if defined(SDL_PLATFORM_ANDROID)
void mobile_initialize(SDL_Window*){}
void mobile_export_file(const std::filesystem::path& path,const char* mime){
    auto* env=static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto activity=static_cast<jobject>(SDL_GetAndroidActivity());
    if(!env||!activity){mobile_set_notice("Export is unavailable.");return;}
    auto cls=env->GetObjectClass(activity);
    auto method=env->GetMethodID(cls,"exportFile","(Ljava/lang/String;Ljava/lang/String;)V");
    if(method){
        auto name=env->NewStringUTF(path.string().c_str()),type=env->NewStringUTF(mime);
        env->CallVoidMethod(activity,method,name,type);env->DeleteLocalRef(name);env->DeleteLocalRef(type);
    }
    if(env->ExceptionCheck()){env->ExceptionClear();mobile_set_notice("Export could not be opened.");}
    env->DeleteLocalRef(cls);env->DeleteLocalRef(activity);
}
extern "C" JNIEXPORT void JNICALL Java_com_seedlens_app_SeedLensActivity_nativeExportResult(JNIEnv* env,jclass,jstring text){
    auto* value=env->GetStringUTFChars(text,nullptr);mobile_set_notice(value?value:"");if(value)env->ReleaseStringUTFChars(text,value);
}
#elif !defined(SDL_PLATFORM_IOS)
void mobile_initialize(SDL_Window*){}
void mobile_export_file(const std::filesystem::path& path,const char*){
    auto* source=new std::filesystem::path(path);
    auto callback=[](void* user,const char*const* files,int){
        std::unique_ptr<std::filesystem::path> source(static_cast<std::filesystem::path*>(user));
        if(!files||!files[0])return;
        std::error_code error;std::filesystem::copy_file(*source,files[0],std::filesystem::copy_options::overwrite_existing,error);
        mobile_set_notice(error?"Could not save the file.":"保存しました / Saved");
    };
    SDL_ShowSaveFileDialog(callback,source,nullptr,nullptr,0,path.filename().string().c_str());
}
#endif
}
