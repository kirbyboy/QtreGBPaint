#ifndef QTREGBPAINT_INDEXED_IMAGE_H
#define QTREGBPAINT_INDEXED_IMAGE_H

#include <cstddef>
#include <string>
#include <vector>

#include "core/color.h"

// Indexed image and .idx file format.
//
// Header:
//   IndexedImage [Index]
//   <width> <height>
//   <bpp>
//
// The first line contains the "IndexedImage" magic word and, optionally, the
// mode. If the second word is absent, "Index" is assumed.
//
// Index mode (implemented):
//   One decimal value per pixel, in the range 0..2^bpp-1, separated by spaces.
//   Rows are written on separate lines, although the reader accepts any
//   whitespace separation.
//
// Future modes (documented, not implemented yet):
//   Binary:
//     One byte per pixel, exactly width*height bytes after the header.
//   Packed:
//     Row-wise packing, MSB-first with zero padding. Each byte is written as
//     two hexadecimal digits; one row per line. The number of bytes per row is
//     ceil(width*bpp/8).
//   BinaryPacked:
//     The same packed payload as Packed, but as raw bytes, with no separators
//     or trailing newline. The exact size is height*ceil(width*bpp/8) bytes
//     after the header.
//
// Binary modes do not allow extra bytes after the payload.
class IndexedImage
{
public:
    IndexedImage() = default;
    IndexedImage(int width, int height, int bitsPerPixel,
                 std::vector<PaintCore::ColorIndex> data);

    // Loads and validates a file from disk. Currently only Index is supported.
    static IndexedImage loadFromFile(const std::string& path);

    bool isValid() const { return m_valid; }
    const std::string& error() const { return m_error; }

    int width() const { return m_width; }
    int height() const { return m_height; }
    int bitsPerPixel() const { return m_bpp; }
    std::size_t expectedSize() const
    {
        return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    }

    const std::vector<PaintCore::ColorIndex>& data() const { return m_data; }

private:
    int m_width = 0;
    int m_height = 0;
    int m_bpp = 0;
    std::vector<PaintCore::ColorIndex> m_data;
    bool m_valid = false;
    std::string m_error;
};

#endif
