> **Traducción informativa al español.** La versión inglesa (`THIRD_PARTY_NOTICES.md`) es la versión de referencia y prevalece en caso de discrepancia.

# Avisos de terceros

QtreGBPaint utiliza componentes de terceros. Este documento recoge sus avisos
de copyright y licencias. No modifica las licencias originales: cada componente
sigue rigiéndose por su propia licencia.

Las licencias de los componentes de terceros no se ven afectadas por la
licencia del código de QtreGBPaint ni por `TRADEMARK.md`. Los assets visuales
originales del proyecto, incluidos el icono de la aplicación y todos los
cursores, se cubren en `assets/LICENSE` (CC0 1.0).

## Componentes con licencia zlib

### SDL2

Simple DirectMedia Layer
Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

```text
This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

### zlib

zlib 1.3.1
Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler

```text
This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## Componentes con licencia MIT

El siguiente texto se aplica a los componentes enumerados debajo.

```text
MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### Dear ImGui

Copyright (c) 2014-2025 Omar Cornut

Se usa para la interfaz de usuario.

### ImGuiFileDialog

ImGuiFileDialog v0.6.8 (commit `ee77f190861193a995435c388682f1512f6fd29a`)
Copyright (c) 2018-2025 Stephane Cuillerdier (aka Aiekick)
<https://github.com/aiekick/ImGuiFileDialog>

Se usa para los diálogos de apertura y guardado de archivos.

La copia de `third_party/ImGuiFileDialog` está vendorizada y modificada
localmente para traducir sus textos en runtime. No es código de QtreGBPaint;
ver `third_party/ImGuiFileDialog/LOCAL_CHANGES.md` y el texto MIT anterior.

### dirent

Copyright (c) 1998-2019 Toni Ronkko

Se incluye dentro de ImGuiFileDialog.

### stb

stb_image v2.29
Copyright (c) 2017 Sean Barrett

Se usa en la aplicación para decodificar imágenes PNG (por ejemplo, el
logotipo del About) y se incluye en ImGuiFileDialog para la carga y el
escalado de imágenes.

### doctest

doctest v2.4.12
Copyright (c) 2016-2025 Viktor Kirilov

Solo se usa en los tests, no se distribuye en el binario de la aplicación.

## GCC Runtime Library

Los AppImage pueden incluir `libstdc++.so.6` y `libgcc_s.so.1`.

Copyright (C) Free Software Foundation, Inc.

Estas bibliotecas se distribuyen bajo GPLv3 con la GCC Runtime Library
Exception 3.1. El texto de la excepción y de la GPLv3 está disponible en:

- <https://www.gnu.org/licenses/gcc-exception-3.1.html>
- <https://www.gnu.org/licenses/gpl-3.0.html>

La excepción de runtime permite distribuir estos componentes junto con
programas que no sean GPL.

## Otros componentes de empaquetado

El AppImage se genera con `linuxdeploy` y `appimagetool`; sus propios avisos y
licencias se aplican a esas herramientas y al runtime del AppImage. No forman
parte del código fuente de QtreGBPaint.

## Nota

Si se añade una dependencia nueva, debe añadirse aquí su aviso de copyright,
su licencia y cualquier requisito de atribución antes de distribuir el
programa.
