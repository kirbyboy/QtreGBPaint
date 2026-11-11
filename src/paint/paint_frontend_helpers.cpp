#include "paint_frontend_internal.h"

#include <cstdio>
#include <utility>

namespace PaintFrontendInternal
{
std::string ensure_extension(std::string path, const char* extension)
{
	if (path.empty())
	{
		return path;
	}
	std::filesystem::path file_path(path);
	if (file_path.extension() != extension)
	{
		file_path.replace_extension(extension);
	}
	return file_path.string();
}

std::string ensure_idx_extension(std::string path)
{
	return ensure_extension(std::move(path), ".idx");
}

std::string recent_file_name(const std::string& path)
{
	const std::string name = std::filesystem::path(path).filename().string();
	return name.empty() ? path : name;
}

bool accessible_directory(const std::filesystem::path& path)
{
	std::error_code error;
	if (!std::filesystem::is_directory(path, error) || error)
	{
		return false;
	}
	// is_directory() only inspects metadata; opening the iterator also checks
	// that the process can actually read the directory.
	std::filesystem::directory_iterator probe(path, error);
	return !error;
}

std::string format_byte_size(std::size_t bytes)
{
	if (bytes < 1024)
	{
		return std::to_string(bytes) + " B";
	}
	char buffer[64] = {};
	std::snprintf(buffer, sizeof(buffer), "%.1f KiB",
	              static_cast<double>(bytes) / 1024.0);
	return buffer;
}

const char* indexed_format_description(IndexedFormat format)
{
	switch (format)
	{
		case IndexedFormat::Index:
			return "Plain decimal text, one value per pixel. Easy to inspect, "
			       "but the largest format.";
		case IndexedFormat::Binary:
			return "One raw byte per pixel, without row packing. Simple and "
			       "exact, but not human-readable.";
		case IndexedFormat::Packed:
			return "MSB-first row packing stored as hexadecimal text. Compact "
			       "and still inspectable; 2bpp halves the data.";
		case IndexedFormat::BinaryPacked:
			return "MSB-first row packing stored as raw bytes. The smallest "
			       "format, not human-readable.";
	}
	return "";
}
} // namespace PaintFrontendInternal
