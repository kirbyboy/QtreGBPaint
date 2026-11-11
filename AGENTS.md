# AGENTS.md

Guía para agentes que trabajen en este repositorio.

## Proyecto

La aplicación es un editor de pixel art con paletas indexadas inspiradas en
consolas portátiles retro; genera imágenes de 1 a 8 bpp. Usa ImGui + SDL2, con
abstracciones para backends.

El código se distribuye bajo `GPL-3.0-or-later` (`LICENSE.txt`). La marca y el
icono se rigen por `TRADEMARK.md`; los assets visuales originales
(`assets/`, `packaging/qtregbpaint.png`) están dedicados al dominio público
bajo CC0 1.0 (`assets/LICENSE`). Esa dedicatoria no afecta a los derechos de
marca. Las licencias de terceros están en `THIRD_PARTY_NOTICES.md`.

## Estructura

- `src/` y `inc/`: código C++17.
  - `core/`: backend, bezier, brush, color, ellipse, fill, line, modifiers, shape_constraint, utilidades, vec2, window_geometry.
  - `paint/`: app_config, theme, canvas_document, floating_region, floating_region_editor, paint_session, buffers, frontend, paleta, checkerboard, png_image, image_export.
  - `tools/`: herramientas (base, draw, freehand, line, curve, pencil, brush, eraser, fill, pick_color, selection, shape, square, circle).
- `scripts/`: `bootstrap_deps.sh` y los Makefiles de las dependencias locales.
- `.github/workflows/ci.yml`: CI de GitHub Actions (`make deps`, `test-full` y smokes bajo Xvfb).
- `lib/`: dependencias locales (imgui, doctest, ImGuiFileDialog) - ignoradas por git.
- `tests/`: tests headless con doctest y su propio `Makefile`.
- `tests/smoke/`: smokes SDL/ImGui y su runner (requieren display; CI usa Xvfb).
- `packaging/`: `.desktop`, icono, MIME XML (magic `IndexedImage`), metadata AppStream, script de AppImage y script del `.deb`.
- `README.md`: presentación en inglés, características, build, uso, licencia y capturas; `docs/screenshots/` contiene las imágenes del README.
- `LICENSE.txt`, `assets/LICENSE`/`.es.md`, `TRADEMARK.md`/`.es.md` y `THIRD_PARTY_NOTICES.md`/`.es.md`: licencia GPL-3.0-or-later, licencia CC0 de los assets, política de marca y avisos de terceros.
- `build/`: binarios debug y release generados por Makefile.
- `obj/`, `deps/`, `obj-release/`, `deps-release/`: objetos y dependencias generados.
- `Makefile`: compilación.

## Build

```sh
make deps               # primera vez: descarga y compila lib/
make                    # debug: build/QtreGBPaint
make release            # release: build/QtreGBPaint-release
make run                # ejecuta el binario debug
make run-release        # ejecuta el binario release
make appimage           # AppImage en build/QtreGBPaint-0.9.1-x86_64.AppImage
make deb                # paquete Debian en build/qtregbpaint_0.9.1_amd64.deb
make release-assets     # .deb + AppImage + build/SHA256SUMS-0.9.1
make -j"$(nproc)" test  # suite normal, en paralelo
make -j"$(nproc)" test-full  # normal + ASAN + UBSAN + sanitize
make test-asan          # tests con AddressSanitizer
make test-ubsan         # tests con UndefinedBehaviorSanitizer
make test-sanitize      # tests con ASAN + UBSAN
make -C tests test-circle  # un solo binario (tests/build/test_circle)
make smoke              # todos los smokes (requiere display)
./tests/smoke/run.sh circle buttons  # smokes concretos
./tests/smoke/run.sh    # todos los smokes (compilación paralela)
make clean              # limpia objetos, binarios y tests/build*
```

Los tests también se pueden ejecutar sin pasar por el Makefile principal:

```sh
make -C tests
make -C tests test-asan
make -C tests test-ubsan
make -C tests test-sanitize
```

Durante el desarrollo basta con el test o el smoke afectado; la matriz completa
(`test-full`) y todos los smokes se reservan para cerrar un bloque. Para un caso
concreto de doctest: `tests/build/test_circle --test-case="*Shift*"`.

