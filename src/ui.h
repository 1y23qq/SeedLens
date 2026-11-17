#pragma once
#include "engine.h"
#include <d3d11.h>
#include <windows.h>

namespace seedlens {
class App {
  public:
    App(HWND, ID3D11Device*, Session, std::filesystem::path);
    ~App();
    void frame(float dpi);
    bool busy() const;
    const Session& session() const;
    std::optional<std::filesystem::path> take_export();
    RECT export_rect() const;
    void notification(std::string);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace seedlens
