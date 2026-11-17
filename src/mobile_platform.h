#pragma once
#include <filesystem>
#include <string>
struct SDL_Window;
struct SDL_Renderer;
namespace seedlens {
void mobile_export_file(const std::filesystem::path&,const char* mime);
void mobile_initialize(SDL_Window*);
void mobile_notifications(std::string&);
}
