#ifndef QTREGBPAINT_CURVE_TOOL_H
#define QTREGBPAINT_CURVE_TOOL_H

#include <vector>

#include "core/color.h"
#include "core/vec2.h"
#include "tools/draw_tool.h"

// Cubic Bezier curve with two visible control handles. The first drag defines
// the endpoints; after releasing, the curve stays as a pending edit until the
// user commits it (Enter, click outside a handle, tool switch) or cancels it
// (Escape or right click). Dragging P1/P2 reshapes the curve live. Five widths
// select a single pixel, a 2x2 square or a centered cross stamp.
class CurveTool : public DrawTool
{
	public:
		CurveTool(PaintCore::ColorIndex foreground = 1,
		          PaintCore::ColorIndex background = 0);

		void on_mouse_press(PaintSession& canvas,
		                    const PaintCore::Vec2& pos, int button) override;
		void on_mouse_move(PaintSession& canvas,
		                   const PaintCore::Vec2& pos) override;
		void on_mouse_release(PaintSession& canvas,
		                      const PaintCore::Vec2& pos, int button) override;
		void on_cancel(PaintSession& canvas) override;
		void set_colors(PaintCore::ColorIndex foreground,
		                PaintCore::ColorIndex background) override;

		bool has_pending_edit() const override;
		bool commit_pending_edit(PaintSession& canvas) override;
		void cancel_pending_edit(PaintSession& canvas) override;

		bool edit_overlay(EditOverlay& overlay) const override;
		void move_edit_handle(int index, const PaintCore::Vec2& pos) override;

	protected:
		void draw_preview(DrawBuffer& buffer,
		                  const PaintCore::Modifiers& modifiers) override;

	private:
		enum class Phase
		{
			Idle,
			Endpoints,
			Handles,
		};

		Phase m_phase = Phase::Idle;
		PaintCore::Vec2 m_a{};
		PaintCore::Vec2 m_b{};
		PaintCore::Vec2 m_p1{};
		PaintCore::Vec2 m_p2{};
		int m_bounds_width = 0;
		int m_bounds_height = 0;
		int m_stroke_width = 1;
		std::vector<PaintCore::Vec2> m_stroke_offsets;

		void reset_state();
		void initialize_controls();
		PaintCore::Vec2 clamp_to_canvas(const PaintCore::Vec2& pos) const;
		void rebuild_preview();
};

#endif
