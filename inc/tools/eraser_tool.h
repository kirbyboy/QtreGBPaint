#ifndef QTREGBPAINT_ERASER_TOOL_H
#define QTREGBPAINT_ERASER_TOOL_H

#include <vector>

#include "core/brush.h"
#include "tools/freehand_tool.h"

// Square eraser with sizes 10, 8, 6 and 4 pixels per side. Left-click paints
// the background color; right-click only replaces pixels that still have the
// foreground color. The preview is an opaque background square with a fixed
// one-pixel screen border, and the OS cursor is hidden over the image.
class EraserTool : public FreehandTool
{
	public:
		enum class Size : int
		{
			Tiny = 0,   // 4 px
			Small,      // 6 px
			Medium,     // 8 px
			Large,      // 10 px
			Count,
		};

		EraserTool(PaintCore::ColorIndex foreground = 1,
		           PaintCore::ColorIndex background = 0);

		void on_mouse_press(PaintSession& canvas,
		                    const PaintCore::Vec2& pos, int button) override;

		StampPreview stamp_preview(
			PaintCore::ColorIndex foreground,
			PaintCore::ColorIndex background) const override;

	protected:
		void stamp(DrawBuffer& buffer, const ImageBuffer& image,
		           int x, int y) override;
		int stamp_margin() const override;
		void on_option_changed(const std::string& id, int value) override;

	private:
		std::vector<PaintCore::Vec2> m_offsets;
		int m_margin = 0;
		PaintCore::ColorIndex m_erase_color = 0;
		PaintCore::ColorIndex m_erase_mask_color = 0;

		void update_size(int value);
};

#endif
