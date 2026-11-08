#include "paint/paint_session.h"
#include "tools/base_tool.h"
#include "paint/canvas_area.h"
#include "paint/color_imgui.h"
#include "core/ibackend.h"
#include <cmath>
#include <cstring>

#include "imgui_internal.h"

using namespace PaintCore;
using namespace std;

namespace
{
struct ContentClip
{
	ImVec2 min{};
	ImVec2 max{};
};

ContentClip current_content_clip()
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	const ImGuiStyle& style = ImGui::GetStyle();
	ContentClip clip;
	clip.min = ImVec2(window->InnerRect.Min.x + style.WindowPadding.x,
	                  window->InnerRect.Min.y + style.WindowPadding.y);
	clip.max = ImVec2(window->InnerRect.Max.x - style.WindowPadding.x,
	                  window->InnerRect.Max.y - style.WindowPadding.y);
	return clip;
}

PaintCore::Rect intersect(const PaintCore::Rect& a, const PaintCore::Rect& b)
{
	if (a.empty() || b.empty())
	{
		return {};
	}

	const int x0 = std::max(a.x, b.x);
	const int y0 = std::max(a.y, b.y);
	const int x1 = std::min(a.right(), b.right());
	const int y1 = std::min(a.bottom(), b.bottom());
	if (x0 >= x1 || y0 >= y1)
	{
		return {};
	}
	return {x0, y0, x1 - x0, y1 - y0};
}

void add_dashed_edge(ImDrawList* draw_list, const ImVec2& a, const ImVec2& b)
{
	constexpr float kDashLength = 4.0f;
	constexpr ImU32 kColors[2] = {
		IM_COL32(0, 0, 0, 255),
		IM_COL32(255, 255, 255, 255),
	};

	const float dx = b.x - a.x;
	const float dy = b.y - a.y;
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length <= 0.0f)
	{
		return;
	}

	int index = 0;
	for (float start = 0.0f; start < length; start += kDashLength, ++index)
	{
		const float end = std::min(start + kDashLength, length);
		const float t0 = start / length;
		const float t1 = end / length;
		draw_list->AddLine(
			ImVec2(a.x + dx * t0, a.y + dy * t0),
			ImVec2(a.x + dx * t1, a.y + dy * t1),
			kColors[index % 2], 1.0f);
	}
}

void add_dashed_rect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max)
{
	add_dashed_edge(draw_list, min, ImVec2(max.x, min.y));
	add_dashed_edge(draw_list, ImVec2(max.x, min.y), max);
	add_dashed_edge(draw_list, max, ImVec2(min.x, max.y));
	add_dashed_edge(draw_list, ImVec2(min.x, max.y), min);
}
}

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

CanvasArea::CanvasArea(IGuiBackend* backend, int w, int h) : m_backend(backend), m_session(w, h), m_grid(backend)
{
}

CanvasArea::~CanvasArea()
{
	if (m_backend)
	{
		if (m_canvas_texture)
		{
			m_backend->destroyTexture(m_canvas_texture);
			m_canvas_texture = nullptr;
		}
		if (m_floating_texture)
		{
			m_backend->destroyTexture(m_floating_texture);
			m_floating_texture = nullptr;
		}
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
	if (!draw_list)
	{
		return;
	}

	ImGuiCanvasRenderer renderer;
	renderer.draw_list = draw_list;
	renderer.view = m_view;

	// Base image as one texture; preview stays as primitives for now.
	const bool palette_changed = update_palette_lut(palette);
	update_canvas_texture(palette_changed);
	update_floating_texture(palette_changed);

	// Keep the image, grid and handles inside the visible content area, not in
	// the child padding or under the scrollbars.
	const ContentClip clip = current_content_clip();
	draw_list->PushClipRect(clip.min, clip.max, true);

	if (m_canvas_texture)
	{
		const int image_w = m_session.getWidth();
		const int image_h = m_session.getHeight();
		const PaintCore::Vec2f p0 = m_view.image_to_screen(PaintCore::Vec2f{0.0f, 0.0f});
		const PaintCore::Vec2f p1 = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(image_w), static_cast<float>(image_h)});
		draw_list->AddImage(reinterpret_cast<ImTextureID>(m_canvas_texture),
							ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y));
		m_session.render_preview(renderer, palette);
	}
	else
	{
		// Fallback if the backend cannot create a texture.
		m_session.render(renderer, m_view, palette);
	}

	// Hole and floating content belong to the image, so the grid still covers
	// them. The marquee is drawn afterwards to keep it crisp.
	draw_floating_region(draw_list, renderer, palette);

	m_grid.render(draw_list, m_view, m_session.getWidth(), m_session.getHeight(),
	              m_grid_enabled);

	if (m_session.has_floating_region())
	{
		draw_marquee(draw_list, m_session.floating_region().rect);
	}
	else if (ToolBase* tool = m_session.active_tool())
	{
		if (tool->has_marquee())
		{
			draw_marquee(draw_list, tool->marquee_rect());
		}
	}

	// Resize handles stay disabled while a floating region must be resolved
	// first.
	if (!m_session.has_floating_region())
	{
		m_resize_gizmo.draw(draw_list, m_view, m_session);
	}

	draw_list->PopClipRect();
}

