#ifndef QTREGBPAINT_DRAW_TOOL_H
#define QTREGBPAINT_DRAW_TOOL_H

#include "core/vec2.h"
#include "paint/canvas.h"
#include "paint/draw_buffer.h"
#include "tools/base_tool.h"

class DrawTool : public ToolBase
{

	public:
		void on_mouse_press(Canvas& canvas, const PaintCore::Vec2& pos, int button) override;
		void on_mouse_move(Canvas& canvas, const PaintCore::Vec2& pos) override;
		void on_mouse_release(Canvas& canvas, const PaintCore::Vec2& pos, int button) override;
		void on_cancel(Canvas& canvas) override;

		bool has_preview() const override;
		DrawBuffer* get_draw_buffer() override;
		void get_normalized_rect(int &x, int &y, int &w, int &h) const;
		PaintCore::Vec2 get_start_pos();
		bool is_drawing() const override;

	protected:
		virtual void draw_preview(DrawBuffer& buffer) = 0;

		DrawBuffer m_preview_buffer;
		bool m_drawing = false;
		PaintCore::Vec2 m_start_pos{};
		PaintCore::Vec2 m_current_pos{};
};




#endif