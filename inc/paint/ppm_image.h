#ifndef QTREGBPAINT_PPM_IMAGE_H
#define QTREGBPAINT_PPM_IMAGE_H

#include <cstdint>
#include <string>
#include <vector>

// Representa un archivo en el formato P2 inventado (basado en PPM).
// Cabecera:
//   P2
//   <width> <height>
//   <bpp>
//   <width*height valores 0..255 separados por espacios>
class PPMImage
{
public:
    PPMImage() = default;
    PPMImage(int width, int height, int bitsPerPixel, std::vector<std::uint8_t> data);

    // Carga y valida un archivo desde disco.
    static PPMImage loadFromFile(const std::string& path);

    bool isValid() const { return m_valid; }
    const std::string& error() const { return m_error; }
    const std::string& warning() const { return m_warning; }

    int width() const { return m_width; }
    int height() const { return m_height; }
    int bitsPerPixel() const { return m_bpp; }
    std::size_t expectedSize() const { return static_cast<std::size_t>(m_width) * m_height; }

    const std::vector<std::uint8_t>& data() const { return m_data; }
    std::vector<std::uint8_t>& data() { return m_data; }

private:
    int m_width = 0;
    int m_height = 0;
    int m_bpp = 0;
    std::vector<std::uint8_t> m_data;
    bool m_valid = false;
    std::string m_error;
    std::string m_warning;
};

#endif
