// Headless tests for the shared ellipse rasterizer and the circle tool.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "core/ellipse.h"
#include "core/modifiers.h"
#include "core/vec2.h"
#include "paint/image_buffer.h"
#include "paint/paint_session.h"
#include "tools/base_tool.h"
#include "tools/circle_tool.h"

using PaintCore::Vec2;

namespace
{
void draw_shape(PaintSession& canvas, CircleTool& tool,
                int start_x, int start_y, int end_x, int end_y,
                int button = 1)
{
	canvas.set_tool(&tool);
	canvas.handle_mouse_press({start_x, start_y}, button);
	canvas.handle_mouse_move({end_x, end_y});
	canvas.handle_mouse_release({end_x, end_y}, button);
}

void check_span_rectangle(int width, int height)
{
	const int right = width - 1;
	const int bottom = height - 1;

	int min_x0 = width;
	int max_x1 = -1;
	for (int y = 0; y < height; ++y)
	{
		const PaintCore::EllipseSpan span =
			PaintCore::ellipse_span_for_row(0, 0, right, bottom, y);
		CHECK(span.x0 >= 0);
		CHECK(span.x0 <= span.x1);
		CHECK(span.x1 <= right);
		min_x0 = std::min(min_x0, span.x0);
		max_x1 = std::max(max_x1, span.x1);

		const PaintCore::EllipseSpan mirrored =
			PaintCore::ellipse_span_for_row(0, 0, right, bottom,
			                                bottom - y);
		CHECK_EQ(span.x0, right - mirrored.x1);
		CHECK_EQ(span.x1, right - mirrored.x0);
	}

	CHECK_EQ(min_x0, 0);
	CHECK_EQ(max_x1, right);
}
}

