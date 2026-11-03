#ifndef QTREGBPAINT_PALETTE_H
#define QTREGBPAINT_PALETTE_H

#include <vector>
#include <cstdint>

#include <core/color.h>

class Palette
{
	public:
		Palette(size_t bitsPerPixel);
		void setColor(PaintCore::ColorIndex index, PaintCore::Color color);
		PaintCore::Color getColor(PaintCore::ColorIndex index) const;
		size_t size() const;            // número de colores (1 << bpp)
		size_t bitsPerPixel() const;

		static Palette dmg();                    // 2bpp, los 4 tonos de Game Boy
		static Palette grayscale(size_t bits);		

	private:
		size_t m_bpp;
		std::vector<PaintCore::Color> m_colors;
};

#endif
