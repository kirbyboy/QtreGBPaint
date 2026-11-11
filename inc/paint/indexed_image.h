#ifndef QTREGBPAINT_INDEXED_IMAGE_H
#define QTREGBPAINT_INDEXED_IMAGE_H

#include <cstddef>
#include <string>
#include <vector>

#include "core/color.h"
#include "paint/file_error.h"

// Storage encodings for the .idx file format. The image model is always one
// byte per pixel; these modes only change how the payload is written/read.
enum class IndexedFormat
{
	Index,
	Binary,
	Packed,
	BinaryPacked,
};

const char* indexed_format_name(IndexedFormat format);

// Number of bytes in one packed row, before text/hex expansion.
std::size_t indexed_image_row_bytes(int width, int bits_per_pixel);

// Approximate payload size for the selected encoding. `Index` uses the maximum
// decimal width for the bpp, so it is an upper bound, not an exact size.
std::size_t indexed_image_data_size(int width, int height, int bits_per_pixel,
                                    IndexedFormat format);

// Payload size plus a small average header allowance (64 bytes). This is the
// value shown in the Save As options.
std::size_t indexed_image_estimated_file_size(int width, int height,
                                               int bits_per_pixel,
                                               IndexedFormat format);

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
// Index mode:
//   One decimal value per pixel, in the range 0..2^bpp-1, separated by spaces.
//   Rows are written on separate lines, although the reader accepts any
//   whitespace separation.
//
// Other modes:
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
    FileError errorCode() const { return m_error_code; }

    int width() const { return m_width; }
    int height() const { return m_height; }
    int bitsPerPixel() const { return m_bpp; }
    IndexedFormat format() const { return m_format; }
    std::size_t expectedSize() const
    {
        return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    }

    const std::vector<PaintCore::ColorIndex>& data() const { return m_data; }

private:
    int m_width = 0;
    int m_height = 0;
    int m_bpp = 0;
    IndexedFormat m_format = IndexedFormat::Index;
    std::vector<PaintCore::ColorIndex> m_data;
    bool m_valid = false;
    std::string m_error;
    FileError m_error_code = FileError::None;
};

// Writes an IndexedImage with the requested encoding without constructing an
// intermediate copy. Returns false and fills `error` on failure.
bool save_indexed_image(const std::string& path, int width, int height,
                        int bits_per_pixel,
                        const std::vector<PaintCore::ColorIndex>& data,
                        IndexedFormat format, std::string& error,
                        FileError* error_code = nullptr);

// Backwards-compatible helper: writes Index mode.
bool save_indexed_image(const std::string& path, int width, int height,
                        int bits_per_pixel,
                        const std::vector<PaintCore::ColorIndex>& data,
                        std::string& error, FileError* error_code = nullptr);

#endif
