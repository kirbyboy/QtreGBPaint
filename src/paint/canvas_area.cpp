#include "paint/paint_session.h"
#include "tools/base_tool.h"
#include "paint/canvas_area.h"
#include "paint/checkerboard.h"
#include "paint/color_imgui.h"
#include "paint/draw_buffer.h"
#include "paint/ui_roles.h"
#include "core/ibackend.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#include "imgui_internal.h"

using namespace PaintCore;
using namespace std;

namespace
{
constexpr int kCheckerboardCell = 8;

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

// The visible rect is derived from scroll values; expand it by one image pixel
// so rounding at the edges can never leave an uncovered strip.
PaintCore::Rect expand_image_rect(const PaintCore::Rect& rect, int amount,
                                  int image_width, int image_height)
{
	if (rect.empty())
	{
		return {};
	}

	const int x0 = std::clamp(rect.x - amount, 0, image_width);
	const int y0 = std::clamp(rect.y - amount, 0, image_height);
	const int x1 = std::clamp(rect.right() + amount, 0, image_width);
	const int y1 = std::clamp(rect.bottom() + amount, 0, image_height);
	if (x0 >= x1 || y0 >= y1)
	{
		return {};
	}
	return {x0, y0, x1 - x0, y1 - y0};
}

// Tiles kept in the cache: the visible ones plus one tile of margin, so small
// scrolls across a tile edge do not destroy and re-upload textures.
PaintCore::TileSpan kept_tile_span(const PaintCore::TileSpan& needed,
                                   int image_width, int image_height)
{
	PaintCore::TileSpan keep;
	if (needed.empty())
	{
		return keep;
	}

	keep.x0 = std::max(0, needed.x0 - 1);
	keep.y0 = std::max(0, needed.y0 - 1);
	keep.x1 = std::min(PaintCore::tile_count(image_width), needed.x1 + 1);
	keep.y1 = std::min(PaintCore::tile_count(image_height), needed.y1 + 1);
	return keep;
}

// 2 px highlight just outside the right and bottom image edges. The grid's
// last line covers the first pixel and the resize handles cover the second one
// where they are visible, so the remaining pixel marks the canvas edge without
// moving any existing element.
void draw_canvas_edge_highlight(ImDrawList* draw_list,
                                const PaintCore::ViewTransform& view,
                                int width, int height)
{
	if (!draw_list || width <= 0 || height <= 0)
	{
		return;
	}

	const ImVec2 clip_min = draw_list->GetClipRectMin();
	const ImVec2 clip_max = draw_list->GetClipRectMax();
	const PaintCore::Vec2f origin = view.image_to_screen({0.0f, 0.0f});
	const PaintCore::Vec2f end = view.image_to_screen(
		{static_cast<float>(width), static_cast<float>(height)});

	const float visible_left = std::max(origin.x, clip_min.x);
	const float visible_top = std::max(origin.y, clip_min.y);
	const float visible_right = std::min(end.x, clip_max.x);
	const float visible_bottom = std::min(end.y, clip_max.y);
	if (visible_left >= visible_right || visible_top >= visible_bottom)
	{
		return;
	}

	// Same blue ImGui uses for selection/hover highlights.
	const ImU32 color = PaintUi::Roles::selection();

	if (end.x >= clip_min.x && end.x <= clip_max.x)
	{
		const float x0 = std::max(end.x, clip_min.x);
		const float x1 = std::min(end.x + 2.0f, clip_max.x);
		if (x0 < x1)
		{
			draw_list->AddRectFilled(
				ImVec2(x0, visible_top), ImVec2(x1, visible_bottom), color);
		}
	}

	if (end.y >= clip_min.y && end.y <= clip_max.y)
	{
		const float y0 = std::max(end.y, clip_min.y);
		const float y1 = std::min(end.y + 2.0f, clip_max.y);
		if (y0 < y1)
		{
			draw_list->AddRectFilled(
				ImVec2(visible_left, y0), ImVec2(visible_right, y1), color);
		}
	}
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

// CanvasArea

// CanvasArea

CanvasArea::CanvasArea(IGuiBackend* backend, PaintSession& session)
	: m_backend(backend), m_session(session), m_grid(backend)
{
}

CanvasArea::~CanvasArea()
{
	destroy_canvas_tiles();
	destroy_preview_tiles();
	m_checkerboard.destroy();
	if (m_backend && m_floating_texture)
	{
		m_backend->destroyTexture(m_floating_texture);
		m_floating_texture = nullptr;
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

void CanvasArea::render(ImDrawList* draw_list, const Palette& palette,
                        PaintCore::ColorIndex foreground,
                        PaintCore::ColorIndex background)
{
	if (!draw_list)
	{
		return;
	}

	ImGuiCanvasRenderer renderer;
	renderer.draw_list = draw_list;
	renderer.view = m_view;

	// Base image as a cache of visible tiles; preview stays as primitives for
	// now.
	const bool palette_changed = update_palette_lut(palette);
	if (palette_changed)
	{
		destroy_preview_tiles();
	}
	const bool canvas_tiles_ready = update_canvas_tiles(palette_changed);
	update_floating_texture(palette_changed);

	// Keep the image, grid and handles inside the visible content area, not in
	// the child padding or under the scrollbars.
	const ContentClip clip = current_content_clip();
	draw_list->PushClipRect(clip.min, clip.max, true);

	// Transparency checkerboard under the image. Fixed screen-space cells,
	// anchored to the image origin so it scrolls with the image.
	if (palette.transparent_index().has_value())
	{
		ensure_checkerboard_texture();
		const PaintCore::Vec2f image_min = m_view.image_to_screen(
			PaintCore::Vec2f{0.0f, 0.0f});
		const PaintCore::Vec2f image_max = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(m_session.getWidth()),
			                 static_cast<float>(m_session.getHeight())});
		const ImVec2 checker_min(
			std::max(image_min.x, clip.min.x),
			std::max(image_min.y, clip.min.y));
		const ImVec2 checker_max(
			std::min(image_max.x, clip.max.x),
			std::min(image_max.y, clip.max.y));
		draw_checkerboard(
			draw_list, checker_min, checker_max,
			ImVec2(image_min.x, image_min.y));
	}

	if (canvas_tiles_ready)
	{
		draw_canvas_tiles(draw_list);
		if (!draw_preview_tiles(draw_list, palette))
		{
			m_session.render_preview(renderer, palette);
		}
	}
	else
	{
		// Fallback if the backend cannot create a texture.
		destroy_preview_tiles();
		m_session.render(renderer, m_view, palette);
	}

	// Transparent preview pixels must show the checkerboard instead of the
	// old canvas pixel that the tile cache still contains.
	draw_preview_transparency(draw_list, palette);

	// Hole and floating content belong to the image, so the grid still covers
	// them. The marquee is drawn afterwards to keep it crisp.
	draw_floating_region(draw_list, renderer, palette);

	// Canvas edge highlight; the grid and handles are drawn over it.
	draw_canvas_edge_highlight(draw_list, m_view,
	                           m_session.getWidth(), m_session.getHeight());

	m_grid.render(draw_list, m_view, m_session.getWidth(), m_session.getHeight(),
	              m_grid_enabled);

	// Brush footprint follows the pointer, snapped to image pixels. It is a
	// view overlay: it never touches the document or the history.
	draw_stamp_preview(draw_list, palette, foreground, background);

	// Pending tool edits (curve handles and guide lines) live above the grid.
	draw_edit_overlay(draw_list);

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

	// Resize handles stay disabled while a floating region or a pending tool
	// edit must be resolved first.
	if (!m_session.has_floating_region() && !m_session.has_pending_edit())
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

void CanvasArea::destroy_canvas_tiles()
{
	if (m_backend)
	{
		for (auto& entry : m_canvas_tiles)
		{
			if (entry.second.texture)
			{
				m_backend->destroyTexture(entry.second.texture);
			}
		}
	}

	m_canvas_tiles.clear();
	m_tile_image_width = 0;
	m_tile_image_height = 0;
	m_tiles_full_upload = false;
}

bool CanvasArea::upload_canvas_tile(CanvasTile& tile, int tile_x, int tile_y,
                                    const ImageBuffer& image)
{
	const PaintCore::Rect rect = PaintCore::tile_rect(
		image.width(), image.height(), tile_x, tile_y);
	if (rect.empty())
	{
		return true;
	}

	if (!tile.texture)
	{
		tile.texture = m_backend->createTexture(rect.w, rect.h);
		tile.width = rect.w;
		tile.height = rect.h;
	}
	if (!tile.texture || tile.width != rect.w || tile.height != rect.h)
	{
		return false;
	}

	const int image_width = image.width();
	const std::vector<PaintCore::ColorIndex>& data = image.data();
	m_upload_pixels.resize(static_cast<std::size_t>(rect.w) *
	                       static_cast<std::size_t>(rect.h));

	for (int y = 0; y < rect.h; ++y)
	{
		const PaintCore::ColorIndex* src = data.data() +
			static_cast<std::size_t>(rect.y + y) * static_cast<std::size_t>(image_width) +
			static_cast<std::size_t>(rect.x);
		PaintCore::Color* dst = m_upload_pixels.data() +
			static_cast<std::size_t>(y) * static_cast<std::size_t>(rect.w);

		for (int x = 0; x < rect.w; ++x)
		{
			dst[x] = m_lut[src[x]];
		}
	}

	m_backend->updateTexture(tile.texture, m_upload_pixels.data(), rect.w, rect.h);
	return true;
}

bool CanvasArea::update_canvas_tiles(bool palette_changed)
{
	if (!m_backend)
	{
		return false;
	}

	const ImageBuffer& image = m_session.get_main_buffer();
	const int image_width = image.width();
	const int image_height = image.height();
	if (image_width <= 0 || image_height <= 0)
	{
		destroy_canvas_tiles();
		return false;
	}

	if (image_width != m_tile_image_width || image_height != m_tile_image_height)
	{
		destroy_canvas_tiles();
		m_tile_image_width = image_width;
		m_tile_image_height = image_height;
	}

	if (palette_changed)
	{
		m_tiles_full_upload = true;
	}

	// Consume the dirty region once per frame. A full upload covers it.
	PaintCore::Rect dirty = m_session.take_dirty_rect();
	if (m_tiles_full_upload)
	{
		dirty = {};
	}
	else
	{
		dirty = intersect(dirty, PaintCore::Rect{0, 0, image_width, image_height});
	}

	const PaintCore::Rect visible = expand_image_rect(
		m_view.visible_image_rect(image_width, image_height), 1,
		image_width, image_height);
	if (visible.empty())
	{
		destroy_canvas_tiles();
		m_tile_image_width = image_width;
		m_tile_image_height = image_height;
		return true;
	}

	const PaintCore::TileSpan needed = PaintCore::tiles_covering(
		visible, image_width, image_height);
	const PaintCore::TileSpan keep = kept_tile_span(
		needed, image_width, image_height);

	// Drop tiles that left the visible area and its margin.
	for (auto it = m_canvas_tiles.begin(); it != m_canvas_tiles.end();)
	{
		const int tile_x = static_cast<int>(it->first & 0xffffffffu);
		const int tile_y = static_cast<int>(it->first >> 32);
		if (tile_x < keep.x0 || tile_x >= keep.x1 ||
		    tile_y < keep.y0 || tile_y >= keep.y1)
		{
			m_backend->destroyTexture(it->second.texture);
			it = m_canvas_tiles.erase(it);
		}
		else
		{
			++it;
		}
	}

	bool ready = true;
	for (int tile_y = keep.y0; tile_y < keep.y1; ++tile_y)
	{
		for (int tile_x = keep.x0; tile_x < keep.x1; ++tile_x)
		{
			const bool visible_tile =
				tile_x >= needed.x0 && tile_x < needed.x1 &&
				tile_y >= needed.y0 && tile_y < needed.y1;
			if (!visible_tile && !m_tiles_full_upload && dirty.empty())
			{
				// Existing margin tiles are already current; missing margin
				// tiles are only created when they become visible.
				continue;
			}

			const std::uint64_t key = PaintCore::tile_key(tile_x, tile_y);
			auto it = m_canvas_tiles.find(key);
			if (it == m_canvas_tiles.end())
			{
				if (!visible_tile)
				{
					continue;
				}

				auto inserted = m_canvas_tiles.emplace(key, CanvasTile{});
				auto it_new = inserted.first;
				if (!upload_canvas_tile(it_new->second, tile_x, tile_y, image))
				{
					m_canvas_tiles.erase(it_new);
					ready = false;
				}
				continue;
			}

			if (m_tiles_full_upload ||
			    !intersect(PaintCore::tile_rect(image_width, image_height,
			                                    tile_x, tile_y), dirty).empty())
			{
				if (!upload_canvas_tile(it->second, tile_x, tile_y, image))
				{
					ready = false;
				}
			}
		}
	}

	m_tiles_full_upload = false;
	return ready;
}

void CanvasArea::draw_canvas_tiles(ImDrawList* draw_list)
{
	const int image_width = m_session.getWidth();
	const int image_height = m_session.getHeight();
	if (image_width <= 0 || image_height <= 0)
	{
		return;
	}

	const PaintCore::Rect visible = expand_image_rect(
		m_view.visible_image_rect(image_width, image_height), 1,
		image_width, image_height);
	const PaintCore::TileSpan span = PaintCore::tiles_covering(
		visible, image_width, image_height);

	for (int tile_y = span.y0; tile_y < span.y1; ++tile_y)
	{
		for (int tile_x = span.x0; tile_x < span.x1; ++tile_x)
		{
			auto it = m_canvas_tiles.find(PaintCore::tile_key(tile_x, tile_y));
			if (it == m_canvas_tiles.end() || !it->second.texture)
			{
				continue;
			}

			const PaintCore::Rect rect = PaintCore::tile_rect(
				image_width, image_height, tile_x, tile_y);
			const PaintCore::Vec2f p0 = m_view.image_to_screen(
				PaintCore::Vec2f{static_cast<float>(rect.x),
				                 static_cast<float>(rect.y)});
			const PaintCore::Vec2f p1 = m_view.image_to_screen(
				PaintCore::Vec2f{static_cast<float>(rect.right()),
				                 static_cast<float>(rect.bottom())});
			draw_list->AddImage(
				reinterpret_cast<ImTextureID>(it->second.texture),
				ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y));
		}
	}
}

void CanvasArea::destroy_preview_tiles()
{
	if (m_backend)
	{
		for (auto& entry : m_preview_tiles)
		{
			if (entry.second.texture)
			{
				m_backend->destroyTexture(entry.second.texture);
			}
		}
	}
	m_preview_tiles.clear();
}

void CanvasArea::ensure_checkerboard_texture()
{
	if (m_checkerboard.valid() || !m_backend)
	{
		return;
	}

	// A 256x256 texture holds 32 cells of 8 px, so the parity pattern repeats
	// and the tiles align at the image origin. If creation fails, drawing
	// falls back to one rect per cell.
	m_checkerboard.create(m_backend, kCheckerboardCell);
}

void CanvasArea::draw_checkerboard(ImDrawList* draw_list, const ImVec2& min,
                                   const ImVec2& max,
                                   const ImVec2& origin) const
{
	if (m_checkerboard.valid())
	{
		m_checkerboard.draw(draw_list, min, max, origin);
		return;
	}

	PaintUi::draw_checkerboard(draw_list, min, max,
	                           static_cast<float>(kCheckerboardCell), origin);
}

bool CanvasArea::draw_preview_tiles(ImDrawList* draw_list,
                                    const Palette& palette)
{
	(void)palette;
	if (!draw_list || !m_backend)
	{
		return false;
	}

	ToolBase* tool = m_session.active_tool();
	DrawBuffer* buffer = tool ? tool->get_draw_buffer() : nullptr;
	if (!buffer || !buffer->is_active() || !buffer->has_painted_pixels())
	{
		destroy_preview_tiles();
		return true;
	}

	const int image_width = m_session.getWidth();
	const int image_height = m_session.getHeight();
	if (image_width <= 0 || image_height <= 0)
	{
		destroy_preview_tiles();
		return true;
	}

	const PaintCore::Rect buffer_rect{buffer->x(), buffer->y(),
	                                  buffer->width(), buffer->height()};
	const PaintCore::Rect visible = m_view.visible_image_rect(
		image_width, image_height);
	const PaintCore::Rect area = intersect(buffer_rect, visible);
	if (area.empty())
	{
		destroy_preview_tiles();
		return true;
	}

	const PaintCore::TileSpan span = PaintCore::tiles_covering(
		area, image_width, image_height);
	const std::uint64_t revision = buffer->revision();
	const std::vector<PaintCore::ColorIndex>& data = buffer->data();

	// Drop textures that left the visible buffer area.
	for (auto it = m_preview_tiles.begin(); it != m_preview_tiles.end();)
	{
		const int tile_x = static_cast<int>(it->first & 0xffffffffu);
		const int tile_y = static_cast<int>(it->first >> 32);
		if (tile_x < span.x0 || tile_x >= span.x1 ||
		    tile_y < span.y0 || tile_y >= span.y1)
		{
			if (it->second.texture)
			{
				m_backend->destroyTexture(it->second.texture);
			}
			it = m_preview_tiles.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (int tile_y = span.y0; tile_y < span.y1; ++tile_y)
	{
		for (int tile_x = span.x0; tile_x < span.x1; ++tile_x)
		{
			const PaintCore::Rect rect = PaintCore::tile_rect(
				image_width, image_height, tile_x, tile_y);
			if (rect.empty())
			{
				continue;
			}

			const std::uint64_t key = PaintCore::tile_key(tile_x, tile_y);
			auto it = m_preview_tiles.find(key);
			if (it == m_preview_tiles.end())
			{
				PreviewTile tile;
				tile.texture = m_backend->createTexture(rect.w, rect.h);
				if (!tile.texture)
				{
					destroy_preview_tiles();
					return false;
				}
				tile.width = rect.w;
				tile.height = rect.h;
				it = m_preview_tiles.emplace(key, tile).first;
			}
			else if (it->second.width != rect.w ||
			         it->second.height != rect.h)
			{
				if (it->second.texture)
				{
					m_backend->destroyTexture(it->second.texture);
				}
				it->second.texture =
					m_backend->createTexture(rect.w, rect.h);
				if (!it->second.texture)
				{
					destroy_preview_tiles();
					return false;
				}
				it->second.width = rect.w;
				it->second.height = rect.h;
				it->second.revision = 0;
			}

			if (it->second.revision != revision)
			{
				m_preview_pixels.assign(
					static_cast<std::size_t>(rect.w) *
						static_cast<std::size_t>(rect.h),
					PaintCore::Color{0, 0, 0, 0});
				for (int y = 0; y < rect.h; ++y)
				{
					const int global_y = rect.y + y;
					const int local_y = global_y - buffer->y();
					if (local_y < 0 || local_y >= buffer->height())
					{
						continue;
					}
					for (int x = 0; x < rect.w; ++x)
					{
						const int global_x = rect.x + x;
						const int local_x = global_x - buffer->x();
						if (local_x < 0 || local_x >= buffer->width() ||
						    !buffer->is_painted(local_x, local_y))
						{
							continue;
						}
						const std::size_t source =
							static_cast<std::size_t>(local_y) *
								static_cast<std::size_t>(buffer->width()) +
							static_cast<std::size_t>(local_x);
						const std::size_t target =
							static_cast<std::size_t>(y) *
								static_cast<std::size_t>(rect.w) +
							static_cast<std::size_t>(x);
						m_preview_pixels[target] = m_lut[data[source]];
					}
				}
				m_backend->updateTexture(it->second.texture,
				                         m_preview_pixels.data(), rect.w,
				                         rect.h);
				it->second.revision = revision;
			}
		}
	}

	for (int tile_y = span.y0; tile_y < span.y1; ++tile_y)
	{
		for (int tile_x = span.x0; tile_x < span.x1; ++tile_x)
		{
			auto it = m_preview_tiles.find(
				PaintCore::tile_key(tile_x, tile_y));
			if (it == m_preview_tiles.end() || !it->second.texture)
			{
				continue;
			}
			const PaintCore::Rect rect = PaintCore::tile_rect(
				image_width, image_height, tile_x, tile_y);
			const PaintCore::Vec2f p0 = m_view.image_to_screen(
				PaintCore::Vec2f{static_cast<float>(rect.x),
				                 static_cast<float>(rect.y)});
			const PaintCore::Vec2f p1 = m_view.image_to_screen(
				PaintCore::Vec2f{static_cast<float>(rect.right()),
				                 static_cast<float>(rect.bottom())});
			draw_list->AddImage(
				reinterpret_cast<ImTextureID>(it->second.texture),
				ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y));
		}
	}
	return true;
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

	const ImageBuffer& content = region.content();
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
	    m_floating_texture_revision == region.revision())
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
			region.transparent_background &&
			index == region.transparent_color;
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
	m_floating_texture_revision = region.revision();
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

	const PaintCore::Rect canvas_rect{
		0, 0, m_session.getWidth(), m_session.getHeight()};

	// The canvas texture still contains the pixels under the hole; cover them
	// with the current fill color while the region floats. A transparent fill
	// must cover them with the checkerboard instead of an alpha-0 rect.
	if (!region.hole.empty())
	{
		if (palette.is_transparent(region.hole_fill))
		{
			const PaintCore::Rect visible_hole =
				intersect(region.hole, canvas_rect);
			if (!visible_hole.empty())
			{
				const PaintCore::Vec2f hole_min = m_view.image_to_screen(
					PaintCore::Vec2f{
						static_cast<float>(visible_hole.x),
						static_cast<float>(visible_hole.y)});
				const PaintCore::Vec2f hole_max = m_view.image_to_screen(
					PaintCore::Vec2f{
						static_cast<float>(visible_hole.right()),
						static_cast<float>(visible_hole.bottom())});
				const PaintCore::Vec2f image_origin =
					m_view.image_to_screen(PaintCore::Vec2f{0.0f, 0.0f});
				draw_checkerboard(
					draw_list, ImVec2(hole_min.x, hole_min.y),
					ImVec2(hole_max.x, hole_max.y),
					ImVec2(image_origin.x, image_origin.y));
			}
		}
		else
		{
			renderer.draw_rect_filled(
				region.hole.x, region.hole.y, region.hole.w, region.hole.h,
				palette.getColor(region.hole_fill));
		}
	}

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
			const PaintCore::ColorIndex color = region.content().get_pixel(
				x - region.rect.x, y - region.rect.y);
			const bool transparent =
				region.transparent_background &&
				color == region.transparent_color;
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

void CanvasArea::draw_stamp_preview(ImDrawList* draw_list,
                                    const Palette& palette,
                                    PaintCore::ColorIndex foreground,
                                    PaintCore::ColorIndex background) const
{
	if (!draw_list || !m_hover_inside ||
	    m_session.has_floating_region())
	{
		return;
	}

	ToolBase* tool = m_session.active_tool();
	if (!tool)
	{
		return;
	}

	const ToolBase::StampPreview preview =
		tool->stamp_preview(foreground, background);
	if (!preview.offsets || preview.offsets->empty())
	{
		return;
	}

	const PaintCore::Color color = palette.getColor(preview.color);
	const int alpha = preview.opaque ? 255 : 150;
	const ImU32 fill = IM_COL32(color.r, color.g, color.b, alpha);
	const bool transparent = palette.is_transparent(preview.color);
	const PaintCore::Vec2f screen_origin = m_view.image_to_screen(
		PaintCore::Vec2f{0.0f, 0.0f});
	const ImVec2 checker_origin(screen_origin.x, screen_origin.y);
	const int width = m_session.getWidth();
	const int height = m_session.getHeight();

	int min_x = preview.offsets->front().x;
	int max_x = preview.offsets->front().x;
	int min_y = preview.offsets->front().y;
	int max_y = preview.offsets->front().y;

	for (const PaintCore::Vec2& offset : *preview.offsets)
	{
		min_x = std::min(min_x, offset.x);
		max_x = std::max(max_x, offset.x);
		min_y = std::min(min_y, offset.y);
		max_y = std::max(max_y, offset.y);

		const int x = m_hover_image_pos.x + offset.x;
		const int y = m_hover_image_pos.y + offset.y;
		if (x < 0 || y < 0 || x >= width || y >= height)
		{
			continue;
		}

		const PaintCore::Vec2f p0 = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(x),
			                 static_cast<float>(y)});
		const PaintCore::Vec2f p1 = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(x + 1),
			                 static_cast<float>(y + 1)});
		if (transparent)
		{
			draw_checkerboard(
				draw_list, ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y),
				checker_origin);
		}
		else
		{
			draw_list->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y),
			                         fill);
		}
	}

	if (preview.border)
	{
		const PaintCore::Vec2f p0 = m_view.image_to_screen(
			PaintCore::Vec2f{
				static_cast<float>(m_hover_image_pos.x + min_x),
				static_cast<float>(m_hover_image_pos.y + min_y)});
		const PaintCore::Vec2f p1 = m_view.image_to_screen(
			PaintCore::Vec2f{
				static_cast<float>(m_hover_image_pos.x + max_x + 1),
				static_cast<float>(m_hover_image_pos.y + max_y + 1)});

		// Fixed one-pixel border, independent of the zoom. It sits just
		// outside the footprint so it never covers an erased pixel.
		const ImU32 border = PaintUi::Roles::selection();
		draw_list->AddRectFilled(ImVec2(p0.x - 1.0f, p0.y - 1.0f),
		                         ImVec2(p1.x + 1.0f, p0.y), border);
		draw_list->AddRectFilled(ImVec2(p0.x - 1.0f, p1.y),
		                         ImVec2(p1.x + 1.0f, p1.y + 1.0f), border);
		draw_list->AddRectFilled(ImVec2(p0.x - 1.0f, p0.y),
		                         ImVec2(p0.x, p1.y), border);
		draw_list->AddRectFilled(ImVec2(p1.x, p0.y),
		                         ImVec2(p1.x + 1.0f, p1.y), border);
	}
}

