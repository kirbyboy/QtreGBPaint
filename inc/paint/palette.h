#ifndef QTREGBPAINT_PALETTE_H
#define QTREGBPAINT_PALETTE_H

#include <vector>
#include <cstdint>

#include <core/color.h>

enum class PaletteMode
{
	Dmg,
	Grayscale,
	Red,
	Green,
	Blue,
};

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
		static Palette grayscale(size_t bits);   // black-to-white ramp for icons

	private:
		size_t m_bpp;
		std::vector<PaintCore::Color> m_colors;
};

// The visual palette panel never shows more than this many swatches.
inline constexpr int kPaletteMaxVisualColors = 32;

// Number of palette entries that can represent colors. Index 255 is reserved
// for transparency, so 8bpp exposes 255 usable colors instead of 256.
int palette_usable_color_count(int bits_per_pixel);

// Number of swatches to display: the usable colors, capped at 32.
int palette_visual_color_count(int bits_per_pixel);

// Actual palette index represented by visual slot [0, visual_count). When the
// palette is larger than 32, indices are sampled evenly so the first swatch is
// the lightest color and the last one is the darkest.
int palette_visual_color_index(int slot, int bits_per_pixel);

// Builds a palette for a tone. Usable colors go from lightest to darkest;
// `inverted` reverses that order.
Palette make_palette(PaletteMode mode, int bits_per_pixel, bool inverted);

#endif
