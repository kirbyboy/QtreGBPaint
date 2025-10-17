#ifndef QTREGBPAINT_PALETTE_H
#define QTREGBPAINT_PALETTE_H

#include <vector>
#include <cstdint>

#include <paint/color.h>

class Palette
{
	public:
		Palette(size_t size);
		void setColor(uint8_t index, PaintCore::Color color);
		PaintCore::Color getColor(uint8_t index) const;
		size_t size() const;

	private:
		size_t m_size;
		std::vector<PaintCore::Color> m_colors;
};

#endif