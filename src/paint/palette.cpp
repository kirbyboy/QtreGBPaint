#include "paint/palette.h"

using namespace PaintCore;

Palette::Palette(size_t size):
m_size(size),
m_colors(size, 0)
{
	if(m_size == 0)
	{
		// Not 0bpp allowed
		m_size = 1;
		m_colors.push_back(0);
	}
}

void Palette::setColor(uint8_t index, PaintCore::Color color)
{
	if(index >= m_colors.size())
	{
		return;
	}
	m_colors[index] = color;
}

PaintCore::Color Palette::getColor(uint8_t index) const
{
	if(index >= m_colors.size())
	{
		return 0xFFFFFFFF;
	}	
	return m_colors[index];
}

size_t Palette::size() const
{
	return m_size;
}
