#include "tools/draw_tool.h"

void DrawTool::on_mouse_press(Canvas &canvas, const PaintCore::Vec2 &pos, int button)
{
	(void)canvas;
	(void)button;

	if(m_drawing)
	{
		return;
	}

	m_drawing = true;
	m_start_pos = pos;
	m_current_pos = pos;
	m_preview_buffer.begin(pos.x, pos.y);
	m_preview_buffer.clear();
}

void DrawTool::on_mouse_move(Canvas &canvas, const PaintCore::Vec2 &pos)
{
	(void)canvas;

	if (!m_drawing)
	{
		return;
	}

	m_current_pos = pos;

	draw_preview(m_preview_buffer);
}

void DrawTool::on_mouse_release(Canvas &canvas, const PaintCore::Vec2 &pos, int button)
{
	(void)button;

	if (!m_drawing)
	{
		return;
	}

	m_current_pos = pos;
	m_drawing = false;

	draw_preview(m_preview_buffer);

	m_preview_buffer.blend_onto(canvas.get_main_buffer());	
	m_preview_buffer.reset();
}

void DrawTool::on_cancel(Canvas &canvas)
{
	(void)canvas;
	
	if (!m_drawing)
	{
		return;
	}

	m_drawing = false;
	m_preview_buffer.reset();
}

bool DrawTool::has_preview() const
{
	return true;
}

DrawBuffer* DrawTool::get_draw_buffer()
{
	return &m_preview_buffer;
}

void DrawTool::get_normalized_rect(int &x, int &y, int &w, int &h) const
{
	x = std::min(m_start_pos.x, m_current_pos.x);
	y = std::min(m_start_pos.y, m_current_pos.y);
	w = std::abs(m_current_pos.x - m_start_pos.x);
	h = std::abs(m_current_pos.y - m_start_pos.y);
}

PaintCore::Vec2 DrawTool::get_start_pos()
{
	return m_start_pos;
}
bool DrawTool::is_drawing() const
{
	return m_drawing;
}