bool CanvasArea::update_palette_lut(const Palette& palette)
{
	std::array<PaintCore::Color, 256> lut{};
	for (int i = 0; i < 256; ++i)
	{
		lut[i] = palette.getColor(static_cast<PaintCore::ColorIndex>(i));
	}

	if (m_lut_ready &&
	    std::memcmp(m_lut.data(), lut.data(), sizeof(m_lut)) == 0)
	{
		return false;
	}

	m_lut = lut;
	m_lut_ready = true;
	return true;
}

void CanvasArea::update_canvas_texture(bool palette_changed)
{
	if (!m_backend)
	{
		return;
	}

	ImageBuffer& image = m_session.get_main_buffer();
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

	if (palette_changed)
	{
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

void CanvasArea::update_floating_texture(bool palette_changed)
{
	const FloatingRegion& region = m_session.floating_region();
	if (!region.active)
	{
		if (m_floating_texture && m_backend)
		{
			m_backend->destroyTexture(m_floating_texture);
		}
		m_floating_texture = nullptr;
		m_floating_texture_width = 0;
		m_floating_texture_height = 0;
		m_floating_texture_revision = 0;
		return;
	}

	if (!m_backend)
	{
		return;
	}

	const ImageBuffer& content = region.content;
	if (content.width() <= 0 || content.height() <= 0)
	{
		return;
	}

	if (!m_floating_texture ||
	    m_floating_texture_width != content.width() ||
	    m_floating_texture_height != content.height())
	{
		if (m_floating_texture)
		{
			m_backend->destroyTexture(m_floating_texture);
			m_floating_texture = nullptr;
		}

		m_floating_texture = m_backend->createTexture(
			content.width(), content.height());
		m_floating_texture_width = content.width();
		m_floating_texture_height = content.height();
		m_floating_texture_revision = 0;
	}

	if (!m_floating_texture)
	{
		return;
	}

	if (!palette_changed &&
	    m_floating_texture_revision == region.content_revision)
	{
		return;
	}

	m_upload_pixels.resize(
		static_cast<std::size_t>(content.width()) *
		static_cast<std::size_t>(content.height()));

	const std::vector<PaintCore::ColorIndex>& data = content.data();
	for (std::size_t i = 0; i < data.size(); ++i)
	{
		const PaintCore::ColorIndex index = data[i];
		const bool transparent =
			index == PaintCore::TRANSPARENT_INDEX ||
			(region.transparent_background &&
			 index == region.transparent_color);
		if (transparent)
		{
			m_upload_pixels[i] = PaintCore::Color{0, 0, 0, 0};
		}
		else
		{
			m_upload_pixels[i] = m_lut[index];
		}
	}

	m_backend->updateTexture(m_floating_texture, m_upload_pixels.data(),
	                         content.width(), content.height());
	m_floating_texture_revision = region.content_revision;
}

void CanvasArea::draw_floating_region(ImDrawList* draw_list,
                                      ImGuiCanvasRenderer& renderer,
                                      const Palette& palette)
{
	const FloatingRegion& region = m_session.floating_region();
	if (!region.active)
	{
		return;
	}

	// The canvas texture still contains the pixels under the hole; cover them
	// with the current fill color while the region floats.
	if (!region.hole.empty())
	{
		renderer.draw_rect_filled(
			region.hole.x, region.hole.y, region.hole.w, region.hole.h,
			palette.getColor(region.hole_fill));
	}

	const PaintCore::Rect canvas_rect{
		0, 0, m_session.getWidth(), m_session.getHeight()};
	const PaintCore::Rect visible = intersect(region.rect, canvas_rect);
	if (visible.empty())
	{
		return;
	}

	if (m_floating_texture)
	{
		const PaintCore::Vec2f p0 = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(visible.x),
			                 static_cast<float>(visible.y)});
		const PaintCore::Vec2f p1 = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(visible.right()),
			                 static_cast<float>(visible.bottom())});

		const float u0 = static_cast<float>(visible.x - region.rect.x) /
			static_cast<float>(region.rect.w);
		const float v0 = static_cast<float>(visible.y - region.rect.y) /
			static_cast<float>(region.rect.h);
		const float u1 = static_cast<float>(visible.right() - region.rect.x) /
			static_cast<float>(region.rect.w);
		const float v1 = static_cast<float>(visible.bottom() - region.rect.y) /
			static_cast<float>(region.rect.h);

		draw_list->AddImage(
			reinterpret_cast<ImTextureID>(m_floating_texture),
			ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y),
			ImVec2(u0, v0), ImVec2(u1, v1));
		return;
	}

	// Fallback if the backend cannot create a texture.
	for (int y = visible.y; y < visible.bottom(); ++y)
	{
		for (int x = visible.x; x < visible.right(); ++x)
		{
			const PaintCore::ColorIndex color = region.content.get_pixel(
				x - region.rect.x, y - region.rect.y);
			const bool transparent =
				color == PaintCore::TRANSPARENT_INDEX ||
				(region.transparent_background &&
				 color == region.transparent_color);
			if (transparent)
			{
				continue;
			}
			renderer.draw_rect_filled(x, y, 1, 1, palette.getColor(color));
		}
	}
}

