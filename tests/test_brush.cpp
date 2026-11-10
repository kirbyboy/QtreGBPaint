// Headless tests for the brush masks and the brush tool.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "core/brush.h"
#include "core/vec2.h"
#include "paint/draw_buffer.h"
#include "paint/image_buffer.h"
#include "paint/paint_session.h"
#include "tools/brush_tool.h"

using PaintCore::BrushShape;
using PaintCore::Vec2;

namespace
{
bool contains(const std::vector<Vec2>& offsets, int x, int y)
{
	return std::any_of(
		offsets.begin(), offsets.end(),
		[x, y](const Vec2& offset)
		{
			return offset.x == x && offset.y == y;
		});
}

void draw_once(PaintSession& canvas, BrushTool& tool, int x, int y,
               int button = 1)
{
	canvas.set_tool(&tool);
	canvas.handle_mouse_press({x, y}, button);
	canvas.handle_mouse_release({x, y}, button);
}
}

TEST_CASE("brush masks match the requested shapes and sizes")
{
	const std::vector<Vec2> circle1 =
		PaintCore::make_brush_offsets(BrushShape::Circle, 3); // radius 1
	REQUIRE_EQ(circle1.size(), 5u);
	CHECK(contains(circle1, 0, 0));
	CHECK(contains(circle1, 1, 0));
	CHECK(contains(circle1, -1, 0));
	CHECK(contains(circle1, 0, 1));
	CHECK(contains(circle1, 0, -1));
	CHECK_FALSE(contains(circle1, 1, 1));

	const std::vector<Vec2> circle2 =
		PaintCore::make_brush_offsets(BrushShape::Circle, 5); // radius 2
	REQUIRE_EQ(circle2.size(), 21u);
	CHECK(contains(circle2, 0, 0));
	CHECK(contains(circle2, 2, 0));
	CHECK(contains(circle2, 0, 2));
	CHECK_FALSE(contains(circle2, 2, 2));

	const std::vector<Vec2> circle3 =
		PaintCore::make_brush_offsets(BrushShape::Circle, 7); // radius 3
	REQUIRE_EQ(circle3.size(), 37u);
	CHECK(contains(circle3, 0, 0));
	CHECK(contains(circle3, 3, 0));
	CHECK(contains(circle3, 1, -3));
	CHECK_FALSE(contains(circle3, 2, -3));
	CHECK_FALSE(contains(circle3, 3, 3));

	const std::vector<Vec2> square2 =
		PaintCore::make_brush_offsets(BrushShape::Square, 2);
	CHECK_EQ(square2.size(), 4u);
	CHECK(contains(square2, 0, 0));
	CHECK(contains(square2, 1, 1));

	CHECK_EQ(PaintCore::make_brush_offsets(BrushShape::Square, 5).size(),
	         25u);
	CHECK_EQ(PaintCore::make_brush_offsets(BrushShape::Square, 8).size(),
	         64u);

	const std::vector<Vec2> down2 =
		PaintCore::make_brush_offsets(BrushShape::DiagonalDown, 2);
	CHECK_EQ(down2.size(), 2u);
	CHECK(contains(down2, 0, 0));
	CHECK(contains(down2, 1, 1));

	const std::vector<Vec2> up2 =
		PaintCore::make_brush_offsets(BrushShape::DiagonalUp, 2);
	CHECK_EQ(up2.size(), 2u);
	CHECK(contains(up2, 0, 1));
	CHECK(contains(up2, 1, 0));

	const std::vector<Vec2> down5 =
		PaintCore::make_brush_offsets(BrushShape::DiagonalDown, 5);
	CHECK_EQ(down5.size(), 5u);
	CHECK(contains(down5, -2, -2));
	CHECK(contains(down5, 2, 2));

	const std::vector<Vec2> up5 =
		PaintCore::make_brush_offsets(BrushShape::DiagonalUp, 5);
	CHECK_EQ(up5.size(), 5u);
	CHECK(contains(up5, -2, 2));
	CHECK(contains(up5, 2, -2));

	CHECK_EQ(PaintCore::brush_margin(circle3), 3);
	CHECK_EQ(PaintCore::brush_margin(square2), 1);
	CHECK_EQ(PaintCore::brush_margin(
		PaintCore::make_brush_offsets(BrushShape::Square, 5)), 2);
	CHECK_EQ(PaintCore::brush_margin(
		PaintCore::make_brush_offsets(BrushShape::Square, 8)), 4);
	CHECK_EQ(PaintCore::brush_margin(down5), 2);
}

