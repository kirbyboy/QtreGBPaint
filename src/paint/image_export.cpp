#include "paint/image_export.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <ostream>
#include <vector>

#include <zlib.h>

namespace
{
void write_u16(std::ostream& output, std::uint16_t value)
{
	const char bytes[2] = {
		static_cast<char>(value & 0xFF),
		static_cast<char>((value >> 8) & 0xFF),
	};
	output.write(bytes, sizeof(bytes));
}

void write_u32(std::ostream& output, std::uint32_t value)
{
	const char bytes[4] = {
		static_cast<char>(value & 0xFF),
		static_cast<char>((value >> 8) & 0xFF),
		static_cast<char>((value >> 16) & 0xFF),
		static_cast<char>((value >> 24) & 0xFF),
	};
	output.write(bytes, sizeof(bytes));
}

void append_u32_be(std::vector<std::uint8_t>& output, std::uint32_t value)
{
	output.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
	output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
	output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
	output.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void update_crc(uLong& crc, const std::uint8_t* data, std::size_t size)
{
	while (size > 0)
	{
		const uInt step = static_cast<uInt>(
			std::min<std::size_t>(size, std::numeric_limits<uInt>::max()));
		crc = crc32(crc, reinterpret_cast<const Bytef*>(data), step);
		data += step;
		size -= step;
	}
}

void append_chunk(std::vector<std::uint8_t>& output, const char type[4],
                  const std::uint8_t* data, std::size_t size)
{
	if (size > std::numeric_limits<std::uint32_t>::max())
	{
		return; // callers validate sizes before getting here
	}

	append_u32_be(output, static_cast<std::uint32_t>(size));
	output.insert(output.end(), type, type + 4);
	if (size > 0)
	{
		output.insert(output.end(), data, data + size);
	}

	uLong crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, reinterpret_cast<const Bytef*>(type), 4);
	if (size > 0)
	{
		update_crc(crc, data, size);
	}
	append_u32_be(output, static_cast<std::uint32_t>(crc));
}

bool fail_export(std::string& error, FileError* error_code, FileError code,
                 const std::string& message)
{
	error = message;
	if (error_code)
	{
		*error_code = code;
	}
	return false;
}

bool validate_export(const ImageBuffer& image, const Palette& palette,
                     std::string& error, FileError* error_code)
{
	error.clear();
	if (error_code)
	{
		*error_code = FileError::None;
	}
	if (image.width() <= 0 || image.height() <= 0)
	{
		return fail_export(error, error_code, FileError::InvalidDimensions,
		                   "Image dimensions must be positive");
	}
	if (image.bitsPerPixel() < 1 || image.bitsPerPixel() > 8)
	{
		return fail_export(error, error_code, FileError::InvalidBitsPerPixel,
		                   "Bits per pixel must be in the range 1..8");
	}
	const std::size_t expected_palette_size =
		static_cast<std::size_t>(1u << image.bitsPerPixel());
	if (palette.size() != expected_palette_size)
	{
		return fail_export(error, error_code, FileError::InvalidPalette,
		                   "Palette size does not match the image bpp");
	}
	const std::size_t expected_pixels =
		static_cast<std::size_t>(image.width()) *
		static_cast<std::size_t>(image.height());
	if (image.data().size() != expected_pixels)
	{
		return fail_export(error, error_code, FileError::InvalidImageData,
		                   "Pixel count does not match width*height");
	}
	for (PaintCore::ColorIndex index : image.data())
	{
		if (index >= palette.size())
		{
			return fail_export(error, error_code, FileError::InvalidPalette,
			                   "An index is outside the palette");
		}
	}
	return true;
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes,
                std::string& error, FileError* error_code)
{
	std::ofstream output(path, std::ios::out | std::ios::binary |
	                              std::ios::trunc);
	if (!output)
	{
		return fail_export(error, error_code, FileError::WriteFailed,
		                   "Could not open the file for writing");
	}
	if (!bytes.empty())
	{
		output.write(reinterpret_cast<const char*>(bytes.data()),
		             static_cast<std::streamsize>(bytes.size()));
	}
	if (!output.good())
	{
		return fail_export(error, error_code, FileError::WriteFailed,
		                   "Failed to write the file");
	}
	return true;
}
}

