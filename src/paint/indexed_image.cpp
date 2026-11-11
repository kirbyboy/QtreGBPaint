#include "paint/indexed_image.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
bool parse_int(const std::string& token, int& value)
{
	const char* first = token.data();
	const char* last = first + token.size();
	const auto result = std::from_chars(first, last, value);
	return result.ec == std::errc{} && result.ptr == last;
}

void strip_carriage_return(std::string& line)
{
	if (!line.empty() && line.back() == '\r')
	{
		line.pop_back();
	}
}

const char* format_name(IndexedFormat format)
{
	switch (format)
	{
		case IndexedFormat::Index:
			return "Index";
		case IndexedFormat::Binary:
			return "Binary";
		case IndexedFormat::Packed:
			return "Packed";
		case IndexedFormat::BinaryPacked:
			return "BinaryPacked";
	}
	return "Index";
}

bool parse_format(const std::string& text, IndexedFormat& format)
{
	if (text == "Index")
	{
		format = IndexedFormat::Index;
		return true;
	}
	if (text == "Binary")
	{
		format = IndexedFormat::Binary;
		return true;
	}
	if (text == "Packed")
	{
		format = IndexedFormat::Packed;
		return true;
	}
	if (text == "BinaryPacked")
	{
		format = IndexedFormat::BinaryPacked;
		return true;
	}
	return false;
}

int hex_value(char character)
{
	if (character >= '0' && character <= '9')
	{
		return character - '0';
	}
	if (character >= 'a' && character <= 'f')
	{
		return character - 'a' + 10;
	}
	if (character >= 'A' && character <= 'F')
	{
		return character - 'A' + 10;
	}
	return -1;
}

std::string compact_whitespace(const std::string& text)
{
	std::string result;
	result.reserve(text.size());
	for (unsigned char character : text)
	{
		if (!std::isspace(character))
		{
			result.push_back(static_cast<char>(character));
		}
	}
	return result;
}

bool pack_indices(const std::vector<PaintCore::ColorIndex>& data,
                  int width, int height, int bits_per_pixel,
                  std::vector<std::uint8_t>& packed)
{
	if (width <= 0 || height <= 0 || bits_per_pixel < 1 ||
	    bits_per_pixel > 8)
	{
		return false;
	}
	const std::size_t row_bytes = static_cast<std::size_t>(
		(static_cast<std::uint64_t>(width) * bits_per_pixel + 7u) / 8u);
	const std::size_t total = row_bytes * static_cast<std::size_t>(height);
	packed.assign(total, 0);

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const unsigned value = data[static_cast<std::size_t>(y) *
			                            static_cast<std::size_t>(width) +
			                            static_cast<std::size_t>(x)];
			const int bit_index = x * bits_per_pixel;
			for (int bit = 0; bit < bits_per_pixel; ++bit)
			{
				if ((value & (1u << (bits_per_pixel - 1 - bit))) != 0)
				{
					const int absolute = bit_index + bit;
					const std::size_t byte_index =
						static_cast<std::size_t>(y) * row_bytes +
						static_cast<std::size_t>(absolute / 8);
					const int shift = 7 - (absolute % 8);
					packed[byte_index] |=
						static_cast<std::uint8_t>(1u << shift);
				}
			}
		}
	}
	return true;
}

bool unpack_indices(const std::vector<std::uint8_t>& packed,
                    int width, int height, int bits_per_pixel,
                    std::vector<PaintCore::ColorIndex>& data)
{
	if (width <= 0 || height <= 0 || bits_per_pixel < 1 ||
	    bits_per_pixel > 8)
	{
		return false;
	}
	const std::size_t row_bytes = static_cast<std::size_t>(
		(static_cast<std::uint64_t>(width) * bits_per_pixel + 7u) / 8u);
	if (packed.size() != row_bytes * static_cast<std::size_t>(height))
	{
		return false;
	}

	data.assign(static_cast<std::size_t>(width) *
	                static_cast<std::size_t>(height), 0);
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const int bit_index = x * bits_per_pixel;
			unsigned value = 0;
			for (int bit = 0; bit < bits_per_pixel; ++bit)
			{
				const int absolute = bit_index + bit;
				const std::size_t byte_index =
					static_cast<std::size_t>(y) * row_bytes +
					static_cast<std::size_t>(absolute / 8);
				const int shift = 7 - (absolute % 8);
				value = (value << 1) |
				        ((packed[byte_index] >> shift) & 1u);
			}
			data[static_cast<std::size_t>(y) *
			         static_cast<std::size_t>(width) +
			         static_cast<std::size_t>(x)] =
				static_cast<PaintCore::ColorIndex>(value);
		}
	}
	return true;
}