void CanvasArea::draw_preview_transparency(ImDrawList* draw_list,
                                           const Palette& palette) const
{
	if (!draw_list || !palette.transparent_index().has_value())
	{
		return;
	}

	ToolBase* tool = m_session.active_tool();
	if (!tool)
	{
		return;
	}

	DrawBuffer* buffer = tool->get_draw_buffer();
	if (!buffer || !buffer->is_active())
	{
		return;
	}

	const int buffer_x = buffer->x();
	const int buffer_y = buffer->y();
	const int buffer_w = buffer->width();
	const int buffer_h = buffer->height();
	if (buffer_w <= 0 || buffer_h <= 0)
	{
		return;
	}

	const std::vector<PaintCore::ColorIndex>& data = buffer->data();
	const PaintCore::Vec2f image_origin = m_view.image_to_screen(
		PaintCore::Vec2f{0.0f, 0.0f});
	const ImVec2 checker_origin(image_origin.x, image_origin.y);

	for (int y = 0; y < buffer_h; ++y)
	{
		int run_start = -1;
		for (int x = 0; x <= buffer_w; ++x)
		{
			bool transparent = false;
			if (x < buffer_w && buffer->is_painted(x, y))
			{
				const std::size_t index =
					static_cast<std::size_t>(y) * buffer_w + x;
				transparent = palette.is_transparent(data[index]);
			}

			if (transparent)
			{
				if (run_start < 0)
				{
					run_start = x;
				}
				continue;
			}

			if (run_start < 0)
			{
				continue;
			}

			// Coalesce each horizontal run into one checkerboard call; the
			// pattern stays anchored to the image origin.
			const PaintCore::Vec2f p0 = m_view.image_to_screen(
				PaintCore::Vec2f{static_cast<float>(buffer_x + run_start),
				                 static_cast<float>(buffer_y + y)});
			const PaintCore::Vec2f p1 = m_view.image_to_screen(
				PaintCore::Vec2f{static_cast<float>(buffer_x + x),
				                 static_cast<float>(buffer_y + y + 1)});
			draw_checkerboard(
				draw_list, ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y),
				checker_origin);
			run_start = -1;
		}
	}
}

