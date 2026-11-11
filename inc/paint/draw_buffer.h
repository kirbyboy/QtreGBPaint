#ifndef QTREGBPAINT_DRAW_BUFFER_H
#define QTREGBPAINT_DRAW_BUFFER_H

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cassert>

#include "core/color.h"
#include "paint/image_buffer.h"

class DrawBuffer
{
	public:
		DrawBuffer();

		void clear();
		void reset();
		void begin(int x, int y);

		// Limits the area the buffer can grow into. Values <= 0 mean unbounded.
		void set_bounds(int width, int height);

		void set_pixel(int x, int y, PaintCore::ColorIndex color_index);
		bool is_active() const;

		// Coverage mask: true only for pixels the tool actually wrote. It is
		// independent of the stored index, so a palette index such as 255 can
		// be painted without being confused with an untouched pixel.
		bool is_painted(int x, int y) const;
		const std::vector<bool>& painted() const { return m_painted; }
		bool has_painted_pixels() const;
		// Changes whenever the buffer contents or bounds change. The preview
		// tile cache uses it to avoid redundant texture uploads.
		std::uint64_t revision() const { return m_revision; }

		void blend_onto(ImageBuffer& target);

		const std::vector<PaintCore::ColorIndex>& data() const;
		int x() const;
		int y() const;
		int width() const;
		int height() const;

		void ensure_fit(int x, int y);

	private:

		int m_x = 0;
		int m_y = 0;
		int m_width = 0;
		int m_height = 0;
		int m_max_width = 0;
		int m_max_height = 0;
		bool m_active = false;

		std::vector<PaintCore::ColorIndex> m_data;
		std::vector<bool> m_painted;
		std::uint64_t m_revision = 0;
};

#endif
