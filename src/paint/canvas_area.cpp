#include "paint/canvas.h"
#include "paint/canvas_area.h"
#include "paint/color_imgui.h"
#include <cmath>

using namespace PaintCore;
using namespace std;

// ImGuiCanvasRenderer

void ImGuiCanvasRenderer::draw_rect_filled(int x, int y, int w, int h, const PaintCore::Color& color)
{
    if(!draw_list)
	{
		return;
	}

    // canvas_pos ya incluye el scroll, por eso NO se resta view.scroll_x/y
    float px = canvas_pos.x + x * view.zoom;
    float py = canvas_pos.y + y * view.zoom;
    float pw = w * view.zoom;
    float ph = h * view.zoom;

    draw_list->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph), toImU32(color));
}

void ImGuiCanvasRenderer::draw_rect(int x, int y, int w, int h, const PaintCore::Color& color, int thickness)
{
	(void)x; (void)y; (void)w; (void)h; (void)color; (void)thickness;
	// TODO
}

void ImGuiCanvasRenderer::draw_line(int x0, int y0, int x1, int y1, const PaintCore::Color& color, int thickness)
{
	(void)x0; (void)y0; (void)x1; (void)y1; (void)color; (void)thickness;
	// TODO
}

// CanvasArea

CanvasArea::CanvasArea(int w, int h) : m_canvas(w, h)
{
}

void CanvasArea::render(ImDrawList* draw_list, 
	const ImVec2& canvas_pos, 
	const ImVec2& size, 
	const ImVec2& scroll_px,
	float zoom, 
	const Palette& palette)
{

	// 1) Estado de vista (si el frontend lo actualiza, usa esos valores)
	//    Si el scroll viene de la ventana ImGui, puedes leerlo aquí:
	// Actualizar viewport visual cada frame (no hay problema al redimensionar/mover)
    m_viewport.pos       = canvas_pos;
    m_viewport.size      = size;
    m_viewport.scroll_px = scroll_px;
    m_viewport.zoom      = zoom;

	// 2) Convertir scroll de píxeles a coordenadas de imagen
    // Crear Viewport de dominio (scroll en imagen)
    PaintCore::Viewport view(
        (m_viewport.zoom > 0) ? (m_viewport.scroll_px.x / m_viewport.zoom) : 0.0f,
        (m_viewport.zoom > 0) ? (m_viewport.scroll_px.y / m_viewport.zoom) : 0.0f,
        m_viewport.zoom,
        static_cast<int>(m_viewport.size.x),
        static_cast<int>(m_viewport.size.y)
    );

	// 3) Renderer que traduce primitivas -> ImDrawList
	ImGuiCanvasRenderer renderer;
	renderer.draw_list = draw_list;
	renderer.canvas_pos = canvas_pos;
	renderer.view = view;

	// 4) El canvas pinta (imagen base + preview)
	m_canvas.render(renderer, view, palette);

	// 5) Actualizar hover/active si lo necesitas
	// m_hovered = ImGui::IsItemHovered();
	// m_active  = ImGui::IsItemActive();
}

Canvas& CanvasArea::canvas()
{
	return m_canvas;
}

const Canvas& CanvasArea::canvas() const
{
	return m_canvas;
}

PaintCore::Vec2 CanvasArea::screen_to_image(const ImVec2& mouse) const
{
    PaintCore::Vec2 img;
    if (m_viewport.zoom <= 0.0f) return img;

    // canvas_pos (pos) ya incluye el scroll (GetCursorScreenPos), por eso NO se suma scroll_px
    img.x = static_cast<int>(floor((mouse.x - m_viewport.pos.x) / m_viewport.zoom));
    img.y = static_cast<int>(floor((mouse.y - m_viewport.pos.y) / m_viewport.zoom));

    // (Opcional) clamp a los límites del buffer
    // img.x = std::clamp(img.x, 0, m_canvas.getWidth() - 1);
    // img.y = std::clamp(img.y, 0, m_canvas.getHeight() - 1);

    return img;
}

ImVec2 CanvasArea::image_to_screen(int ix, int iy) const
{
    if (m_viewport.zoom <= 0.0f) return m_viewport.pos;

    // canvas_pos (pos) ya incluye el scroll, por eso NO se resta scroll_img
    ImVec2 screen;
    screen.x = m_viewport.pos.x + ix * m_viewport.zoom;
    screen.y = m_viewport.pos.y + iy * m_viewport.zoom;
    return screen;
}

void CanvasArea::handle_input(const ImVec2& canvas_pos, const ImVec2& size, float zoom)
{
    // 1) Actualizar viewport visual (igual que en render), con zoom del frame actual
    m_viewport.pos       = canvas_pos;
    m_viewport.size      = size;
    m_viewport.zoom      = zoom;
    m_viewport.scroll_px = ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());

    // 2) Traducir ratón a coordenadas de imagen (con zoom actualizado)
    ImVec2 mouse = ImGui::GetMousePos();
    PaintCore::Vec2 img_pos = screen_to_image(mouse);

    // 4) Delegar a Canvas según estado del botón
    bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    if (mouse_down && !m_mouse_was_down && ImGui::IsItemHovered())
    {
        m_canvas.handle_mouse_press(img_pos, 1);
        m_mouse_was_down = true;
    }
    else if (mouse_released && m_canvas.active_tool() && m_canvas.active_tool()->is_drawing())
    {
        m_canvas.handle_mouse_release(img_pos, 1);
        m_mouse_was_down = false;
    }
    else if (ImGui::IsItemActive() && m_canvas.active_tool() && m_canvas.active_tool()->is_drawing())
    {
        m_canvas.handle_mouse_move(img_pos);
    }

    m_mouse_was_down = mouse_down;
}