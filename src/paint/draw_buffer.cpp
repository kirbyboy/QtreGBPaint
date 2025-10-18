#include <algorithm>

#include "paint/draw_buffer.h"
#include "core/color.h"

DrawBuffer::DrawBuffer()
{
	clear();
}

void DrawBuffer::clear()
{
	std::fill(m_data.begin(), m_data.end(), PaintCore::TRANSPARENT_INDEX);
	m_active = false;
}

void DrawBuffer::reset()
{
	m_data.clear();
	m_x = m_y = 0;
	m_width = m_height = 0;
	m_active = false;
}

void DrawBuffer::begin(int x, int y)
{
	reset();
	m_x = x;
	m_y = y;
	m_width = 1;
	m_height = 1;
	m_data.resize(1, PaintCore::TRANSPARENT_INDEX);
	m_active = true;
}

void DrawBuffer::ensure_fit(int px, int py)
{
	if (!m_active)
	{
		begin(px, py);
	}

	int min_x = std::min(m_x, px);
	int min_y = std::min(m_y, py);
	int max_x = std::max(m_x + m_width - 1, px);
	int max_y = std::max(m_y + m_height - 1, py);

	if (min_x == m_x && 
		min_y == m_y && 
		max_x == m_x + m_width - 1 && 
		max_y == m_y + m_height - 1)
	{
		return;
	}

	int new_width = max_x - min_x + 1;
	int new_height = max_y - min_y + 1;

	std::vector<uint8_t> new_data(new_width * new_height, 255);

	for (int y = 0; y < m_height; ++y)
	{
		for (int x = 0; x < m_width; ++x)
		{
			int old_index = y * m_width + x;
			int new_index = (y + (m_y - min_y)) * new_width + (x + (m_x - min_x));
			new_data[new_index] = m_data[old_index];
		}
	}

	m_data.swap(new_data);
	m_x = min_x;
	m_y = min_y;
	m_width = new_width;
	m_height = new_height;
}

void DrawBuffer::set_pixel(int local_x, int local_y, PaintCore::ColorIndex color_index)
{
	if (local_x < 0 || local_y < 0 || local_x >= m_width || local_y >= m_height)
	{
		return;
	}	

	int idx = local_y * m_width + local_x;
	m_data[idx] = color_index;
	m_active = true;
}

bool DrawBuffer::is_active() const
{
	return m_active;
}

void DrawBuffer::blend_onto(ImageBuffer &target)
{
	if(!m_active)
	{
		return;
	}

	int start_x = std::max(0, -m_x);
	int start_y = std::max(0, -m_y);
	int end_x = std::min(m_width, target.width - m_x);
	int end_y = std::min(m_height, target.height - m_y);

	for(int y = start_y; y < end_y; ++y)
	{
		for(int x = start_x; x < end_x; ++x)
		{
			uint8_t color = m_data[y * m_width + x];
			if(color == PaintCore::TRANSPARENT_INDEX)
			{
				continue;
			}
			target.setPixel(m_x + x, m_y + y, color);
		}
	}
	clear();
}

const std::vector<uint8_t> &DrawBuffer::data() const
{
	return m_data;
}

int DrawBuffer::x() const
{
	return m_x;
}

int DrawBuffer::y() const
{
	return m_y;
}

int DrawBuffer::width() const
{
	return m_width;
}

int DrawBuffer::height() const
{
	return m_height;
}