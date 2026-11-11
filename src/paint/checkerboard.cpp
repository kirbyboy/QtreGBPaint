#include "paint/checkerboard.h"

#include <cstddef>
#include <vector>

#include "core/ibackend.h"

namespace PaintUi
{
namespace
{
const PaintCore::Color kCheckerDark{64, 64, 64, 255};
const PaintCore::Color kCheckerLight{192, 192, 192, 255};
} // namespace

CheckerboardTexture::~CheckerboardTexture()
{
	destroy();
}

bool CheckerboardTexture::create(IGuiBackend* backend, int cell,
                                 int texture_size)
{
	destroy();

	if (!backend || cell <= 0 || texture_size <= 0 ||
	    texture_size % (cell * 2) != 0)
	{
		return false;
	}

	std::vector<PaintCore::Color> pixels(
		static_cast<std::size_t>(texture_size) *
		static_cast<std::size_t>(texture_size));
	for (int y = 0; y < texture_size; ++y)
	{
		for (int x = 0; x < texture_size; ++x)
		{
			const int cell_x = x / cell;
			const int cell_y = y / cell;
			pixels[static_cast<std::size_t>(y) *
			           static_cast<std::size_t>(texture_size) +
			       static_cast<std::size_t>(x)] =
				((cell_x + cell_y) & 1) ? kCheckerDark : kCheckerLight;
		}
	}

	void* texture = backend->createTexture(texture_size, texture_size);
	if (!texture)
	{
		return false;
	}

	backend->updateTexture(texture, pixels.data(), texture_size,
	                       texture_size);
	m_backend = backend;
	m_texture = texture;
	m_cell = cell;
	m_size = texture_size;
	return true;
}

void CheckerboardTexture::destroy()
{
	if (m_texture && m_backend)
	{
		m_backend->destroyTexture(m_texture);
	}
	m_texture = nullptr;
	m_backend = nullptr;
	m_cell = 0;
	m_size = 0;
}

void CheckerboardTexture::draw(ImDrawList* draw_list, const ImVec2& min,
                               const ImVec2& max,
                               const ImVec2& origin) const
{
	if (!draw_list || !m_texture || m_cell <= 0 || m_size <= 0 ||
	    max.x <= min.x || max.y <= min.y)
	{
		return;
	}

	const float size = static_cast<float>(m_size);
	const float half = static_cast<float>(m_cell) * 0.5f;
	const float start_x = origin.x - half;
	const float start_y = origin.y - half;

	const int first_x = static_cast<int>(std::floor((min.x - start_x) / size));
	const int last_x = static_cast<int>(std::ceil((max.x - start_x) / size));
	const int first_y = static_cast<int>(std::floor((min.y - start_y) / size));
	const int last_y = static_cast<int>(std::ceil((max.y - start_y) / size));

	const ImTextureID texture = reinterpret_cast<ImTextureID>(m_texture);

	for (int tile_y = first_y; tile_y < last_y; ++tile_y)
	{
		const float tile_y0 = start_y + tile_y * size;
		const float visible_y0 = std::max(min.y, tile_y0);
		const float visible_y1 = std::min(max.y, tile_y0 + size);
		if (visible_y0 >= visible_y1)
		{
			continue;
		}

		const float v0 = (visible_y0 - tile_y0) / size;
		const float v1 = (visible_y1 - tile_y0) / size;

		for (int tile_x = first_x; tile_x < last_x; ++tile_x)
		{
			const float tile_x0 = start_x + tile_x * size;
			const float visible_x0 = std::max(min.x, tile_x0);
			const float visible_x1 = std::min(max.x, tile_x0 + size);
			if (visible_x0 >= visible_x1)
			{
				continue;
			}

			const float u0 = (visible_x0 - tile_x0) / size;
			const float u1 = (visible_x1 - tile_x0) / size;
			draw_list->AddImage(
				texture, ImVec2(visible_x0, visible_y0),
				ImVec2(visible_x1, visible_y1), ImVec2(u0, v0),
				ImVec2(u1, v1));
		}
	}
}
} // namespace PaintUi