bool validate_pixels(int width, int height, int bits_per_pixel,
                     const std::vector<PaintCore::ColorIndex>& data,
                     std::string& error)
{
	if (width <= 0 || height <= 0)
	{
		error = "Image dimensions must be positive";
		return false;
	}
	if (bits_per_pixel < 1 || bits_per_pixel > 8)
	{
		error = "Bits per pixel must be in the range 1..8";
		return false;
	}
	const std::size_t expected =
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	if (data.size() != expected)
	{
		error = "Pixel count does not match width*height";
		return false;
	}
	const unsigned max_value = 1u << bits_per_pixel;
	for (PaintCore::ColorIndex value : data)
	{
		if (value >= max_value)
		{
			error = "An index is outside the range allowed by bpp";
			return false;
		}
	}
	return true;
}
}

const char* indexed_format_name(IndexedFormat format)
{
	return format_name(format);
}

std::size_t indexed_image_row_bytes(int width, int bits_per_pixel)
{
	if (width <= 0 || bits_per_pixel < 1 || bits_per_pixel > 8)
	{
		return 0;
	}
	return static_cast<std::size_t>(
		(static_cast<std::uint64_t>(width) * bits_per_pixel + 7u) / 8u);
}

std::size_t indexed_image_data_size(int width, int height, int bits_per_pixel,
                                    IndexedFormat format)
{
	if (width <= 0 || height <= 0 || bits_per_pixel < 1 ||
	    bits_per_pixel > 8)
	{
		return 0;
	}

	const std::uint64_t pixels =
		static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
	const std::uint64_t row_bytes = static_cast<std::uint64_t>(
		indexed_image_row_bytes(width, bits_per_pixel));
	std::uint64_t result = 0;

	switch (format)
	{
		case IndexedFormat::Index:
		{
			const unsigned max_value = (1u << bits_per_pixel) - 1u;
			unsigned digits = 1;
			for (unsigned value = max_value; value >= 10; value /= 10)
			{
				++digits;
			}
			// Values plus the space/newline after each one. This is an upper
			// bound because actual values may use fewer digits.
			result = pixels * (digits + 1u);
			break;
		}
		case IndexedFormat::Binary:
			result = pixels;
			break;
		case IndexedFormat::Packed:
			result = static_cast<std::uint64_t>(height) *
			         (row_bytes * 2u + 1u);
			break;
		case IndexedFormat::BinaryPacked:
			result = static_cast<std::uint64_t>(height) * row_bytes;
			break;
	}

	const std::uint64_t limit =
		static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
	return static_cast<std::size_t>(std::min(result, limit));
}

std::size_t indexed_image_estimated_file_size(int width, int height,
                                               int bits_per_pixel,
                                               IndexedFormat format)
{
	const std::size_t data =
		indexed_image_data_size(width, height, bits_per_pixel, format);
	constexpr std::size_t kAverageHeader = 64;
	if (data > std::numeric_limits<std::size_t>::max() - kAverageHeader)
	{
		return std::numeric_limits<std::size_t>::max();
	}
	return data + kAverageHeader;
}

IndexedImage::IndexedImage(int width, int height, int bitsPerPixel,
                           std::vector<PaintCore::ColorIndex> data)
	: m_width(width),
	  m_height(height),
	  m_bpp(bitsPerPixel),
	  m_data(std::move(data))
{
	if (m_width <= 0 || m_height <= 0)
	{
		throw std::invalid_argument("Image dimensions must be positive");
	}
	if (m_bpp < 1 || m_bpp > 8)
	{
		throw std::invalid_argument("Bits per pixel must be in the range 1..8");
	}

	const std::size_t expected =
		static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
	if (m_data.size() != expected)
	{
		throw std::invalid_argument("Pixel count does not match width*height");
	}

	const unsigned max_value = 1u << m_bpp;
	for (const PaintCore::ColorIndex value : m_data)
	{
		if (value >= max_value)
		{
			throw std::invalid_argument("An index is outside the range allowed by bpp");
		}
	}

	m_valid = true;
}

