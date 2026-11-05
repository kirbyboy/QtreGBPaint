#include "paint/canvas.h"
#include "paint/canvas_area.h"
#include "paint/color_imgui.h"
#include "core/ibackend.h"
#include <cmath>
#include <cstring>

using namespace PaintCore;
using namespace std;

// ImGuiCanvasRenderer

void ImGuiCanvasRenderer::draw_rect_filled(int x, int y, int w, int h, const PaintCore::Color& color)
{
	if(!draw_list)
	{
		return;
	}

	const PaintCore::Vec2f p0 = view.image_to_screen(
		PaintCore::Vec2f{static_cast<float>(x), static_cast<float>(y)});
	const PaintCore::Vec2f p1 = view.image_to_screen(
		PaintCore::Vec2f{static_cast<float>(x + w), static_cast<float>(y + h)});

	draw_list->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y), toImU32(color));
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

CanvasArea::CanvasArea(IGuiBackend* backend, int w, int h) : m_backend(backend), m_canvas(w, h)
{
}

CanvasArea::~CanvasArea()
{
	if (m_backend && m_canvas_texture)
	{
		m_backend->destroyTexture(m_canvas_texture);
		m_canvas_texture = nullptr;
	}
}

void CanvasArea::update_view(const ImVec2& canvas_pos,
							 const ImVec2& size,
							 const ImVec2& scroll_px,
							 float zoom)
{
	m_view = PaintCore::ViewTransform::make(
		PaintCore::Vec2f{canvas_pos.x, canvas_pos.y},
		PaintCore::Vec2f{scroll_px.x, scroll_px.y},
		PaintCore::Vec2f{size.x, size.y},
		zoom);
}

void CanvasArea::render(ImDrawList* draw_list, const Palette& palette)
{
	ImGuiCanvasRenderer renderer;
	renderer.draw_list = draw_list;
	renderer.view = m_view;

	// Base image as one texture; preview stays as primitives for now.
	update_canvas_texture(palette);

	if (draw_list && m_canvas_texture)
	{
		const int image_w = m_canvas.getWidth();
		const int image_h = m_canvas.getHeight();
		const PaintCore::Vec2f p0 = m_view.image_to_screen(PaintCore::Vec2f{0.0f, 0.0f});
		const PaintCore::Vec2f p1 = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(image_w), static_cast<float>(image_h)});
		draw_list->AddImage(reinterpret_cast<ImTextureID>(m_canvas_texture),
							ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y));
		m_canvas.render_preview(renderer, palette);
	}
	else
	{
		// Fallback if the backend cannot create a texture.
		m_canvas.render(renderer, m_view, palette);
	}
}

void CanvasArea::update_canvas_texture(const Palette& palette)
{
	if (!m_backend)
	{
		return;
	}

	ImageBuffer& image = m_canvas.get_main_buffer();
	if (image.width() <= 0 || image.height() <= 0)
	{
		return;
	}

	if (!m_canvas_texture || m_texture_width != image.width() || m_texture_height != image.height())
	{
		if (m_canvas_texture)
		{
			m_backend->destroyTexture(m_canvas_texture);
			m_canvas_texture = nullptr;
		}

		m_canvas_texture = m_backend->createTexture(image.width(), image.height());
		m_texture_width = image.width();
		m_texture_height = image.height();

		if (m_canvas_texture)
		{
			image.mark_all_dirty();
		}
	}

	if (!m_canvas_texture)
	{
		return;
	}

	// Palette -> RGBA lookup table. Rebuild/upload only if it changed.
	std::array<PaintCore::Color, 256> lut{};
	for (int i = 0; i < 256; ++i)
	{
		lut[i] = palette.getColor(static_cast<PaintCore::ColorIndex>(i));
	}

	if (!m_lut_ready || std::memcmp(m_lut.data(), lut.data(), sizeof(m_lut)) != 0)
	{
		m_lut = lut;
		m_lut_ready = true;
		image.mark_all_dirty();
	}

	if (!image.has_dirty_rect())
	{
		return;
	}

	const PaintCore::Rect dirty = image.dirty_rect();
	const int x0 = std::max(0, dirty.x);
	const int y0 = std::max(0, dirty.y);
	const int x1 = std::min(image.width(), dirty.right());
	const int y1 = std::min(image.height(), dirty.bottom());

	if (x0 >= x1 || y0 >= y1)
	{
		image.clear_dirty_rect();
		return;
	}

	const int w = x1 - x0;
	const int h = y1 - y0;
	m_upload_pixels.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));

	for (int y = 0; y < h; ++y)
	{
		const PaintCore::ColorIndex* src = image.data().data() +
			static_cast<std::size_t>(y0 + y) * static_cast<std::size_t>(image.width()) +
			static_cast<std::size_t>(x0);
		PaintCore::Color* dst = m_upload_pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);

		for (int x = 0; x < w; ++x)
		{
			dst[x] = m_lut[src[x]];
		}
	}

	m_backend->updateTextureRegion(m_canvas_texture, m_upload_pixels.data(), x0, y0, w, h);
	image.clear_dirty_rect();
}

Canvas& CanvasArea::canvas()
{
	return m_canvas;
}

const Canvas& CanvasArea::canvas() const
{
	return m_canvas;
}

void CanvasArea::handle_input(const ImVec2& mouse)
{
    // Misma transformación que usa el render en este frame. Intencionalmente
    // sin clamp: las herramientas reciben coordenadas reales aunque el puntero
    // salga del canvas, para que las figuras puedan extenderse fuera del borde
    // (comportamiento tipo Paint). DrawBuffer recorta la preview.
    const PaintCore::Vec2 img_pos = m_view.screen_to_image_pixel(
        PaintCore::Vec2f{mouse.x, mouse.y});

    // Delegar a Canvas según estado del botón
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