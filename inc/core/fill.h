#ifndef QTREGBPAINT_FILL_H
#define QTREGBPAINT_FILL_H

#include <algorithm>
#include <cstddef>
#include <vector>

#include "core/color.h"
#include "core/vec2.h"

namespace PaintCore
{
struct FillSpan
{
	int y = 0;
	int x0 = 0;
	int x1 = -1;
};

// Collects the horizontal runs (inclusive endpoints) of the 4-connected region
// of `target` that contains the seed. The caller provides the current pixel
// value through `color_at`, so the flood fill does not depend on ImageBuffer or
// DrawBuffer. `replacement` is only used to reject no-op fills; this function
// never writes pixels.
//
// The scanline algorithm expands each seed horizontally, emits that run, and
// scans the rows above and below for new seeds. Visited pixels are tracked
// separately because the caller may apply the spans after the collection
// finishes (the tool sizes its DrawBuffer from the final bounding box first).
// Spans are sorted top to bottom and left to right.
template <typename ColorAt>
std::vector<FillSpan> collect_flood_fill_spans(
	int width, int height, int seed_x, int seed_y,
	ColorIndex target, ColorIndex replacement, ColorAt&& color_at)
{
	std::vector<FillSpan> spans;
	if (width <= 0 || height <= 0 || target == replacement ||
	    seed_x < 0 || seed_y < 0 || seed_x >= width || seed_y >= height)
	{
		return spans;
	}

	if (color_at(seed_x, seed_y) != target)
	{
		return spans;
	}

	const std::size_t row_size = static_cast<std::size_t>(width);
	std::vector<bool> visited(
		row_size * static_cast<std::size_t>(height), false);
	std::vector<Vec2> stack;
	stack.push_back(Vec2{seed_x, seed_y});

	while (!stack.empty())
	{
		const Vec2 seed = stack.back();
		stack.pop_back();

		const std::size_t row =
			static_cast<std::size_t>(seed.y) * row_size;
		if (visited[row + static_cast<std::size_t>(seed.x)] ||
		    color_at(seed.x, seed.y) != target)
		{
			continue;
		}

		int x0 = seed.x;
		while (x0 > 0 && !visited[row + static_cast<std::size_t>(x0 - 1)] &&
		       color_at(x0 - 1, seed.y) == target)
		{
			--x0;
		}

		int x1 = seed.x;
		while (x1 + 1 < width &&
		       !visited[row + static_cast<std::size_t>(x1 + 1)] &&
		       color_at(x1 + 1, seed.y) == target)
		{
			++x1;
		}

		for (int x = x0; x <= x1; ++x)
		{
			visited[row + static_cast<std::size_t>(x)] = true;
		}
		spans.push_back(FillSpan{seed.y, x0, x1});

		const int neighbor_rows[2] = {seed.y - 1, seed.y + 1};
		for (const int neighbor_y : neighbor_rows)
		{
			if (neighbor_y < 0 || neighbor_y >= height)
			{
				continue;
			}

			const std::size_t neighbor_row =
				static_cast<std::size_t>(neighbor_y) * row_size;
			bool in_run = false;
			for (int x = x0; x <= x1; ++x)
			{
				const bool candidate =
					!visited[neighbor_row + static_cast<std::size_t>(x)] &&
					color_at(x, neighbor_y) == target;
				if (candidate && !in_run)
				{
					stack.push_back(Vec2{x, neighbor_y});
					in_run = true;
				}
				else if (!candidate)
				{
					in_run = false;
				}
			}
		}
	}

	std::sort(spans.begin(), spans.end(),
	          [](const FillSpan& a, const FillSpan& b)
	          {
	              if (a.y != b.y)
	              {
	                  return a.y < b.y;
	              }
	              return a.x0 < b.x0;
	          });
	return spans;
}

// Collects every horizontal run with `target`, whether or not it is connected
// to a seed. Used by the global fill mode. Spans are sorted like the
// contiguous variant.
template <typename ColorAt>
std::vector<FillSpan> collect_global_fill_spans(
	int width, int height, ColorIndex target, ColorIndex replacement,
	ColorAt&& color_at)
{
	std::vector<FillSpan> spans;
	if (width <= 0 || height <= 0 || target == replacement)
	{
		return spans;
	}

	for (int y = 0; y < height; ++y)
	{
		int x = 0;
		while (x < width)
		{
			if (color_at(x, y) != target)
			{
				++x;
				continue;
			}

			const int x0 = x;
			while (x + 1 < width && color_at(x + 1, y) == target)
			{
				++x;
			}
			spans.push_back(FillSpan{y, x0, x});
			++x;
		}
	}

	return spans;
}
}

#endif
