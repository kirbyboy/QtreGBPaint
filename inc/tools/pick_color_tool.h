#ifndef QTREGBPAINT_PICK_COLOR_TOOL_H
#define QTREGBPAINT_PICK_COLOR_TOOL_H

#include "core/color.h"
#include "core/vec2.h"
#include "tools/base_tool.h"

// Eyedropper. A left click samples the canvas color for the foreground; a
// right click samples it for the background. The application consumes the
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

		bool take_color_pick(ColorPick& pick) override;

	private:
		ColorPick m_pick{};
		bool m_has_pick = false;
};

#endif
