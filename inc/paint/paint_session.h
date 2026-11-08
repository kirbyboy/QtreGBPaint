#ifndef QTREGBPAINT_PAINT_SESSION_H
#define QTREGBPAINT_PAINT_SESSION_H

#include "core/color.h"
#include "core/vec2.h"
#include "paint/canvas_document.h"
#include "paint/floating_region_editor.h"

class DrawBuffer;
class ICanvasRenderer;
class Palette;
class ToolBase;

// Editing session: owns the document, the floating selection/paste editor and
// the active tool. It also coordinates undo/redo, tool switching and resize
// around the floating region.
class PaintSession
{
public:
	PaintSession(int w = 256, int h = 256,
	             HistoryLimits history_limits = {});

	PaintSession(const PaintSession&) = delete;
	PaintSession& operator=(const PaintSession&) = delete;

	CanvasDocument& document() { return m_document; }
	const CanvasDocument& document() const { return m_document; }

	FloatingRegionEditor& floating_editor() { return m_floating_editor; }
	const FloatingRegionEditor& floating_editor() const
	{
		return m_floating_editor;
	}

	void set_tool(ToolBase* tool);
	ToolBase* active_tool() const;

	// Input already translated to image coordinates.
	void handle_mouse_press(const PaintCore::Vec2& pos, int button);
	void handle_mouse_move(const PaintCore::Vec2& pos);
	void handle_mouse_release(const PaintCore::Vec2& pos, int button);

	void commit_draw_buffer(DrawBuffer& buffer);

	bool can_undo() const;
	bool can_redo() const;
	bool undo();
	bool redo();
	void clear_history();

	bool resize(int new_width, int new_height, PaintCore::ColorIndex fill);

	// Floating region / clipboard forwards.
	bool begin_selection(const PaintCore::Rect& rect,
	                     PaintCore::ColorIndex hole_fill,
	                     bool transparent_background = false);
	bool begin_paste(ImageBuffer content, const PaintCore::Vec2& pos);
	bool move_floating_region(int dx, int dy);
	bool set_floating_transparency(bool transparent_background,
	                               PaintCore::ColorIndex color);
	bool commit_floating_region();
	void cancel_floating_region();

	bool has_floating_region() const
	{
		return m_floating_editor.has_floating_region();
	}
	const FloatingRegion& floating_region() const
	{
		return m_floating_editor.floating_region();
	}
	bool floating_region_contains(const PaintCore::Vec2& pos) const;

	bool copy_to_clipboard();
	bool cut_to_clipboard();
	bool paste_from_clipboard(const PaintCore::Vec2& pos = {});
	bool invert_floating_region_colors();
	bool has_clipboard() const { return m_floating_editor.has_clipboard(); }
	const ImageBuffer& clipboard() const
	{
		return m_floating_editor.clipboard();
	}

	void set_history_limits(HistoryLimits limits);
	HistoryLimits history_limits() const;

	int getWidth() const;
	int getHeight() const;

	const ImageBuffer& get_main_buffer() const;
	ImageBuffer& get_main_buffer();

	// Document image plus the active tool preview.
	void render(ICanvasRenderer& renderer, const PaintCore::ViewTransform& view,
	            const Palette& palette) const;
	void render_preview(ICanvasRenderer& renderer, const Palette& palette) const;

private:
	CanvasDocument m_document;
	FloatingRegionEditor m_floating_editor;
	ToolBase* m_active_tool = nullptr; // non-owning: owned by PaintBackend
};

#endif
