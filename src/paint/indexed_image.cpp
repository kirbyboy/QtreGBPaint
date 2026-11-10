#include "paint/indexed_image.h"

#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>
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

	std::ifstream file(path);
	if (!file)
	{
		return fail("Could not open the file");
	}

	std::ostringstream contents;
	contents << file.rdbuf();
	std::istringstream in(contents.str());

	std::string header;
	if (!std::getline(in, header))
	{
		return fail("Empty file");
	}
	strip_carriage_return(header);

	std::istringstream header_stream(header);
	std::string magic;
	if (!(header_stream >> magic) || magic != "IndexedImage")
	{
		return fail("Invalid header: expected 'IndexedImage'");
	}

	std::string mode;
	if (header_stream >> mode)
	{
		if (mode != "Index")
		{
			if (mode == "Binary" || mode == "Packed" || mode == "BinaryPacked")
			{
				return fail("Mode '" + mode + "' is not implemented yet");
			}
			return fail("Invalid mode: " + mode);
		}
	}

	std::string extra;
	if (header_stream >> extra)
	{
		return fail("Invalid header: extra tokens");
	}

	std::string width_token;
	std::string height_token;
	std::string bpp_token;
	if (!(in >> width_token >> height_token >> bpp_token))
	{
		return fail("Incomplete header: width, height or bpp is missing");
	}

	int width = 0;
	int height = 0;
	int bpp = 0;
	if (!parse_int(width_token, width) || width <= 0)
	{
		return fail("Invalid width");
	}
	if (!parse_int(height_token, height) || height <= 0)
	{
		return fail("Invalid height");
	}
	if (!parse_int(bpp_token, bpp) || bpp < 1 || bpp > 8)
	{
		return fail("Invalid bpp: must be in the range 1..8");
	}

	const std::size_t expected =
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	const unsigned max_value = 1u << bpp;

	std::vector<PaintCore::ColorIndex> data;
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
		return fail("Insufficient data: expected " + std::to_string(expected) +
					" values but read " + std::to_string(count));
	}

	try
	{
		return IndexedImage(width, height, bpp, std::move(data));
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
	error.clear();

	if (path.empty())
	{
		error = "No file path";
		return false;
	}
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
	for (const PaintCore::ColorIndex value : data)
	{
		if (value >= max_value)
		{
			error = "An index is outside the range allowed by bpp";
			return false;
		}
	}

	std::ofstream file(path, std::ios::trunc);
	if (!file)
	{
		error = "Could not open the file for writing";
		return false;
	}

	file << "IndexedImage Index\n";
	file << width << ' ' << height << '\n';
	file << bits_per_pixel << '\n';

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const std::size_t index =
				static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
				static_cast<std::size_t>(x);
			file << static_cast<int>(data[index]);
			file << (x + 1 == width ? '\n' : ' ');
		}
	}

	if (!file.good())
	{
		error = "Failed to write the file";
		return false;
	}

	file.flush();
	if (!file.good())
	{
		error = "Failed to flush the file";
		return false;
	}
	return true;
}
