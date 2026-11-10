#ifndef QTREGBPAINT_CANVAS_TILE_GRID_H
#define QTREGBPAINT_CANVAS_TILE_GRID_H

#include <algorithm>
#include <cstdint>

#include "core/rect.h"

namespace PaintCore
{
// The canvas is uploaded to the GPU as a grid of fixed-size tiles. Only the
// tiles around the visible viewport are kept, so VRAM usage does not depend on
// the canvas size and the image can exceed the backend texture limit.
inline constexpr int kCanvasTileSize = 256;

// Half-open range of tile columns/rows: [x0, x1) x [y0, y1).
struct TileSpan
{
	int x0 = 0;
	int y0 = 0;
	int x1 = 0;
	int y1 = 0;

	bool empty() const { return x0 >= x1 || y0 >= y1; }
};

inline int tile_count(int size)
{
	if (size <= 0)
	{
		return 0;
	}
	return (size + kCanvasTileSize - 1) / kCanvasTileSize;
}

// Pixel rect of tile (tile_x, tile_y), clamped to the image. Empty when the
// tile is outside the image or the coordinates are negative.
inline Rect tile_rect(int image_width, int image_height, int tile_x, int tile_y)
{
	if (image_width <= 0 || image_height <= 0 || tile_x < 0 || tile_y < 0)
	{
		return {};
	}

	const int x0 = tile_x * kCanvasTileSize;
	const int y0 = tile_y * kCanvasTileSize;
	if (x0 >= image_width || y0 >= image_height)
	{
		return {};
	}

	const int x1 = std::min(image_width, x0 + kCanvasTileSize);
	const int y1 = std::min(image_height, y0 + kCanvasTileSize);
	return Rect{x0, y0, x1 - x0, y1 - y0};
}

// Tile columns/rows needed to cover `rect`, clamped to the image bounds.
inline TileSpan tiles_covering(const Rect& rect, int image_width, int image_height)
{
	TileSpan span;
	if (rect.empty() || image_width <= 0 || image_height <= 0)
	{
		return span;
	}

	const int x0 = std::clamp(rect.x, 0, image_width);
	const int y0 = std::clamp(rect.y, 0, image_height);
	const int x1 = std::clamp(rect.right(), 0, image_width);
	const int y1 = std::clamp(rect.bottom(), 0, image_height);
	if (x0 >= x1 || y0 >= y1)
	{
		return span;
	}

	span.x0 = x0 / kCanvasTileSize;
	span.y0 = y0 / kCanvasTileSize;
	span.x1 = (x1 - 1) / kCanvasTileSize + 1;
	span.y1 = (y1 - 1) / kCanvasTileSize + 1;
	return span;
}

// Stable key for the tile map. Tiles are non-negative, so the two 32-bit
// halves can be packed without collisions.
inline std::uint64_t tile_key(int tile_x, int tile_y)
{
	return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(tile_y)) << 32) |
	       static_cast<std::uint64_t>(static_cast<std::uint32_t>(tile_x));
}
}

#endif
