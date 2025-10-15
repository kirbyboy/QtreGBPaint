#ifndef QTREGBPAINT_IMAGE_BUFFER_H
#define QTREGBPAINT_IMAGE_BUFFER_H

#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>

class ImageBuffer
{
	public:
		int width;
		int height;
		int bitsPerPixel; // e.g., 1, 2, 4, 8, 24, 32
		std::vector<uint8_t> data; // 1 byte per pixel (unpacked)

		ImageBuffer(int w = 0, int h = 0, int bpp = 8);
		void allocate();

		uint8_t getPixel(int x, int y) const;
		void setPixel(int x, int y, uint8_t value);

		void fill(uint8_t value);
		ImageBuffer subBuffer(int startX, int startY, int w, int h) const;
		void resize(int newW, int newH);
		void copyFrom(const ImageBuffer& src, int dstX, int dstY);
};


#endif