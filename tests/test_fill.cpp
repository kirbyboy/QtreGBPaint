// Headless tests for the scanline flood fill and the bucket tool.
// Run with `make -C tests test_fill` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "core/fill.h"
#include "core/vec2.h"
#include "paint/image_buffer.h"
#include "paint/paint_session.h"
#include "tools/base_tool.h"
#include "tools/fill_tool.h"

using PaintCore::ColorIndex;
using PaintCore::FillSpan;

namespace
{
using Grid = std::vector<ColorIndex>;

Grid make_grid(int width, int height, ColorIndex fill = 0)
{
	return Grid(static_cast<std::size_t>(width) *
	            static_cast<std::size_t>(height), fill);
}

void set_pixels(Grid& grid, int width, int x0, int y0, int x1, int y1,
                ColorIndex color)
{
	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
			     static_cast<std::size_t>(x)] = color;
		}
	}
}

void draw_border(Grid& grid, int width, int x0, int y0, int x1, int y1,
                 ColorIndex color)
{
	set_pixels(grid, width, x0, y0, x1, y0, color);
	set_pixels(grid, width, x0, y1, x1, y1, color);
	set_pixels(grid, width, x0, y0, x0, y1, color);
	set_pixels(grid, width, x1, y0, x1, y1, color);
}

std::vector<FillSpan> flood(const Grid& grid, int width, int height,
                            int seed_x, int seed_y, ColorIndex target,
                            ColorIndex replacement)
{
	const auto color_at = [&grid, width](int x, int y)
	{
		return grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
		            static_cast<std::size_t>(x)];
	};
	return PaintCore::collect_flood_fill_spans(
		width, height, seed_x, seed_y, target, replacement, color_at);
}

std::vector<FillSpan> global(const Grid& grid, int width, int height,
                             ColorIndex target, ColorIndex replacement)
{
	const auto color_at = [&grid, width](int x, int y)
	{
		return grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
		            static_cast<std::size_t>(x)];
	};
	return PaintCore::collect_global_fill_spans(
		width, height, target, replacement, color_at);
}

std::size_t span_pixels(const std::vector<FillSpan>& spans)
{
	std::size_t count = 0;
	for (const FillSpan& span : spans)
	{
		count += static_cast<std::size_t>(span.x1 - span.x0 + 1);
	}
	return count;
}

bool spans_cover(const std::vector<FillSpan>& spans, int x, int y)
{
	for (const FillSpan& span : spans)
	{
		if (span.y == y && x >= span.x0 && x <= span.x1)
		{
			return true;
		}
	}
	return false;
}

void load_pixels(PaintSession& session, int width, int height,
                 const Grid& pixels)
{
	ImageBuffer image(width, height, 2);
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			image.set_pixel(
				x, y,
				pixels[static_cast<std::size_t>(y) *
				           static_cast<std::size_t>(width) +
				       static_cast<std::size_t>(x)]);
		}
	}
	session.load_image(std::move(image));
}

void click(FillTool& tool, PaintSession& session, int x, int y,
           int button = ToolBase::kLeftButton)
{
	session.set_tool(&tool);
	session.handle_mouse_press({x, y}, button);
}
}

TEST_CASE("flood fill emits one span per row of a connected area")
{
	const int width = 5;
	const int height = 4;
	const Grid grid = make_grid(width, height, 0);

	const std::vector<FillSpan> spans =
		flood(grid, width, height, 2, 2, 0, 1);

	REQUIRE_EQ(spans.size(), static_cast<std::size_t>(height));
	for (int y = 0; y < height; ++y)
	{
		CHECK_EQ(spans[static_cast<std::size_t>(y)].y, y);
		CHECK_EQ(spans[static_cast<std::size_t>(y)].x0, 0);
		CHECK_EQ(spans[static_cast<std::size_t>(y)].x1, width - 1);
	}
	CHECK_EQ(span_pixels(spans), 20u);
}

TEST_CASE("flood fill stops at an enclosing border")
{
	const int width = 5;
	const int height = 5;
	Grid grid = make_grid(width, height, 0);
	// One-pixel frame at x=0/4 and y=0/4; interior remains color 0.
	for (int x = 0; x < width; ++x)
	{
		grid[static_cast<std::size_t>(x)] = 1;
		grid[static_cast<std::size_t>(height - 1) * width + x] = 1;
	}
	for (int y = 0; y < height; ++y)
	{
		grid[static_cast<std::size_t>(y) * width] = 1;
		grid[static_cast<std::size_t>(y) * width + width - 1] = 1;
	}

	const std::vector<FillSpan> spans =
		flood(grid, width, height, 2, 2, 0, 2);

	REQUIRE_EQ(spans.size(), 3u);
	for (const FillSpan& span : spans)
	{
		CHECK(span.y >= 1);
		CHECK(span.y <= 3);
		CHECK_EQ(span.x0, 1);
		CHECK_EQ(span.x1, 3);
	}
	CHECK_EQ(span_pixels(spans), 9u);
	CHECK_FALSE(spans_cover(spans, 0, 0));
	CHECK_FALSE(spans_cover(spans, 4, 4));
}

