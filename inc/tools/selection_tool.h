#ifndef QTREGBPAINT_SELECTION_TOOL_H
#define QTREGBPAINT_SELECTION_TOOL_H

#include "tools/base_tool.h"

class SelectionTool : public ToolBase
{
	private:
		PaintCore::Vec2 m_start;
		PaintCore::Vec2 m_end;
		bool m_selecting = false;

	public:
		void on_mouse_press(Canvas& canvas, const PaintCore::Vec2& pos, int button) override;
		void on_mouse_move(Canvas& canvas, const PaintCore::Vec2& pos) override;
		void on_mouse_release(Canvas& canvas, const PaintCore::Vec2& pos, int button) override;
};

#endif