Requisitos: g++, SDL2, zlib (`-lz`), make y git. Include paths configurados en
Makefile. `make deps` ejecuta `scripts/bootstrap_deps.sh`, que clona versiones
fijas en `lib/` y construye `libimgui.a`/`libImGuiFileDialog.a`; las
dependencias locales (imgui, doctest, ImGuiFileDialog) no se versionan.
`lib/ImGuiFileDialog/stb/stb_image.h` se usa para decodificar PNG. Para un clon
nuevo hay que ejecutar `make deps` antes de compilar.

## Convenciones

- Código, comentarios, mensajes de error y textos de UI en inglés. La
  documentación del proyecto (`AGENTS.md`, `HANDOFF.md`, `temp/README.md`) se
  mantiene en español.
- Namespace `PaintCore` para primitivas compartidas (`Color`, `ColorIndex`, `Vec2`, etc.).
- `ColorIndex` es `uint8_t`; cualquier índice de paleta visible puede ser el
  color transparente (propiedad opcional de la paleta). No hay índice centinela.
- Imágenes en memoria: vector de bytes, un byte por píxel (índice de paleta).
- `ImageBuffer`: buffer principal del canvas.
- `DrawBuffer`: buffer temporal de dibujo con origen variable; se mezcla sobre el canvas al confirmar. Una máscara de cobertura separa píxeles pintados de no pintados.
- Canvas solo renderiza la parte visible; operaciones confirmadas afectan todo el buffer.

## Backup

Al leer `HANDOFF.md`, ejecutar `tmp/backup/backup.sh` para crear un backup con
timestamp en `tmp/backup/<yyyymmdd_hh24mmss>/`.

## Reglas para agentes

- No ejecutar comandos git destructivos (reset --hard, clean -fd, checkout --, etc.). Solo consultas.
- No escribir/modificar archivos del proyecto sin permiso explícito del usuario, salvo los archivos de contexto acordados (AGENTS.md, HANDOFF.md, etc.).
- Usar `temp/` (ignorada por git) para scripts, capturas y artefactos del agente
  que deban persistir entre reinicios.
- Usar `/tmp/opencode` (o `/tmp`) para archivos efímeros, builds de prueba o
  experimentos.
- Mantener cambios mínimos y consistentes con el estilo existente.
- Si se añade una herramienta nueva, heredar de `DrawTool` (dibujo) o `SelectionTool` (selección) según corresponda.
- Al renombrar archivos, usar `git mv` y revisar `git status` para no dejar
  archivos tracked que ya no existen en disco.

## Decisiones de producto y no objetivos

- **Ventana única.** Sin pestañas ni múltiples documentos. El zoom y la paleta
  son globales a propósito.
- **Sin capas, transformaciones de selección ni selecciones no rectangulares.**
- **Sin portapapeles externo del sistema.** Puede estudiarse un portapapeles
  entre instancias de la misma aplicación con formato propio, pero no está
  comprometido.
- **La paleta no se persiste en esta app** por decisión de producto. En la
  futura app Qt será parte del documento. Mantener el dominio
  (`CanvasDocument`, `FloatingRegionEditor`, `PaintSession`) libre de ImGui
  para poder reutilizarlo.
- **Transparencia de paleta.** Es una propiedad opcional de `Palette`, no un
  índice centinela: cualquier swatch visible (también 255 en 8bpp) puede ser
  transparente y todos los bpp se tratan igual. No entra en el historial ni
  marca el documento como modificado; `New`/`Open` la reinician, mientras que
  cambiar de tonalidad o invertir la conservan. El damero es solo de vista.
- **Configuración de aplicación.** `AppConfig` persiste solo preferencias
  globales en `$XDG_CONFIG_HOME/qtregbpaint/config.ini` (o
  `~/.config/qtregbpaint/config.ini`): tema, estado de la rejilla, geometría y
  maximizado de la ventana, última carpeta de Open/Save As y hasta 10 archivos
  recientes. No se persisten la herramienta activa (siempre arranca Pencil), la
  paleta ni el zoom. El `imgui.ini` de ImGui está desactivado
  (`io.IniFilename = nullptr`) porque el layout es fijo y el directorio de
  trabajo no es fiable en AppImage/.deb. Una carpeta guardada inaccesible no
  muestra error: se usa Pictures, luego home y luego el directorio actual.
- **Cambiar solo los colores de paleta no entra en el historial.** Cambiar el
  bpp es una propiedad del documento: desde `Image > Properties...` se puede
  cambiar y entra en el historial como un paso propio (con remapeo de píxeles).
