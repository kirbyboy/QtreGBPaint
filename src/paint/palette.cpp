#include "paint/palette.h"

using namespace PaintCore;

Palette::Palette(size_t bitsPerPixel)
	: m_bpp(bitsPerPixel == 0 ? 1 : bitsPerPixel),
	  m_colors(1 << (bitsPerPixel == 0 ? 1 : bitsPerPixel), PaintCore::Color{})
{
}

void Palette::setColor(PaintCore::ColorIndex index, PaintCore::Color color)
{
	if (index >= m_colors.size())
	{
		return;
	}
	m_colors[index] = color;
}

PaintCore::Color Palette::getColor(PaintCore::ColorIndex index) const
{
	if (index >= m_colors.size())
	{
		return {255, 0, 255, 255}; // magenta: out-of-range sentinel
	}
	return m_colors[index];
}

size_t Palette::size() const
{
	return m_colors.size();
}

size_t Palette::bitsPerPixel() const
{
	return m_bpp;
}

Palette Palette::dmg()
{
	Palette palette(2);

	palette.setColor(0, {0xE0, 0xF8, 0xD0, 0xFF});
	palette.setColor(1, {0x88, 0xC0, 0x70, 0xFF});
	palette.setColor(2, {0x34, 0x68, 0x56, 0xFF});
	palette.setColor(3, {0x08, 0x18, 0x20, 0xFF});
	
	return palette;
}

Palette Palette::grayscale(size_t bitsPerPixel)
{
	Palette palette(bitsPerPixel);
	const size_t levels = palette.size();          // siempre >= 2
	
	for (size_t i = 0; i < levels; ++i)
	{
		const uint8_t v = static_cast<uint8_t>(i * 255 / (levels - 1));
		palette.setColor(static_cast<ColorIndex>(i), {v, v, v, 0xFF});
	}

	return palette;
}