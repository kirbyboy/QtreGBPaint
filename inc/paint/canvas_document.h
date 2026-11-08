#ifndef QTREGBPAINT_CANVAS_DOCUMENT_H
#define QTREGBPAINT_CANVAS_DOCUMENT_H

#include <vector>

#include "core/color.h"
#include "core/rect.h"
#include "core/view_transform.h"
#include "paint/image_buffer.h"
#include "paint/palette.h"
#include "paint/undo_history.h"

class ICanvasRenderer;
class DrawBuffer;

// Pure canvas data: pixels and undo history. Tools, selection, clipboard and
// editing policy live in PaintSession / FloatingRegionEditor.
class CanvasDocument
{
public:
    CanvasDocument(int w = 256, int h = 256,
                   HistoryLimits history_limits = {});

    // Confirms a tool preview buffer and pushes one patch if pixels changed.
    void commit_draw_buffer(DrawBuffer& buffer);

    // Pushes one pixel patch if before != after. Returns true if pushed.
    bool push_pixel_patch(const PaintCore::Rect& rect,
                          std::vector<PaintCore::ColorIndex> before,
                          std::vector<PaintCore::ColorIndex> after);

    bool can_undo() const;
    bool can_redo() const;
    bool undo();
    bool redo();
    void clear_history();

    bool resize(int new_width, int new_height, PaintCore::ColorIndex fill);

    void set_history_limits(HistoryLimits limits);
    HistoryLimits history_limits() const;

    int getWidth() const;
    int getHeight() const;

    const ImageBuffer& get_main_buffer() const;
    ImageBuffer& get_main_buffer();

    // Image layer only; the tool preview belongs to PaintSession.
    void render(ICanvasRenderer& renderer, const PaintCore::ViewTransform& view,
                const Palette& palette) const;

private:
    ImageBuffer m_image_buffer;
    UndoHistory m_history;

    void render_image_buffer(ICanvasRenderer& renderer,
                             const PaintCore::ViewTransform& view,
                             const Palette& palette) const;
};

#endif
