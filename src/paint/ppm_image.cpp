#include "paint/ppm_image.h"

#include <fstream>
#include <sstream>
#include <string>

PPMImage::PPMImage(int width, int height, int bitsPerPixel, std::vector<std::uint8_t> data)
    : m_width(width),
      m_height(height),
      m_bpp(bitsPerPixel),
      m_data(std::move(data)),
      m_valid(true)
{
}

PPMImage PPMImage::loadFromFile(const std::string& path)
{
    PPMImage img;

    std::ifstream file(path);
    if (!file)
    {
        img.m_error = "No se pudo abrir el archivo";
        return img;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::istringstream in(ss.str());

    std::string magic;
    if (!(in >> magic) || magic != "P2")
    {
        img.m_error = "Cabecera invalida: se esperaba 'P2'";
        return img;
    }

    int w = 0, h = 0, bpp = 0;
    if (!(in >> w) || w <= 0)
    {
        img.m_error = "Ancho invalido";
        return img;
    }
    if (!(in >> h) || h <= 0)
    {
        img.m_error = "Alto invalido";
        return img;
    }
    if (!(in >> bpp) || bpp <= 0)
    {
        img.m_error = "bpp invalido";
        return img;
    }

    img.m_width = w;
    img.m_height = h;
    img.m_bpp = bpp;

    const std::size_t expected = static_cast<std::size_t>(w) * h;

    int value = 0;
    std::size_t count = 0;
    while (in >> value)
    {
        if (value < 0 || value > 255)
        {
            img.m_error = "Valor de pixel fuera de rango (0..255)";
            return img;
        }

        if (count < expected)
        {
            img.m_data.push_back(static_cast<std::uint8_t>(value));
        }
        else
        {
            if (img.m_warning.empty())
            {
                img.m_warning = "Datos extra: se ignoran los valores sobrantes";
            }
        }
        ++count;
    }

    if (count < expected)
    {
        img.m_error = "Datos insuficientes: se esperaban " + std::to_string(expected) +
                      " valores y se leyeron " + std::to_string(count);
        return img;
    }

    if (count > expected && img.m_warning.empty())
    {
        img.m_warning = "Datos extra: se ignoran los valores sobrantes";
    }

    img.m_valid = true;
    return img;
}
