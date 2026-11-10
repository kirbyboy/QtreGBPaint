// Headless tests for the cubic Bezier rasterizer and the Curve tool.
// Run with `make -C tests test-curve` or `make test` from the root.

#include "doctest/doctest.h"

#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/bezier.h"
#include "core/line_stamp.h"
#include "core/vec2.h"
#include "paint/canvas_document.h"
#include "paint/draw_buffer.h"
#include "paint/image_buffer.h"
#include "paint/paint_session.h"
#include "tools/base_tool.h"
#include "tools/curve_tool.h"
#include "tools/line_width.h"

using PaintCore::ColorIndex;
using PaintCore::CubicBezier;
using PaintCore::Vec2;

namespace
{
void drag_curve(CurveTool& tool, PaintSession& session,
                const Vec2& a, const Vec2& b,
                int button = ToolBase::kLeftButton)
{
	session.set_tool(&tool);
	session.handle_mouse_press(a, button);
	session.handle_mouse_move(b);
	session.handle_mouse_release(b, button);
}

ColorIndex pixel_at(PaintSession& session, int x, int y)
{
	return session.get_main_buffer().get_pixel(x, y);
}

std::vector<std::pair<int, int>> rasterize(const CubicBezier& curve)
{
	std::vector<std::pair<int, int>> points;
	PaintCore::rasterize_cubic_bezier(
		curve, [&](int x, int y) { points.push_back({x, y}); });
	return points;
}

std::set<std::pair<int, int>> curve_stamp_pixels(const CubicBezier& curve,
                                                  int width)
{
	std::set<std::pair<int, int>> pixels;
	PaintCore::rasterize_cubic_bezier(
		curve,
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

TEST_CASE("cubic Bezier endpoints and midpoint")
{
	const CubicBezier curve{{0, 0}, {10, 30}, {30, -10}, {40, 20}};

	const PaintCore::Vec2f start = PaintCore::cubic_bezier_point(curve, 0.0f);
	const PaintCore::Vec2f end = PaintCore::cubic_bezier_point(curve, 1.0f);
	CHECK(std::abs(start.x - 0.0f) < 1e-5f);
	CHECK(std::abs(start.y - 0.0f) < 1e-5f);
	CHECK(std::abs(end.x - 40.0f) < 1e-5f);
	CHECK(std::abs(end.y - 20.0f) < 1e-5f);

	const PaintCore::Vec2f middle =
		PaintCore::cubic_bezier_point(curve, 0.5f);
	CHECK(std::abs(middle.x - 20.0f) < 1e-5f);
	CHECK(std::abs(middle.y - 10.0f) < 1e-5f);
}

TEST_CASE("cubic Bezier with collinear controls is a straight line")
{
	const CubicBezier curve{{0, 0}, {10, 0}, {20, 0}, {30, 0}};
	const std::vector<std::pair<int, int>> points = rasterize(curve);

	REQUIRE_EQ(points.size(), 31u);
	for (int x = 0; x <= 30; ++x)
	{
		REQUIRE_EQ(points[static_cast<std::size_t>(x)].first, x);
		CHECK_EQ(points[static_cast<std::size_t>(x)].second, 0);
	}
}

TEST_CASE("cubic Bezier stays inside the control polygon bounds")
{
	const CubicBezier curve{{5, 5}, {5, 20}, {25, 0}, {25, 15}};
	const std::vector<std::pair<int, int>> points = rasterize(curve);

	REQUIRE_FALSE(points.empty());
	CHECK_EQ(points.front().first, 5);
	CHECK_EQ(points.front().second, 5);
	CHECK_EQ(points.back().first, 25);
	CHECK_EQ(points.back().second, 15);

	for (const auto& point : points)
	{
		CHECK(point.first >= 5);
		CHECK(point.first <= 25);
		CHECK(point.second >= 0);
		CHECK(point.second <= 20);
	}
}

TEST_CASE("cubic Bezier rasterization has no 2x2 filled blocks")
{
	const CubicBezier curve{{2, 30}, {18, 2}, {46, 58}, {62, 30}};
	std::set<std::pair<int, int>> points;
	PaintCore::rasterize_cubic_bezier(
		curve, [&](int x, int y) { points.insert({x, y}); });

	REQUIRE_FALSE(points.empty());
	for (const auto& point : points)
	{
		const bool right = points.count({point.first + 1, point.second}) != 0;
		const bool down = points.count({point.first, point.second + 1}) != 0;
		const bool diagonal =
			points.count({point.first + 1, point.second + 1}) != 0;
		const bool full_block = right && down && diagonal;
		CHECK_FALSE(full_block);
	}
}

TEST_CASE("Curve first drag leaves a pending edit with handles")
{
	PaintSession session(32, 32);
	CurveTool tool(2, 1);

	drag_curve(tool, session, {4, 4}, {28, 28});

	CHECK(tool.has_pending_edit());
	CHECK(session.has_pending_edit());
	CHECK_FALSE(tool.is_drawing());
	CHECK(session.can_undo());
	CHECK_FALSE(session.can_redo());

	ToolBase::EditOverlay overlay;
	REQUIRE(tool.edit_overlay(overlay));
	REQUIRE_EQ(overlay.guides.size(), 3u);
	REQUIRE_EQ(overlay.handles.size(), 4u);
	CHECK_EQ(overlay.handles[0].pos.x, 4);
	CHECK_EQ(overlay.handles[0].pos.y, 4);
	CHECK_FALSE(overlay.handles[0].draggable);
	CHECK_EQ(overlay.handles[1].pos.x, 12);
	CHECK_EQ(overlay.handles[1].pos.y, 12);
	CHECK(overlay.handles[1].draggable);
	CHECK_EQ(overlay.handles[2].pos.x, 20);
	CHECK_EQ(overlay.handles[2].pos.y, 20);
	CHECK(overlay.handles[2].draggable);
	CHECK_EQ(overlay.handles[3].pos.x, 28);
	CHECK_EQ(overlay.handles[3].pos.y, 28);
	CHECK_FALSE(overlay.handles[3].draggable);

	DrawBuffer* preview = tool.get_draw_buffer();
	REQUIRE(preview != nullptr);
	CHECK(preview->is_active());
	CHECK_EQ(preview->data()[static_cast<std::size_t>(16 - preview->y()) *
	                            static_cast<std::size_t>(preview->width()) +
	                        static_cast<std::size_t>(16 - preview->x())],
	         2);

	CHECK(session.commit_pending_edit());
	CHECK_FALSE(session.has_pending_edit());
	CHECK_EQ(pixel_at(session, 16, 16), 2);
	CHECK(session.can_undo());

	session.undo();
	CHECK_EQ(pixel_at(session, 16, 16), 0);
}

TEST_CASE("Curve handles reshape the curve and commit one history step")
{
	PaintSession session(32, 32);
	CurveTool tool(2, 1);

	drag_curve(tool, session, {4, 16}, {28, 16});
	REQUIRE(tool.has_pending_edit());

	session.begin_edit_handle(1);
	session.move_edit_handle(1, {8, 4});
	session.end_edit_handle();
	session.begin_edit_handle(2);
	session.move_edit_handle(2, {24, 28});
	session.end_edit_handle();

	ToolBase::EditOverlay overlay;
	REQUIRE(tool.edit_overlay(overlay));
	CHECK_EQ(overlay.handles[1].pos.x, 8);
	CHECK_EQ(overlay.handles[1].pos.y, 4);
	CHECK_EQ(overlay.handles[2].pos.x, 24);
	CHECK_EQ(overlay.handles[2].pos.y, 28);

	const CubicBezier curve{{4, 16}, {8, 4}, {24, 28}, {28, 16}};
	const PaintCore::Vec2f point = PaintCore::cubic_bezier_point(curve, 0.25f);
	const int px = static_cast<int>(std::lround(point.x));
	const int py = static_cast<int>(std::lround(point.y));

	DrawBuffer* preview = tool.get_draw_buffer();
	REQUIRE(preview != nullptr);
	CHECK(preview->is_active());

	CHECK(session.commit_pending_edit());
	CHECK_EQ(pixel_at(session, px, py), 2);
	CHECK(session.can_undo());

	session.undo();
	CHECK_EQ(pixel_at(session, px, py), 0);
}

TEST_CASE("Curve Shift constrains only the initial line")
{
	PaintSession session(32, 32);
	CurveTool tool(2, 1);

	session.set_modifiers(PaintCore::Modifiers{true, false, false});
	drag_curve(tool, session, {4, 4}, {28, 10});
	REQUIRE(tool.has_pending_edit());

	ToolBase::EditOverlay overlay;
	REQUIRE(tool.edit_overlay(overlay));
	CHECK_EQ(overlay.handles[3].pos.x, 28);
	CHECK_EQ(overlay.handles[3].pos.y, 4);

	// Shift no longer affects the control handles.
	session.begin_edit_handle(1);
	session.move_edit_handle(1, {8, 2});
	session.end_edit_handle();
	REQUIRE(tool.edit_overlay(overlay));
	CHECK_EQ(overlay.handles[1].pos.x, 8);
	CHECK_EQ(overlay.handles[1].pos.y, 2);
}

TEST_CASE("Curve right-button drag uses the background color")
{
	PaintSession session(32, 32);
	CurveTool tool(2, 3);

	drag_curve(tool, session, {4, 4}, {28, 28},
	           ToolBase::kRightButton);

	REQUIRE(tool.has_pending_edit());
	CHECK(session.commit_pending_edit());
	CHECK_EQ(pixel_at(session, 16, 16), 3);
}

TEST_CASE("Curve exposes five widths with 1..5 pixel line previews")
{
	CurveTool tool(1, 0);
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

		CHECK_EQ(max_x - min_x + 1, LineWidth::kPreviewLength);
		CHECK_EQ(max_y - min_y + 1, index + 1);
	}
}

TEST_CASE("Curve sweeps each of the five width footprints")
{
	for (int width = 1; width <= 5; ++width)
	{
		PaintSession session(40, 40);
		CurveTool tool(2, 0);
		if (width != 1)
		{
			REQUIRE(tool.set_option("width", width));
		}

		drag_curve(tool, session, {4, 20}, {36, 20});
		REQUIRE(tool.has_pending_edit());

		session.begin_edit_handle(1);
		session.move_edit_handle(1, {10, 4});
		session.end_edit_handle();
		session.begin_edit_handle(2);
		session.move_edit_handle(2, {30, 36});
		session.end_edit_handle();

		REQUIRE(session.commit_pending_edit());

		const CubicBezier curve{{4, 20}, {10, 4}, {30, 36}, {36, 20}};
		CHECK_EQ(painted_pixels(session.get_main_buffer()),
		         curve_stamp_pixels(curve, width));
	}
}

TEST_CASE("Curve freezes the width chosen when the stroke starts")
{
	PaintSession session(32, 32);
	CurveTool tool(2, 1);

	drag_curve(tool, session, {4, 16}, {28, 16});
	REQUIRE(tool.has_pending_edit());
	REQUIRE(tool.set_option("width", 5));
	REQUIRE(session.commit_pending_edit());

	// The pending curve keeps the one-pixel width selected on press.
	CHECK_EQ(pixel_at(session, 16, 16), 2);
	CHECK_EQ(pixel_at(session, 16, 15), 0);
	CHECK_EQ(pixel_at(session, 16, 17), 0);
}

TEST_CASE("Curve tool switch commits the pending edit")
{
	PaintSession session(32, 32);
	CurveTool tool(2, 1);

	drag_curve(tool, session, {4, 4}, {28, 28});
	REQUIRE(tool.has_pending_edit());

	session.set_tool(nullptr);

	CHECK_FALSE(session.has_pending_edit());
	CHECK_EQ(pixel_at(session, 16, 16), 2);
	CHECK(session.can_undo());
}

TEST_CASE("Curve undo cancels a pending edit before document history")
{
	PaintSession session(32, 32);
	REQUIRE(session.document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 1, 1}, {3}));