void CanvasArea::draw_marquee(ImDrawList* draw_list,
                              const PaintCore::Rect& rect) const
{
	if (!draw_list || rect.empty())
	{
		return;
	}

	const PaintCore::Rect canvas_rect{
		0, 0, m_session.getWidth(), m_session.getHeight()};
	const PaintCore::Rect visible = intersect(rect, canvas_rect);
	if (visible.empty())
	{
		return;
	}

	const PaintCore::Vec2f p0 = m_view.image_to_screen(
		PaintCore::Vec2f{static_cast<float>(visible.x),
		                 static_cast<float>(visible.y)});
	const PaintCore::Vec2f p1 = m_view.image_to_screen(
		PaintCore::Vec2f{static_cast<float>(visible.right()),
		                 static_cast<float>(visible.bottom())});

	// Clip to the image so a partially-outside region does not paint its
	// marquee over the gray margin.
	const PaintCore::Vec2f image_p0 = m_view.image_to_screen(
		PaintCore::Vec2f{0.0f, 0.0f});
	const PaintCore::Vec2f image_p1 = m_view.image_to_screen(
		PaintCore::Vec2f{static_cast<float>(m_session.getWidth()),
		                 static_cast<float>(m_session.getHeight())});

	const ImVec2 current_min = draw_list->GetClipRectMin();
	const ImVec2 current_max = draw_list->GetClipRectMax();
	const ImVec2 clip_min(
		std::max(current_min.x, image_p0.x),
		std::max(current_min.y, image_p0.y));
	const ImVec2 clip_max(
		std::min(current_max.x, image_p1.x),
		std::min(current_max.y, image_p1.y));
	if (clip_min.x >= clip_max.x || clip_min.y >= clip_max.y)
	{
		return;
	}

	draw_list->PushClipRect(clip_min, clip_max, true);
	add_dashed_rect(draw_list, ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y));
	draw_list->PopClipRect();
}

PaintSession& CanvasArea::session()
{
	return m_session;
}