TEST_CASE("ellipse spans fit and touch the bounding box")
{
	const std::pair<int, int> sizes[] = {
		{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {8, 8}, {9, 9},
		{8, 5}, {9, 5}, {5, 8}, {3, 7}, {7, 3}, {2, 20}, {20, 2},
		{4, 20}, {20, 4}, {6, 100}, {100, 6}, {3, 100}, {100, 3},
	};

	for (const auto& size : sizes)
	{
		check_span_rectangle(size.first, size.second);
	}
}

TEST_CASE("ellipse spans use the expected shape for common sizes")
{
	const PaintCore::EllipseSpan top =
		PaintCore::ellipse_span_for_row(0, 0, 7, 7, 0);
	CHECK_EQ(top.x0, 2);
	CHECK_EQ(top.x1, 5);

	const PaintCore::EllipseSpan shoulder =
		PaintCore::ellipse_span_for_row(0, 0, 7, 7, 1);
	CHECK_EQ(shoulder.x0, 1);
	CHECK_EQ(shoulder.x1, 6);

	const PaintCore::EllipseSpan middle =
		PaintCore::ellipse_span_for_row(0, 0, 7, 7, 3);
	CHECK_EQ(middle.x0, 0);
	CHECK_EQ(middle.x1, 7);

	const PaintCore::EllipseSpan bottom =
		PaintCore::ellipse_span_for_row(0, 0, 7, 7, 7);
	CHECK_EQ(bottom.x0, 2);
	CHECK_EQ(bottom.x1, 5);

	// Non-square: 8x5 is wider than tall, but still an ellipse.
	const PaintCore::EllipseSpan flat_top =
		PaintCore::ellipse_span_for_row(0, 0, 7, 4, 0);
	CHECK_EQ(flat_top.x0, 2);
	CHECK_EQ(flat_top.x1, 5);

	const PaintCore::EllipseSpan flat_middle =
		PaintCore::ellipse_span_for_row(0, 0, 7, 4, 2);
	CHECK_EQ(flat_middle.x0, 0);
	CHECK_EQ(flat_middle.x1, 7);
}

TEST_CASE("ellipse outline is symmetric and inside the fill spans")
{
	const std::pair<int, int> sizes[] = {
		{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {8, 8}, {9, 9},
		{8, 5}, {9, 5}, {5, 8}, {3, 7}, {7, 3}, {2, 20}, {20, 2},
		{4, 20}, {20, 4}, {6, 25}, {25, 6}, {3, 40}, {40, 3},
	};

	for (const auto& size : sizes)
	{
		const int width = size.first;
		const int height = size.second;
		const int right = width - 1;
		const int bottom = height - 1;

		std::vector<PaintCore::EllipseSpan> spans;
		PaintCore::rasterize_ellipse_spans(
			0, 0, right, bottom,
			[&](int y, int x0, int x1)
			{
				REQUIRE_EQ(y, static_cast<int>(spans.size()));
				spans.push_back({x0, x1});
			});
		REQUIRE_EQ(static_cast<int>(spans.size()), height);

		std::set<std::pair<int, int>> outline;
		PaintCore::rasterize_ellipse_outline(
			0, 0, right, bottom,
			[&](int x, int y)
			{
				outline.emplace(x, y);
			});

		CHECK_FALSE(outline.empty());
		for (const auto& point : outline)
		{
			const int x = point.first;
			const int y = point.second;
			REQUIRE(y >= 0);
			REQUIRE(y < height);
			CHECK(x >= spans[y].x0);
			CHECK(x <= spans[y].x1);

			CHECK(outline.count({right - x, y}) == 1u);
			CHECK(outline.count({x, bottom - y}) == 1u);
		}

		for (int y = 0; y < height; ++y)
		{
			bool has_outline = false;
			for (int x = spans[y].x0; x <= spans[y].x1; ++x)
			{
				has_outline = has_outline || outline.count({x, y}) != 0u;
			}
			CHECK(has_outline);
		}
	}
}

TEST_CASE("CircleTool exposes the same shape options as the rectangle")
{
	CircleTool circle(1, 2);
	const std::vector<ToolOption>& options = circle.options();

	REQUIRE(options.size() == 1u);
	CHECK(options[0].id == "mode");
	CHECK(options[0].type == ToolOptionType::Choice);
	CHECK(options[0].visible_in_options_bar);
	CHECK_EQ(options[0].value, static_cast<int>(CircleTool::Mode::Both));
	REQUIRE(options[0].choices.size() == 3u);
	CHECK_EQ(options[0].choices[0].value,
	         static_cast<int>(CircleTool::Mode::Outline));
	CHECK_EQ(options[0].choices[1].value,
	         static_cast<int>(CircleTool::Mode::Fill));
	CHECK_EQ(options[0].choices[2].value,
	         static_cast<int>(CircleTool::Mode::Both));
	CHECK(options[0].choices[0].icon == "rect.outline");

	CHECK(circle.set_option("mode", static_cast<int>(CircleTool::Mode::Fill)));
	CHECK_EQ(circle.option_value("mode"),
	         static_cast<int>(CircleTool::Mode::Fill));
	CHECK_FALSE(circle.set_option("mode", 99));
	CHECK_FALSE(circle.set_option("unknown", 1));
}

TEST_CASE("CircleTool draws an ellipse outline inside the drag box")
{
	PaintSession canvas(10, 10);
	CircleTool tool(1, 2);
	tool.set_option("mode", static_cast<int>(CircleTool::Mode::Outline));
	draw_shape(canvas, tool, 1, 1, 8, 8);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(4, 1), 1); // top cap
	CHECK_EQ(image.get_pixel(3, 1), 1);
	CHECK_EQ(image.get_pixel(6, 1), 1);
	CHECK_EQ(image.get_pixel(1, 4), 1); // left side
	CHECK_EQ(image.get_pixel(8, 4), 1); // right side
	CHECK_EQ(image.get_pixel(4, 8), 1); // bottom cap
	CHECK_EQ(image.get_pixel(4, 4), 0); // hollow interior
	CHECK_EQ(image.get_pixel(1, 1), 0); // bbox corner outside the ellipse
	CHECK_EQ(image.get_pixel(9, 1), 0);

	PaintSession flat_canvas(10, 10);
	CircleTool flat_tool(1, 2);
	flat_tool.set_option("mode", static_cast<int>(CircleTool::Mode::Outline));
	draw_shape(flat_canvas, flat_tool, 1, 1, 8, 5);

	const ImageBuffer& flat = flat_canvas.get_main_buffer();
	CHECK_EQ(flat.get_pixel(3, 1), 1);
	CHECK_EQ(flat.get_pixel(6, 1), 1);
	CHECK_EQ(flat.get_pixel(2, 1), 0); // rounded top proves it is an ellipse
	CHECK_EQ(flat.get_pixel(7, 1), 0);
	CHECK_EQ(flat.get_pixel(1, 3), 1);
	CHECK_EQ(flat.get_pixel(8, 3), 1);
	CHECK_EQ(flat.get_pixel(4, 3), 0);
}

TEST_CASE("CircleTool exposes the press point as drag anchor")
{
	PaintSession canvas(10, 10);
	CircleTool tool(1, 2);
	canvas.set_tool(&tool);

	PaintCore::Vec2 anchor{};
	CHECK_FALSE(tool.drag_anchor(anchor));

	tool.on_mouse_press(canvas, PaintCore::Vec2{2, 3}, 1);
	REQUIRE(tool.drag_anchor(anchor));
	CHECK_EQ(anchor.x, 2);
	CHECK_EQ(anchor.y, 3);

	tool.on_mouse_move(canvas, PaintCore::Vec2{7, 8});
	REQUIRE(tool.drag_anchor(anchor));
	CHECK_EQ(anchor.x, 2);
	CHECK_EQ(anchor.y, 3);

	tool.on_mouse_release(canvas, PaintCore::Vec2{7, 8}, 1);
	CHECK_FALSE(tool.drag_anchor(anchor));
}

TEST_CASE("CircleTool fill mode uses the background color")
{
	PaintSession canvas(10, 10);
	CircleTool tool(1, 2);
	tool.set_option("mode", static_cast<int>(CircleTool::Mode::Fill));
	draw_shape(canvas, tool, 1, 1, 8, 8);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(4, 1), 2); // top cap is filled
	CHECK_EQ(image.get_pixel(1, 4), 2); // widest rows reach the box edges
	CHECK_EQ(image.get_pixel(8, 4), 2);
	CHECK_EQ(image.get_pixel(4, 4), 2); // interior
	CHECK_EQ(image.get_pixel(1, 1), 0); // corner outside the ellipse
	CHECK_EQ(image.get_pixel(0, 0), 0);
}

