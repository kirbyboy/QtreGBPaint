#ifndef QTREGBPAINT_COLOR_H
#define QTREGBPAINT_COLOR_H

#include <cstdint>

namespace PaintCore
{
	using ColorIndex = uint8_t;

	struct Color
	{
		uint8_t r = 0;
		uint8_t g = 0;
		uint8_t b = 0;
		uint8_t a = 255;
	};	
}

#endif