const PaintSession& CanvasArea::session() const
{
	return m_session;
}

PaintCore::Vec2 CanvasArea::visible_image_origin() const
{
	const PaintCore::Rect visible = m_view.visible_image_rect(
		m_session.getWidth(), m_session.getHeight());
	// visible_image_rect returns an empty rect when nothing is visible; the
	// default Vec2 (0,0) keeps paste usable in that edge case.
	return PaintCore::Vec2{visible.x, visible.y};
}

bool CanvasArea::handle_floating_input(const PaintCore::Vec2& img_pos,
                                       bool pressed, bool released,
                                       bool item_hovered)
{
    // Right-click cancels, whether the region is being dragged or not. This
    // happens before any tool routing so the click cannot start a stroke.
    if ((item_hovered || m_floating_dragging) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        m_session.cancel_floating_region();
        m_floating_dragging = false;
        return true;
    }

    if (m_floating_dragging)
    {
        if (released)
        {
            m_floating_dragging = false;
            return true;
        }

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const int dx = img_pos.x - m_floating_last_img.x;
            const int dy = img_pos.y - m_floating_last_img.y;
            if (dx != 0 || dy != 0)
            {
                m_session.move_floating_region(dx, dy);
                m_floating_last_img = img_pos;
            }
            if (m_backend)
            {
                m_backend->set_cursor(PaintCore::CursorType::Move);
            }
        }
        return true;
    }

    if (pressed && item_hovered)
    {
        if (m_session.floating_region_contains(img_pos))
        {
            m_floating_dragging = true;
            m_floating_last_img = img_pos;
            if (m_backend)
            {
                m_backend->set_cursor(PaintCore::CursorType::Move);
            }
        }
        else
        {
            // Clicking outside confirms the region; the click is consumed so
            // it cannot also start a new stroke/selection.
            m_session.commit_floating_region();
        }
        return true;
    }

    if (item_hovered && m_session.floating_region_contains(img_pos) && m_backend)
    {
        m_backend->set_cursor(PaintCore::CursorType::Move);
    }

    return false;
}

void CanvasArea::handle_input(const ImVec2& mouse)
{
    // Same transformation used by rendering in this frame. Intentionally
    // unclamped: tools receive real coordinates even when the pointer leaves
    // the canvas, so shapes can extend beyond the border (Paint-like behavior).
    // DrawBuffer clips the preview.
    const PaintCore::Vec2 img_pos = m_view.screen_to_image_pixel(
        PaintCore::Vec2f{mouse.x, mouse.y});

    const bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    const bool pressed = mouse_down && !m_mouse_was_down;
    const bool item_hovered = ImGui::IsItemHovered();
    const ContentClip clip = current_content_clip();

    // The floating region owns input while it exists: it can be dragged,
    // confirmed or cancelled before resize handles or tools see the mouse.
    if (m_session.has_floating_region())
    {
        handle_floating_input(img_pos, pressed, mouse_released, item_hovered);
        m_mouse_was_down = mouse_down;
        return;
    }

    m_floating_dragging = false;

    // Resize handles take priority over the active tool.
    if (m_resize_gizmo.handle_input(m_session, m_backend, m_view, mouse,
                                    pressed, mouse_released, item_hovered,
                                    m_resize_fill, clip.min, clip.max))
    {
        m_mouse_was_down = mouse_down;
        return;
    }

    const bool inside_image =
        img_pos.x >= 0 && img_pos.y >= 0 &&
        img_pos.x < m_session.getWidth() && img_pos.y < m_session.getHeight();

    if (mouse_down && !m_mouse_was_down && item_hovered && inside_image)
    {
        m_session.handle_mouse_press(img_pos, 1);
        m_mouse_was_down = true;
    }
    else if (mouse_released && m_session.active_tool() && m_session.active_tool()->is_drawing())
    {
        m_session.handle_mouse_release(img_pos, 1);
        m_mouse_was_down = false;
    }
    else if (ImGui::IsItemActive() && m_session.active_tool() && m_session.active_tool()->is_drawing())
    {
        m_session.handle_mouse_move(img_pos);
    }

    m_mouse_was_down = mouse_down;
}