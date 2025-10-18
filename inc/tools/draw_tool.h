#ifndef QTREGBPAINT_DRAW_TOOL_H
#define QTREGBPAINT_DRAW_TOOL_H

#include "core/vec2.h"
#include "paint/canvas.h"
#include "paint/draw_buffer.h"

class DrawTool
{
	public:
		virtual ~DrawTool() = default;

		virtual void on_mouse_press(class Canvas& canvas, const PaintCore::Vec2& pos);
		virtual void on_mouse_move(class Canvas& canvas, const PaintCore::Vec2& pos);
		virtual void on_mouse_release(class Canvas& canvas, const PaintCore::Vec2& pos);
		virtual void cancel();

	protected:
		PaintCore::Vec2 m_startPos;
		DrawBuffer m_temp_buffer;
		bool m_drawing = false;

		virtual void generate_content(const PaintCore::Vec2& currentPos) = 0;
};

#endif