#ifndef QTREGBPAINT_SELECTION_TOOL_H
#define QTREGBPAINT_SELECTION_TOOL_H

#include "core/color.h"
#include "core/vec2.h"
#include "tools/base_tool.h"

// Creates a rectangular marquee. Once released, the floating region editor
// takes ownership of the selected pixels; this tool does not move or store them.
class SelectionTool : public ToolBase
{
	public:
		enum class Mode : int
		{
			Normal = 0,
			Transparent = 1,
		};

		SelectionTool();

		void on_mouse_press(PaintSession& canvas, const PaintCore::Vec2& pos, int button) override;
		void on_mouse_move(PaintSession& canvas, const PaintCore::Vec2& pos) override;
		void on_mouse_release(PaintSession& canvas, const PaintCore::Vec2& pos, int button) override;
		void on_cancel(PaintSession& canvas) override;

		void set_colors(PaintCore::ColorIndex foreground,
		                PaintCore::ColorIndex background) override;

		bool is_drawing() const override { return m_dragging; }
		bool has_marquee() const override { return m_dragging; }
		PaintCore::Rect marquee_rect() const override;
		void sync_floating_region(PaintSession& canvas) override;

	private:
		PaintCore::Vec2 m_start{};
		PaintCore::Vec2 m_current{};
		PaintCore::ColorIndex m_background = 0;
		bool m_dragging = false;
		bool m_shift = false;
};

#endif