TEST_CASE("flood fill is 4-connected and global fill is not")
{
	const int width = 3;
	const int height = 3;
	Grid grid = make_grid(width, height, 1);
	grid[0] = 0;                                  // (0,0)
	grid[static_cast<std::size_t>(2) * width + 2] = 0; // (2,2)

	const std::vector<FillSpan> connected =
		flood(grid, width, height, 0, 0, 0, 2);
	REQUIRE_EQ(connected.size(), 1u);
	CHECK_EQ(connected[0].y, 0);
	CHECK_EQ(connected[0].x0, 0);
	CHECK_EQ(connected[0].x1, 0);

	const std::vector<FillSpan> all =
		global(grid, width, height, 0, 2);
	REQUIRE_EQ(all.size(), 2u);
	CHECK(spans_cover(all, 0, 0));
	CHECK(spans_cover(all, 2, 2));
	CHECK_EQ(span_pixels(all), 2u);
}

TEST_CASE("flood fill rejects no-op, mismatched and out-of-bounds seeds")
{
	const int width = 3;
	const int height = 3;
	const Grid grid = make_grid(width, height, 0);

	CHECK(flood(grid, width, height, 0, 0, 0, 0).empty());
	CHECK(flood(grid, width, height, 0, 0, 1, 2).empty());
	CHECK(flood(grid, width, height, -1, 0, 0, 2).empty());
	CHECK(flood(grid, width, height, 0, -1, 0, 2).empty());
	CHECK(flood(grid, width, height, width, 0, 0, 2).empty());
	CHECK(flood(grid, width, height, 0, height, 0, 2).empty());

	CHECK(PaintCore::collect_global_fill_spans(
	          0, height, 0, 2,
	          [](int, int) { return static_cast<ColorIndex>(0); })
	          .empty());
}

TEST_CASE("global fill groups every run of the target color")
{
	const int width = 4;
	const int height = 3;
	Grid grid = make_grid(width, height, 0);
	// 0 1 0 0
	// 1 0 1 0
	// 0 1 0 0
	grid[1] = 1;
	grid[static_cast<std::size_t>(1) * width] = 1;
	grid[static_cast<std::size_t>(1) * width + 2] = 1;
	grid[static_cast<std::size_t>(2) * width + 1] = 1;

	const std::vector<FillSpan> spans =
		global(grid, width, height, 0, 2);

	REQUIRE_EQ(spans.size(), 6u);
	CHECK_EQ(spans[0].y, 0);
	CHECK_EQ(spans[0].x0, 0);
	CHECK_EQ(spans[0].x1, 0);
	CHECK_EQ(spans[1].y, 0);
	CHECK_EQ(spans[1].x0, 2);
	CHECK_EQ(spans[1].x1, 3);
	CHECK_EQ(spans[2].y, 1);
	CHECK_EQ(spans[2].x0, 1);
	CHECK_EQ(spans[2].x1, 1);
	CHECK_EQ(spans[3].y, 1);
	CHECK_EQ(spans[3].x0, 3);
	CHECK_EQ(spans[3].x1, 3);
	CHECK_EQ(spans[4].y, 2);
	CHECK_EQ(spans[4].x0, 0);
	CHECK_EQ(spans[4].x1, 0);
	CHECK_EQ(spans[5].y, 2);
	CHECK_EQ(spans[5].x0, 2);
	CHECK_EQ(spans[5].x1, 3);
	CHECK_EQ(span_pixels(spans), 8u);
}

TEST_CASE("FillTool exposes contiguous and global modes")
{
	FillTool tool(1, 2);
	const std::vector<ToolOption>& options = tool.options();

	REQUIRE_EQ(options.size(), 1u);
	CHECK(options[0].id == "mode");
	CHECK(options[0].type == ToolOptionType::Choice);
	CHECK_EQ(options[0].value,
	         static_cast<int>(FillTool::Mode::Contiguous));
	REQUIRE_EQ(options[0].choices.size(), 2u);
	CHECK(options[0].choices[0].label == "Contiguous");
	CHECK(options[0].choices[0].icon == "fill.contiguous");
	CHECK(options[0].choices[1].label == "Global");
	CHECK(options[0].choices[1].icon == "fill.global");

	CHECK(tool.set_option("mode",
	                      static_cast<int>(FillTool::Mode::Global)));
	CHECK_EQ(tool.option_value("mode"),
	         static_cast<int>(FillTool::Mode::Global));
}