- **Semántica de transparencia de selección fijada:** `hole_fill` conserva el
  background del momento de crear la selección; `transparent_color` sigue al
  background actual. No cambiarla sin una decisión nueva.
- **Opciones de herramientas.** `kToolOptionsHeight` es 116 px para admitir la
  cuadrícula 4x3 de Brush. `ToolOption::grid_columns` permite pintar opciones
  `Choice` como cuadrícula compacta; con 1 se mantiene la lista vertical.
- **Overlays de `CanvasArea`.** Además de la rejilla y los handles, Brush y
  Eraser añaden una huella de preview bajo el puntero. Sigue siendo un overlay de
  vista; su separación está apuntada como revisión de baja prioridad.
- **Cursores `.ipx`:** usan exactamente 2 bpp con `0`=transparente,
  `1`=hotspot, `2`=negro y `3`=blanco. Selection, Line, Curve, Rect y Circle
  comparten `cross.ipx`; Brush usa `slim_cross.ipx` y es la única excepción de
  color: su índice 2 se pinta como gris oscuro `(96,96,96)` al cargarlo.
- **Antes de añadir funcionalidades**, revisar “Decisiones de producto y
  revisión pendiente — 2026-11-08” en `HANDOFF.md`: opciones en cuadrícula y
  portapapeles entre instancias. Render/scroll quedó resuelto con tiles.
- **No mutar píxeles del `ImageBuffer` directamente.** Toda edición del canvas
  pasa por `CanvasDocument::apply_pixel_patch` o `commit_draw_buffer`, que
  capturan before/after y registran el historial. El buffer principal solo se
  expone en lectura.

## Estado actual

La paleta de la aplicación vive en `PaintBackend`: tonalidad
(DMG/Grayscale/Red/Green/Blue), inversión y foreground/background. `PaintFrontend`
solo la pinta y consulta; el menú `Palette` cambia la tonalidad, ofrece
`Invert Palette`, `Transparent Foreground` y `Clear Transparent Color`. El
número de swatches sale del bpp del documento, limitado a 32; 8bpp usa los 256
índices (el último swatch es 255, un color normal). Cualquier swatch visible
puede marcarse como transparente: la paleta guarda un índice opcional y
`getColor()` devuelve alfa 0 para él, conservando el color almacenado por si se
limpia. La transparencia de paleta no entra en el historial ni modifica el
documento; `New`/`Open` la reinician y cambiar de tonalidad o invertir la
conservan. Al crear o abrir, el foreground es el último swatch visible y el
background el primero; cambiar de tonalidad o invertir conserva esos índices y
solo cambia sus colores. El canvas pinta un damero de celdas fijas en pantalla
(8 px, anclado al origen de la imagen) bajo la imagen cuando hay transparencia;
los swatches, el foreground/background y el color de la barra de estado usan el
mismo damero (4 px). El damero se dibuja con una textura de 256×256
(`PaintUi::CheckerboardTexture`) en quads alineados al origen de la imagen; si
la textura no se puede crear, se pinta una celda por rectángulo.

La configuración de la aplicación vive en `AppConfig`
(`inc/paint/app_config.h`): tema, estado de `View > Grid`, geometría y
maximizado de la ventana, última carpeta de Open/Save As y hasta 10 archivos
recientes. Los recientes se deduplican y, al abrir uno, pasa al principio. Se carga una vez al arrancar y se guarda al salir en
`$XDG_CONFIG_HOME/qtregbpaint/config.ini` (fallback
`~/.config/qtregbpaint/config.ini`) mediante escritura atómica. `main` aplica
la geometría antes de mostrar la ventana y `SDLRendererBackend` conserva la
última geometría no maximizada. La herramienta activa siempre es Pencil al
arrancar; la paleta y el zoom no se persisten. ImGui no lee ni escribe
`imgui.ini` (`io.IniFilename = nullptr`). Si la última carpeta no existe o no
se puede leer, Open/Save As caen sin mensaje de error a `XDG_PICTURES_DIR`,
`~/Pictures`, `$HOME` o el directorio actual, en ese orden.

