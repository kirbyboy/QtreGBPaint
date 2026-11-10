#include "tools/circle_tool.h"

#include <algorithm>

#include "core/ellipse.h"
#include "core/shape_constraint.h"

CircleTool::CircleTool(PaintCore::ColorIndex fg, PaintCore::ColorIndex bg)
	: ShapeTool(fg, bg)
{
}

void CircleTool::draw_preview(DrawBuffer& buffer,
                              const PaintCore::Modifiers& modifiers)
{
	const PaintCore::Vec2 end = modifiers.shift
		? PaintCore::square_constrained_end(m_start_pos, m_current_pos)
		: m_current_pos;

	buffer.ensure_fit(m_start_pos.x, m_start_pos.y);
	buffer.ensure_fit(end.x, end.y);

	// Keep the shape in image coordinates and translate once to the buffer's
	// local space. Pixels outside the canvas are dropped by set_pixel(), so a
	// drag that leaves the canvas is clipped instead of squashed.
	const int left = std::min(m_start_pos.x, end.x) - buffer.x();
	const int top = std::min(m_start_pos.y, end.y) - buffer.y();
	const int right = std::max(m_start_pos.x, end.x) - buffer.x();
	const int bottom = std::max(m_start_pos.y, end.y) - buffer.y();

	buffer.clear();

	const Mode shape_mode = mode();

	if (shape_mode != Mode::Outline)
	{
		PaintCore::rasterize_ellipse_spans(
			left, top, right, bottom,
			[&](int y, int x0, int x1)
			{
				for (int x = x0; x <= x1; ++x)
				{
					buffer.set_pixel(x, y, m_stroke_background);
				}
			});
	}

	if (shape_mode != Mode::Fill)
	{
		PaintCore::rasterize_ellipse_outline(
			left, top, right, bottom,
			[&](int x, int y)
			{
				buffer.set_pixel(x, y, m_stroke_foreground);
			});
	}
}
