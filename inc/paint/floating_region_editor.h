#ifndef QTREGBPAINT_FLOATING_REGION_EDITOR_H
#define QTREGBPAINT_FLOATING_REGION_EDITOR_H

#include "core/color.h"
#include "core/image_transform.h"
#include "core/vec2.h"
#include "paint/floating_region.h"
#include "paint/image_buffer.h"

class CanvasDocument;

// Owns the movable selection/paste region and the clipboard. Pixel reads and
// writes plus history go through CanvasDocument, but the editing policy stays
// out of the document itself.
class FloatingRegionEditor
{
public:
	explicit FloatingRegionEditor(CanvasDocument& document);

	FloatingRegionEditor(const FloatingRegionEditor&) = delete;
	FloatingRegionEditor& operator=(const FloatingRegionEditor&) = delete;

	bool begin_selection(const PaintCore::Rect& rect,
	                     PaintCore::ColorIndex hole_fill,
	                     bool transparent_background = false);
	bool begin_paste(ImageBuffer content, const PaintCore::Vec2& pos);
	bool move_floating_region(int dx, int dy);

	// Updates the live transparency mask. Returns true if mode/color changed.
	bool set_floating_transparency(bool transparent_background,
	                               PaintCore::ColorIndex color);

	// Applies hole + visible content as one history step.
	bool commit_floating_region();
	void cancel_floating_region();

	bool has_floating_region() const { return m_floating.active; }
	const FloatingRegion& floating_region() const { return m_floating; }
	bool floating_region_contains(const PaintCore::Vec2& pos) const;

	bool copy_to_clipboard();
	bool cut_to_clipboard();
	bool delete_floating_region();
	bool paste_from_clipboard(const PaintCore::Vec2& pos = {});
	bool invert_floating_region_colors();

	// Flips/rotates the floating content without committing or cancelling the
	// selection. 90/270 swap the width/height and recenter the region; the
	// original hole is unchanged.
	bool transform_floating_region(PaintCore::ImageTransform transform);

	bool has_clipboard() const
	{
		return m_clipboard.width() > 0 && m_clipboard.height() > 0;
	}
	const ImageBuffer& clipboard() const { return m_clipboard; }

private:
	CanvasDocument& m_document;
	FloatingRegion m_floating;
	ImageBuffer m_clipboard;

	// Fills the selection hole with its saved background and discards the
	// floating content. Used by Cut (after copying) and Delete.
	void remove_floating_region();
};

#endif
