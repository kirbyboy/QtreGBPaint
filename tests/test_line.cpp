// Headless tests for the shared line rasterizer and the straight-line tool.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/line.h"
#include "core/line_stamp.h"
#include "core/shape_constraint.h"
#include "core/vec2.h"
#include "paint/image_buffer.h"
#include "paint/paint_session.h"
#include "tools/line_tool.h"
#include "tools/line_width.h"

using PaintCore::Vec2;

namespace
{
std::vector<std::pair<int, int>> collect_line(int x0, int y0, int x1, int y1)
{
	std::vector<std::pair<int, int>> points;
	PaintCore::rasterize_line(x0, y0, x1, y1,
		[&](int x, int y) { points.emplace_back(x, y); });
	return points;
}

void check_line_properties(int x0, int y0, int x1, int y1)
{
	const std::vector<std::pair<int, int>> points =
		collect_line(x0, y0, x1, y1);
	REQUIRE_FALSE(points.empty());
	CHECK_EQ(points.front(), std::make_pair(x0, y0));
	CHECK_EQ(points.back(), std::make_pair(x1, y1));

	const int expected =
		std::max(std::abs(x1 - x0), std::abs(y1 - y0)) + 1;
	CHECK_EQ(points.size(), static_cast<std::size_t>(expected));

	for (std::size_t i = 0; i < points.size(); ++i)
	{
		const int x = points[i].first;
		const int y = points[i].second;
		CHECK(x >= std::min(x0, x1));
		CHECK(x <= std::max(x0, x1));
		CHECK(y >= std::min(y0, y1));
		CHECK(y <= std::max(y0, y1));

		if (i > 0)
		{
			CHECK(std::abs(x - points[i - 1].first) <= 1);
			CHECK(std::abs(y - points[i - 1].second) <= 1);
		}
	}
}

std::set<std::pair<int, int>> line_stamp_pixels(
	int x0, int y0, int x1, int y1, int width)
{
	std::set<std::pair<int, int>> pixels;
	PaintCore::rasterize_line(
		x0, y0, x1, y1,
		[&](int x, int y)
		{
			for (const Vec2& offset :
			     PaintCore::make_line_stamp_offsets(width))
			{
				pixels.insert({x + offset.x, y + offset.y});
			}
		});
	return pixels;
}

std::set<std::pair<int, int>> painted_pixels(const ImageBuffer& image)
{
	std::set<std::pair<int, int>> pixels;
	for (int y = 0; y < image.height(); ++y)
	{
		for (int x = 0; x < image.width(); ++x)
		{
			if (image.get_pixel(x, y) != 0)
			{
				pixels.insert({x, y});
			}
		}
	}
	return pixels;
}
}

TEST_CASE("rasterize_line covers all directions without gaps")
{
	check_line_properties(2, 3, 2, 3);
	check_line_properties(0, 0, 7, 0);
	check_line_properties(7, 0, 0, 0);
	check_line_properties(0, 0, 0, 7);
	check_line_properties(0, 7, 0, 0);
	check_line_properties(0, 0, 7, 7);
	check_line_properties(7, 7, 0, 0);
	check_line_properties(0, 7, 7, 0);
	check_line_properties(7, 0, 0, 7);
	check_line_properties(0, 0, 2, 9);
	check_line_properties(9, 2, 0, 0);
	check_line_properties(0, 0, 9, 2);
	check_line_properties(9, 0, 0, 2);
}

TEST_CASE("rasterize_line keeps endpoints and step continuity")
{
	const std::vector<std::pair<int, int>> points =
		collect_line(1, 1, 9, 4);

	CHECK_EQ(points.front(), std::make_pair(1, 1));
	CHECK_EQ(points.back(), std::make_pair(9, 4));
	CHECK_EQ(points.size(), 9u);

	// The exact Bresenham path for this slope.
	const std::vector<std::pair<int, int>> expected{
		{1, 1}, {2, 1}, {3, 2}, {4, 2}, {5, 3},
		{6, 3}, {7, 3}, {8, 4}, {9, 4},
	};
	CHECK(points == expected);
}

