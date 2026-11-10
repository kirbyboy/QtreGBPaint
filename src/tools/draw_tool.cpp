#include "tools/draw_tool.h"

DrawTool::DrawTool(PaintCore::ColorIndex foreground, PaintCore::ColorIndex background) :
m_foreground(foreground),
m_background(background),
m_stroke_foreground(foreground),
m_stroke_background(background)
{
}

void DrawTool::set_colors(PaintCore::ColorIndex foreground, PaintCore::ColorIndex background)
{
	m_foreground = foreground;
	m_background = background;
	if (!m_drawing)
	{
		m_stroke_foreground = foreground;
		m_stroke_background = background;
	}
}

void DrawTool::on_mouse_press(PaintSession &canvas, const PaintCore::Vec2 &pos, int button)
{
	if (m_drawing)
	{
		if (button != m_active_button)
		{
			on_cancel(canvas);
		}
		return;
	}

	m_active_button = button;
	const bool right_button = button == kRightButton;
	m_stroke_foreground = right_button ? m_background : m_foreground;
	m_stroke_background = right_button ? m_foreground : m_background;

	m_drawing = true;
	m_start_pos = pos;
	m_current_pos = pos;
	m_preview_buffer.set_bounds(canvas.getWidth(), canvas.getHeight());
	m_preview_buffer.begin(pos.x, pos.y);
	m_preview_buffer.clear();
}

void DrawTool::on_mouse_move(PaintSession &canvas, const PaintCore::Vec2 &pos)
{
	(void)canvas;

	if (!m_drawing)
	{
		return;
	}

	m_current_pos = pos;

	update_preview(canvas);
}

void DrawTool::on_mouse_release(PaintSession &canvas, const PaintCore::Vec2 &pos, int button)
{
	if (!m_drawing)
	{
		return;
	}
	if (button != m_active_button)
	{
		return;
	}

	m_current_pos = pos;
	m_drawing = false;
	m_active_button = 0;

	update_preview(canvas);

	canvas.commit_draw_buffer(m_preview_buffer);
}

void DrawTool::on_cancel(PaintSession &canvas)
{
	(void)canvas;
	
	if (!m_drawing)
	{
		return;
	}

	m_drawing = false;
	m_active_button = 0;
	m_preview_buffer.reset();
}

void DrawTool::update_preview(PaintSession& canvas)
{
	m_preview_buffer.set_bounds(canvas.getWidth(), canvas.getHeight());
	draw_preview(m_preview_buffer, canvas.modifiers());
}

DrawBuffer* DrawTool::get_draw_buffer()
{
	return &m_preview_buffer;
}

PaintCore::Vec2 DrawTool::get_start_pos()
{
	return m_start_pos;
}
bool DrawTool::is_drawing() const
{
	return m_drawing;
}