TEST_CASE("FillTool fills an enclosed area and commits one undo step")
{
	const int width = 6;
	const int height = 6;
	Grid grid = make_grid(width, height, 0);
	draw_border(grid, width, 1, 1, 4, 4, 1);

	PaintSession session(width, height);
	load_pixels(session, width, height, grid);

	FillTool tool(2, 1);
	click(tool, session, 2, 2);

	// The bucket is one-shot: it commits on press, not on release.
	CHECK_FALSE(tool.is_drawing());

	const ImageBuffer& image = session.get_main_buffer();
	for (int y = 2; y <= 3; ++y)
	{
		for (int x = 2; x <= 3; ++x)
		{
			CHECK_EQ(image.get_pixel(x, y), 2);
		}
	}
	CHECK_EQ(image.get_pixel(1, 1), 1);
	CHECK_EQ(image.get_pixel(4, 4), 1);
	CHECK_EQ(image.get_pixel(0, 0), 0);
	CHECK_EQ(image.get_pixel(5, 5), 0);
	CHECK(session.can_undo());

	session.undo();
	for (int y = 2; y <= 3; ++y)
	{
		for (int x = 2; x <= 3; ++x)
		{
			CHECK_EQ(image.get_pixel(x, y), 0);
		}
	}

	session.redo();
	for (int y = 2; y <= 3; ++y)
	{
		for (int x = 2; x <= 3; ++x)
		{
			CHECK_EQ(image.get_pixel(x, y), 2);
		}
	}
}

TEST_CASE("FillTool right button fills with the background color")
{
	const int width = 6;
	const int height = 6;
	Grid grid = make_grid(width, height, 0);
	draw_border(grid, width, 1, 1, 4, 4, 1);

	PaintSession session(width, height);
	load_pixels(session, width, height, grid);

	FillTool tool(2, 3);
	click(tool, session, 0, 0, ToolBase::kRightButton);

	const ImageBuffer& image = session.get_main_buffer();
	CHECK_EQ(image.get_pixel(0, 0), 3);
	CHECK_EQ(image.get_pixel(5, 5), 3);
	CHECK_EQ(image.get_pixel(1, 1), 1);
	CHECK_EQ(image.get_pixel(2, 2), 0);
}

TEST_CASE("FillTool does not create history when the color already matches")
{
	const int width = 4;
	const int height = 4;
	const Grid grid = make_grid(width, height, 0);

	PaintSession session(width, height);
	load_pixels(session, width, height, grid);

	FillTool tool(0, 1);
	click(tool, session, 1, 1);

	CHECK_FALSE(tool.is_drawing());
	CHECK_FALSE(session.can_undo());
	CHECK_EQ(session.get_main_buffer().get_pixel(1, 1), 0);
}

TEST_CASE("FillTool global mode replaces disconnected runs")
{
	const int width = 5;
	const int height = 1;
	Grid grid = make_grid(width, height, 0);
	grid[1] = 1;
	grid[4] = 1;

	PaintSession session(width, height);
	load_pixels(session, width, height, grid);

	FillTool tool(2, 1);
	REQUIRE(tool.set_option("mode",
	                        static_cast<int>(FillTool::Mode::Global)));
	click(tool, session, 0, 0);

	const ImageBuffer& image = session.get_main_buffer();
	CHECK_EQ(image.get_pixel(0, 0), 2);
	CHECK_EQ(image.get_pixel(1, 0), 1);
	CHECK_EQ(image.get_pixel(2, 0), 2);
	CHECK_EQ(image.get_pixel(3, 0), 2);
	CHECK_EQ(image.get_pixel(4, 0), 1);
}

TEST_CASE("FillTool ignores presses outside the canvas")
{
	const int width = 4;
	const int height = 4;
	const Grid grid = make_grid(width, height, 0);

	PaintSession session(width, height);
	load_pixels(session, width, height, grid);

	FillTool tool(2, 1);
	click(tool, session, -1, 0);

	CHECK_FALSE(session.can_undo());
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			CHECK_EQ(session.get_main_buffer().get_pixel(x, y), 0);
		}
	}
}
