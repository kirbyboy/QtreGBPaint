// Headless tests for the canvas tile grid math.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include "paint/canvas_tile_grid.h"

using PaintCore::Rect;
using PaintCore::TileSpan;

namespace
{
bool rect_eq(const Rect& r, int x, int y, int w, int h)
{
	return r.x == x && r.y == y && r.w == w && r.h == h;
}

bool span_eq(const TileSpan& span, int x0, int y0, int x1, int y1)
{
	return span.x0 == x0 && span.y0 == y0 && span.x1 == x1 && span.y1 == y1;
}
}

TEST_CASE("tile_count rounds up and rejects empty sizes")
{
	CHECK(PaintCore::tile_count(0) == 0);
	CHECK(PaintCore::tile_count(-1) == 0);
	CHECK(PaintCore::tile_count(1) == 1);
	CHECK(PaintCore::tile_count(PaintCore::kCanvasTileSize) == 1);
	CHECK(PaintCore::tile_count(PaintCore::kCanvasTileSize + 1) == 2);
	CHECK(PaintCore::tile_count(PaintCore::kCanvasTileSize * 2) == 2);
	CHECK(PaintCore::tile_count(PaintCore::kCanvasTileSize * 2 + 1) == 3);
}

TEST_CASE("tile_rect clamps edge tiles to the image")
{
	const int tile = PaintCore::kCanvasTileSize;

	CHECK(rect_eq(PaintCore::tile_rect(300, 100, 0, 0),
	              0, 0, tile, 100));
	CHECK(rect_eq(PaintCore::tile_rect(300, 100, 1, 0),
	              tile, 0, 300 - tile, 100));
	CHECK(rect_eq(PaintCore::tile_rect(300, 300, 1, 1),
	              tile, tile, 300 - tile, 300 - tile));
}

TEST_CASE("tile_rect rejects tiles outside the image")
{
	const int tile = PaintCore::kCanvasTileSize;

	CHECK(PaintCore::tile_rect(300, 100, 2, 0).empty());
	CHECK(PaintCore::tile_rect(300, 100, 0, 1).empty());
	CHECK(PaintCore::tile_rect(300, 100, -1, 0).empty());
	CHECK(PaintCore::tile_rect(300, 100, 0, -1).empty());
	CHECK(PaintCore::tile_rect(0, 100, 0, 0).empty());
	CHECK(PaintCore::tile_rect(100, 0, 0, 0).empty());
}

TEST_CASE("tiles_covering spans every tile touched by a rect")
{
	const int tile = PaintCore::kCanvasTileSize;

	CHECK(span_eq(PaintCore::tiles_covering(Rect{0, 0, 300, 100}, 300, 100),
	              0, 0, 2, 1));
	CHECK(span_eq(PaintCore::tiles_covering(Rect{0, 0, 1, 1}, 300, 100),
	              0, 0, 1, 1));
	CHECK(span_eq(PaintCore::tiles_covering(Rect{tile - 1, 0, 2, tile}, 300, 300),
	              0, 0, 2, 1));
	CHECK(span_eq(PaintCore::tiles_covering(Rect{tile, tile, tile, tile},
	                                        2 * tile, 2 * tile),
	              1, 1, 2, 2));
	// A rect that ends exactly on a tile boundary does not include the next
	// tile.
	CHECK(span_eq(PaintCore::tiles_covering(Rect{0, 0, tile, tile}, 2 * tile, 2 * tile),
	              0, 0, 1, 1));
}

TEST_CASE("tiles_covering clamps to the image and rejects empty rects")
{
	CHECK(span_eq(PaintCore::tiles_covering(Rect{-10, -10, 50, 50}, 300, 100),
	              0, 0, 1, 1));
	CHECK(span_eq(PaintCore::tiles_covering(Rect{280, 0, 100, 10}, 300, 100),
	              1, 0, 2, 1));
	CHECK(PaintCore::tiles_covering(Rect{400, 0, 10, 10}, 300, 100).empty());
	CHECK(PaintCore::tiles_covering(Rect{0, 0, 0, 10}, 300, 100).empty());
	CHECK(PaintCore::tiles_covering(Rect{0, 0, 10, 10}, 0, 100).empty());
	CHECK(PaintCore::tiles_covering(Rect{0, 0, 10, 10}, 300, 0).empty());
}

TEST_CASE("tile_rects cover the image exactly once")
{
	const int width = 2 * PaintCore::kCanvasTileSize + 37;
	const int height = PaintCore::kCanvasTileSize + 11;
	const int columns = PaintCore::tile_count(width);
	const int rows = PaintCore::tile_count(height);

	int covered = 0;
	for (int y = 0; y < rows; ++y)
	{
		for (int x = 0; x < columns; ++x)
		{
			const Rect rect = PaintCore::tile_rect(width, height, x, y);
			REQUIRE_FALSE(rect.empty());
			CHECK(rect.x == x * PaintCore::kCanvasTileSize);
			CHECK(rect.y == y * PaintCore::kCanvasTileSize);
			CHECK(rect.right() <= width);
			CHECK(rect.bottom() <= height);
			covered += rect.w * rect.h;
		}
	}

	CHECK(covered == width * height);
}

TEST_CASE("tile_key is unique for distinct tiles")
{
	CHECK(PaintCore::tile_key(0, 0) != PaintCore::tile_key(1, 0));
	CHECK(PaintCore::tile_key(0, 0) != PaintCore::tile_key(0, 1));
	CHECK(PaintCore::tile_key(0, 1) != PaintCore::tile_key(1, 0));
	CHECK(PaintCore::tile_key(123, 456) == PaintCore::tile_key(123, 456));
}
