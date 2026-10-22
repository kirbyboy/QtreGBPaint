#ifndef QTREGBPAINT_IMAGE_BUFFER_H
#define QTREGBPAINT_IMAGE_BUFFER_H

#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <core/color.h>

class ImageBuffer
{
	public:
		int width;
		int height;
		int bitsPerPixel; // e.g., 1, 2, 4, 8, 24, 32
		std::vector<PaintCore::ColorIndex> data; // 1 byte per pixel (unpacked)

		ImageBuffer(int w = 0, int h = 0, int bpp = 8);
		void allocate();

		PaintCore::ColorIndex get_pixel(int x, int y) const;
		void set_pixel(int x, int y, PaintCore::ColorIndex value);

		void fill(PaintCore::ColorIndex value);
		ImageBuffer sub_buffer(int startX, int startY, int w, int h) const;
		void resize(int newW, int newH);
		void copy_from(const ImageBuffer& src, int dstX, int dstY);
};


#endif