TEST_CASE("CircleTool both mode combines outline and fill")
{
	PaintSession canvas(10, 10);
	CircleTool tool(1, 2);
	tool.set_option("mode", static_cast<int>(CircleTool::Mode::Both));
	draw_shape(canvas, tool, 1, 1, 8, 8);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(4, 1), 1); // foreground outline
	CHECK_EQ(image.get_pixel(1, 4), 1);
	CHECK_EQ(image.get_pixel(4, 8), 1);
	CHECK_EQ(image.get_pixel(4, 4), 2); // background fill
	CHECK_EQ(image.get_pixel(3, 3), 2);
	CHECK_EQ(image.get_pixel(1, 1), 0);
}

TEST_CASE("CircleTool constrains to a circle with Shift")
{
	PaintSession canvas(12, 12);
	CircleTool tool(1, 2);
	tool.set_option("mode", static_cast<int>(CircleTool::Mode::Outline));
	canvas.set_tool(&tool);
	PaintCore::Modifiers modifiers;
	modifiers.shift = true;
	canvas.set_modifiers(modifiers);

	// 9x5 drag -> the same min-side square constraint as the rectangle.
	canvas.handle_mouse_press({1, 1}, 1);
	canvas.handle_mouse_move({9, 5});
	canvas.handle_mouse_release({9, 5}, 1);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(3, 1), 1); // top cap of the 5x5 circle
	CHECK_EQ(image.get_pixel(1, 3), 1); // left side
	CHECK_EQ(image.get_pixel(5, 3), 1); // right side
	CHECK_EQ(image.get_pixel(3, 5), 1); // bottom cap
	CHECK_EQ(image.get_pixel(3, 2), 0); // hollow interior
	CHECK_EQ(image.get_pixel(9, 5), 0); // raw drag end is outside
	CHECK_EQ(image.get_pixel(5, 1), 0); // 5-wide circle, not the 9-wide drag
}

