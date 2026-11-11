// Headless tests for the main-window title format.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include "paint/window_title.h"

TEST_CASE("window title for an untitled document")
{
	CHECK_EQ(PaintCore::make_window_title("Untitled", false),
	         "Untitled - QtreGBPaint");
	CHECK_EQ(PaintCore::make_window_title("Untitled", true),
	         "*Untitled - QtreGBPaint");
}

TEST_CASE("window title for a saved document")
{
	CHECK_EQ(PaintCore::make_window_title("tiles.idx", false),
	         "tiles.idx - QtreGBPaint");
	CHECK_EQ(PaintCore::make_window_title("tiles.idx", true),
	         "*tiles.idx - QtreGBPaint");
}
