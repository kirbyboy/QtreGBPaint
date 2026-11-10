// Headless tests for DocumentFile savepoint/path state.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include "paint/document_file.h"

TEST_CASE("new document has no path and starts clean")
{
	DocumentFile file;

	CHECK_FALSE(file.has_path());
	CHECK(file.path().empty());
	CHECK_EQ(file.display_name(), "Untitled");
	CHECK_FALSE(file.is_modified(0, false));
	CHECK(file.is_modified(1, false));
	CHECK(file.is_modified(0, true));
}

TEST_CASE("saving records path and savepoint")
{
	DocumentFile file;
	file.mark_saved("images/tiles.idx", 7);

	CHECK(file.has_path());
	CHECK_EQ(file.path(), "images/tiles.idx");
	CHECK_EQ(file.display_name(), "tiles.idx");
	CHECK_FALSE(file.is_modified(7, false));
	CHECK(file.is_modified(6, false));
	CHECK(file.is_modified(8, false));
	CHECK(file.is_modified(7, true));
}

TEST_CASE("mark_new clears path and savepoint")
{
	DocumentFile file;
	file.mark_saved("C:\\art\\room.idx", 12);
	CHECK_EQ(file.display_name(), "room.idx");

	file.mark_new();
	CHECK_FALSE(file.has_path());
	CHECK_EQ(file.display_name(), "Untitled");
	CHECK_FALSE(file.is_modified(0, false));
}