IndexedImage IndexedImage::loadFromFile(const std::string& path)
{
	auto fail = [](const std::string& message)
	{
		IndexedImage image;
		image.m_error = message;
		return image;
	};

	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return fail("Could not open the file");
	}

	std::string contents((std::istreambuf_iterator<char>(file)),
	                     std::istreambuf_iterator<char>());
	std::size_t position = 0;
	auto next_line = [&](std::string& line) -> bool
	{
		if (position >= contents.size())
		{
			return false;
		}
		const std::size_t end = contents.find('\n', position);
		if (end == std::string::npos)
		{
			line = contents.substr(position);
			position = contents.size();
		}
		else
		{
			line = contents.substr(position, end - position);
			position = end + 1;
		}
		strip_carriage_return(line);
		return true;
	};

	std::string header;
	if (!next_line(header))
	{
		return fail("Empty file");
	}

	std::istringstream header_stream(header);
	std::string magic;
	if (!(header_stream >> magic) || magic != "IndexedImage")
	{
		return fail("Invalid header: expected 'IndexedImage'");
	}

	IndexedFormat format = IndexedFormat::Index;
	std::string mode;
	if (header_stream >> mode)
	{
		if (!parse_format(mode, format))
		{
			return fail("Invalid mode: " + mode);
		}
	}
	std::string extra;
	if (header_stream >> extra)
	{
		return fail("Invalid header: extra tokens");
	}

	std::string dimensions_line;
	if (!next_line(dimensions_line))
	{
		return fail("Incomplete header: width or height is missing");
	}
	std::istringstream dimensions_stream(dimensions_line);
	std::string width_token;
	std::string height_token;
	if (!(dimensions_stream >> width_token >> height_token))
	{
		return fail("Incomplete header: width or height is missing");
	}
	if (dimensions_stream >> extra)
	{
		return fail("Invalid header: extra tokens");
	}

	int width = 0;
	int height = 0;
	if (!parse_int(width_token, width) || width <= 0)
	{
		return fail("Invalid width");
	}
	if (!parse_int(height_token, height) || height <= 0)
	{
		return fail("Invalid height");
	}

	std::string bpp_line;
	if (!next_line(bpp_line))
	{
		return fail("Incomplete header: bpp is missing");
	}
	std::istringstream bpp_stream(bpp_line);
	std::string bpp_token;
	if (!(bpp_stream >> bpp_token) || (bpp_stream >> extra))
	{
		return fail("Invalid bpp");
	}
	int bpp = 0;
	if (!parse_int(bpp_token, bpp) || bpp < 1 || bpp > 8)
	{
		return fail("Invalid bpp: must be in the range 1..8");
	}

	const std::size_t expected =
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	const unsigned max_value = 1u << bpp;

	std::vector<PaintCore::ColorIndex> data;
	if (format == IndexedFormat::Index)
	{
		std::istringstream in(contents.substr(position));
		std::string token;
		std::size_t count = 0;

		while (in >> token)
		{
			int value = 0;
			if (!parse_int(token, value))
			{
				return fail("Invalid pixel value: " + token);
			}
			if (count >= expected)
			{
				return fail("Extra data: expected " + std::to_string(expected) +
				            " values");
			}
			if (value < 0 || value > 255)
			{
				return fail("Pixel value out of range (0..255)");
			}
			if (static_cast<unsigned>(value) >= max_value)
			{
				return fail("Pixel value out of range for the given bpp");
			}

			data.push_back(static_cast<PaintCore::ColorIndex>(value));
			++count;
		}

		if (count < expected)
		{
			return fail("Insufficient data: expected " +
			            std::to_string(expected) + " values but read " +
			            std::to_string(count));
		}
	}
	else if (format == IndexedFormat::Binary)
	{
		const std::size_t payload_size = contents.size() - position;
		if (payload_size != expected)
		{
			return fail("Binary payload size does not match width*height");
		}
		data.reserve(expected);
		for (std::size_t index = 0; index < expected; ++index)
		{
			const unsigned value = static_cast<unsigned char>(
				contents[position + index]);
			if (value >= max_value)
			{
				return fail("Pixel value out of range for the given bpp");
			}
			data.push_back(static_cast<PaintCore::ColorIndex>(value));
		}
	}
	else
	{
		const std::size_t row_bytes = indexed_image_row_bytes(width, bpp);
		const std::size_t packed_size = row_bytes * static_cast<std::size_t>(height);

		if (format == IndexedFormat::BinaryPacked)
		{
			const std::size_t payload_size = contents.size() - position;
			if (payload_size != packed_size)
			{
				return fail("BinaryPacked payload size does not match the packed rows");
			}
			const std::vector<std::uint8_t> packed(
				contents.begin() + static_cast<std::ptrdiff_t>(position),
				contents.end());
			if (!unpack_indices(packed, width, height, bpp, data))
			{
				return fail("Could not unpack the pixel data");
			}
		}
		else
		{
			std::vector<std::uint8_t> packed;
			packed.reserve(packed_size);
			for (int y = 0; y < height; ++y)
			{
				std::string line;
				if (!next_line(line))
				{
					return fail("Insufficient Packed rows");
				}
				const std::string compact = compact_whitespace(line);
				if (compact.size() != row_bytes * 2u)
				{
					return fail("Invalid Packed row length");
				}
				for (std::size_t byte = 0; byte < row_bytes; ++byte)
				{
					const int high = hex_value(compact[byte * 2]);
					const int low = hex_value(compact[byte * 2 + 1]);
					if (high < 0 || low < 0)
					{
						return fail("Invalid hexadecimal digit in Packed data");
					}
					packed.push_back(static_cast<std::uint8_t>((high << 4) | low));
				}
			}
			if (contents.substr(position).find_first_not_of(" \t\r\n") !=
			    std::string::npos)
			{
				return fail("Extra data after Packed rows");
			}
			if (!unpack_indices(packed, width, height, bpp, data))
			{
				return fail("Could not unpack the pixel data");
			}
		}
	}

	try
	{
		IndexedImage image(width, height, bpp, std::move(data));
		image.m_format = format;
		return image;
	}
	catch (const std::invalid_argument& error)
	{
		return fail(error.what());
	}
}

