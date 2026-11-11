#ifndef QTREGBPAINT_PAINT_FILE_ERROR_H
#define QTREGBPAINT_PAINT_FILE_ERROR_H

// Stable categories for errors that reach the UI. The technical description
// stays in the existing `std::string& error`; the UI translates the category.
enum class FileError
{
	None,
	NoPath,
	InvalidDimensions,
	InvalidBitsPerPixel,
	InvalidImageData,
	InvalidPalette,
	OpenFailed,
	ReadFailed,
	InvalidHeader,
	UnsupportedFormat,
	InvalidPixel,
	TruncatedData,
	ExtraData,
	WriteFailed,
	TooLarge,
	CompressionFailed,
	Unknown,
};

#endif
