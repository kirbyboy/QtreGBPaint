#include "paint/paint_backend.h"

#include <cstdint>

PaintBackend::PaintBackend(IGuiBackend* backend) : m_backend(backend)
{
	// Icono por defecto (interrogante) hardcodeado, sin bordes 3D
	static const uint8_t kDefaultIcon[16 * 16] = {
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 1, 1, 1, 1, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 1, 1, 1, 1, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 1, 1, 1, 1, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 1, 1, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 1, 1, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 1, 1, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 1, 1, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 1, 1, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
	};

	if (!m_backend) return;

	const Palette gray = Palette::grayscale(3);

	void* tex = m_backend->createTexture(16, 16);
	if (!tex) return;

	std::vector<PaintCore::Color> pixels(16 * 16);
	for (int i = 0; i < 16 * 16; ++i)
	{
		uint8_t level = kDefaultIcon[i];
		if (level >= gray.size()) level = static_cast<uint8_t>(gray.size() - 1);
		pixels[i] = gray.getColor(level);
	}
	m_backend->updateTexture(tex, pixels.data(), 16, 16);
	m_defaultIcon = reinterpret_cast<ImTextureID>(tex);
}

ImTextureID PaintBackend::loadIcon(const std::string& path, const Palette& palette)
{
	if (!m_backend) return m_defaultIcon;

	PPMImage ppm = PPMImage::loadFromFile(path);
	if (!ppm.isValid()) return m_defaultIcon;

	int w = ppm.width();
	int h = ppm.height();
	if (w <= 0 || h <= 0) return m_defaultIcon;

	void* tex = m_backend->createTexture(w, h);
	if (!tex) return m_defaultIcon;

	std::vector<PaintCore::Color> pixels(static_cast<std::size_t>(w) * h);
	const auto& data = ppm.data();
	for (std::size_t i = 0; i < pixels.size(); ++i)
	{
		std::uint8_t level = (i < data.size()) ? data[i] : 0;
		if (level >= palette.size()) level = static_cast<std::uint8_t>(palette.size() - 1);
		pixels[i] = palette.getColor(level);
	}

	m_backend->updateTexture(tex, pixels.data(), w, h);
	m_textures.push_back(tex);
	return reinterpret_cast<ImTextureID>(tex);
}

void PaintBackend::addButton(const std::string& name,
							 std::shared_ptr<ToolBase> tool,
							 const std::string& iconNormalPath,
							 const std::string& iconHoverPath,
							 const std::string& iconPressedPath,
							 const Palette& palette)
{
	ToolButton btn;
	btn.name = name;
	btn.tool = std::move(tool);
	btn.iconNormal  = loadIcon(iconNormalPath, palette);
	btn.iconHover   = loadIcon(iconHoverPath, palette);
	btn.iconPressed = loadIcon(iconPressedPath, palette);
	m_buttons.push_back(std::move(btn));
}

ToolBase* PaintBackend::selectButton(const std::string& name)
{
	for (const auto& btn : m_buttons)
	{
		if(btn.name != name)
		{
			continue;
		}

		m_activeButton = name;
		m_activeTool = btn.tool;
		return m_activeTool.get();
	}
	return m_activeTool.get();
}
