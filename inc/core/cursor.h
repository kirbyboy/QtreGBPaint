#ifndef QTREGBPAINT_CURSOR_H
#define QTREGBPAINT_CURSOR_H

#include <cstdint>
#include <vector>

#include "core/color.h"

namespace PaintCore
{
	// Cursor associated with a toolbar button. The backend maps these values
	// to its own cursors; tools do not know about them.
	enum class CursorType : std::uint8_t
	{
		Arrow,
		Crosshair,
		Pencil,
		Fill,
		Rect,
		Circle,
		ResizeEW,
		ResizeNS,
		ResizeNWSE,
		Move,
		Count,
	};

	// Platform-independent cursor bitmap. Pixels are straight RGBA; alpha 0 is
	// transparent. The hotspot is the pixel that receives the click.
	struct CursorImage
	{
		int width = 0;
		int height = 0;
		int hotspot_x = 0;
		int hotspot_y = 0;
		std::vector<Color> pixels;
	};
}

#endif
