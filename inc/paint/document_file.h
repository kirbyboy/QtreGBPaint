#ifndef QTREGBPAINT_DOCUMENT_FILE_H
#define QTREGBPAINT_DOCUMENT_FILE_H

#include <cstdint>
#include <string>
#include <utility>

// Identity and savepoint of the current document. It does not perform I/O; the
// application backend owns it and updates it after New/Open/Save.
class DocumentFile
{
public:
	bool has_path() const { return !m_path.empty(); }
	const std::string& path() const { return m_path; }

	// File name without directories, or "Untitled".
	std::string display_name() const;

	bool is_modified(std::uint64_t current_state,
	                 bool has_floating_region) const
	{
		return current_state != m_saved_state || has_floating_region;
	}

	void mark_new()
	{
		m_path.clear();
		m_saved_state = 0;
	}

	void mark_saved(std::string path, std::uint64_t state)
	{
		m_path = std::move(path);
		m_saved_state = state;
	}

private:
	std::string m_path;
	std::uint64_t m_saved_state = 0;
};

#endif