bool export_bmp(const std::string& path, const ImageBuffer& image,
                const Palette& palette, std::string& error,
                FileError* error_code)
{
	if (error_code)
	{
		*error_code = FileError::None;
	}
	if (path.empty())
	{
		return fail_export(error, error_code, FileError::NoPath,
		                   "No file path");
	}
	if (!validate_export(image, palette, error, error_code))
	{
		return false;
	}

	const std::uint64_t row_bytes =
		((static_cast<std::uint64_t>(image.width()) * 3u + 3u) / 4u) * 4u;
	const std::uint64_t pixel_bytes =
		row_bytes * static_cast<std::uint64_t>(image.height());
	const std::uint64_t file_size = 54u + pixel_bytes;
	if (file_size > std::numeric_limits<std::uint32_t>::max() ||
	    pixel_bytes > std::numeric_limits<std::uint32_t>::max())
	{
		return fail_export(error, error_code, FileError::TooLarge,
		                   "Image is too large for a BMP file");
	}

	std::ofstream output(path, std::ios::out | std::ios::binary |
	                              std::ios::trunc);
	if (!output)
	{
		return fail_export(error, error_code, FileError::WriteFailed,
		                   "Could not open the file for writing");
	}

	// BITMAPFILEHEADER
	write_u16(output, 0x4D42); // "BM"
	write_u32(output, static_cast<std::uint32_t>(file_size));
	write_u16(output, 0);
	write_u16(output, 0);
	write_u32(output, 54);

	// BITMAPINFOHEADER
	write_u32(output, 40);
	write_u32(output, static_cast<std::uint32_t>(image.width()));
	write_u32(output, static_cast<std::uint32_t>(image.height()));
	write_u16(output, 1);
	write_u16(output, 24);
	write_u32(output, 0); // BI_RGB
	write_u32(output, static_cast<std::uint32_t>(pixel_bytes));
	write_u32(output, 2835); // 72 DPI
	write_u32(output, 2835);
	write_u32(output, 0);
	write_u32(output, 0);
	if (!output.good())
	{
		return fail_export(error, error_code, FileError::WriteFailed,
		                   "Failed to write the file");
	}

	const std::vector<PaintCore::ColorIndex>& data = image.data();
	std::vector<std::uint8_t> row(static_cast<std::size_t>(row_bytes), 0);
	for (int y = image.height() - 1; y >= 0; --y)
	{
		for (int x = 0; x < image.width(); ++x)
		{
			const PaintCore::Color color = palette.getStoredColor(
				data[static_cast<std::size_t>(y) *
				         static_cast<std::size_t>(image.width()) +
				         static_cast<std::size_t>(x)]);
			const std::size_t offset = static_cast<std::size_t>(x) * 3u;
			row[offset] = color.b;
			row[offset + 1] = color.g;
			row[offset + 2] = color.r;
		}
		output.write(reinterpret_cast<const char*>(row.data()),
		             static_cast<std::streamsize>(row.size()));
		if (!output.good())
		{
			return fail_export(error, error_code, FileError::WriteFailed,
			                   "Failed to write the file");
		}
	}

	return true;
}