TEST_CASE("BrushTool exposes a 4x3 grid in the expected order")
{
	BrushTool tool(1, 2);
	const std::vector<ToolOption>& options = tool.options();

	REQUIRE_EQ(options.size(), 1u);
	CHECK(options[0].id == "brush");
	CHECK(options[0].type == ToolOptionType::Choice);
	CHECK_EQ(options[0].grid_columns, 3);
	CHECK_EQ(options[0].value,
	         static_cast<int>(BrushTool::Brush::CircleLarge));
	REQUIRE_EQ(options[0].choices.size(), 12u);

	CHECK(options[0].choices[0].label == "Circle r3");
	CHECK(options[0].choices[1].label == "Circle r2");
	CHECK(options[0].choices[2].label == "Circle r1");
	CHECK(options[0].choices[3].label == "Square 8");
	CHECK(options[0].choices[4].label == "Square 5");
	CHECK(options[0].choices[5].label == "Square 2");
	CHECK(options[0].choices[6].label == "Diagonal 45 8");
	CHECK(options[0].choices[9].label == "Diagonal 135 8");

	const std::size_t expected_sizes[12] = {
		37, 21, 5, 64, 25, 4, 8, 5, 2, 8, 5, 2,
	};
	for (std::size_t i = 0; i < 12; ++i)
	{
		CHECK_EQ(options[0].choices[i].preview.size(), expected_sizes[i]);
	}

	CHECK(tool.set_option("brush",
	                      static_cast<int>(BrushTool::Brush::SquareMedium)));
	CHECK_EQ(tool.option_value("brush"),
	         static_cast<int>(BrushTool::Brush::SquareMedium));
	const ToolBase::StampPreview preview = tool.stamp_preview(1, 2);
	REQUIRE(preview.offsets != nullptr);
	CHECK_EQ(preview.offsets->size(), 25u);
	CHECK_EQ(preview.color, 1);
	CHECK_FALSE(preview.opaque);
	CHECK_FALSE(preview.border);
}

