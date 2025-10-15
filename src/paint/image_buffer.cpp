#include "paint/image_buffer.h"

ImageBuffer::ImageBuffer(int w, int h, int bpp) : 
width(w), 
height(h), 
bitsPerPixel(bpp)
{
	if (bitsPerPixel <= 0 || bitsPerPixel > 32)
	{
		throw std::invalid_argument("Unsupported bits per pixel");
	}
	allocate();
}

void ImageBuffer::allocate()
{
	data.assign(width * height, 0);
}
uint8_t ImageBuffer::getPixel(int x, int y) const
{
	if(x < 0 || y < 0 || x >= width || y >= height)
	{
		return 0;
	}
	return data[y * width + x];
}

void ImageBuffer::setPixel(int x, int y, uint8_t value)
{
	if(x < 0 || y < 0 || x >= width || y >= height)
	{
		return;
	}
	data[y * width + x] = value;
}

void ImageBuffer::fill(uint8_t value)
{
	std::fill(data.begin(), data.end(), value);
}

ImageBuffer ImageBuffer::subBuffer(int startX, int startY, int w, int h) const
{
	ImageBuffer sub(w, h, bitsPerPixel);
	for(int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			sub.setPixel(x, y, getPixel(startX + x, startY + y));
		}
	}
	return sub;
}

void ImageBuffer::resize(int newW, int newH)
{
		std::vector<uint8_t> newData(newW * newH, 0);
		for (int y = 0; y < std::min(height, newH); ++y)
			for (int x = 0; x < std::min(width, newW); ++x)
				newData[y * newW + x] = getPixel(x, y);
		width = newW;
		height = newH;
		data.swap(newData);
}
void ImageBuffer::copyFrom(const ImageBuffer &src, int dstX, int dstY)
{
	if(src.bitsPerPixel != bitsPerPixel)
	{
		throw std::invalid_argument("Mismatched bits per pixel");
	}
	for(int y = 0; y < src.height; ++y)
	{
		for (int x = 0; x < src.width; ++x)
		{
			setPixel(dstX + x, dstY + y, src.getPixel(x, y));
		}
	}
}