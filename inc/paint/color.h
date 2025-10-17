#ifndef QTREGBPAINT_COLOR_H
#define QTREGBPAINT_COLOR_H

#include <cstdint>

namespace PaintCore
{
	using Color = uint32_t;
	using ColorIndex = uint8_t;
	inline constexpr ColorIndex TRANSPARENT_INDEX = 255;
}

#endif
