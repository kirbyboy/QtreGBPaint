#ifndef QTREGBPAINT_CANVAS_H
#define QTREGBPAINT_CANVAS_H

#include <memory>
#include "core/color.h"
#include "core/vec2.h"
#include "paint/image_buffer.h"
#include "paint/palette.h"
#include "tools/base_tool.h"
#include "core/viewport.h"

class ICanvasRenderer;

class Canvas
{
public:
    Canvas(int w = 256, int h = 256);

    void set_tool(std::shared_ptr<ToolBase> tool);
    ToolBase* active_tool() const;

    // Input ya traducido a coordenadas de imagen
    void handle_mouse_press(const PaintCore::Vec2& pos, int button);
    void handle_mouse_move(const PaintCore::Vec2& pos);
    void handle_mouse_release(const PaintCore::Vec2& pos, int button);

    int getWidth() const;
    int getHeight() const;

    const ImageBuffer& get_main_buffer() const;
    ImageBuffer& get_main_buffer();

    // Render desacoplado: la vista pasa viewport + renderer + paleta
    void render(ICanvasRenderer& renderer, const PaintCore::Viewport& view, const Palette& palette);

private:
    ImageBuffer m_image_buffer;

    std::shared_ptr<ToolBase> m_active_tool;

    void render_image_buffer(ICanvasRenderer& r, const PaintCore::Viewport& v, const Palette& p);
    void render_draw_buffer(ICanvasRenderer& r, const PaintCore::Viewport& v, const Palette& p);
};
#endif