El menú `File` ofrece New (ancho, alto y bpp 1..8), Open, Save, Save As...,
Export > Bitmap/PNG, hasta 10 archivos recientes
(persistidos en la configuración; al pulsarlos se abren directamente y, si
falla la apertura, solo se muestra `File Error` sin tocar el documento) y Exit.
La exportación escribe una copia: BMP 24 bits sin compresión (BGR, filas de
abajo arriba con padding a 4 bytes) y PNG indexado de 8 bits con `PLTE` y `tRNS`
(colores exactos de la paleta y transparencia real). El BMP usa el color
almacenado para el índice transparente porque no tiene alfa. `-lz` aporta
`compress2`/`crc32`. Export no cambia el savepoint ni los recientes, pero
confirma antes la región flotante o la edición pendiente para exportar lo que
se ve.
`PaintBackend` posee `DocumentFile` (path + savepoint) y las operaciones de
archivo; `PaintFrontend` solo abre `ImGuiFileDialog` y los modales. El título de
la ventana ImGui muestra
`*<archivo> - QtreGBPaint` (o `Untitled`) centrado, y el título del SO es
`QtreGBPaint`; cerrar con cambios pendientes abre `Unsaved Changes`, y
`Ctrl+N/O/S` replican File > New/Open/Save. El savepoint usa IDs de estado del
historial, así que undo/redo de vuelta al punto guardado deja el documento
limpio. El formato es `.idx` con modos `Index`, `Binary`, `Packed` y
`BinaryPacked` (paleta no persistida). `Save` usa el formato actual del
documento; `Save As` abre `IDX Options` tras elegir el archivo, con la
explicación del modo y una tabla de tamaños aproximados para el canvas actual.
Al abrir un `.idx` se detecta el modo desde la cabecera, y `New` vuelve a
`Index`. Save confirma la región flotante antes de escribir. El guardado del
`.idx` es atómico (`.tmp` + `rename`). `main` acepta una ruta (`QtreGBPaint
archivo.idx`) y el `.desktop` usa `%f`; el MIME propio
`application/x-qtregbpaint-index` se detecta por magic (`IndexedImage` al
inicio), no por la extensión.