	CurveTool tool(2, 1);
	drag_curve(tool, session, {4, 4}, {28, 28});
	REQUIRE(tool.has_pending_edit());

	CHECK(session.undo());
	CHECK_FALSE(session.has_pending_edit());
	CHECK_EQ(pixel_at(session, 0, 0), 3);
	CHECK_EQ(pixel_at(session, 16, 16), 0);
	CHECK(session.can_undo());

	CHECK(session.undo());
	CHECK_EQ(pixel_at(session, 0, 0), 0);
	CHECK(session.can_redo());
}

TEST_CASE("Curve cancel and resize resolve the pending edit")
{
	PaintSession session(32, 32);
	CurveTool tool(2, 1);

	drag_curve(tool, session, {4, 4}, {28, 28});
	REQUIRE(tool.has_pending_edit());
	CHECK(session.cancel_pending_edit());
	CHECK_FALSE(session.has_pending_edit());
	CHECK_FALSE(session.can_undo());
	CHECK_EQ(pixel_at(session, 16, 16), 0);

	drag_curve(tool, session, {4, 4}, {28, 28});
	REQUIRE(tool.has_pending_edit());
	CHECK(session.resize(40, 40, 0));
	CHECK_FALSE(session.has_pending_edit());
	CHECK_EQ(pixel_at(session, 16, 16), 2);
	CHECK_EQ(session.getWidth(), 40);

	CHECK(session.undo());
	CHECK_EQ(session.getWidth(), 32);
	CHECK_EQ(pixel_at(session, 16, 16), 2);
	CHECK(session.undo());
	CHECK_EQ(pixel_at(session, 16, 16), 0);
}
