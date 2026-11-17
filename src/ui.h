#pragma once
#include "engine.h"
#include "platform.h"

namespace seedlens {
class App {
  public:
    App(Platform, Session, std::filesystem::path);
    ~App();
    void frame(float dpi);
    bool busy() const;
    const Session& session() const;
    std::optional<std::filesystem::path> take_export();
    PixelRect export_rect() const;
    void notification(std::string);
    bool gesture(double factor,float x,float y,float dx=0,float dy=0);
    bool context_at(float x,float y);
    bool map_input_at(float x,float y) const;
    bool back();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace seedlens
