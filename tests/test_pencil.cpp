// Headless tests for the freehand pencil and its segment interpolation.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include "core/line.h"
#include "core/vec2.h"
#include "paint/paint_session.h"
#include "tools/pencil_tool.h"

using PaintCore::ColorIndex;
using PaintCore::Vec2;

namespace
{
using Point = std::pair<int, int>;

std::vector<Point> line_points(int x0, int y0, int x1, int y1)
{
	std::vector<Point> points;
	PaintCore::rasterize_line(x0, y0, x1, y1,
		[&](int x, int y) { points.emplace_back(x, y); });
	return points;
}

std::vector<Point> painted_pixels(const PaintSession& canvas,
                                  ColorIndex color)
{
	std::vector<Point> pixels;
	for (int y = 0; y < canvas.getHeight(); ++y)
	{
		for (int x = 0; x < canvas.getWidth(); ++x)
		{
			if (canvas.get_main_buffer().get_pixel(x, y) == color)
			{
				pixels.emplace_back(x, y);
			}
		}
	}
	return pixels;
}

void add_points(std::set<Point>& target, const std::vector<Point>& points)
{
	for (const Point& point : points)
	{
		target.insert(point);
	}
}
}

TEST_CASE("pencil interpolates a fast movement without gaps")
{
	PaintSession canvas(20, 20);
	PencilTool tool(3, 0);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{1, 1}, 1);
	tool.on_mouse_move(canvas, Vec2{18, 15});
	tool.on_mouse_release(canvas, Vec2{18, 15}, 1);

	std::set<Point> expected;
	add_points(expected, line_points(1, 1, 18, 15));

	const std::vector<Point> painted = painted_pixels(canvas, 3);
	std::set<Point> actual(painted.begin(), painted.end());
	CHECK(actual == expected);
	CHECK(canvas.can_undo());
}

TEST_CASE("pencil accumulates consecutive frame segments")
{
	PaintSession canvas(20, 20);
	PencilTool tool(2, 0);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{1, 1}, 1);
	tool.on_mouse_move(canvas, Vec2{10, 5});
	tool.on_mouse_move(canvas, Vec2{18, 15});
	tool.on_mouse_release(canvas, Vec2{18, 15}, 1);

	std::set<Point> expected;
	add_points(expected, line_points(1, 1, 10, 5));
	add_points(expected, line_points(10, 5, 18, 15));

	const std::vector<Point> painted = painted_pixels(canvas, 2);
	std::set<Point> actual(painted.begin(), painted.end());
	CHECK(actual == expected);
}

TEST_CASE("pencil draws a single pixel on a click")
{
	PaintSession canvas(6, 6);
	PencilTool tool(3, 0);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{2, 2}, 1);
	tool.on_mouse_release(canvas, Vec2{2, 2}, 1);

	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 3);
	CHECK_EQ(painted_pixels(canvas, 3).size(), 1u);
}

TEST_CASE("pencil clips freehand strokes to the canvas")
{
	PaintSession canvas(6, 6);
	PencilTool tool(2, 0);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{-3, 4}, 1);
	tool.on_mouse_move(canvas, Vec2{9, 4});
	tool.on_mouse_release(canvas, Vec2{9, 4}, 1);

	for (int x = 0; x < 6; ++x)
	{
		CHECK_EQ(canvas.get_main_buffer().get_pixel(x, 4), 2);
	}
}

TEST_CASE("pencil uses the background color on the right button")
{
	PaintSession canvas(6, 6);
	PencilTool tool(3, 1);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{1, 1}, 2);
	tool.on_mouse_release(canvas, Vec2{5, 1}, 2);

	for (int x = 1; x <= 5; ++x)
	{
		CHECK_EQ(canvas.get_main_buffer().get_pixel(x, 1), 1);
	}
}

TEST_CASE("pencil cancels an unconfirmed stroke when both buttons are used")
{
	PaintSession canvas(6, 6);
	PencilTool tool(3, 1);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, Vec2{1, 1}, 1);
	tool.on_mouse_move(canvas, Vec2{5, 1});
	tool.on_mouse_press(canvas, Vec2{5, 1}, 2);

	CHECK_FALSE(tool.is_drawing());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_FALSE(canvas.can_undo());
}
