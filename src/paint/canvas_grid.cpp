#include "paint/canvas_grid.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/ibackend.h"

namespace
{
constexpr float kMinimumZoom = 6.0f;
constexpr PaintCore::Color kGridDark{64, 64, 64, 255};
constexpr PaintCore::Color kGridLight{192, 192, 192, 255};

void fill_pattern(std::vector<PaintCore::Color>& pixels, int count, int phase)
{
	pixels.resize(static_cast<std::size_t>(count));
	for (int i = 0; i < count; ++i)
	{
		pixels[static_cast<std::size_t>(i)] =
			((i + phase) % 2 == 0) ? kGridDark : kGridLight;
	}
}

int parity(float value)
{
	int result = static_cast<int>(std::floor(value)) % 2;
	if (result < 0)
	{
		result += 2;
	}
	return result;
}
}

CanvasGrid::CanvasGrid(IGuiBackend* backend) : m_backend(backend)
{
}

CanvasGrid::~CanvasGrid()
{
	destroy_textures();
}

void CanvasGrid::destroy_textures()
{
	if (m_backend)
	{
		if (m_vertical_texture)
		{
			m_backend->destroyTexture(m_vertical_texture);
		}
		if (m_horizontal_texture)
		{
			m_backend->destroyTexture(m_horizontal_texture);
		}
	}
	m_vertical_texture = nullptr;
	m_vertical_height = 0;
	m_vertical_phase = 0;
	m_horizontal_texture = nullptr;
	m_horizontal_width = 0;
	m_horizontal_phase = 0;
}

void CanvasGrid::ensure_textures(int vertical_height, int horizontal_width,
                                 int vertical_phase, int horizontal_phase)
{
	if (!m_backend)
	{
		return;
	}

	if (vertical_height > 0 &&
	    (vertical_height != m_vertical_height ||
	     vertical_phase != m_vertical_phase))
	{
		if (m_vertical_texture)
		{
			m_backend->destroyTexture(m_vertical_texture);
			m_vertical_texture = nullptr;
		}
		m_vertical_height = 0;
		m_vertical_phase = 0;

		std::vector<PaintCore::Color> pixels;
		fill_pattern(pixels, vertical_height, vertical_phase);
		m_vertical_texture = m_backend->createTexture(1, vertical_height);
		if (m_vertical_texture)
		{
			m_backend->updateTexture(m_vertical_texture, pixels.data(),
			                         1, vertical_height);
			m_vertical_height = vertical_height;
			m_vertical_phase = vertical_phase;
		}
	}

	if (horizontal_width > 0 &&
	    (horizontal_width != m_horizontal_width ||
	     horizontal_phase != m_horizontal_phase))
	{
		if (m_horizontal_texture)
		{
			m_backend->destroyTexture(m_horizontal_texture);
			m_horizontal_texture = nullptr;
		}
		m_horizontal_width = 0;
		m_horizontal_phase = 0;

		std::vector<PaintCore::Color> pixels;
		fill_pattern(pixels, horizontal_width, horizontal_phase);
		m_horizontal_texture = m_backend->createTexture(horizontal_width, 1);
		if (m_horizontal_texture)
		{
			m_backend->updateTexture(m_horizontal_texture, pixels.data(),
			                         horizontal_width, 1);
			m_horizontal_width = horizontal_width;
			m_horizontal_phase = horizontal_phase;
		}
	}
}

void CanvasGrid::render(ImDrawList* draw_list,
                        const PaintCore::ViewTransform& view,
                        int canvas_width, int canvas_height, bool enabled)
{
	if (!enabled || !draw_list || view.zoom < kMinimumZoom ||
	    canvas_width <= 0 || canvas_height <= 0)
	{
		return;
	}

	const ImVec2 clip_min = draw_list->GetClipRectMin();
	const ImVec2 clip_max = draw_list->GetClipRectMax();
	const PaintCore::Vec2f origin = view.image_to_screen({0.0f, 0.0f});
	const PaintCore::Vec2f end = view.image_to_screen(
		{static_cast<float>(canvas_width), static_cast<float>(canvas_height)});

	const float visible_left = std::max(origin.x, clip_min.x);
	const float visible_top = std::max(origin.y, clip_min.y);
	const float visible_right = std::min(end.x, clip_max.x);
	const float visible_bottom = std::min(end.y, clip_max.y);
	if (visible_left >= visible_right || visible_top >= visible_bottom)
	{
		return;
	}

	const int x0 = static_cast<int>(std::floor(visible_left));
	const int y0 = static_cast<int>(std::floor(visible_top));
	const int x1 = static_cast<int>(std::ceil(visible_right));
	const int y1 = static_cast<int>(std::ceil(visible_bottom));
	const int visible_width = x1 - x0;
	const int visible_height = y1 - y0;
	if (visible_width <= 0 || visible_height <= 0)
	{
		return;
	}

	ensure_textures(visible_height, visible_width,
	                parity(visible_top - origin.y),
	                parity(visible_left - origin.x));
	if (!m_vertical_texture || !m_horizontal_texture)
	{
		return;
	}

	const ImTextureID vertical_texture =
		reinterpret_cast<ImTextureID>(m_vertical_texture);
	const ImTextureID horizontal_texture =
		reinterpret_cast<ImTextureID>(m_horizontal_texture);

	const float zoom = view.zoom;
	int first_x = static_cast<int>(std::floor((x0 - origin.x) / zoom));
	int last_x = static_cast<int>(std::ceil((x1 - origin.x) / zoom));
	first_x = std::max(first_x, 0);
	last_x = std::min(last_x, canvas_width);
	for (int i = first_x; i <= last_x; ++i)
	{
		const int x = static_cast<int>(std::lround(origin.x + i * zoom));
		if (x >= x1)
		{
			// The last line sits just outside the image, in the extra margin.
			if (i != canvas_width || x >= clip_max.x)
			{
				break;
			}
		}
		if (x + 1 <= x0)
		{
			continue;
		}
		draw_list->AddImage(vertical_texture,
		                    ImVec2(static_cast<float>(x), static_cast<float>(y0)),
		                    ImVec2(static_cast<float>(x + 1),
		                           static_cast<float>(y1 + (i == canvas_width ? 1 : 0))));
	}

	int first_y = static_cast<int>(std::floor((y0 - origin.y) / zoom));
	int last_y = static_cast<int>(std::ceil((y1 - origin.y) / zoom));
	first_y = std::max(first_y, 0);
	last_y = std::min(last_y, canvas_height);
	for (int i = first_y; i <= last_y; ++i)
	{
		const int y = static_cast<int>(std::lround(origin.y + i * zoom));
		if (y >= y1)
		{
			// The last line sits just outside the image, in the extra margin.
			if (i != canvas_height || y >= clip_max.y)
			{
				break;
			}
		}
		if (y + 1 <= y0)
		{
			continue;
		}
		draw_list->AddImage(horizontal_texture,
		                    ImVec2(static_cast<float>(x0), static_cast<float>(y)),
		                    ImVec2(static_cast<float>(x1 + (i == canvas_height ? 1 : 0)),
		                           static_cast<float>(y + 1)));
	}
}
