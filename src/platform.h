#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace seedlens {
using TextureHandle=uintptr_t;
struct PixelRect {int left=0,top=0,right=0,bottom=0;};
struct ImageData {int width=0,height=0;std::vector<uint32_t> pixels;};
struct Platform {
    bool touch=false;
    std::function<void()> wake;
    std::function<TextureHandle(const uint32_t*,int,int)> create_texture;
    std::function<void(TextureHandle)> release_texture;
    std::function<ImageData(int)> image_resource;
    std::function<std::vector<uint8_t>(int)> resource_data;
    std::function<std::filesystem::path(const wchar_t*,const std::wstring&)> choose_save;
    std::function<void(const std::filesystem::path&,const char*)> finish_export;
};
#ifdef _WIN32
Platform make_windows_platform(void* window,void* device);
#endif
std::span<const uint8_t> embedded_resource(int id);
}
