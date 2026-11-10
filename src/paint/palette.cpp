#include "paint/palette.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace PaintCore;

namespace
{
size_t checked_bits_per_pixel(size_t bitsPerPixel)
{
	if (bitsPerPixel < 1 || bitsPerPixel > 8)
	{
		throw std::invalid_argument("Bits per pixel must be in the range 1..8");
	}
	return bitsPerPixel;
}

PaintCore::Color mix(const PaintCore::Color& a, const PaintCore::Color& b, float t)
{
	auto channel = [t](uint8_t av, uint8_t bv)
	{
		return static_cast<uint8_t>(std::lround(av + (bv - av) * t));
	};
	return PaintCore::Color{
		channel(a.r, b.r),
		channel(a.g, b.g),
		channel(a.b, b.b),
		255,
	};
}

PaintCore::Color tone_color(PaletteMode mode, float t)
{
	const PaintCore::Color white{255, 255, 255, 255};
	const PaintCore::Color black{0, 0, 0, 255};

	if (mode == PaletteMode::Dmg)
	{
		const Palette dmg = Palette::dmg();
		const float position = t * 3.0f;
		const int lower = std::clamp(static_cast<int>(position), 0, 2);
		return mix(dmg.getColor(static_cast<ColorIndex>(lower)),
		           dmg.getColor(static_cast<ColorIndex>(lower + 1)),
		           position - static_cast<float>(lower));
	}

	if (mode == PaletteMode::Grayscale)
	{
		const uint8_t value = static_cast<uint8_t>(
			std::lround(255.0f * (1.0f - t)));
		return {value, value, value, 255};
	}

	PaintCore::Color base;
	switch (mode)
	{
		case PaletteMode::Red:   base = {255, 0, 0, 255}; break;
		case PaletteMode::Green: base = {0, 255, 0, 255}; break;
		case PaletteMode::Blue:  base = {0, 0, 255, 255}; break;
		case PaletteMode::Dmg:
		case PaletteMode::Grayscale:
			break;
	}

	// White highlight, pure tone in the middle, black shadow.
	return t <= 0.5f
		? mix(white, base, t * 2.0f)
		: mix(base, black, (t - 0.5f) * 2.0f);
}
}

Palette::Palette(size_t bitsPerPixel)
	: m_bpp(checked_bits_per_pixel(bitsPerPixel)),
	  m_colors(std::size_t{1} << m_bpp, PaintCore::Color{})
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
	const size_t levels = palette.size();          // always >= 2
	
	for (size_t i = 0; i < levels; ++i)
	{
		const uint8_t v = static_cast<uint8_t>(i * 255 / (levels - 1));
		palette.setColor(static_cast<ColorIndex>(i), {v, v, v, 0xFF});
	}

	return palette;
}

int palette_usable_color_count(int bits_per_pixel)
{
	if (bits_per_pixel < 1 || bits_per_pixel > 8)
	{
		return 0;
	}
	const int count = 1 << bits_per_pixel;
	return bits_per_pixel == 8 ? 255 : count;
}

int palette_visual_color_count(int bits_per_pixel)
{
	return std::min(palette_usable_color_count(bits_per_pixel),
	                kPaletteMaxVisualColors);
}

int palette_visual_color_index(int slot, int bits_per_pixel)
{
	const int usable = palette_usable_color_count(bits_per_pixel);
	const int visual = palette_visual_color_count(bits_per_pixel);
	if (usable <= 0 || visual <= 0 || slot < 0 || slot >= visual)
	{
		return 0;
	}
	if (visual == 1)
	{
		return 0;
	}

	// Endpoint-inclusive sampling: slot 0 is the lightest color and slot
	// visual-1 is the darkest.
	return static_cast<int>(
		(static_cast<long long>(slot) * (usable - 1)) / (visual - 1));
}

Palette make_palette(PaletteMode mode, int bits_per_pixel, bool inverted)
{
	Palette palette(static_cast<size_t>(bits_per_pixel));
	const int usable = palette_usable_color_count(bits_per_pixel);
	if (usable <= 0)
	{
		return palette;
	}

	Color last{};
	for (int i = 0; i < usable; ++i)
	{
		const int source = inverted ? (usable - 1 - i) : i;
		const float t = usable == 1
			? 0.0f
			: static_cast<float>(source) / static_cast<float>(usable - 1);
		last = tone_color(mode, t);
		palette.setColor(static_cast<ColorIndex>(i), last);
	}

	// Entries above the usable range are never selectable. Keeping them at
	// the last color avoids the out-of-range sentinel if an index leaks in.
	for (size_t i = static_cast<size_t>(usable); i < palette.size(); ++i)
	{
		palette.setColor(static_cast<ColorIndex>(i), last);
	}

	return palette;
}