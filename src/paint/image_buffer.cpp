#include "paint/image_buffer.h"

ImageBuffer::ImageBuffer(int w, int h, int bpp) :
m_width(w),
m_height(h),
m_bits_per_pixel(bpp)
{
	if (m_width < 0 || m_height < 0)
	{
		throw std::invalid_argument("Image dimensions must be non-negative");
	}
	if (m_bits_per_pixel < 1 || m_bits_per_pixel > 8)
	{
		throw std::invalid_argument("Bits per pixel must be in the range 1..8");
	}
	allocate();
}

void ImageBuffer::allocate()
{
	const std::size_t pixel_count =
		static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
	m_data.assign(pixel_count, 0);
	mark_all_dirty();
}

void ImageBuffer::mark_dirty(int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
	{
		return;
	}

	const int x0 = std::max(0, x);
	const int y0 = std::max(0, y);
	const int x1 = std::min(m_width, x + w);
	const int y1 = std::min(m_height, y + h);

	if (x0 >= x1 || y0 >= y1)
	{
		return;
	}

	m_dirty.unite({x0, y0, x1 - x0, y1 - y0});
}

uint8_t ImageBuffer::get_pixel(int x, int y) const
{
	if(x < 0 || y < 0 || x >= m_width || y >= m_height)
	{
		return 0;
	}
	return m_data[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
				  static_cast<std::size_t>(x)];
}

void ImageBuffer::set_pixel(int x, int y, PaintCore::ColorIndex value)
{
	if(x < 0 || y < 0 || x >= m_width || y >= m_height)
	{
		return;
	}
	m_data[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
		   static_cast<std::size_t>(x)] = value;
	mark_dirty(x, y, 1, 1);
}

void ImageBuffer::fill(PaintCore::ColorIndex value)
{
	std::fill(m_data.begin(), m_data.end(), value);
	mark_all_dirty();
}

ImageBuffer ImageBuffer::sub_buffer(int startX, int startY, int w, int h) const
{
	ImageBuffer sub(w, h, m_bits_per_pixel);
	for(int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			sub.set_pixel(x, y, get_pixel(startX + x, startY + y));
		}
	}
	return sub;
}

void ImageBuffer::resize(int newW, int newH)
{
	if (newW < 0 || newH < 0)
	{
		throw std::invalid_argument("Image dimensions must be non-negative");
	}

	const std::size_t pixel_count =
		static_cast<std::size_t>(newW) * static_cast<std::size_t>(newH);
	std::vector<PaintCore::ColorIndex> newData(pixel_count, 0);
	for (int y = 0; y < std::min(m_height, newH); ++y)
	{
		for (int x = 0; x < std::min(m_width, newW); ++x)
		{
			newData[static_cast<std::size_t>(y) * static_cast<std::size_t>(newW) +
					static_cast<std::size_t>(x)] = get_pixel(x, y);
		}
	}
	m_width = newW;
	m_height = newH;
	m_data.swap(newData);
	mark_all_dirty();
}

void ImageBuffer::copy_from(const ImageBuffer &src, int dstX, int dstY)
{
	if(src.bitsPerPixel() != bitsPerPixel())
	{
		throw std::invalid_argument("Mismatched bits per pixel");
	}
	for(int y = 0; y < src.height(); ++y)
	{
		for (int x = 0; x < src.width(); ++x)
		{
			set_pixel(dstX + x, dstY + y, src.get_pixel(x, y));
		}
	}
}

std::vector<PaintCore::ColorIndex> ImageBuffer::read_region(const PaintCore::Rect& rect) const
{
	if (rect.empty() || rect.x < 0 || rect.y < 0 ||
		rect.right() > m_width || rect.bottom() > m_height)
	{
		return {};
	}

	std::vector<PaintCore::ColorIndex> result(
		static_cast<std::size_t>(rect.w) * static_cast<std::size_t>(rect.h));

	for (int y = 0; y < rect.h; ++y)
	{
		const PaintCore::ColorIndex* src =
			m_data.data() + static_cast<std::size_t>(rect.y + y) * static_cast<std::size_t>(m_width) +
			static_cast<std::size_t>(rect.x);
		PaintCore::ColorIndex* dst = result.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(rect.w);
		std::copy_n(src, rect.w, dst);
	}

	return result;
}

void ImageBuffer::write_region(const PaintCore::Rect& rect, const PaintCore::ColorIndex* src)
{
	if (!src || rect.empty() || rect.x < 0 || rect.y < 0 ||
		rect.right() > m_width || rect.bottom() > m_height)
	{
		return;
	}

	for (int y = 0; y < rect.h; ++y)
	{
		const PaintCore::ColorIndex* row_src =
			src + static_cast<std::size_t>(y) * static_cast<std::size_t>(rect.w);
		PaintCore::ColorIndex* row_dst =
			m_data.data() + static_cast<std::size_t>(rect.y + y) * static_cast<std::size_t>(m_width) +
			static_cast<std::size_t>(rect.x);
		std::copy_n(row_src, rect.w, row_dst);
	}

	mark_dirty(rect.x, rect.y, rect.w, rect.h);
}