int CanvasArea::edit_handle_at(const ImVec2& mouse) const
{
	ToolBase* tool = m_session.active_tool();
	if (!tool)
	{
		return -1;
	}

	ToolBase::EditOverlay overlay;
	if (!tool->edit_overlay(overlay))
	{
		return -1;
	}

	constexpr float kHitRadius = 6.0f;
	float best_distance = kHitRadius * kHitRadius;
	int best_index = -1;

	for (std::size_t i = 0; i < overlay.handles.size(); ++i)
	{
		const ToolBase::EditHandle& handle = overlay.handles[i];
		if (!handle.draggable)
		{
			continue;
		}

		const PaintCore::Vec2f screen = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(handle.pos.x) + 0.5f,
			                 static_cast<float>(handle.pos.y) + 0.5f});
		const float dx = mouse.x - screen.x;
		const float dy = mouse.y - screen.y;
		const float distance = dx * dx + dy * dy;
		if (distance <= best_distance)
		{
			best_distance = distance;
			best_index = static_cast<int>(i);
		}
	}

	return best_index;
}

void CanvasArea::draw_edit_overlay(ImDrawList* draw_list)
{
	if (!draw_list)
	{
		return;
	}

	ToolBase* tool = m_session.active_tool();
	if (!tool)
	{
		return;
	}

	ToolBase::EditOverlay overlay;
	if (!tool->edit_overlay(overlay))
	{
		return;
	}

	const ImU32 guide_color = PaintUi::Roles::guide();
	for (const ToolBase::EditGuide& guide : overlay.guides)
	{
		const PaintCore::Vec2f from = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(guide.from.x) + 0.5f,
			                 static_cast<float>(guide.from.y) + 0.5f});
		const PaintCore::Vec2f to = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(guide.to.x) + 0.5f,
			                 static_cast<float>(guide.to.y) + 0.5f});
		draw_list->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y),
		                   guide_color, 1.0f);
	}

	for (std::size_t i = 0; i < overlay.handles.size(); ++i)
	{
		const ToolBase::EditHandle& handle = overlay.handles[i];
		const PaintCore::Vec2f center = m_view.image_to_screen(
			PaintCore::Vec2f{static_cast<float>(handle.pos.x) + 0.5f,
			                 static_cast<float>(handle.pos.y) + 0.5f});
		const float half = handle.draggable ? 3.5f : 2.5f;
		const ImVec2 min(std::floor(center.x - half),
		                std::floor(center.y - half));
		const ImVec2 max(std::floor(center.x + half),
		                std::floor(center.y + half));

		ImU32 fill = handle.draggable
			? PaintUi::Roles::selection()
			: PaintUi::Roles::text();
		if (static_cast<int>(i) == m_edit_handle)
		{
			fill = PaintUi::Roles::accent();
		}

		draw_list->AddRectFilled(min, max, fill);
		draw_list->AddRect(min, max, IM_COL32(0, 0, 0, 200));
	}
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