Los cursores por herramienta usan `ToolButton`/`PaintBackend` (`inc/core/cursor.h`,
`IGuiBackend::set_cursor`) y,
desde `assets/cursors/*.ipx`, `IGuiBackend::set_cursor_image` +
`PaintCore::load_cursor_asset`. Selection, Line, Curve, Rect y Circle comparten
`assets/cursors/cross.ipx` (cruz de precisión); Pencil, Fill y Pick Color tienen
sus propios `.ipx`; Brush carga `assets/cursors/slim_cross.ipx`, una cruz de
17x17 con patrón alterno blanco/gris oscuro; Move usa `assets/cursors/move.ipx`.
Los `.ipx` usan exactamente 2 bpp con `0`=transparente, `1`=hotspot, `2`=negro
y `3`=blanco. El Brush es la excepción: al cargar su archivo, el índice 2 se
pinta como gris oscuro `(96,96,96)`. Eraser usa un cursor invisible de 1×1
transparente (`PaintCore::make_invisible_cursor`). Pick Color usa
`assets/cursors/pick.ipx` (creado por el proyecto, hotspot en `(0,15)`). El
cursor de Pencil es original del proyecto, con el hotspot en la punta, en
`(4,16)`. Move usa `assets/cursors/move.ipx`, también creado por el proyecto
(hotspot `(12,11)`). Todos los cursores están cubiertos por `assets/LICENSE`
(CC0). La vista solo muestra el cursor de la tool cuando el puntero está sobre
la imagen; en el margen gris usa la flecha. Rect y
Circle comparten la base `ShapeTool` (`inc/tools/shape_tool.h`), que declara las
opciones Outline/Fill/Both en `ToolBase` (`inc/tools/tool_option.h`) y las
muestra en el hueco del toolbar; Both es el modo por defecto y Outline deja el
interior transparente (el icono de Outline también usa índice 0 en el interior,
así que se ve el color del tema). Line y Curve declaran 5 anchos (1 px, cuadrado
2x2 y cruces de 3, 4 y 5 px) representados por cinco líneas horizontales de
1 a 5 px; Pencil no declara opciones; Brush
declara una cuadrícula 4x3 con 12 huellas; Eraser una lista vertical 4/6/8/10,
de menor a mayor, con 4 por defecto; Fill declara `Contiguous` y `Global` con
los iconos de `assets/fill_options.idx`. `FillTool` (`inc/tools/fill_tool.h`)
usa `PaintCore::collect_flood_fill_spans()` / `collect_global_fill_spans()`
(`inc/core/fill.h`), un scanline flood fill 4-conectado por spans; el relleno
se calcula y confirma al pulsar (sin preview de arrastre) y el botón derecho
usa el background. `PickColorTool` (`inc/tools/pick_color_tool.h`) es un
cuentagotas: mientras se mantiene pulsado el botón izquierdo o derecho muestra
el color bajo el puntero en la zona de opciones y guarda el índice al soltar
(si se suelta fuera de la imagen no cambia nada; cualquier índice es válido,
incluido el que la paleta renderiza como transparente).
Expone el resultado una vez con `ToolBase::take_color_pick()` y
`PaintBackend::apply_pending_color_pick()` actualiza foreground (izquierdo) o
background (derecho) y vuelve a la última herramienta de edición. `ToolBase`
expone `is_operating()` para que `CanvasArea` enrute el release de tools que no
pintan, y `color_pick_preview()` para el relleno de la zona de opciones.
Selection y Pick Color no reemplazan esa última
herramienta; seleccionar cualquier botón confirma la región flotante como el
resto. `CurveTool` (`inc/tools/curve_tool.h`) usa una Bézier cúbica
(`inc/core/bezier.h`): el primer arrastre fija los extremos, con Shift igual
que Line; el primer click o drag mueve el control P1 y el segundo P2. Shift ya
no afecta a los handles. A diferencia de Paint, los dos handles
son visibles y la curva queda como **edición pendiente** hasta confirmarla con
Enter, un click fuera de los handles o un cambio de tool; Escape, botón derecho
o ambos botones la cancelan. `ToolBase` expone `has_pending_edit` /
`commit_pending_edit` / `cancel_pending_edit` y `EditOverlay` (guías y handles);
`PaintSession` la hace exclusiva con la selección flotante, cancela con undo y
bloquea redo; `CanvasArea` pinta el overlay y enruta los drags de los handles;
guardar y resize la confirman, y `is_modified` la cuenta. El rasterizador usa
subdivisión adaptativa con tolerancia subpíxel y une cada tramo plano con
`rasterize_line()`, evitando el engrosamiento de un muestreo fijo denso. Tanto
Line como Curve seleccionan uno de cinco anchos; el trazo barre la huella de
`PaintCore::make_line_stamp_offsets()` (`inc/core/line_stamp.h`): 1 px, cuadrado
2x2, y cruces centradas de 3/4/5 px de longitud con brazos de 1/2/3 px. El ancho
se congela al empezar el trazo, igual que los colores, y la huella expande el
`DrawBuffer` por su margen para no recortarse en los extremos. Line usa
`PaintCore::rasterize_line()` (`inc/core/line.h`), el rasterizador común que
también usa `FreehandTool` (`inc/tools/freehand_tool.h`) para unir la posición
anterior con la actual en cada frame; Brush y Eraser heredan de `FreehandTool`.
`CircleTool` (`inc/tools/circle_tool.h`) dibuja una elipse inscrita en el
bounding box del arrastre y, con Shift, aplica la misma restricción de cuadrado
que Rect/Selection para obtener un círculo perfecto. El relleno usa spans
horizontales por fila con `PaintCore::rasterize_ellipse_spans()`
(`inc/core/ellipse.h`), no círculos concéntricos; el contorno se deriva de los
mismos spans con `rasterize_ellipse_outline()`, así que Outline, Fill y Both
coinciden. `BrushTool` (`inc/tools/brush_tool.h`) usa las máscaras de
`PaintCore::make_brush_offsets()` (`inc/core/brush.h`): círculos de radio 3/2/1
(7x7, 5x5 y 3x3), cuadrados 8/5/2 y diagonales 45/135 de 8/5/2, en ese orden y
de mayor a menor. `FreehandTool::stamp_margin()` expande el `DrawBuffer` para
que una huella grande no se recorte en los bordes. `stamp_segment_step()` es un
hook que Brush usa para añadir la línea intermedia cuando una brocha diagonal
se mueve en la diagonal contraria; así no queda un hueco sin engordar los
trazos paralelos. `CanvasArea` pinta la huella seleccionada
semitransparente, centrada en el píxel bajo el hotspot, por encima del canvas y
la rejilla y por debajo del cursor del sistema; es solo un overlay de vista. Si
el color de la huella es el transparente de la paleta, el overlay muestra el
damero, y los píxeles transparentes del trazo activo también lo muestran durante
el preview aunque el tile cache aún tenga el píxel anterior. El preview del
trazo se cachea por tiles de 256×256 píxeles de imagen (`CanvasArea`), con una
revisión del `DrawBuffer` para no re-subir texturas sin cambios; si no se puede
crear la textura se vuelve al render por píxeles.
`EraserTool` (`inc/tools/eraser_tool.h`) usa cuadrados de 10/8/6/4 px en una
lista vertical de menor a mayor, con 4 por defecto. El botón izquierdo pinta el
background; el derecho solo sustituye los píxeles de la huella que tengan el
foreground, comprobando cada píxel contra la imagen principal. Su preview es opaco con el color de background y un borde
fijo de 1 px de pantalla (`ToolBase::StampPreview`), y su cursor es invisible
sobre la imagen. Pulsar ambos botones cancela la goma. El canvas
soporta resize de dominio con undo/redo por
snapshots: cada paso de resize guarda solo el estado previo y reconstruye el
posterior con `ImageBuffer::resize`, y el diálogo
`Image > Properties...` permite cambiar ancho, alto y bpp. Al cambiar el bpp se
avisa en el propio diálogo y se remapea cada píxel al tono más cercano de la
nueva paleta; el paso entra en el historial y undo/redo regeneran la paleta del
bpp restaurado. `CanvasResizeGizmo` dibuja los
handles 1 px fuera del borde real y solo cuando ese borde está visible: derecho
e inferior se centran en el tramo visible de su borde y desaparecen si el tramo
es menor que el handle; la esquina solo aparece al alcanzar la esquina real y
sirve de fallback para canvas pequeños. El `ViewTransform` ya no ajusta el
scroll a píxeles completos: sigue 1:1 a ImGui y puede mostrar un píxel parcial
en el borde superior/izquierdo. Al cambiar el zoom, `PaintFrontend` declara el
nuevo content size antes de `BeginChild("Canvas")` para que ImGui recalcule
`ScrollMax` y recorte el scroll en el mismo frame, evitando el parpadeo en los
bordes. La rejilla, el recorte al contenido del child y
los bordes están validados.