TEST_CASE("line tool previews and commits the straight line")
{
	PaintSession canvas(6, 6);
	LineTool tool(3, 0);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{1, 1}, 1);
	tool.on_mouse_move(canvas, Vec2{4, 3});
	tool.on_mouse_release(canvas, Vec2{4, 3}, 1);

	const std::vector<std::pair<int, int>> expected = collect_line(1, 1, 4, 3);
	std::vector<std::pair<int, int>> painted;
	for (int y = 0; y < 6; ++y)
	{
		for (int x = 0; x < 6; ++x)
		{
			if (canvas.get_main_buffer().get_pixel(x, y) == 3)
			{
				painted.emplace_back(x, y);
			}
		}
	}

	CHECK(painted == expected);
	CHECK(canvas.can_undo());
}

TEST_CASE("line tool clips lines that leave the canvas")
{
	PaintSession canvas(6, 6);
	LineTool tool(2, 0);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{-4, 2}, 1);
	tool.on_mouse_release(canvas, Vec2{9, 2}, 1);

	for (int x = 0; x < 6; ++x)
	{
		CHECK_EQ(canvas.get_main_buffer().get_pixel(x, 2), 2);
	}
}

TEST_CASE("line tool uses the background color on the right button")
{
	PaintSession canvas(6, 6);
	LineTool tool(3, 1);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{0, 1}, 2);
	tool.on_mouse_release(canvas, Vec2{5, 1}, 2);

	for (int x = 0; x < 6; ++x)
	{
		CHECK_EQ(canvas.get_main_buffer().get_pixel(x, 1), 1);
	}
}

TEST_CASE("line stamp footprints follow the five width contract")
{
	using OffsetSet = std::set<std::pair<int, int>>;

	const auto collect_offsets = [](int width)
	{
		OffsetSet offsets;
		for (const Vec2& offset :
		     PaintCore::make_line_stamp_offsets(width))
		{
			offsets.insert({offset.x, offset.y});
		}
		return offsets;
	};

	CHECK(collect_offsets(0).empty());
	CHECK_EQ(collect_offsets(1), (OffsetSet{{0, 0}}));
	CHECK_EQ(collect_offsets(2),
	         (OffsetSet{{0, 0}, {1, 0}, {0, 1}, {1, 1}}));
	CHECK_EQ(collect_offsets(3),
	         (OffsetSet{{0, -1}, {-1, 0}, {0, 0}, {1, 0}, {0, 1}}));

	OffsetSet cross4;
	for (int y = -1; y <= 2; ++y)
	{
		for (int x = -1; x <= 2; ++x)
		{
			if ((x == 0 || x == 1) || (y == 0 || y == 1))
			{
				cross4.insert({x, y});
			}
		}
	}
	CHECK_EQ(collect_offsets(4), cross4);
	CHECK_EQ(collect_offsets(4).size(), 12u);

	OffsetSet cross5;
	for (int y = -2; y <= 2; ++y)
	{
		for (int x = -2; x <= 2; ++x)
		{
			if ((x >= -1 && x <= 1) || (y >= -1 && y <= 1))
			{
				cross5.insert({x, y});
			}
		}
	}
	CHECK_EQ(collect_offsets(5), cross5);
	CHECK_EQ(collect_offsets(5).size(), 21u);
}

