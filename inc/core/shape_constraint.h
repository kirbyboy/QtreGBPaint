#ifndef QTREGBPAINT_SHAPE_CONSTRAINT_H
#define QTREGBPAINT_SHAPE_CONSTRAINT_H

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/vec2.h"

namespace PaintCore
{
// Returns the end point of the largest square that fits inside the drag
// rectangle from `start` to `end`, preserving the drag direction. The side is
// the smaller of the two deltas.
inline Vec2 square_constrained_end(const Vec2& start, const Vec2& end)
{
	const int dx = end.x - start.x;
	const int dy = end.y - start.y;
	const int side = std::min(std::abs(dx), std::abs(dy));
	return Vec2{
		start.x + (dx < 0 ? -side : side),
		start.y + (dy < 0 ? -side : side),
	};
}

// Shift-constrained line end point:
//   angle < 30°  -> horizontal
//   30°..60°     -> 45° diagonal, using the larger delta as the square side
//   angle > 60°  -> vertical
// The rule is applied in all four quadrants.
inline Vec2 snapped_line_end(const Vec2& start, const Vec2& end)
{
	const int dx = end.x - start.x;
	const int dy = end.y - start.y;
	const int adx = std::abs(dx);
	const int ady = std::abs(dy);
	if (adx == 0 && ady == 0)
	{
		return end;
	}

	constexpr float kRadiansToDegrees = 57.29577951308232f;
	const float angle =
		std::atan2(static_cast<float>(ady), static_cast<float>(adx)) *
		kRadiansToDegrees;

	if (angle < 30.0f)
	{
		return Vec2{end.x, start.y};
	}
	if (angle < 60.0f)
	{
		const int side = std::max(adx, ady);
		return Vec2{
			start.x + (dx < 0 ? -side : side),
			start.y + (dy < 0 ? -side : side),
		};
	}
	return Vec2{start.x, end.y};
}
}

#endif
