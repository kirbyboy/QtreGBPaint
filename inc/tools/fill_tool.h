#ifndef QTREGBPAINT_FILL_TOOL_H
#define QTREGBPAINT_FILL_TOOL_H

#include "core/color.h"
#include "core/vec2.h"
#include "tools/draw_tool.h"

// Paint bucket. Left-click fills with the foreground color and right-click
// with the background color. Contiguous mode fills only the 4-connected region
// under the pointer; Global mode replaces every pixel of that color.
//
// The fill is applied on press and committed immediately: unlike shape or
// freehand tools there is no drag preview to accumulate, and rendering a
// full-canvas preview as per-pixel quads would be expensive on large canvases.
class FillTool : public DrawTool
{
	public:
		enum class Mode : int
		{
			Contiguous = 0,
			Global = 1,
		};

		FillTool(PaintCore::ColorIndex foreground = 1,
		         PaintCore::ColorIndex background = 0);

		void on_mouse_press(PaintSession& canvas,
		                    const PaintCore::Vec2& pos, int button) override;

	protected:
		void draw_preview(DrawBuffer& buffer,
		                  const PaintCore::Modifiers& modifiers) override;

	private:
		Mode mode() const;
		void fill_from(PaintSession& canvas, const PaintCore::Vec2& seed);
};

#endif