TEST_CASE("Line exposes five widths with 1..5 pixel line previews")
{
	LineTool tool(1, 0);
	const std::vector<ToolOption>& options = tool.options();

	REQUIRE_EQ(options.size(), 1u);
	CHECK(options[0].id == "width");
	CHECK_EQ(options[0].value, 1);
	REQUIRE_EQ(options[0].choices.size(), 5u);

	for (int index = 0; index < 5; ++index)
	{
		const ToolOptionChoice& choice =
			options[0].choices[static_cast<std::size_t>(index)];
		CHECK_EQ(choice.value, index + 1);
		CHECK_EQ(choice.label, "Width " + std::to_string(index + 1) + " px");
		CHECK_EQ(choice.short_label, std::to_string(index + 1));
		REQUIRE_FALSE(choice.preview.empty());

		int min_x = choice.preview.front().x;
		int max_x = choice.preview.front().x;
		int min_y = choice.preview.front().y;
		int max_y = choice.preview.front().y;
		for (const Vec2& offset : choice.preview)
		{
			min_x = std::min(min_x, offset.x);
			max_x = std::max(max_x, offset.x);
			min_y = std::min(min_y, offset.y);
			max_y = std::max(max_y, offset.y);
		}

		// A horizontal line whose thickness is exactly the option width.
		CHECK_EQ(max_x - min_x + 1, LineWidth::kPreviewLength);
		CHECK_EQ(max_y - min_y + 1, index + 1);
	}
}

TEST_CASE("line tool draws each of the five width footprints")
{
	for (int width = 1; width <= 5; ++width)
	{
		PaintSession canvas(20, 20);
		LineTool tool(3, 0);
		if (width != 1)
		{
			REQUIRE(tool.set_option("width", width));
		}
		canvas.set_tool(&tool);

		tool.on_mouse_press(canvas, Vec2{2, 8}, 1);
		tool.on_mouse_move(canvas, Vec2{14, 8});
		tool.on_mouse_release(canvas, Vec2{14, 8}, 1);

		CHECK_EQ(painted_pixels(canvas.get_main_buffer()),
		         line_stamp_pixels(2, 8, 14, 8, width));
	}
}

TEST_CASE("line tool clips thick strokes at the canvas edges")
{
	PaintSession canvas(6, 6);
	LineTool tool(2, 0);
	REQUIRE(tool.set_option("width", 5));
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{-4, 2}, 1);
	tool.on_mouse_release(canvas, Vec2{9, 2}, 1);

	std::set<std::pair<int, int>> expected;
	for (const std::pair<int, int>& pixel :
	     line_stamp_pixels(-4, 2, 9, 2, 5))
	{
		if (pixel.first >= 0 && pixel.first < 6 &&
		    pixel.second >= 0 && pixel.second < 6)
		{
			expected.insert(pixel);
		}
	}

	CHECK_EQ(painted_pixels(canvas.get_main_buffer()), expected);
}

TEST_CASE("line tool freezes the width when the stroke starts")
{
	PaintSession canvas(12, 12);
	LineTool tool(3, 0);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{2, 2}, 1);
	REQUIRE(tool.set_option("width", 5));
	tool.on_mouse_move(canvas, Vec2{9, 2});
	tool.on_mouse_release(canvas, Vec2{9, 2}, 1);

	// The stroke keeps the one-pixel width selected on press.
	for (int x = 2; x <= 9; ++x)
	{
		CHECK_EQ(canvas.get_main_buffer().get_pixel(x, 2), 3);
	}
	CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 1), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 3), 0);
}

TEST_CASE("line tool uses the background color with a thick right-button stroke")
{
	PaintSession canvas(12, 12);
	LineTool tool(3, 1);
	REQUIRE(tool.set_option("width", 3));
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{2, 5}, 2);
	tool.on_mouse_release(canvas, Vec2{9, 5}, 2);

	for (int x = 2; x <= 9; ++x)
	{
		CHECK_EQ(canvas.get_main_buffer().get_pixel(x, 5), 1);
	}
	CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 4), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 6), 1);
}