TEST_CASE("BrushTool stamps the selected footprint centered on the pointer")
{
	{
		PaintSession canvas(12, 12);
		BrushTool tool(1, 2);
		draw_once(canvas, tool, 5, 5);

		const ImageBuffer& image = canvas.get_main_buffer();
		CHECK_EQ(image.get_pixel(5, 5), 1);
		CHECK_EQ(image.get_pixel(5, 2), 1);
		CHECK_EQ(image.get_pixel(4, 2), 1);
		CHECK_EQ(image.get_pixel(6, 2), 1);
		CHECK_EQ(image.get_pixel(3, 2), 0);
		CHECK_EQ(image.get_pixel(2, 5), 1);
		CHECK_EQ(image.get_pixel(8, 5), 1);
		CHECK_EQ(image.get_pixel(5, 8), 1);
		CHECK_EQ(image.get_pixel(2, 2), 0);
		CHECK_EQ(image.get_pixel(8, 8), 0);
	}

	{
		PaintSession canvas(12, 12);
		BrushTool tool(1, 2);
		tool.set_option("brush",
		                static_cast<int>(BrushTool::Brush::CircleSmall));
		draw_once(canvas, tool, 5, 5);

		// Radius 1 is the 3x3 plus, not a full square.
		const ImageBuffer& image = canvas.get_main_buffer();
		CHECK_EQ(image.get_pixel(5, 5), 1);
		CHECK_EQ(image.get_pixel(4, 5), 1);
		CHECK_EQ(image.get_pixel(6, 5), 1);
		CHECK_EQ(image.get_pixel(5, 4), 1);
		CHECK_EQ(image.get_pixel(5, 6), 1);
		CHECK_EQ(image.get_pixel(4, 4), 0);
	}

	{
		PaintSession canvas(12, 12);
		BrushTool tool(1, 2);
		tool.set_option("brush",
		                static_cast<int>(BrushTool::Brush::SquareSmall));
		draw_once(canvas, tool, 5, 5);

		const ImageBuffer& image = canvas.get_main_buffer();
		CHECK_EQ(image.get_pixel(5, 5), 1);
		CHECK_EQ(image.get_pixel(6, 5), 1);
		CHECK_EQ(image.get_pixel(5, 6), 1);
		CHECK_EQ(image.get_pixel(6, 6), 1);
		CHECK_EQ(image.get_pixel(4, 4), 0);
	}

	{
		PaintSession canvas(12, 12);
		BrushTool tool(1, 2);
		tool.set_option("brush",
		                static_cast<int>(BrushTool::Brush::DiagonalDownMedium));
		draw_once(canvas, tool, 5, 5);

		const ImageBuffer& image = canvas.get_main_buffer();
		CHECK_EQ(image.get_pixel(3, 3), 1);
		CHECK_EQ(image.get_pixel(4, 4), 1);
		CHECK_EQ(image.get_pixel(5, 5), 1);
		CHECK_EQ(image.get_pixel(6, 6), 1);
		CHECK_EQ(image.get_pixel(7, 7), 1);
		CHECK_EQ(image.get_pixel(6, 5), 0);
	}

	{
		PaintSession canvas(12, 12);
		BrushTool tool(1, 2);
		tool.set_option("brush",
		                static_cast<int>(BrushTool::Brush::DiagonalUpMedium));
		draw_once(canvas, tool, 5, 5);

		const ImageBuffer& image = canvas.get_main_buffer();
		CHECK_EQ(image.get_pixel(3, 7), 1);
		CHECK_EQ(image.get_pixel(4, 6), 1);
		CHECK_EQ(image.get_pixel(5, 5), 1);
		CHECK_EQ(image.get_pixel(6, 4), 1);
		CHECK_EQ(image.get_pixel(7, 3), 1);
		CHECK_EQ(image.get_pixel(6, 5), 0);
	}
}

TEST_CASE("BrushTool expands the buffer for the footprint at the edges")
{
	PaintSession canvas(14, 14);
	BrushTool tool(1, 2);
	tool.set_option("brush",
	                static_cast<int>(BrushTool::Brush::SquareMedium));
	canvas.set_tool(&tool);

	canvas.handle_mouse_press({1, 1}, 1);
	canvas.handle_mouse_move({8, 1});
	canvas.handle_mouse_release({8, 1}, 1);

	const ImageBuffer& image = canvas.get_main_buffer();
	// The 5x5 stamp extends two pixels around the path, clipped at the top and
	// left edges but complete inside the canvas.
	CHECK_EQ(image.get_pixel(0, 0), 1);
	CHECK_EQ(image.get_pixel(0, 3), 1);
	CHECK_EQ(image.get_pixel(10, 0), 1);
	CHECK_EQ(image.get_pixel(10, 3), 1);
	CHECK_EQ(image.get_pixel(11, 1), 0);
	CHECK_EQ(image.get_pixel(0, 4), 0);
}

