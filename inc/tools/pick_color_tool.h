#ifndef QTREGBPAINT_PICK_COLOR_TOOL_H
#define QTREGBPAINT_PICK_COLOR_TOOL_H

#include "core/color.h"
#include "core/vec2.h"
#include "tools/base_tool.h"

// Eyedropper. The left button samples the canvas color for the foreground; the
// right button samples it for the background. The sample is taken on release so
// the tool can preview the color under the pointer while the button is held. A
// release outside the canvas cancels the pick. The application consumes the
// result after input routing and returns to the last editing tool.
class PickColorTool : public ToolBase
{
	public:
		void on_mouse_press(PaintSession& canvas,
		                    const PaintCore::Vec2& pos, int button) override;
		void on_mouse_move(PaintSession& canvas,
		                   const PaintCore::Vec2& pos) override;
		void on_mouse_release(PaintSession& canvas,
		                      const PaintCore::Vec2& pos, int button) override;
		void on_cancel(PaintSession& canvas) override;

		bool accepts_button(int button) const override
		{
			return button == kLeftButton || button == kRightButton;
		}

		bool is_operating() const override { return m_pressed_button != 0; }
		bool take_color_pick(ColorPick& pick) override;
		bool color_pick_preview(PaintCore::ColorIndex& color) const override;

	private:
		void update_preview(PaintSession& canvas,
		                    const PaintCore::Vec2& pos);
		void clear_press();

		ColorPick m_pick{};
		bool m_has_pick = false;
		int m_pressed_button = 0;
		PaintCore::ColorIndex m_preview_color = 0;
		bool m_has_preview = false;
};

#endif
