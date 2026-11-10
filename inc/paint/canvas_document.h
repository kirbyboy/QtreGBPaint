#ifndef QTREGBPAINT_CANVAS_DOCUMENT_H
#define QTREGBPAINT_CANVAS_DOCUMENT_H

#include <cstdint>
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

    // Replaces the pixels covered by `rect` with `after` as one history step.
    // The rect must be fully inside the canvas and `after` must contain
    // exactly rect.w * rect.h pixels. Returns true if the pixels changed
    // (with or without an active history).
    bool apply_pixel_patch(const PaintCore::Rect& rect,
                           std::vector<PaintCore::ColorIndex> after);

    bool can_undo() const;
    bool can_redo() const;
    bool undo();
    bool redo();
    void clear_history();
    std::uint64_t current_state_id() const;

    // Replaces the whole image (New/Open) and clears the undo history. The
    // caller is responsible for validating the new image.
    void replace_image(ImageBuffer image);

    bool resize(int new_width, int new_height, PaintCore::ColorIndex fill);

    void set_history_limits(HistoryLimits limits);
    HistoryLimits history_limits() const;

    int getWidth() const;
    int getHeight() const;
    int getBitsPerPixel() const;

    const ImageBuffer& get_main_buffer() const;

    // Returns the accumulated dirty region and clears it. The view consumes
    // it to update its texture.
    PaintCore::Rect take_dirty_rect();

    // Image layer only; the tool preview belongs to PaintSession.
    void render(ICanvasRenderer& renderer, const PaintCore::ViewTransform& view,
                const Palette& palette) const;

private:
    ImageBuffer m_image_buffer;
    UndoHistory m_history;

    // Builds and pushes a step if before != after. The caller guarantees that
    // the rect and both vectors are well formed.
    bool record_patch(const PaintCore::Rect& rect,
                      std::vector<PaintCore::ColorIndex> before,
                      std::vector<PaintCore::ColorIndex> after);

    void render_image_buffer(ICanvasRenderer& renderer,
                             const PaintCore::ViewTransform& view,
                             const Palette& palette) const;
};

#endif