TEST_CASE("BrushTool fills diagonal gaps without thickening parallel strokes")
{
	{
		// A down-right diagonal brush moving along the opposite diagonal:
		// the intermediate corner must be stamped.
		PaintSession canvas(12, 12);
		BrushTool tool(1, 2);
		tool.set_option("brush",
		                static_cast<int>(BrushTool::Brush::DiagonalDownSmall));
		canvas.set_tool(&tool);

		canvas.handle_mouse_press({5, 5}, 1);
		canvas.handle_mouse_move({6, 4});
		canvas.handle_mouse_release({6, 4}, 1);

		const ImageBuffer& image = canvas.get_main_buffer();
		CHECK_EQ(image.get_pixel(5, 5), 1);
		CHECK_EQ(image.get_pixel(6, 6), 1);
		CHECK_EQ(image.get_pixel(6, 5), 1); // gap-filling corner
		CHECK_EQ(image.get_pixel(7, 6), 1);
		CHECK_EQ(image.get_pixel(6, 4), 1);
		CHECK_EQ(image.get_pixel(7, 5), 1);
		CHECK_EQ(image.get_pixel(5, 4), 0);
	}

	{
		// Same geometry with the length-5 brush: the missing parallel line is
		// at the center of the swept band.
		PaintSession canvas(12, 12);
		BrushTool tool(1, 2);
		tool.set_option("brush",
		                static_cast<int>(BrushTool::Brush::DiagonalDownMedium));
		canvas.set_tool(&tool);

		canvas.handle_mouse_press({5, 5}, 1);
		canvas.handle_mouse_move({6, 4});
		canvas.handle_mouse_release({6, 4}, 1);

		const ImageBuffer& image = canvas.get_main_buffer();
		CHECK_EQ(image.get_pixel(5, 5), 1);
		CHECK_EQ(image.get_pixel(6, 4), 1);
		CHECK_EQ(image.get_pixel(4, 4), 1);
		CHECK_EQ(image.get_pixel(5, 4), 1); // gap-filling center line
		CHECK_EQ(image.get_pixel(6, 3), 0);
	}

	{
		// The same brush moving in its own diagonal direction must stay a
		// one-pixel line: no extra corner stamps.
		PaintSession canvas(12, 12);
		BrushTool tool(1, 2);
		tool.set_option("brush",
		                static_cast<int>(BrushTool::Brush::DiagonalDownSmall));
		canvas.set_tool(&tool);

		canvas.handle_mouse_press({5, 5}, 1);
		canvas.handle_mouse_move({6, 6});
		canvas.handle_mouse_release({6, 6}, 1);

		const ImageBuffer& image = canvas.get_main_buffer();
		CHECK_EQ(image.get_pixel(5, 5), 1);
		CHECK_EQ(image.get_pixel(6, 6), 1);
		CHECK_EQ(image.get_pixel(7, 7), 1);
		CHECK_EQ(image.get_pixel(6, 5), 0);
		CHECK_EQ(image.get_pixel(5, 6), 0);
	}

	{
		// Inverted brush crossing a down-right path.
		PaintSession canvas(12, 12);
		BrushTool tool(1, 2);
		tool.set_option("brush",
		                static_cast<int>(BrushTool::Brush::DiagonalUpSmall));
		canvas.set_tool(&tool);

		canvas.handle_mouse_press({5, 5}, 1);
		canvas.handle_mouse_move({6, 6});
		canvas.handle_mouse_release({6, 6}, 1);

		const ImageBuffer& image = canvas.get_main_buffer();
		CHECK_EQ(image.get_pixel(5, 6), 1);
		CHECK_EQ(image.get_pixel(6, 5), 1);
		CHECK_EQ(image.get_pixel(6, 6), 1); // gap-filling corner line
		CHECK_EQ(image.get_pixel(7, 5), 1);
		CHECK_EQ(image.get_pixel(6, 7), 1);
		CHECK_EQ(image.get_pixel(7, 6), 1);
		CHECK_EQ(image.get_pixel(5, 5), 0);
		CHECK_EQ(image.get_pixel(6, 4), 0);
	}
}

TEST_CASE("BrushTool swaps colors on the right button and cancels with both")
{
	{
		PaintSession canvas(10, 10);
		BrushTool tool(1, 2);
		tool.set_option("brush",
		                static_cast<int>(BrushTool::Brush::CircleSmall));
		draw_once(canvas, tool, 5, 5, 2);

		CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 5), 2);
		CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 5), 2);
		CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 4), 0);
	}

	{
		PaintSession canvas(10, 10);
		BrushTool tool(1, 2);
		canvas.set_tool(&tool);

		canvas.handle_mouse_press({5, 5}, 1);
		canvas.handle_mouse_move({7, 7});
		canvas.handle_mouse_press({7, 7}, 2);
		CHECK_FALSE(tool.is_drawing());
		canvas.handle_mouse_release({7, 7}, 2);
		canvas.handle_mouse_release({7, 7}, 1);

		CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 5), 0);
		CHECK_EQ(canvas.get_main_buffer().get_pixel(7, 7), 0);
		CHECK_FALSE(canvas.can_undo());
	}
}
