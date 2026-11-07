#ifndef QTREGBPAINT_PALETTE_H
#define QTREGBPAINT_PALETTE_H

#include <vector>
#include <cstdint>

#include <core/color.h>

class Palette
{
	public:
		// `bitsPerPixel` must be in 1..8: the palette size is 2^bpp and
		// ImageBuffer indices are 8-bit.
		Palette(size_t bitsPerPixel);
		void setColor(PaintCore::ColorIndex index, PaintCore::Color color);
		PaintCore::Color getColor(PaintCore::ColorIndex index) const;
		size_t size() const;            // number of colors (1 << bpp)
		size_t bitsPerPixel() const;

		static Palette dmg();                    // 2bpp, the four Game Boy shades
		static Palette grayscale(size_t bits);		

	private:
		size_t m_bpp;
		std::vector<PaintCore::Color> m_colors;
};

#endif
