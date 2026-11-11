#ifndef QTREGBPAINT_PAINT_CHECKERBOARD_H
#define QTREGBPAINT_PAINT_CHECKERBOARD_H

#include <algorithm>
#include <cmath>

#include <imgui.h>

class IGuiBackend;

namespace PaintUi
{
// Screen-space transparency checkerboard, clipped to [min, max]. `origin` is
// the pattern anchor: the first pixel at the origin sits in the middle of a
// square. Cell size is in screen pixels, so zoom never changes it.
inline void draw_checkerboard(ImDrawList* draw_list, const ImVec2& min,
                              const ImVec2& max, float cell,
                              const ImVec2& origin)
{
	if (!draw_list || cell <= 0.0f ||
	    max.x <= min.x || max.y <= min.y)
	{
		return;
	}

	const ImU32 dark = IM_COL32(64, 64, 64, 255);
	const ImU32 light = IM_COL32(192, 192, 192, 255);
	const float half = cell * 0.5f;

	const int first_x = static_cast<int>(std::floor(
		(min.x - origin.x + half) / cell));
	const int last_x = static_cast<int>(std::ceil(
		(max.x - origin.x + half) / cell));
	const int first_y = static_cast<int>(std::floor(
		(min.y - origin.y + half) / cell));
	const int last_y = static_cast<int>(std::ceil(
		(max.y - origin.y + half) / cell));

	for (int cell_y = first_y; cell_y < last_y; ++cell_y)
	{
		const float y0 = origin.y - half + cell_y * cell;
		const float y1 = y0 + cell;
		const float visible_y0 = std::max(min.y, y0);
		const float visible_y1 = std::min(max.y, y1);
		if (visible_y0 >= visible_y1)
		{
			continue;
		}

		for (int cell_x = first_x; cell_x < last_x; ++cell_x)
		{
			const float x0 = origin.x - half + cell_x * cell;
			const float x1 = x0 + cell;
			const float visible_x0 = std::max(min.x, x0);
			const float visible_x1 = std::min(max.x, x1);
			if (visible_x0 >= visible_x1)
			{
				continue;
			}

			const ImU32 color =
				((cell_x + cell_y) & 1) ? dark : light;
			draw_list->AddRectFilled(
				ImVec2(visible_x0, visible_y0),
				ImVec2(visible_x1, visible_y1), color);
		}
	}
}

// Repeating checkerboard backed by a texture: one quad per texture-sized tile
// instead of one rect per cell. The texture is filled with whole cells and its
// side is a multiple of two cells, so the parity pattern tiles seamlessly.
// The backend must outlive the instance.
class CheckerboardTexture
{
	public:
		CheckerboardTexture() = default;
		~CheckerboardTexture();

		CheckerboardTexture(const CheckerboardTexture&) = delete;
		CheckerboardTexture& operator=(const CheckerboardTexture&) = delete;
		CheckerboardTexture(CheckerboardTexture&&) = delete;
		CheckerboardTexture& operator=(CheckerboardTexture&&) = delete;

		bool create(IGuiBackend* backend, int cell, int texture_size = 256);
		void destroy();

		bool valid() const { return m_texture != nullptr; }
		int cell() const { return m_cell; }
		int texture_size() const { return m_size; }

		// Same pattern and anchor as draw_checkerboard(), clipped to
		// [min, max] through the quad UVs.
		void draw(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
		          const ImVec2& origin) const;

	private:
		IGuiBackend* m_backend = nullptr;
		void* m_texture = nullptr;
		int m_cell = 0;
		int m_size = 0;
};
} // namespace PaintUi

#endif
