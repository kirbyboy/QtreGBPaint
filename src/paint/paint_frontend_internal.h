#ifndef QTREGBPAINT_PAINT_FRONTEND_INTERNAL_H
#define QTREGBPAINT_PAINT_FRONTEND_INTERNAL_H

#include <cstddef>
#include <filesystem>
#include <string>

#include "paint/file_error.h"
#include "paint/indexed_image.h"

// Helpers shared by the PaintFrontend translation units. They are internal to
// the UI implementation, not part of the public frontend API.
namespace PaintFrontendInternal
{
std::string ensure_extension(std::string path, const char* extension);
std::string ensure_idx_extension(std::string path);
std::string recent_file_name(const std::string& path);
bool accessible_directory(const std::filesystem::path& path);
std::string format_byte_size(std::size_t bytes);
const char* indexed_format_description(IndexedFormat format);
const char* file_error_text(FileError error);
} // namespace PaintFrontendInternal

#endif
