#pragma once
#include "backend.h"
namespace seedlens::minecraft {
inline int sprite_index(int kind, unsigned flags = 0) {
    if (kind == SL_VILLAGE && (flags & SL_ABANDONED)) return 31;
    if (kind == SL_END_CITY && (flags & SL_END_SHIP)) return 32;
    if (kind == -1) return 33;
    return kind >= 0 && kind < SL_KIND_COUNT ? kind : 33;
}
inline constexpr int columns = 8, width = 256, height = 160, cell = 32, inset = 4, content = 24;
}