TEST_CASE("CircleTool swaps foreground and background on the right button")
{
	PaintSession canvas(10, 10);
	CircleTool tool(1, 2);
	tool.set_option("mode", static_cast<int>(CircleTool::Mode::Both));
	draw_shape(canvas, tool, 1, 1, 8, 8, 2);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(4, 1), 2); // swapped outline
	CHECK_EQ(image.get_pixel(1, 4), 2);
	CHECK_EQ(image.get_pixel(4, 4), 1); // swapped fill
	CHECK_EQ(image.get_pixel(1, 1), 0);
}

TEST_CASE("CircleTool clips large ellipses to the canvas without squashing")
{
	PaintSession canvas(10, 10);
	CircleTool tool(1, 2);
	tool.set_option("mode", static_cast<int>(CircleTool::Mode::Fill));

	// A 31x31 circle centered at (-5,-5): the top-left of the canvas is
	// inside, the bottom-right is outside. Clamping the bbox to the canvas
	// would turn it into a 10x10 ellipse and lose the top-left fill.
	draw_shape(canvas, tool, -20, -20, 10, 10);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(0, 0), 2);
	CHECK_EQ(image.get_pixel(5, 5), 2);
	CHECK_EQ(image.get_pixel(9, 9), 0);
}

TEST_CASE("CircleTool handles one-pixel and two-pixel shapes")
{
	PaintSession pixel_canvas(4, 4);
	CircleTool pixel_tool(1, 2);
	pixel_tool.set_option("mode", static_cast<int>(CircleTool::Mode::Fill));
	draw_shape(pixel_canvas, pixel_tool, 1, 1, 1, 1);
	CHECK_EQ(pixel_canvas.get_main_buffer().get_pixel(1, 1), 2);
	CHECK_EQ(pixel_canvas.get_main_buffer().get_pixel(0, 0), 0);

	PaintSession two_canvas(4, 4);
	CircleTool two_tool(1, 2);
	two_tool.set_option("mode", static_cast<int>(CircleTool::Mode::Fill));
	draw_shape(two_canvas, two_tool, 1, 1, 2, 2);
	CHECK_EQ(two_canvas.get_main_buffer().get_pixel(1, 1), 2);
	CHECK_EQ(two_canvas.get_main_buffer().get_pixel(2, 1), 2);
	CHECK_EQ(two_canvas.get_main_buffer().get_pixel(1, 2), 2);
	CHECK_EQ(two_canvas.get_main_buffer().get_pixel(2, 2), 2);
}

TEST_CASE("CircleTool cancels an unconfirmed stroke when both buttons are used")
{
	PaintSession canvas(10, 10);
	CircleTool tool(1, 2);
	canvas.set_tool(&tool);

	canvas.handle_mouse_press({1, 1}, 1);
	canvas.handle_mouse_move({8, 8});
	canvas.handle_mouse_press({8, 8}, 2);
	CHECK_FALSE(tool.is_drawing());
	canvas.handle_mouse_release({8, 8}, 2);
	canvas.handle_mouse_release({8, 8}, 1);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(4, 1), 0);
	CHECK_EQ(image.get_pixel(4, 4), 0);
	CHECK_FALSE(canvas.can_undo());
}
