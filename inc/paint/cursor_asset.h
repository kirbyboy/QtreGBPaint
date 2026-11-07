#ifndef QTREGBPAINT_CURSOR_ASSET_H
#define QTREGBPAINT_CURSOR_ASSET_H

#include <string>

#include "core/cursor.h"

namespace PaintCore
{
// Loads a cursor from the .ipx format. The file uses the same textual
// IndexedImage format as .idx, with an additional cursor convention:
//   0 -> transparent
//   1 -> hotspot (exactly one pixel)
//   3 -> black
//   4 -> white
// Index 2 and 5..7 are currently reserved and rejected. The image must use at
// least 3 bpp so that indices 3 and 4 are valid.
bool load_cursor_asset(const std::string& path, CursorImage& image,
                       std::string* error = nullptr);
}

#endif