bool CanvasArea::pointer_over_image(const ImVec2& mouse) const
{
	const PaintCore::Vec2 img = image_position_at(mouse);
	return img.x >= 0 && img.y >= 0 &&
	       img.x < m_session.getWidth() && img.y < m_session.getHeight();
}

PaintCore::Vec2 CanvasArea::image_position_at(const ImVec2& mouse) const
{
	return m_view.screen_to_image_pixel(
		PaintCore::Vec2f{mouse.x, mouse.y});
}

bool CanvasArea::image_pixel_at(const ImVec2& mouse, PaintCore::Vec2& pos,
                                PaintCore::ColorIndex& color) const
{
	pos = image_position_at(mouse);
	const ImageBuffer& image = m_session.get_main_buffer();
	if (pos.x < 0 || pos.y < 0 ||
	    pos.x >= image.width() || pos.y >= image.height())
	{
		return false;
	}

	color = image.get_pixel(pos.x, pos.y);
	return true;
}

ImVec2 CanvasArea::visible_image_center() const
{
	const float viewport_x0 = m_view.origin.x + m_view.scroll_px.x;
	const float viewport_y0 = m_view.origin.y + m_view.scroll_px.y;
	const float viewport_x1 = viewport_x0 + m_view.viewport_size.x;
	const float viewport_y1 = viewport_y0 + m_view.viewport_size.y;

	const float image_x0 = m_view.origin.x;
	const float image_y0 = m_view.origin.y;
	const float image_x1 =
		image_x0 + static_cast<float>(m_session.getWidth()) * m_view.zoom;
	const float image_y1 =
		image_y0 + static_cast<float>(m_session.getHeight()) * m_view.zoom;

	const float x0 = std::max(viewport_x0, image_x0);
	const float y0 = std::max(viewport_y0, image_y0);
	const float x1 = std::min(viewport_x1, image_x1);
	const float y1 = std::min(viewport_y1, image_y1);
	if (x0 < x1 && y0 < y1)
	{
		return ImVec2((x0 + x1) * 0.5f, (y0 + y1) * 0.5f);
	}

	return ImVec2((viewport_x0 + viewport_x1) * 0.5f,
	              (viewport_y0 + viewport_y1) * 0.5f);
}