TEST_CASE("shape constraints snap squares and lines")
{
	PaintCore::Vec2 point =
		PaintCore::square_constrained_end(Vec2{10, 10}, Vec2{15, 12});
	CHECK_EQ(point.x, 12);
	CHECK_EQ(point.y, 12);

	point = PaintCore::square_constrained_end(Vec2{10, 10}, Vec2{5, 8});
	CHECK_EQ(point.x, 8);
	CHECK_EQ(point.y, 8);

	// 0..30 degrees -> horizontal.
	point = PaintCore::snapped_line_end(Vec2{0, 0}, Vec2{10, 1});
	CHECK_EQ(point.x, 10);
	CHECK_EQ(point.y, 0);

	// 30..60 degrees -> 45 degrees, using the larger delta.
	point = PaintCore::snapped_line_end(Vec2{0, 0}, Vec2{10, 6});
	CHECK_EQ(point.x, 10);
	CHECK_EQ(point.y, 10);

	// 60..90 degrees -> vertical.
	point = PaintCore::snapped_line_end(Vec2{0, 0}, Vec2{5, 10});
	CHECK_EQ(point.x, 0);
	CHECK_EQ(point.y, 10);

	// Negative quadrants.
	point = PaintCore::snapped_line_end(Vec2{10, 10}, Vec2{0, 8});
	CHECK_EQ(point.x, 0);
	CHECK_EQ(point.y, 10);
	point = PaintCore::snapped_line_end(Vec2{10, 10}, Vec2{8, 0});
	CHECK_EQ(point.x, 10);
	CHECK_EQ(point.y, 0);
	point = PaintCore::snapped_line_end(Vec2{10, 10}, Vec2{0, 4});
	CHECK_EQ(point.x, 0);
	CHECK_EQ(point.y, 0);
}

TEST_CASE("line tool snaps to horizontal, diagonal and vertical with Shift")
{
	PaintSession canvas(20, 20);
	LineTool tool(3, 0);
	canvas.set_tool(&tool);
	PaintCore::Modifiers modifiers;
	modifiers.shift = true;
	canvas.set_modifiers(modifiers);

	// Mostly horizontal.
	tool.on_mouse_press(canvas, Vec2{1, 1}, 1);
	tool.on_mouse_release(canvas, Vec2{10, 3}, 1);
	for (int x = 1; x <= 10; ++x)
	{
		CHECK_EQ(canvas.get_main_buffer().get_pixel(x, 1), 3);
	}
	CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 3), 0);

	// Mostly vertical.
	PaintSession vertical_canvas(20, 20);
	LineTool vertical_tool(3, 0);
	vertical_canvas.set_tool(&vertical_tool);
	vertical_canvas.set_modifiers(modifiers);
	vertical_tool.on_mouse_press(vertical_canvas, Vec2{1, 1}, 1);
	vertical_tool.on_mouse_release(vertical_canvas, Vec2{3, 10}, 1);
	for (int y = 1; y <= 10; ++y)
	{
		CHECK_EQ(vertical_canvas.get_main_buffer().get_pixel(1, y), 3);
	}
	CHECK_EQ(vertical_canvas.get_main_buffer().get_pixel(3, 10), 0);

	// Diagonal: a 9x7 drag snaps to a 9x9 diagonal.
	PaintSession diagonal_canvas(20, 20);
	LineTool diagonal_tool(3, 0);
	diagonal_canvas.set_tool(&diagonal_tool);
	diagonal_canvas.set_modifiers(modifiers);
	diagonal_tool.on_mouse_press(diagonal_canvas, Vec2{1, 1}, 1);
	diagonal_tool.on_mouse_release(diagonal_canvas, Vec2{10, 8}, 1);
	CHECK_EQ(diagonal_canvas.get_main_buffer().get_pixel(5, 5), 3);
	CHECK_EQ(diagonal_canvas.get_main_buffer().get_pixel(10, 10), 3);
	CHECK_EQ(diagonal_canvas.get_main_buffer().get_pixel(10, 8), 0);
}

TEST_CASE("line tool cancels an unconfirmed stroke when both buttons are used")
{
	PaintSession canvas(6, 6);
	LineTool tool(3, 1);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{1, 1}, 1);
	tool.on_mouse_move(canvas, Vec2{4, 4});
	CHECK(tool.is_drawing());

	tool.on_mouse_press(canvas, Vec2{4, 4}, 2);
	CHECK_FALSE(tool.is_drawing());
	tool.on_mouse_release(canvas, Vec2{4, 4}, 1);

	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_FALSE(canvas.can_undo());
}
