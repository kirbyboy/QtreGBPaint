#ifndef QTREGBPAINT_FREEHAND_TOOL_H
#define QTREGBPAINT_FREEHAND_TOOL_H

#include "core/color.h"
#include "core/vec2.h"
#include "tools/draw_tool.h"

// Base for tools that paint along the pointer path (pencil, brush, eraser).
// Every update appends the rasterized segment from the previous image position
// to the current one, so fast pointer movement does not leave gaps. Derived
// tools only define how a single pixel/brush footprint is stamped.
class FreehandTool : public DrawTool
{
public:
	FreehandTool(PaintCore::ColorIndex foreground = 1,
	             PaintCore::ColorIndex background = 0);

	void on_mouse_press(PaintSession& canvas, const PaintCore::Vec2& pos,
	                    int button) override;
	void on_mouse_move(PaintSession& canvas, const PaintCore::Vec2& pos) override;
	void on_mouse_release(PaintSession& canvas, const PaintCore::Vec2& pos,
	                      int button) override;

protected:
	// Stamps one footprint pixel in image coordinates. The buffer origin may
	// be offset; implementations should subtract buffer.x()/buffer.y() when
	// writing. `image` is the main canvas image, useful for tools that need to
	// inspect the target pixel (right-button eraser).
	virtual void stamp(DrawBuffer& buffer, const ImageBuffer& image,
	                   int x, int y) = 0;

	// Called for every unit step between consecutive path pixels, before the
	// destination is stamped. Diagonal line brushes override it to fill the
	// gap left when the movement is perpendicular to the brush orientation.
	virtual void stamp_segment_step(DrawBuffer& buffer,
	                                const ImageBuffer& image,
	                                const PaintCore::Vec2& from,
	                                const PaintCore::Vec2& to)
	{
		(void)buffer;
		(void)image;
		(void)from;
		(void)to;
	}

	// Extra pixels around the pointer path that a stamp can cover. Brush-like
	// tools override it so the preview buffer includes the whole footprint.
	virtual int stamp_margin() const { return 0; }

	// Freehand strokes are appended in the mouse callbacks; this full-redraw
	// hook is unused.
	void draw_preview(DrawBuffer& buffer, const PaintCore::Modifiers& modifiers) override;

private:
	void append_segment(PaintSession& canvas, const PaintCore::Vec2& from,
	                    const PaintCore::Vec2& to);
	PaintCore::Vec2 m_last_pos{};
};

#endif
