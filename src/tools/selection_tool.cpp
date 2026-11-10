#include "tools/selection_tool.h"

#include <algorithm>

#include "core/shape_constraint.h"
#include "paint/paint_session.h"

SelectionTool::SelectionTool()
{
	add_option(ToolOption::choice(
		"mode", "Mode", static_cast<int>(Mode::Normal),
		{
			{static_cast<int>(Mode::Normal), "Normal", "N",
			 "selection.normal", {}},
			{static_cast<int>(Mode::Transparent), "Transparent", "T",
			 "selection.transparent", {}},
		}));
}

void SelectionTool::on_mouse_press(PaintSession& canvas,
                                   const PaintCore::Vec2& pos, int button)
{
	if (button != 1 || m_dragging)
	{
		return;
	}

	m_start = pos;
	m_current = pos;
	m_shift = canvas.modifiers().shift;
	m_dragging = true;
}

void SelectionTool::on_mouse_move(PaintSession& canvas, const PaintCore::Vec2& pos)
{
	if (!m_dragging)
	{
		return;
	}

	m_current = pos;
	m_shift = canvas.modifiers().shift;
}

void SelectionTool::on_mouse_release(PaintSession& canvas,
                                     const PaintCore::Vec2& pos, int button)
{
	if (button != 1 || !m_dragging)
	{
		return;
	}

	m_current = pos;
	m_shift = canvas.modifiers().shift;
	m_dragging = false;

	// The floating editor clips the rectangle and copies the pixels. The tool
	// only contributes the marquee, the background color that fills the source
	// hole and whether background pixels should become transparent.
	const bool transparent =
		option_value("mode") == static_cast<int>(Mode::Transparent);
	canvas.begin_selection(marquee_rect(), m_background, transparent);
}

void SelectionTool::on_cancel(PaintSession& canvas)
{
	(void)canvas;
	m_dragging = false;
}

void SelectionTool::set_colors(PaintCore::ColorIndex foreground,
                               PaintCore::ColorIndex background)
{
	(void)foreground;
	m_background = background;
}

PaintCore::Rect SelectionTool::marquee_rect() const
{
	const PaintCore::Vec2 end = m_shift
		? PaintCore::square_constrained_end(m_start, m_current)
		: m_current;

	const int left = std::min(m_start.x, end.x);
	const int top = std::min(m_start.y, end.y);
	const int right = std::max(m_start.x, end.x);
	const int bottom = std::max(m_start.y, end.y);
	return {left, top, right - left + 1, bottom - top + 1};
}

void SelectionTool::sync_floating_region(PaintSession& canvas)
{
	canvas.set_floating_transparency(
		option_value("mode") == static_cast<int>(Mode::Transparent),
		m_background);
}
