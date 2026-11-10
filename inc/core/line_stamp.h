#ifndef QTREGBPAINT_LINE_STAMP_H
#define QTREGBPAINT_LINE_STAMP_H

#include <vector>

#include "core/vec2.h"

namespace PaintCore
{
// Pixel offsets of the stamp swept along a straight or curved line.
//
// Widths 1..5 follow the line tool contract:
//   1 -> a single pixel;
//   2 -> a 2x2 square;
//   3 -> a centered cross with 3-pixel long, 1-pixel thick arms;
//   4 -> a centered cross with 4-pixel long, 2-pixel thick arms;
//   5 -> a centered cross with 5-pixel long, 3-pixel thick arms.
//
// Even sizes follow the same hotspot convention as the brush footprints: the
// hotspot is the upper-left of the two central pixels, so the extra row and
// column extend to the right/bottom.
inline std::vector<Vec2> make_line_stamp_offsets(int size)
{
	std::vector<Vec2> offsets;
	if (size <= 0)
	{
		return offsets;
	}

	if (size == 1)
	{
		offsets.push_back({0, 0});
		return offsets;
	}

	if (size == 2)
	{
		for (int y = 0; y < 2; ++y)
		{
			for (int x = 0; x < 2; ++x)
			{
				offsets.push_back({x, y});
			}
		}
		return offsets;
	}

	const int hotspot = (size - 1) / 2;
	const int thickness = size - 2;
	const int bar_start = -((thickness - 1) / 2);
	const int max_offset = size - 1 - hotspot;

	for (int y = -hotspot; y <= max_offset; ++y)
	{
		for (int x = -hotspot; x <= max_offset; ++x)
		{
			const bool in_vertical_bar =
				x >= bar_start && x < bar_start + thickness;
			const bool in_horizontal_bar =
				y >= bar_start && y < bar_start + thickness;
			if (in_vertical_bar || in_horizontal_bar)
			{
				offsets.push_back({x, y});
			}
		}
	}

	return offsets;
}
}

#endif
