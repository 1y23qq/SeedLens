#pragma once
#include "engine.h"
#include "platform.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <string>

namespace seedlens {
bool write_png(const std::filesystem::path&, int width, int height, const uint32_t* rgba, std::string& error);
bool write_csv(const std::filesystem::path&, const World&, const std::vector<SLMarker>&, std::string& error);
#ifdef _WIN32
std::filesystem::path save_dialog(HWND, const wchar_t* extension, const std::wstring& suggested);
#endif
std::string utf8(const std::wstring&);
std::wstring wide(const std::string&);
bool read_png_resource(int id, std::vector<uint32_t>& rgba, int& width, int& height);
} // namespace seedlens
