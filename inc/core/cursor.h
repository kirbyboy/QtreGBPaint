#ifndef QTREGBPAINT_CURSOR_H
#define QTREGBPAINT_CURSOR_H

#include <cstdint>
#include <vector>

#include "core/color.h"

namespace PaintCore
{
	// Cursor used by toolbar buttons. Several tools may share one type
	// (for example the precision Crosshair); tools do not know the mapping.
	enum class CursorType : std::uint8_t
	{
		Arrow,
		Crosshair,
		Pencil,
		Brush,
		Eraser,
		Pick,
		Fill,
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

	// Fully transparent cursor used by the eraser: the preview square is the
	// only pointer indicator over the image.
	inline CursorImage make_invisible_cursor()
	{
		CursorImage image;
		image.width = 1;
		image.height = 1;
		image.hotspot_x = 0;
		image.hotspot_y = 0;
		image.pixels.assign(1, Color{0, 0, 0, 0});
		return image;
	}
}

#endif
