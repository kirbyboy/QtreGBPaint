#ifndef QTREGBPAINT_IMAGE_EXPORT_H
#define QTREGBPAINT_IMAGE_EXPORT_H

#include <string>

#include "paint/file_error.h"
#include "paint/image_buffer.h"
#include "paint/palette.h"

enum class ImageExportFormat
{
	Bmp,
	Png,
};

// BMP is written as uncompressed 24-bit BGR (bottom-up, rows padded to 4
// bytes). The palette's stored color is used for the transparent index because
// BMP has no alpha channel.
bool export_bmp(const std::string& path, const ImageBuffer& image,
                const Palette& palette, std::string& error,
                FileError* error_code = nullptr);

// PNG is written as an 8-bit indexed image (color type 3): PLTE contains the
// palette colors and tRNS marks the optional transparent index. IDAT contains
// zlib-compressed scanlines with filter 0.
bool export_png(const std::string& path, const ImageBuffer& image,
                const Palette& palette, std::string& error,
                FileError* error_code = nullptr);

bool export_image(const std::string& path, ImageExportFormat format,
                  const ImageBuffer& image, const Palette& palette,
                  std::string& error, FileError* error_code = nullptr);

#endif