bool CanvasArea::scroll_for_zoom_anchor(const ImVec2& screen_anchor,
                                        float new_zoom, ImVec2& scroll) const
{
	if (m_view.zoom <= 0.0f ||
	    m_view.viewport_size.x <= 0.0f || m_view.viewport_size.y <= 0.0f ||
	    new_zoom <= 0.0f)
	{
		return false;
	}

	const PaintCore::Vec2f result = m_view.scroll_for_zoom_anchor(
		PaintCore::Vec2f{screen_anchor.x, screen_anchor.y}, new_zoom);
	scroll = ImVec2(result.x, result.y);
	return true;
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

    PaintCore::Modifiers modifiers;
    const ImGuiIO& io = ImGui::GetIO();
    modifiers.shift = io.KeyShift;
    modifiers.ctrl = io.KeyCtrl;
    modifiers.alt = io.KeyAlt;
    m_session.set_modifiers(modifiers);

    const bool left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool right_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool left_pressed = left_down && !m_left_was_down;
    const bool left_released = !left_down && m_left_was_down;
    const bool right_pressed = right_down && !m_right_was_down;
    const bool right_released = !right_down && m_right_was_down;
    const bool pressed = left_pressed;
    const bool released = left_released;
    const bool item_hovered = ImGui::IsItemHovered();
    m_mouse_over_canvas = item_hovered || ImGui::IsItemActive();
    const ContentClip clip = current_content_clip();
    m_hover_inside = false;

    const auto update_buttons = [&]()
    {
        m_left_was_down = left_down;
        m_right_was_down = right_down;
    };

    // The floating region owns input while it exists: it can be dragged,
    // confirmed or cancelled before resize handles or tools see the mouse.
    if (m_session.has_floating_region())
    {
        handle_floating_input(img_pos, pressed, left_released, item_hovered);
        update_buttons();
        return;
    }

    m_floating_dragging = false;

    if (m_edit_handle >= 0 && !m_session.has_pending_edit())
    {
        m_edit_handle = -1;
    }

    // A pending tool edit is modal: its handles are dragged with the left
    // button, any other left click commits the edit, and the right button
    // cancels it. The handles can be outside the image but inside the child.
    if (m_session.has_pending_edit())
    {
        if (left_down && right_down)
        {
            m_edit_handle = -1;
            m_session.cancel_pending_edit();
            update_buttons();
            return;
        }

        if (m_edit_handle >= 0)
        {
            if (!left_down || right_pressed)
            {
                m_session.end_edit_handle();
                m_edit_handle = -1;
                if (right_pressed)
                {
                    m_session.cancel_pending_edit();
                }
            }
            else
            {
                m_session.move_edit_handle(m_edit_handle, img_pos);
                if (m_backend)
                {
                    m_backend->set_cursor(PaintCore::CursorType::Move);
                }
            }
            update_buttons();
            return;
        }

        if (!item_hovered)
        {
            update_buttons();
            return;
        }

        if (left_pressed)
        {
            const int handle = edit_handle_at(mouse);
            if (handle >= 0)
            {
                m_edit_handle = handle;
                m_session.begin_edit_handle(handle);
                if (m_backend)
                {
                    m_backend->set_cursor(PaintCore::CursorType::Move);
                }
            }
            else
            {
                m_session.commit_pending_edit();
            }
            update_buttons();
            return;
        }

        if (right_pressed)
        {
            m_session.cancel_pending_edit();
            update_buttons();
            return;
        }

        if (edit_handle_at(mouse) >= 0 && m_backend)
        {
            m_backend->set_cursor(PaintCore::CursorType::Move);
        }
        update_buttons();
        return;
    }

    // Resize handles take priority over the active tool.
    if (m_resize_gizmo.handle_input(m_session, m_backend, m_view, mouse,
                                    pressed, released, item_hovered,
                                    m_resize_fill, clip.min, clip.max))
    {
        update_buttons();
        return;
    }

    // Both buttons cancel the unconfirmed stroke. Suppress a new one until
    // both are released, so releasing one does not immediately start another.
    if (left_down && right_down)
    {
        if (m_stroke_button != 0 ||
            (m_session.active_tool() && m_session.active_tool()->is_operating()))
        {
            m_session.cancel_active_stroke();
        }
        m_stroke_button = 0;
        m_suppress_stroke = true;
        update_buttons();
        return;
    }

    if (m_suppress_stroke)
    {
        if (!left_down && !right_down)
        {
            m_suppress_stroke = false;
        }
        update_buttons();
        return;
    }

    ToolBase* tool = m_session.active_tool();

    // Undo or a tool switch may have cancelled the stroke outside this router.
    if (m_stroke_button != 0 && (!tool || !tool->is_operating()))
    {
        m_stroke_button = 0;
    }

    const bool inside_image =
        img_pos.x >= 0 && img_pos.y >= 0 &&
        img_pos.x < m_session.getWidth() && img_pos.y < m_session.getHeight();

    m_hover_image_pos = img_pos;
    m_hover_inside =
        (item_hovered || ImGui::IsItemActive()) && inside_image;

    int start_button = 0;
    if (left_pressed)
    {
        start_button = ToolBase::kLeftButton;
    }
    else if (right_pressed)
    {
        start_button = ToolBase::kRightButton;
    }

    if (m_stroke_button == 0 && start_button != 0 &&
        item_hovered && inside_image && tool &&
        tool->accepts_button(start_button))
    {
        m_session.handle_mouse_press(img_pos, start_button);
        if (m_session.active_tool() && m_session.active_tool()->is_operating())
        {
            m_stroke_button = start_button;
        }
    }
    else if (m_stroke_button != 0)
    {
        const bool active_down =
            (m_stroke_button == ToolBase::kLeftButton) ? left_down : right_down;
        const bool active_released =
            (m_stroke_button == ToolBase::kLeftButton) ? left_released : right_released;

        if (active_released)
        {
            m_session.handle_mouse_release(img_pos, m_stroke_button);
            m_stroke_button = 0;
            if (left_down || right_down)
            {
                // The other button is still held: wait for a clean release.
                m_suppress_stroke = true;
            }
        }
        else if (active_down)
        {
            m_session.handle_mouse_move(img_pos);
        }
    }

    update_buttons();
}