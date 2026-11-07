// Headless tests for tool options and the Rect outline/fill modes.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include "paint/canvas.h"
#include "paint/image_buffer.h"
#include "tools/base_tool.h"
#include "tools/square_tool.h"

namespace
{
struct ProbeTool : ToolBase
{
	ProbeTool()
	{
		add_option(ToolOption::integer("size", "Size", 3, 1, 5));
	}

	void on_mouse_press(Canvas&, const PaintCore::Vec2&, int) override {}
	void on_mouse_move(Canvas&, const PaintCore::Vec2&) override {}
	void on_mouse_release(Canvas&, const PaintCore::Vec2&, int) override {}
	void on_cancel(Canvas&) override {}
};

void draw_rectangle(Canvas& canvas, SquareTool& tool,
                    int start_x, int start_y, int end_x, int end_y)
{
	canvas.set_tool(&tool);
	canvas.handle_mouse_press({start_x, start_y}, 1);
	canvas.handle_mouse_move({end_x, end_y});
	canvas.handle_mouse_release({end_x, end_y}, 1);
}
}

TEST_CASE("Tool options expose defaults and validate values")
{
	SquareTool square(1, 2);
	const std::vector<ToolOption>& options = square.options();

	REQUIRE(options.size() == 1u);
	CHECK(options[0].id == "mode");
	CHECK(options[0].type == ToolOptionType::Choice);
	CHECK(options[0].visible_in_options_bar);
	CHECK_EQ(options[0].value, static_cast<int>(SquareTool::Mode::Outline));
	REQUIRE(options[0].choices.size() == 3u);
	CHECK_EQ(options[0].choices[0].value, static_cast<int>(SquareTool::Mode::Outline));
	CHECK_EQ(options[0].choices[1].value, static_cast<int>(SquareTool::Mode::Fill));
	CHECK_EQ(options[0].choices[2].value, static_cast<int>(SquareTool::Mode::Both));
	CHECK(options[0].choices[0].icon == "rect.outline");

	CHECK(square.set_option("mode", static_cast<int>(SquareTool::Mode::Fill)));
	CHECK_EQ(square.option_value("mode"), static_cast<int>(SquareTool::Mode::Fill));
	CHECK_FALSE(square.set_option("mode", 99));
	CHECK_EQ(square.option_value("mode"), static_cast<int>(SquareTool::Mode::Fill));
	CHECK_FALSE(square.set_option("unknown", 1));
	CHECK_FALSE(square.set_option("mode", static_cast<int>(SquareTool::Mode::Fill)));
}

TEST_CASE("Integer tool options clamp to their range")
{
	ProbeTool probe;
	CHECK_EQ(probe.option_value("size"), 3);

	CHECK(probe.set_option("size", 10));
	CHECK_EQ(probe.option_value("size"), 5);

	CHECK(probe.set_option("size", 0));
	CHECK_EQ(probe.option_value("size"), 1);

	CHECK_FALSE(probe.set_option("size", 1));
}

TEST_CASE("SquareTool draws only the outline by default")
{
	Canvas canvas(8, 8);
	SquareTool tool(1, 2);
	draw_rectangle(canvas, tool, 1, 1, 4, 4);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(1, 1), 1);
	CHECK_EQ(image.get_pixel(4, 4), 1);
	CHECK_EQ(image.get_pixel(2, 2), 0);
	CHECK_EQ(image.get_pixel(0, 0), 0);
}

TEST_CASE("SquareTool fill mode uses the background color")
{
	Canvas canvas(8, 8);
	SquareTool tool(1, 2);
	tool.set_option("mode", static_cast<int>(SquareTool::Mode::Fill));
	draw_rectangle(canvas, tool, 1, 1, 4, 4);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(1, 1), 2);
	CHECK_EQ(image.get_pixel(2, 2), 2);
	CHECK_EQ(image.get_pixel(4, 4), 2);
	CHECK_EQ(image.get_pixel(0, 0), 0);
}

TEST_CASE("SquareTool both mode combines outline and fill")
{
	Canvas canvas(8, 8);
	SquareTool tool(1, 2);
	tool.set_option("mode", static_cast<int>(SquareTool::Mode::Both));
	draw_rectangle(canvas, tool, 1, 1, 4, 4);

	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK_EQ(image.get_pixel(1, 1), 1);
	CHECK_EQ(image.get_pixel(4, 4), 1);
	CHECK_EQ(image.get_pixel(2, 2), 2);
	CHECK_EQ(image.get_pixel(0, 0), 0);
}

TEST_CASE("Tool option state is independent per tool instance")
{
	SquareTool first(1, 2);
	SquareTool second(1, 2);

	first.set_option("mode", static_cast<int>(SquareTool::Mode::Fill));
	CHECK_EQ(first.option_value("mode"), static_cast<int>(SquareTool::Mode::Fill));
	CHECK_EQ(second.option_value("mode"), static_cast<int>(SquareTool::Mode::Outline));
}
