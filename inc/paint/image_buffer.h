#ifndef QTREGBPAINT_IMAGE_BUFFER_H
#define QTREGBPAINT_IMAGE_BUFFER_H

#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <core/color.h>
#include <core/rect.h>

class ImageBuffer
{
	public:
		// `bpp` es el tamaño de paleta del formato (1..8 bits). El almacenamiento
		// sigue siendo un byte por píxel; el bpp solo describe cuántos índices
		// tienen sentido para la imagen.
		ImageBuffer(int w = 0, int h = 0, int bpp = 8);

		int width() const { return m_width; }
		int height() const { return m_height; }
		int bitsPerPixel() const { return m_bits_per_pixel; }
		const std::vector<PaintCore::ColorIndex>& data() const { return m_data; }

		PaintCore::ColorIndex get_pixel(int x, int y) const;
		void set_pixel(int x, int y, PaintCore::ColorIndex value);

		void fill(PaintCore::ColorIndex value);
		ImageBuffer sub_buffer(int startX, int startY, int w, int h) const;
		void resize(int newW, int newH);
		void copy_from(const ImageBuffer& src, int dstX, int dstY);

		// Copia una región contigua (fila a fila). El rect debe estar dentro
		// del buffer; los rects fuera de rango se rechazan.
		std::vector<PaintCore::ColorIndex> read_region(const PaintCore::Rect& rect) const;
		void write_region(const PaintCore::Rect& rect, const PaintCore::ColorIndex* src);

		// Dirty-region tracking: consumers (texture upload, partial redraw,
		// undo, save...) can update only the pixels that actually changed.
		bool has_dirty_rect() const { return !m_dirty.empty(); }
		const PaintCore::Rect& dirty_rect() const { return m_dirty; }
		void clear_dirty_rect() { m_dirty.reset(); }
		void mark_all_dirty() { m_dirty = {0, 0, m_width, m_height}; }

	private:
		int m_width = 0;
		int m_height = 0;
		int m_bits_per_pixel = 8;
		std::vector<PaintCore::ColorIndex> m_data; // 1 byte per pixel (unpacked)
		PaintCore::Rect m_dirty{};

		void allocate();
		void mark_dirty(int x, int y, int w, int h);
};


#endif