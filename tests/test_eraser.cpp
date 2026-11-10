// Headless tests for the square eraser tool.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <utility>
#include <vector>

#include "core/vec2.h"
#include "paint/draw_buffer.h"
#include "paint/image_buffer.h"
#include "paint/paint_session.h"
#include "tools/eraser_tool.h"

using PaintCore::ColorIndex;

namespace
{
void fill_canvas(PaintSession& canvas, ColorIndex color)
{
	DrawBuffer buffer;
	buffer.set_bounds(canvas.getWidth(), canvas.getHeight());
	buffer.begin(0, 0);
	buffer.ensure_fit(canvas.getWidth() - 1, canvas.getHeight() - 1);

	for (int y = 0; y < canvas.getHeight(); ++y)
	{
		for (int x = 0; x < canvas.getWidth(); ++x)
		{
			buffer.set_pixel(x, y, color);
		}
	}

	canvas.commit_draw_buffer(buffer);
	canvas.clear_history();
}

void set_pixel(PaintSession& canvas, int x, int y, ColorIndex color)
{
	DrawBuffer buffer;
	buffer.set_bounds(canvas.getWidth(), canvas.getHeight());
	buffer.begin(0, 0);
	buffer.ensure_fit(x, y);
	buffer.set_pixel(x, y, color);
	canvas.commit_draw_buffer(buffer);
	canvas.clear_history();
}
}

TEST_CASE("EraserTool exposes four square sizes as a 2x2 grid")
{
	EraserTool tool(1, 2);
	const std::vector<ToolOption>& options = tool.options();

	REQUIRE_EQ(options.size(), 1u);
	CHECK(options[0].id == "size");
	CHECK(options[0].type == ToolOptionType::Choice);
	CHECK_EQ(options[0].grid_columns, 1);
	CHECK_EQ(options[0].value,
	         static_cast<int>(EraserTool::Size::Tiny));
	REQUIRE_EQ(options[0].choices.size(), 4u);
	CHECK(options[0].choices[0].label == "Eraser 4");
	CHECK(options[0].choices[1].label == "Eraser 6");
	CHECK(options[0].choices[2].label == "Eraser 8");
	CHECK(options[0].choices[3].label == "Eraser 10");

	const std::size_t expected_sizes[4] = {16, 36, 64, 100};
	for (std::size_t i = 0; i < 4; ++i)
	{
		CHECK_EQ(options[0].choices[i].preview.size(), expected_sizes[i]);
	}

	CHECK(tool.set_option("size",
	                      static_cast<int>(EraserTool::Size::Large)));
	CHECK_EQ(tool.option_value("size"),
	         static_cast<int>(EraserTool::Size::Large));

	const ToolBase::StampPreview preview = tool.stamp_preview(1, 2);
	REQUIRE(preview.offsets != nullptr);
	CHECK_EQ(preview.offsets->size(), 100u);
	CHECK_EQ(preview.color, 2);
	CHECK(preview.opaque);
	CHECK(preview.border);
}

TEST_CASE("EraserTool left click paints the background color")
{
	PaintSession canvas(12, 12);
	fill_canvas(canvas, 1);

	EraserTool tool(1, 2);
	tool.set_option("size", static_cast<int>(EraserTool::Size::Tiny));
	canvas.set_tool(&tool);

	canvas.handle_mouse_press({5, 5}, 1);
	canvas.handle_mouse_release({5, 5}, 1);

	const ImageBuffer& image = canvas.get_main_buffer();
	for (int y = 4; y <= 7; ++y)
	{
		for (int x = 4; x <= 7; ++x)
		{
			CHECK_EQ(image.get_pixel(x, y), 2);
		}
	}
	CHECK_EQ(image.get_pixel(3, 3), 1);
	CHECK_EQ(image.get_pixel(4, 3), 1);
	CHECK_EQ(image.get_pixel(3, 4), 1);
	CHECK_EQ(image.get_pixel(8, 8), 1);

	CHECK(canvas.can_undo());
	REQUIRE(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 5), 1);
}

TEST_CASE("EraserTool right click only replaces foreground pixels")
{
	PaintSession canvas(12, 12);
	fill_canvas(canvas, 1);
	set_pixel(canvas, 5, 5, 3); // neither foreground nor background
	set_pixel(canvas, 6, 5, 2); // already background

	EraserTool tool(1, 2);
	tool.set_option("size", static_cast<int>(EraserTool::Size::Tiny));
	canvas.set_tool(&tool);

	canvas.handle_mouse_press({5, 5}, 2);
	canvas.handle_mouse_release({5, 5}, 2);

	const ImageBuffer& image = canvas.get_main_buffer();
	for (int y = 4; y <= 7; ++y)
	{
		for (int x = 4; x <= 7; ++x)
		{
			if ((x == 5 && y == 5) || (x == 6 && y == 5))
			{
				continue;
			}
			CHECK_EQ(image.get_pixel(x, y), 2);
		}
	}
	CHECK_EQ(image.get_pixel(5, 5), 3);
	CHECK_EQ(image.get_pixel(6, 5), 2);
	CHECK_EQ(image.get_pixel(3, 3), 1);
	CHECK_EQ(image.get_pixel(8, 8), 1);
}

TEST_CASE("EraserTool cancels an unconfirmed erasure with both buttons")
{
	PaintSession canvas(12, 12);
	fill_canvas(canvas, 1);

	EraserTool tool(1, 2);
	canvas.set_tool(&tool);

	canvas.handle_mouse_press({5, 5}, 1);
	canvas.handle_mouse_move({6, 6});
	canvas.handle_mouse_press({6, 6}, 2);
	CHECK_FALSE(tool.is_drawing());
	canvas.handle_mouse_release({6, 6}, 2);
	canvas.handle_mouse_release({6, 6}, 1);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(5, 5), 1);
	CHECK_EQ(image.get_pixel(6, 6), 1);
	CHECK_EQ(image.get_pixel(0, 0), 1);
	CHECK_FALSE(canvas.can_undo());
}

TEST_CASE("EraserTool expands the buffer for the full square footprint")
{
	PaintSession canvas(24, 24);
	fill_canvas(canvas, 1);

	EraserTool tool(1, 2);
	tool.set_option("size", static_cast<int>(EraserTool::Size::Large));
	canvas.set_tool(&tool);

	// Size 10 around a horizontal path: the square spans 5 pixels on each
	// side of the pointer and must not be clipped by the DrawBuffer.
	canvas.handle_mouse_press({5, 5}, 1);
	canvas.handle_mouse_move({15, 5});
	canvas.handle_mouse_release({15, 5}, 1);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(1, 1), 2);
	CHECK_EQ(image.get_pixel(20, 10), 2);
	CHECK_EQ(image.get_pixel(10, 5), 2);
	CHECK_EQ(image.get_pixel(11, 5), 2);
	CHECK_EQ(image.get_pixel(0, 5), 1);
	CHECK_EQ(image.get_pixel(21, 5), 1);
}
