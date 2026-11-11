# Third-Party Notices

QtreGBPaint uses third-party components. This document collects their
copyright notices and licenses. It does not modify the original licenses: each
component remains governed by its own license.

The third-party licenses are not affected by the QtreGBPaint source-code
license or by `TRADEMARK.md`. The project-original visual assets, including the
application icon and all cursor bitmaps, are covered by `assets/LICENSE`
(CC0 1.0).

## zlib-licensed components

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

## MIT-licensed components

The following text applies to the components listed below.

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

Used for the user interface.

### ImGuiFileDialog

ImGuiFileDialog v0.6.8 (commit `ee77f190861193a995435c388682f1512f6fd29a`)
Copyright (c) 2018-2025 Stephane Cuillerdier (aka Aiekick)
<https://github.com/aiekick/ImGuiFileDialog>

Used for the file open and save dialogs.

The copy in `third_party/ImGuiFileDialog` is vendored and locally modified for
runtime UI translations. It is not QtreGBPaint code; see
`third_party/ImGuiFileDialog/LOCAL_CHANGES.md` and the MIT text above.

### dirent

Copyright (c) 1998-2019 Toni Ronkko

Included by ImGuiFileDialog.

### stb

stb_image v2.29
Copyright (c) 2017 Sean Barrett

Used by the application to decode PNG images (for example, the About logo)
and included by ImGuiFileDialog for image loading and resizing.

### doctest

doctest v2.4.12
Copyright (c) 2016-2025 Viktor Kirilov

Used only by the tests; it is not distributed in the application binary.

## GCC Runtime Library

AppImages may include `libstdc++.so.6` and `libgcc_s.so.1`.

Copyright (C) Free Software Foundation, Inc.

These libraries are distributed under GPLv3 with the GCC Runtime Library
Exception 3.1. The text of the exception and GPLv3 is available at:

- <https://www.gnu.org/licenses/gcc-exception-3.1.html>
- <https://www.gnu.org/licenses/gpl-3.0.html>

The runtime exception permits distributing these components together with
programs that are not GPL.

## Other packaging components

The AppImage is generated with `linuxdeploy` and `appimagetool`; their own
notices and licenses apply to those tools and to the AppImage runtime. They are
not part of the QtreGBPaint source code.

## Note

If a new dependency is added, its copyright notice, license and any attribution
requirements must be added here before distributing the program.
