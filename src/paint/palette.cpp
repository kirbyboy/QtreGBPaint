#include "paint/palette.h"

using namespace PaintCore;

Palette::Palette(size_t bitsPerPixel)
    : m_bpp(bitsPerPixel == 0 ? 1 : bitsPerPixel),
      m_colors(1 << (bitsPerPixel == 0 ? 1 : bitsPerPixel), 0)
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
        return 0xFFFF00FF;
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
