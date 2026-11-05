#ifndef QTREGBPAINT_BASE_TOOL_H
#define QTREGBPAINT_BASE_TOOL_H

#include "core/vec2.h"


class Canvas;
class DrawBuffer;

class ToolBase
{
	public:
		virtual ~ToolBase() = default;

		virtual void on_mouse_press(Canvas& canvas, const PaintCore::Vec2& pos, int button) = 0;
		virtual void on_mouse_move(Canvas& canvas, const PaintCore::Vec2& pos) = 0;
		virtual void on_mouse_release(Canvas& canvas, const PaintCore::Vec2& pos, int button) = 0;

		virtual void on_cancel(Canvas& canvas) = 0;

		virtual DrawBuffer* get_draw_buffer() { return nullptr; }
		virtual bool is_drawing() const { return false; }
};

#endif