bool export_png(const std::string& path, const ImageBuffer& image,
                const Palette& palette, std::string& error,
                FileError* error_code)
{
	if (error_code)
	{
		*error_code = FileError::None;
	}
	if (path.empty())
	{
		return fail_export(error, error_code, FileError::NoPath,
		                   "No file path");
	}
	if (!validate_export(image, palette, error, error_code))
	{
		return false;
	}
	if (palette.size() > 256)
	{
		return fail_export(error, error_code, FileError::InvalidPalette,
		                   "PNG palettes cannot contain more than 256 colors");
	}

	const std::size_t row_size = static_cast<std::size_t>(image.width()) + 1u;
	if (static_cast<std::size_t>(image.height()) >
	    std::numeric_limits<std::size_t>::max() / row_size)
	{
		return fail_export(error, error_code, FileError::TooLarge,
		                   "Image is too large for a PNG file");
	}

	std::vector<std::uint8_t> raw;
	raw.reserve(static_cast<std::size_t>(image.height()) * row_size);
	const std::vector<PaintCore::ColorIndex>& data = image.data();
	for (int y = 0; y < image.height(); ++y)
	{
		raw.push_back(0); // filter: None
		const std::size_t offset =
			static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width());
		raw.insert(raw.end(), data.begin() + offset,
		           data.begin() + offset + image.width());
	}

	uLongf compressed_size = compressBound(static_cast<uLong>(raw.size()));
	std::vector<std::uint8_t> compressed(compressed_size);
	const int result = compress2(
		compressed.data(), &compressed_size, raw.data(),
		static_cast<uLong>(raw.size()), Z_DEFAULT_COMPRESSION);
	if (result != Z_OK)
	{
		return fail_export(error, error_code, FileError::CompressionFailed,
		                   "Could not compress the PNG data");
	}
	compressed.resize(compressed_size);

	std::vector<std::uint8_t> png;
	png.reserve(compressed.size() + 256 + palette.size() * 3);
	static const std::uint8_t signature[8] = {
		0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	png.insert(png.end(), signature, signature + 8);

	std::uint8_t ihdr[13] = {};
	const std::uint32_t width = static_cast<std::uint32_t>(image.width());
	const std::uint32_t height = static_cast<std::uint32_t>(image.height());
	ihdr[0] = static_cast<std::uint8_t>((width >> 24) & 0xFF);
	ihdr[1] = static_cast<std::uint8_t>((width >> 16) & 0xFF);
	ihdr[2] = static_cast<std::uint8_t>((width >> 8) & 0xFF);
	ihdr[3] = static_cast<std::uint8_t>(width & 0xFF);
	ihdr[4] = static_cast<std::uint8_t>((height >> 24) & 0xFF);
	ihdr[5] = static_cast<std::uint8_t>((height >> 16) & 0xFF);
	ihdr[6] = static_cast<std::uint8_t>((height >> 8) & 0xFF);
	ihdr[7] = static_cast<std::uint8_t>(height & 0xFF);
	ihdr[8] = 8;  // bit depth
	ihdr[9] = 3;  // color type: indexed
	ihdr[10] = 0; // compression
	ihdr[11] = 0; // filter
	ihdr[12] = 0; // interlace
	append_chunk(png, "IHDR", ihdr, sizeof(ihdr));

	std::vector<std::uint8_t> plte;
	plte.reserve(palette.size() * 3);
	for (std::size_t index = 0; index < palette.size(); ++index)
	{
		const PaintCore::Color color = palette.getStoredColor(
			static_cast<PaintCore::ColorIndex>(index));
		plte.push_back(color.r);
		plte.push_back(color.g);
		plte.push_back(color.b);
	}
	append_chunk(png, "PLTE", plte.data(), plte.size());

	if (const std::optional<PaintCore::ColorIndex> transparent =
	        palette.transparent_index())
	{
		if (*transparent < palette.size())
		{
			std::vector<std::uint8_t> trns(
				static_cast<std::size_t>(*transparent) + 1u, 255);
			trns[*transparent] = 0;
			append_chunk(png, "tRNS", trns.data(), trns.size());
		}
	}

	append_chunk(png, "IDAT", compressed.data(), compressed.size());
	append_chunk(png, "IEND", nullptr, 0);

	return write_file(path, png, error, error_code);
}

bool export_image(const std::string& path, ImageExportFormat format,
                  const ImageBuffer& image, const Palette& palette,
                  std::string& error, FileError* error_code)
{
	if (error_code)
	{
		*error_code = FileError::None;
	}
	switch (format)
	{
		case ImageExportFormat::Bmp:
			return export_bmp(path, image, palette, error, error_code);
		case ImageExportFormat::Png:
			return export_png(path, image, palette, error, error_code);
	}
	return fail_export(error, error_code, FileError::UnsupportedFormat,
	                   "Unknown export format");
}
