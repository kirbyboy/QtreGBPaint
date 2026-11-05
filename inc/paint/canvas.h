#ifndef QTREGBPAINT_CANVAS_H
#define QTREGBPAINT_CANVAS_H

#include <memory>
#include "core/color.h"
#include "core/vec2.h"
#include "paint/image_buffer.h"
#include "paint/palette.h"
#include "paint/undo_history.h"
#include "tools/base_tool.h"
#include "core/view_transform.h"

class ICanvasRenderer;
class DrawBuffer;

class Canvas
{
public:
    Canvas(int w = 256, int h = 256, HistoryLimits history_limits = {});

    void set_tool(ToolBase* tool);
    ToolBase* active_tool() const;

    // Input ya traducido a coordenadas de imagen
    void handle_mouse_press(const PaintCore::Vec2& pos, int button);
    void handle_mouse_move(const PaintCore::Vec2& pos);
    void handle_mouse_release(const PaintCore::Vec2& pos, int button);

    // Confirma el buffer de preview y, si cambia píxeles, lo apila en el
    // historial. Las herramientas no gestionan undo directamente.
    void commit_draw_buffer(DrawBuffer& buffer);

    bool can_undo() const;
    bool can_redo() const;
    bool undo();
    bool redo();
    void clear_history();

    void set_history_limits(HistoryLimits limits);
    HistoryLimits history_limits() const;

    int getWidth() const;
    int getHeight() const;

    const ImageBuffer& get_main_buffer() const;
    ImageBuffer& get_main_buffer();

    // Render desacoplado: la vista pasa la transformación + renderer + paleta
    void render(ICanvasRenderer& renderer, const PaintCore::ViewTransform& view, const Palette& palette);
    // Preview layer only (draw buffer); used when the base image is a texture.
    void render_preview(ICanvasRenderer& renderer, const Palette& palette);

private:
    ImageBuffer m_image_buffer;
    UndoHistory m_history;

    ToolBase* m_active_tool = nullptr; // non-owning: owned by PaintBackend

    void render_image_buffer(ICanvasRenderer& r, const PaintCore::ViewTransform& v, const Palette& p);
};
#endif