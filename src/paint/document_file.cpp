#include "paint/document_file.h"

std::string DocumentFile::display_name() const
{
	if (m_path.empty())
	{
		return "Untitled";
	}

	const std::size_t separator = m_path.find_last_of("/\\");
	if (separator == std::string::npos)
	{
		return m_path;
	}
	return m_path.substr(separator + 1);
}
