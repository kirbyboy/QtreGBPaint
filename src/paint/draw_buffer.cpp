#include <algorithm>

#include "paint/draw_buffer.h"
#include "core/color.h"

namespace
{
int clamp_to_bound(int value, int max_size)
{
	if (max_size <= 0)
	{
		return value;
	}
	return std::clamp(value, 0, max_size - 1);
}
}

DrawBuffer::DrawBuffer()
{
	clear();
}

void DrawBuffer::clear()
{
	if(m_active)
	{
		std::fill(m_data.begin(), m_data.end(), 0);
		std::fill(m_painted.begin(), m_painted.end(), false);
		++m_revision;
	}
}

void DrawBuffer::reset()
{
	m_data.clear();
	m_painted.clear();
	m_x = m_y = 0;
	m_width = m_height = 0;
	m_active = false;
	++m_revision;
}

void DrawBuffer::set_bounds(int width, int height)
{
	m_max_width = std::max(0, width);
	m_max_height = std::max(0, height);
}

void DrawBuffer::begin(int x, int y)
{
	reset();
	m_x = clamp_to_bound(x, m_max_width);
	m_y = clamp_to_bound(y, m_max_height);
	m_width = 1;
	m_height = 1;
	m_data.resize(1, 0);
	m_painted.resize(1, false);
	m_active = true;
	++m_revision;
}

void DrawBuffer::ensure_fit(int px, int py)
{
	// The pointer may be outside the canvas, but the buffer must not grow
	// beyond it. Tools keep the true shape coordinates and rely on
	// set_pixel() to drop the pixels that fall outside this clipped area.
	px = clamp_to_bound(px, m_max_width);
	py = clamp_to_bound(py, m_max_height);

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

	std::vector<uint8_t> new_data(new_width * new_height, 0);
	std::vector<bool> new_painted(new_width * new_height, false);

	for (int y = 0; y < m_height; ++y)
	{
		for (int x = 0; x < m_width; ++x)
		{
			int old_index = y * m_width + x;
			int new_index = (y + (m_y - min_y)) * new_width + (x + (m_x - min_x));
			new_data[new_index] = m_data[old_index];
			new_painted[new_index] = m_painted[old_index];
		}
	}

	m_data.swap(new_data);
	m_painted.swap(new_painted);
	m_x = min_x;
	m_y = min_y;
	m_width = new_width;
	m_height = new_height;
	++m_revision;
}

void DrawBuffer::set_pixel(int local_x, int local_y, PaintCore::ColorIndex color_index)
{
	if (local_x < 0 || local_y < 0 || local_x >= m_width || local_y >= m_height)
	{
		return;
	}	

	int idx = local_y * m_width + local_x;
	m_data[idx] = color_index;
	m_painted[idx] = true;
	m_active = true;
	++m_revision;
}

bool DrawBuffer::is_active() const
{
	return m_active;
}

bool DrawBuffer::has_painted_pixels() const
{
	return std::any_of(m_painted.begin(), m_painted.end(),
	                   [](bool painted) { return painted; });
}

bool DrawBuffer::is_painted(int x, int y) const
{
	if (x < 0 || y < 0 || x >= m_width || y >= m_height)
	{
		return false;
	}
	return m_painted[static_cast<std::size_t>(y) * m_width + x];
}

void DrawBuffer::blend_onto(ImageBuffer &target)
{
	if(!m_active)
	{
		return;
	}

	int start_x = std::max(0, -m_x);
	int start_y = std::max(0, -m_y);
	int end_x = std::min(m_width, target.width() - m_x);
	int end_y = std::min(m_height, target.height() - m_y);

	for(int y = start_y; y < end_y; ++y)
	{
		for(int x = start_x; x < end_x; ++x)
		{
			const std::size_t idx =
				static_cast<std::size_t>(y) * m_width + x;
			if (!m_painted[idx])
			{
				continue;
			}
			target.set_pixel(m_x + x, m_y + y, m_data[idx]);
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