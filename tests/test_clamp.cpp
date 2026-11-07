#include "doctest/doctest.h"

#include <climits>

#include "paint/canvas.h"
#include "paint/draw_buffer.h"
#include "tools/square_tool.h"

TEST_CASE("DrawBuffer and Canvas clamp")
{
	// The draw buffer must stay inside the canvas even with extreme pointers.
	DrawBuffer buffer;
	buffer.set_bounds(256, 256);
	buffer.begin(100, 100);
	buffer.ensure_fit(-1000000, 1000000);
	CHECK(buffer.x() == 0);
	CHECK(buffer.y() == 100);
	CHECK(buffer.width() == 101);
	CHECK(buffer.height() == 156);

	buffer.ensure_fit(INT_MAX, INT_MIN);
	CHECK(buffer.x() == 0);
	CHECK(buffer.y() == 0);
	CHECK(buffer.width() == 256);
	CHECK(buffer.height() == 256);
	CHECK(buffer.data().size() == 256u * 256u);

	buffer.ensure_fit(INT_MAX, INT_MAX);
	CHECK(buffer.width() == 256);
	CHECK(buffer.height() == 256);
	CHECK(buffer.data().size() == 256u * 256u);

	// Square dragged down-left outside the canvas: the out-of-canvas left
	// border is dropped and no fake border appears on the canvas edge.
	{
		Canvas canvas(256, 256);
		SquareTool tool(1, 2);
		tool.set_option("mode", static_cast<int>(SquareTool::Mode::Both));
		canvas.set_tool(&tool);
		canvas.handle_mouse_press({100, 100}, 1);
		canvas.handle_mouse_move({-50, 200});
		canvas.handle_mouse_release({-50, 200}, 1);

		const ImageBuffer& img = canvas.get_main_buffer();
		CHECK(img.get_pixel(100, 100) == 1); // top-right corner
		CHECK(img.get_pixel(0, 100) == 1);   // top border reaches the edge
		CHECK(img.get_pixel(0, 200) == 1);   // bottom border reaches the edge
		CHECK(img.get_pixel(100, 200) == 1); // right border
		CHECK(img.get_pixel(0, 150) == 2);   // interior, no fake left border
		CHECK(img.get_pixel(50, 150) == 2);  // interior
		CHECK(img.get_pixel(101, 150) == 0); // outside the shape, untouched
	}

	// Square dragged down-right outside the canvas: right/bottom borders are
	// dropped and the interior reaches the canvas edges.
	{
		Canvas canvas(256, 256);
		SquareTool tool(1, 2);
		tool.set_option("mode", static_cast<int>(SquareTool::Mode::Both));
		canvas.set_tool(&tool);
		canvas.handle_mouse_press({100, 100}, 1);
		canvas.handle_mouse_move({1000, 1000});
		canvas.handle_mouse_release({1000, 1000}, 1);

		const ImageBuffer& img = canvas.get_main_buffer();
		CHECK(img.get_pixel(100, 100) == 1); // top-left corner
		CHECK(img.get_pixel(150, 100) == 1); // top border
		CHECK(img.get_pixel(100, 150) == 1); // left border
		CHECK(img.get_pixel(255, 150) == 2); // interior, no fake right border
		CHECK(img.get_pixel(150, 255) == 2); // interior, no fake bottom border
		CHECK(img.get_pixel(255, 255) == 2); // interior corner
	}
}

TEST_CASE("SquareTool uses the colors selected from the palette")
{
	Canvas canvas(8, 8);
	SquareTool tool(1, 2);
	tool.set_option("mode", static_cast<int>(SquareTool::Mode::Both));
	canvas.set_tool(&tool);

	ToolBase* base = &tool;
	base->set_colors(3, 2);

	canvas.handle_mouse_press({1, 1}, 1);
	canvas.handle_mouse_move({4, 4});
	canvas.handle_mouse_release({4, 4}, 1);

	const ImageBuffer& img = canvas.get_main_buffer();
	CHECK(img.get_pixel(1, 1) == 3); // foreground border
	CHECK(img.get_pixel(4, 4) == 3); // foreground border
	CHECK(img.get_pixel(2, 2) == 2); // background interior
	CHECK(img.get_pixel(0, 0) == 0); // untouched canvas
}
