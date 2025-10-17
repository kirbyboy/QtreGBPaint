#ifndef QTREGBPAINT_DRAW_BUFFER_H
#define QTREGBPAINT_DRAW_BUFFER_H

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cassert>

#include "paint/color.h"
#include "paint/image_buffer.h"

class DrawBuffer
{
	public:
		DrawBuffer();

		void clear();
		void reset();
		void begin(int x, int y);
		
		void set_pixel(int x, int y, uint8_t color_index);
		bool is_active() const;

		void blend_onto(ImageBuffer& target);

		const std::vector<uint8_t>& data() const;
		int x() const;
		int y() const;
		int width() const;
		int height() const;

	private:
		void ensure_fit(int x, int y);

		int m_x = 0;
		int m_y = 0;
		int m_width = 0;
		int m_height = 0;
		bool m_active = false;

		std::vector<uint8_t> m_data;
};

#endif
