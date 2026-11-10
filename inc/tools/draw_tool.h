#ifndef QTREGBPAINT_DRAW_TOOL_H
#define QTREGBPAINT_DRAW_TOOL_H

#include "core/vec2.h"
#include "paint/paint_session.h"
#include "paint/draw_buffer.h"
#include "tools/base_tool.h"

class DrawTool : public ToolBase
{

	public:
		DrawTool(PaintCore::ColorIndex foreground = 1,
		         PaintCore::ColorIndex background = 0);

		void on_mouse_press(PaintSession& canvas, const PaintCore::Vec2& pos, int button) override;
		void on_mouse_move(PaintSession& canvas, const PaintCore::Vec2& pos) override;
		void on_mouse_release(PaintSession& canvas, const PaintCore::Vec2& pos, int button) override;
		void on_cancel(PaintSession& canvas) override;

		bool accepts_button(int button) const override
		{
			return button == kLeftButton || button == kRightButton;
		}

		void set_colors(PaintCore::ColorIndex foreground,
		                PaintCore::ColorIndex background) override;

		DrawBuffer* get_draw_buffer() override;
		PaintCore::Vec2 get_start_pos();
		bool is_drawing() const override;

	protected:
		virtual void draw_preview(DrawBuffer& buffer, const PaintCore::Modifiers& modifiers) = 0;
		void update_preview(PaintSession& canvas);

		PaintCore::ColorIndex m_foreground;
		PaintCore::ColorIndex m_background;

		// Effective colors for the current stroke. Right-button strokes swap
		// them, so shape tools can invert outline/fill and freehand tools use
		// the background as their drawing color.
		PaintCore::ColorIndex m_stroke_foreground;
		PaintCore::ColorIndex m_stroke_background;
		int m_active_button = 0;

		DrawBuffer m_preview_buffer;
		bool m_drawing = false;
		PaintCore::Vec2 m_start_pos{};
		PaintCore::Vec2 m_current_pos{};
};




#endif