La separación del lienzo es:
- `CanvasDocument` (`paint/canvas_document.h`): solo datos. `ImageBuffer`,
  `UndoHistory`, resize, `transform_image` (flip/rotación de píxeles; 90/270
  intercambian dimensiones y el undo guarda solo el tipo de transformación, sin
  copia completa), commit de `DrawBuffer`, `apply_pixel_patch` y render de la
  imagen. No conoce tools, selección ni portapapeles.
- `FloatingRegionEditor` (`paint/floating_region_editor.h`): estado de edición
  flotante. Posee `FloatingRegion` y el portapapeles, y aplica begin/move/
  commit/cancel, copy/cut/paste, invert y la máscara de transparencia sobre un
  `CanvasDocument`.
- `PaintSession` (`paint/paint_session.h`): posee documento, editor flotante y
  tool activa. Coordina undo/redo (cancelar región o trazo), set_tool y resize
  (confirmar región antes), y reenvía la API de documento/selección a la UI.
- `PaintBackend` es el estado de aplicación: posee `PaintSession`, la paleta y
  sus colores, la tool activa y los recursos de toolbar/cursores. La vista
  consulta `session()`, `palette()`, `foreground_index()` y
  `background_index()`, y ejecuta comandos como `select_palette()`,
  `toggle_palette_inverted()`, `set_transparent_foreground()`,
  `clear_transparent_color()` o `selectButton()`.
- `DrawBuffer` guarda además una máscara de cobertura (`std::vector<bool>`,
  empaquetada a nivel de bit) para distinguir los píxeles escritos de los no
  escritos. El índice almacenado deja de ser un centinela: pintar 255 o el
  índice transparente es una escritura normal. `is_painted()`/`painted()` son la
  fuente de verdad para previews y commits.
- `PaintFrontend` es la vista/controller ImGui: layout, menús, toolbar, canvas y
  panel de paleta. Solo conserva estado transitorio de vista (zoom, rejilla,
  popups y campos de diálogo). La implementación está dividida en
  `paint_frontend.cpp` (núcleo/render), `paint_frontend_menu.cpp`,
  `paint_frontend_dialogs.cpp`, `paint_frontend_file.cpp`,
  `paint_frontend_actions.cpp` y `paint_frontend_helpers.cpp`; la API pública
  sigue en `inc/paint/paint_frontend.h`.
