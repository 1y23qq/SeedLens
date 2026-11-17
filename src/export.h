#pragma once
#include "engine.h"
#include <windows.h>
#include <string>

namespace seedlens {
bool write_png(const std::filesystem::path&, int width, int height, const uint32_t* rgba, std::string& error);
bool write_csv(const std::filesystem::path&, const World&, const std::vector<SLMarker>&, std::string& error);
std::filesystem::path save_dialog(HWND, const wchar_t* extension, const std::wstring& suggested);
std::string utf8(const std::wstring&);
std::wstring wide(const std::string&);
bool read_png_resource(int id, std::vector<uint32_t>& rgba, int& width, int& height);
} // namespace seedlens
