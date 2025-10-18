#include "tools/draw_tool.h"

void DrawTool::on_mouse_press(Canvas &canvas, const PaintCore::Vec2 &pos)
{
	(void)canvas;
	m_startPos = pos;
	m_temp_buffer.reset();
	m_temp_buffer.begin(0,0);
	m_drawing = true;
}

void DrawTool::on_mouse_move(Canvas &canvas, const PaintCore::Vec2 &pos)
{
	(void)canvas;
	if(!m_drawing)
	{
		return;
	}

	m_temp_buffer.reset();
	generate_content(pos);
	canvas.update();
}

void DrawTool::on_mouse_release(Canvas &canvas, const PaintCore::Vec2 &pos)
{
	(void)canvas;
	(void)pos;
	if(!m_drawing)
	{
		return;
	}

	m_temp_buffer.blend_onto(canvas.get_main_buffer());
	m_temp_buffer.reset();
	m_drawing = false;

	canvas.update();
}

void DrawTool::cancel()
{
	m_temp_buffer.reset();
	m_drawing = false;
}