- `CanvasArea` recibe `PaintSession&` del backend; no la posee. Las tools reciben
  `PaintSession&`, no `CanvasDocument&`. La imagen se sube como tiles de 256×256
  (`paint/canvas_tile_grid.h`) con cache de la zona visible más un margen de un
  tile; los tiles que salen se destruyen, el rect sucio refresca los afectados y
  un fallo de creación cae al render por píxel. Así el VRAM no depende del
  tamaño del canvas. También pinta un borde de 2 px (`ImGuiCol_HeaderActive`)
  justo fuera de los bordes derecho e inferior; rejilla y handles lo tapan en
  parte sin mover posiciones.

La selección rectangular vive en `FloatingRegionEditor`; `SelectionTool` solo
crea el marquee y pasa el background/modo. `SelectionTool::select_all()` crea la
misma región flotante para todo el canvas con su modo actual (Normal/Transparent)
y su background guardado; Edit > Select All y Ctrl+A lo invocan tras activar la
tool. `CanvasArea` gestiona el arrastre, el
commit al clicar fuera, la cancelación con clic derecho, el cursor `Move` y el
render de hueco/contenido/marquee. El hueco se rellena con el color de
background al confirmar. `FloatingRegion` expone el contenido solo en lectura y
lo reemplaza con `set_content()`; `revision()` avanza con cada reemplazo o
cambio de máscara. El commit construye el bloque final y lo aplica con
`CanvasDocument::apply_pixel_patch`, que decide si hay historial comparando
before/after. El menú `Selection` ofrece Copy, Cut, Paste, Delete e Invert Colors; Delete
(Del) rellena el hueco original con el background guardado y descarta el
contenido sin copiarlo al portapapeles. El menú File > Open usa los filtros
`Indexed Image` y `All files`, para poder elegir también `.ipx`; si el archivo
no es válido, el error se muestra en `File Error`. El portapapeles y la inversión
(`max_index - index`) viven en `FloatingRegionEditor`. Paste se coloca en
`CanvasArea::visible_image_origin()` (la esquina superior izquierda visible, con
scroll/zoom) y el cursor `Move` carga `assets/cursors/move.ipx` mediante
`PaintBackend::loadCursor(CursorType, ...)`; si el asset falta, se mantiene el
cursor `SIZEALL` del sistema. `FloatingRegionEditor::transform_floating_region()`
invierte o rota el contenido sin confirmar ni cancelar la región: los flips
mantienen el rect; 90/270 intercambian ancho/alto y recentran el rect (con clamp
para dejar al menos 1 px visible). Selection > Flip/Rotate... (Ctrl+R) abre el
modal con Flip horizontal/vertical y Rotate 90/180/270; OK transforma y Cancel
no hace nada. Image > Flip/Rotate... usa el mismo modal con «Transform image»:
acepta antes el trazo activo, la región flotante o la edición pendiente (un punto de historial) y
después transforma el canvas (otro punto); el undo de la transformación guarda
solo el tipo y aplica el inverso. Selection tiene dos modos (`Normal` y
`Transparent`): en transparente, el editor conserva los índices originales del
contenido y aplica una máscara viva al dibujar y al confirmar, saltando los
píxeles iguales al background. `PaintSession` expone
`set_floating_transparency()`; la tool la sincroniza al cambiar de modo o de
background (`ToolBase::sync_floating_region`). La máscara no reescribe el
contenido: `invalidate()` incrementa la revisión para refrescar la textura.
Copy/Cut/Paste conservan los píxeles crudos; el menú Paste cambia a Selection
para usar su modo actual. Las
texturas del backend usan `SDL_BLENDMODE_BLEND` para respetar el alfa del
contenido flotante y de los iconos.
Cambiar de tool y `resize` confirman la región; `undo` la cancela y `redo` queda
bloqueado mientras flota. No hay handles de selección ni menú contextual todavía.
El menú `Settings > Theme` ofrece los presets de color de ImGui
(`Light`, `Dark` y `Classic`, en ese orden; `Light` por defecto) y
`PaintFrontend::set_theme()` los aplica en
caliente. Los presets solo cambian colores, no métricas de layout; la elección
todavía no se persiste.
El orden del toolbar es Selection, Eraser, Fill, Pick Color, Pencil, Brush,
Line, Curve, Rect y Circle (2 columnas); `main` selecciona Pencil al arrancar.
Cada botón usa un único `assets/*_button.idx` de 32×32 (índice 0 transparente y
el resto en gris); los botones no activos tienen fondo transparente y
hover/pressed usan un overlay neutro del color del texto; solo el activo usa
`ImGuiCol_ButtonActive`.
La zona de opciones reparte su alto entre las opciones con iconos (Rect/Circle,
Selection y Fill); Brush y Eraser mantienen sus previews compactos. Line y Curve
reducen la altura de sus cinco filas para que quepan y muestran previews de
líneas de 1 a 5 px. Las opciones se activan al pulsar el botón izquierdo, no al
soltarlo, tanto en listas como en la cuadrícula de Brush. Los iconos
de Fill usan celdas 60x46 en `assets/fill_options.idx`.
La ventana incluye una barra de estado inferior siempre visible (child `Status`,
incluida en el mínimo de ventana) con tamaño del canvas, zoom, coordenadas,
color bajo el puntero e indicador `Modified`/`Saved`. Durante las operaciones de
dos puntos (`Selection`, `Line`, `Curve` al crear la recta, `Rect` y `Circle`)
la primera coordenada queda fija en el punto de pulsación y aparece una segunda
con la posición actual; mover una región flotante o editar handles de Curve
muestra solo la coordenada actual. Las tools lo exponen con
`ToolBase::drag_anchor()`.
Los menús son File, Edit, View, Selection, Image, Palette, Settings y Help.
Edit añade Select All (Ctrl+A, activo: selecciona todo el canvas con el modo y
background actuales de Selection); View contiene Grid (Ctrl+G) y
Zoom + / Zoom - (Ctrl++ / Ctrl+-), ambos activos; Selection añade
Flip/Rotate... (Ctrl+R, activo: modal de flip/rotación sobre la región flotante)
y mantiene Ctrl+C/X/V, Del e Invert Colors;
Image contiene Flip/Rotate... (Ctrl+R, activo: transforma todo el canvas,
aceptando antes el trazo activo, la región flotante o la edición pendiente) y
Properties... (Ctrl+E,
antiguo Canvas Size). Ctrl+R usa el objetivo Selection si hay una región
flotante y, si no, el canvas; el menú Image siempre apunta al canvas. Help
contiene About QtreGBPaint... (modal con el logo PNG, “Made by kirbyboy”, la
licencia GPL-3.0-or-later y los enlaces al proyecto). Save As usa
Ctrl+Shift+S. Los elementos sin funcionalidad se muestran deshabilitados.
El zoom (slider de la paleta, View > Zoom +/- y Ctrl+rueda) mantiene fijo el
punto de imagen bajo el cursor cuando el ratón está sobre el canvas; si no, usa
el centro de la parte visible. `ViewTransform::scroll_for_zoom_anchor()` calcula
el scroll y el frontend lo aplica con `SetNextWindowContentSize()` +
`SetNextWindowScroll()` en el frame siguiente. `CanvasArea::mouse_over_canvas()`
evita mostrar coordenadas/color de imagen cuando el puntero está sobre la barra
de estado, la paleta o los scrollbars.
El paquete Debian `0.9.1` se genera con `make deb` (`packaging/build_deb.sh`):
instala el binario en `/usr/bin`, los assets en `/usr/share/qtregbpaint/assets`,
el `.desktop`, el icono, el MIME XML y la metadata AppStream en
`io.github.kirbyboy.QtreGBPaint.metainfo.xml`. `main::resolve_asset_base()`
busca los assets junto al ejecutable, en `../assets`, en
`../share/qtregbpaint/assets`, en `/usr/share/qtregbpaint/assets` y en
`/usr/local/share/qtregbpaint/assets`. El icono de ventana se sube con
`SDL_SetWindowIcon` (64×64; X11 rechaza iconos mayores) y el class/app_id es
`io.github.kirbyboy.QtreGBPaint`, que coincide con el `.desktop` y su
`StartupWMClass`. La CI de GitHub Actions (`.github/workflows/ci.yml`) ejecuta
`make deps`, `make`, `test-full` y los 42 smokes bajo Xvfb; la primera
ejecución terminó en verde.
Tests actuales: 269 casos y 9780 aserciones (normal); `test-full` suma
1076 casos y 39120 aserciones. Smokes: 42 en verde. Revisar `HANDOFF.md` y
`git diff` antes de tocar estos archivos.