bool save_indexed_image(const std::string& path, int width, int height,
                        int bits_per_pixel,
                        const std::vector<PaintCore::ColorIndex>& data,
                        std::string& error)
{
	return save_indexed_image(path, width, height, bits_per_pixel, data,
	                          IndexedFormat::Index, error);
}

bool save_indexed_image(const std::string& path, int width, int height,
                        int bits_per_pixel,
                        const std::vector<PaintCore::ColorIndex>& data,
                        IndexedFormat format, std::string& error)
{
	error.clear();

	if (path.empty())
	{
		error = "No file path";
		return false;
	}
	if (!validate_pixels(width, height, bits_per_pixel, data, error))
	{
		return false;
	}

	std::ofstream file(
		std::filesystem::path(path).string() + ".tmp",
		std::ios::binary | std::ios::trunc);
	if (!file)
	{
		error = "Could not open the file for writing";
		return false;
	}

	const std::filesystem::path target(path);
	const std::filesystem::path temporary(path + ".tmp");
	auto fail_and_remove = [&](const std::string& message)
	{
		file.close();
		std::error_code remove_error;
		std::filesystem::remove(temporary, remove_error);
		error = message;
		return false;
	};

	file << "IndexedImage " << format_name(format) << '\n';
	file << width << ' ' << height << '\n';
	file << bits_per_pixel << '\n';

	switch (format)
	{
		case IndexedFormat::Index:
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					const std::size_t index =
						static_cast<std::size_t>(y) *
							static_cast<std::size_t>(width) +
						static_cast<std::size_t>(x);
					file << static_cast<int>(data[index]);
					file << (x + 1 == width ? '\n' : ' ');
				}
			}
			break;
		case IndexedFormat::Binary:
			file.write(reinterpret_cast<const char*>(data.data()),
			           static_cast<std::streamsize>(data.size()));
			break;
		case IndexedFormat::Packed:
		{
			std::vector<std::uint8_t> packed;
			if (!pack_indices(data, width, height, bits_per_pixel, packed))
			{
				return fail_and_remove("Could not pack the pixel data");
			}
			const std::size_t row_bytes = indexed_image_row_bytes(
				width, bits_per_pixel);
			static const char hex[] = "0123456789abcdef";
			for (int y = 0; y < height; ++y)
			{
				const std::uint8_t* row = packed.data() +
					static_cast<std::size_t>(y) * row_bytes;
				for (std::size_t byte = 0; byte < row_bytes; ++byte)
				{
					const char pair[2] = {
						hex[(row[byte] >> 4) & 0x0F],
						hex[row[byte] & 0x0F],
					};
					file.write(pair, sizeof(pair));
				}
				file << '\n';
			}
			break;
		}
		case IndexedFormat::BinaryPacked:
		{
			std::vector<std::uint8_t> packed;
			if (!pack_indices(data, width, height, bits_per_pixel, packed))
			{
				return fail_and_remove("Could not pack the pixel data");
			}
			file.write(reinterpret_cast<const char*>(packed.data()),
			           static_cast<std::streamsize>(packed.size()));
			break;
		}
	}

	if (!file.good())
	{
		return fail_and_remove("Failed to write the file");
	}
	file.flush();
	if (!file.good())
	{
		return fail_and_remove("Failed to flush the file");
	}
	file.close();

	std::error_code rename_error;
	std::filesystem::rename(temporary, target, rename_error);
	if (rename_error)
	{
		std::error_code remove_error;
		std::filesystem::remove(temporary, remove_error);
		error = "Could not replace the target file";
		return false;
	}
	return true;
}
