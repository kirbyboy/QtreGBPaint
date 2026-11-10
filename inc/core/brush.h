#ifndef QTREGBPAINT_BRUSH_H
#define QTREGBPAINT_BRUSH_H

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "core/vec2.h"

namespace PaintCore
{
enum class BrushShape
{
	Circle,
	Square,
	DiagonalDown,
	DiagonalUp,
};

// Pixel offsets of a brush footprint relative to its hotspot. For odd sizes
// the hotspot is the exact center; for even sizes it is the upper-left of the
// two central pixels, so the extra row/column extends to the right/bottom.
// For circles, `size` is the bounding-box size (radius r -> 2*r+1).
inline std::vector<Vec2> make_brush_offsets(BrushShape shape, int size)
{
	std::vector<Vec2> offsets;
	if (size <= 0)
	{
		return offsets;
	}

	const int hotspot = (size - 1) / 2;

	switch (shape)
	{
		case BrushShape::Circle:
		{
			// Sample each pixel's area and keep it when at least 60% is inside
			// the inscribed circle. The bias matters for the smallest sizes:
			// a 3x3 circle becomes a plus instead of a full square.
			constexpr int kSamples = 16;
			constexpr int kTotal = kSamples * kSamples;
			const double center = size * 0.5;
			const double radius = size * 0.5;
			for (int y = 0; y < size; ++y)
			{
				for (int x = 0; x < size; ++x)
				{
					int inside = 0;
					for (int sy = 0; sy < kSamples; ++sy)
					{
						for (int sx = 0; sx < kSamples; ++sx)
						{
							const double px =
								x + (sx + 0.5) / kSamples;
							const double py =
								y + (sy + 0.5) / kSamples;
							const double dx = px - center;
							const double dy = py - center;
							if (dx * dx + dy * dy <= radius * radius)
							{
								++inside;
							}
						}
					}

					if (inside * 10 >= kTotal * 6)
					{
						offsets.push_back({x - hotspot, y - hotspot});
					}
				}
			}
			break;
		}
		case BrushShape::Square:
			for (int y = 0; y < size; ++y)
			{
				for (int x = 0; x < size; ++x)
				{
					offsets.push_back({x - hotspot, y - hotspot});
				}
			}
			break;
		case BrushShape::DiagonalDown:
			for (int i = 0; i < size; ++i)
			{
				offsets.push_back({i - hotspot, i - hotspot});
			}
			break;
		case BrushShape::DiagonalUp:
			for (int i = 0; i < size; ++i)
			{
				offsets.push_back(
					{i - hotspot, (size - 1 - i) - hotspot});
			}
			break;
	}

	return offsets;
}

// Largest distance from the hotspot covered by the footprint. Freehand tools
// use it to expand their DrawBuffer so the stamp is not clipped at the path.
inline int brush_margin(const std::vector<Vec2>& offsets)
{
	int margin = 0;
	for (const Vec2& offset : offsets)
	{
		margin = std::max(margin, std::max(std::abs(offset.x),
		                                   std::abs(offset.y)));
	}
	return margin;
}
}

#endif
