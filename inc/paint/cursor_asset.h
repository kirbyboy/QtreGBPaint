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
//   2 -> black
//   3 -> white
// The image must use exactly 2 bpp (indices 0..3) and no other values.
// When `black_as_dark_gray` is true, index 2 renders as the low-visibility
// RGB 96,96,96 used by the Brush cursor; the file keeps the same black slot
// as any other cursor.
bool load_cursor_asset(const std::string& path, CursorImage& image,
                       std::string* error = nullptr,
                       bool black_as_dark_gray = false);
}

#endif
