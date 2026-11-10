#ifndef QTREGBPAINT_BRUSH_TOOL_H
#define QTREGBPAINT_BRUSH_TOOL_H

#include <vector>

#include "core/brush.h"
#include "tools/freehand_tool.h"

// Paintbrush with twelve selectable footprints: three circles, three squares
// and two diagonal line sets (45 and 135 degrees). It inherits the freehand
// interpolation from FreehandTool, so fast pointer movement leaves no gaps.
class BrushTool : public FreehandTool
{
	public:
		enum class Brush : int
		{
			CircleLarge = 0,
			CircleMedium,
			CircleSmall,
			SquareLarge,
			SquareMedium,
			SquareSmall,
			DiagonalDownLarge,
			DiagonalDownMedium,
			DiagonalDownSmall,
			DiagonalUpLarge,
			DiagonalUpMedium,
			DiagonalUpSmall,
			Count,
		};

		BrushTool(PaintCore::ColorIndex foreground = 1,
		          PaintCore::ColorIndex background = 0);

		StampPreview stamp_preview(
			PaintCore::ColorIndex foreground,
			PaintCore::ColorIndex background) const override;

	protected:
		void stamp(DrawBuffer& buffer, const ImageBuffer& image,
		           int x, int y) override;
		void stamp_segment_step(DrawBuffer& buffer, const ImageBuffer& image,
		                        const PaintCore::Vec2& from,
		                        const PaintCore::Vec2& to) override;
		int stamp_margin() const override;
		void on_option_changed(const std::string& id, int value) override;

	private:
		std::vector<PaintCore::Vec2> m_offsets;
		PaintCore::BrushShape m_shape = PaintCore::BrushShape::Circle;
		int m_margin = 0;

		void update_brush(int value);
};

#endif
