#include "paint_frontend_internal.h"
#include "paint/i18n.h"

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

const char* file_error_text(FileError error)
{
	switch (error)
	{
		case FileError::None:
			return "";
		case FileError::NoPath:
			return i18n::tr("No file path selected.");
		case FileError::InvalidDimensions:
			return i18n::tr("The image dimensions are not valid.");
		case FileError::InvalidBitsPerPixel:
			return i18n::tr("The bits per pixel value is not valid.");
		case FileError::InvalidImageData:
			return i18n::tr("The image data is not valid.");
		case FileError::InvalidPalette:
			return i18n::tr("The palette is not valid for this image.");
		case FileError::OpenFailed:
			return i18n::tr("Could not open the file.");
		case FileError::ReadFailed:
			return i18n::tr("Could not read the file.");
		case FileError::InvalidHeader:
			return i18n::tr("The file is not a valid .idx image.");
		case FileError::UnsupportedFormat:
			return i18n::tr("The file uses an unsupported format.");
		case FileError::InvalidPixel:
			return i18n::tr("The file contains a pixel outside the palette.");
		case FileError::TruncatedData:
			return i18n::tr("The file is incomplete.");
		case FileError::ExtraData:
			return i18n::tr("The file contains unexpected extra data.");
		case FileError::WriteFailed:
			return i18n::tr("Could not write the file.");
		case FileError::TooLarge:
			return i18n::tr("The image is too large for the selected format.");
		case FileError::CompressionFailed:
			return i18n::tr("Could not compress the export data.");
		case FileError::Unknown:
			return i18n::tr("The operation failed.");
	}
	return i18n::tr("The operation failed.");
}

const char* indexed_format_description(IndexedFormat format)
{
	switch (format)
	{
		case IndexedFormat::Index:
			return i18n::tr(
				"Plain decimal text, one value per pixel. Easy to inspect, "
				"but the largest format.");
		case IndexedFormat::Binary:
			return i18n::tr(
				"One raw byte per pixel, without row packing. Simple and "
				"exact, but not human-readable.");
		case IndexedFormat::Packed:
			return i18n::tr(
				"MSB-first row packing stored as hexadecimal text. Compact "
				"and still inspectable; 2bpp halves the data.");
		case IndexedFormat::BinaryPacked:
			return i18n::tr(
				"MSB-first row packing stored as raw bytes. The smallest "
				"format, not human-readable.");
	}
	return "";
}
} // namespace PaintFrontendInternal
