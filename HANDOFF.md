# HANDOFF.md

Contexto de traspaso del proyecto QtreGBPaint.

## Backup automático

Al leer este handoff, ejecutar `tmp/backup/backup.sh` para crear un backup del código
en `tmp/backup/<yyyymmdd_hh24mmss>/`.

## Qué es

Paint minimalista con paletas indexadas inspiradas en consolas portátiles retro
(documentos de 1 a 8 bpp). Base: Paint de Windows XP.
Stack: C++17, ImGui, SDL2. Backend abstracto (`IGuiBackend`).

## Licencia y marca

- Código: **GPL-3.0-or-later** en `LICENSE.txt`; el texto oficial de GPLv3 está
  incluido literalmente.
- `TRADEMARK.md` (inglés) y `TRADEMARK.es.md` (traducción informativa) regulan
  nombre, logo e icono. Titular público: «the QtreGBPaint project»; contacto por
  issues en <https://github.com/kirbyboy/QtreGBPaint/issues>. No se publican
  nombre ni email privados; para litigios o registro habrá que acreditar la
  identidad en privado.
- La marca no está registrada. Se usa `™`; la protección sin registro es más
  débil y menos predecible.
- `THIRD_PARTY_NOTICES.md` y su traducción recogen SDL2, ImGui,
  ImGuiFileDialog, dirent, stb, doctest, zlib y el runtime de GCC.
- Icono y assets: `assets/QtreGBPaint.png` se generó con ChatGPT y los assets
  visuales originales están dedicados al dominio público bajo CC0 1.0
  (`assets/LICENSE`, con traducción `.es.md`). La dedicatoria cubre solo
  copyright; el icono y el logotipo siguen siendo Marcas y su uso se rige por
  `TRADEMARK.md`. Todos los cursores, incluido Move, son originales del
  proyecto y están cubiertos por la misma CC0.

## Estructura principal

- `inc/core`: bezier, brush, color, cursor, backend, imgui manager, ellipse, fill, line, modifiers, shape_constraint, vec2, rect, view_transform, window_geometry, util.
- `inc/paint`: app_config, theme, canvas_document, canvas_area, canvas_grid, canvas_renderer, canvas_resize_gizmo, canvas_tile_grid, checkerboard, png_image, cursor_asset, document_file, floating_region, floating_region_editor, paint_session, image_buffer, image_export, draw_buffer, indexed_image, palette, paint_backend, paint_frontend.
- `inc/tools`: base_tool, draw_tool, freehand_tool, line_tool, curve_tool, pencil_tool, brush_tool, eraser_tool, fill_tool, pick_color_tool, selection_tool, shape_tool, square_tool, circle_tool, tool_option.
- `scripts/`: `bootstrap_deps.sh` y los Makefiles de imgui/ImGuiFileDialog.
- `src/`: implementaciones correspondientes.
- `assets/`: iconos de toolbar `*_button.idx` (32×32, 3 bpp), cursores `.ipx`
  en `assets/cursors/` (todos creados por el proyecto), hojas de opciones
  `rect_options.idx`, `selection_options.idx` y `fill_options.idx`, icono final
  `QtreGBPaint.png` (256×256 RGBA) y las licencias `LICENSE`/`LICENSE.es.md`.
- `README.md`: presentación en inglés con características, build, uso, licencia y capturas; las imágenes están en `docs/screenshots/`.
- `packaging/`: `.desktop`, icono, MIME XML, metadata AppStream, script de AppImage y script del `.deb`.
- `tests/`: tests headless con doctest y `tests/Makefile` autónomo.
- `tests/smoke/`: smokes SDL/ImGui y `run.sh`; escriben binarios y capturas en `temp/`.
- `.github/workflows/ci.yml`: CI que ejecuta `make deps`, `test-full` y todos los smokes bajo Xvfb.
- `lib/`: dependencias locales (imgui, doctest). `lib/ImGuiFileDialog` contiene ImGuiFileDialog v0.6.8 y su Makefile; el directorio no se versiona y se rellena con `make deps`.
- `build/`: binarios debug y release generados por Makefile.
- `obj/`, `deps/`, `obj-release/`, `deps-release/`: objetos y dependencias generados.
- `Makefile`: build debug/release; `make test` delega en `tests/Makefile`.
- `LICENSE.txt`, `assets/LICENSE`/`.es.md`, `TRADEMARK.md`/`.es.md`, `THIRD_PARTY_NOTICES.md`/`.es.md`: licencia GPL-3.0-or-later, licencia CC0 de los assets, política de marca y avisos de terceros.

## Flujo de pruebas

- Suite normal: `make -j"$(nproc)" test` (muy rápida cuando está compilada).
- Un solo binario: `make -C tests test-circle` (el stem mapea a
  `tests/build/test_<stem>`). Para un caso de doctest concreto:
  `tests/build/test_circle --test-case="*Shift*"`.
- Matriz completa: `make -j"$(nproc)" test-full`, que ejecuta la suite normal
  más ASAN, UBSAN y ASAN+UBSAN. Las variantes usan directorios separados y
  comparten el jobserver.
- Smokes: `./tests/smoke/run.sh circle buttons` compila en paralelo y ejecuta
  solo esos; sin argumentos ejecuta todos. `--list`, `--build-only`,
  `--no-build` y `-j N` están soportados. El script vive en `temp/` (ignorado
  por git).
- Durante el desarrollo usar el test/smoke afectado; reservar la matriz completa
  y todos los smokes para cerrar un bloque.

## Conceptos clave

- `ImageBuffer`: imagen en memoria (vector de bytes, 1 byte/pixel = índice de paleta). Dimensiones, bpp y datos son privados; se accede con `width()`, `height()`, `bitsPerPixel()` y `data()` (solo lectura). El bpp válido es 1..8 (2bpp => 4 niveles). `resize(w,h,fill)` conserva la parte superior izquierda y rellena la zona nueva con `fill`.
- `DrawBuffer`: buffer temporal de dibujo; origen variable; se mezcla sobre el canvas al confirmar. Guarda una máscara de cobertura `std::vector<bool>` (`is_painted()`, `painted()`) para distinguir lo pintado de lo no pintado; ya no hay índice transparente sentinela y pintar 255 es una escritura normal.
- `CanvasDocument` (`inc/paint/canvas_document.h`): dominio puro (sin ImGui) del documento. Solo guarda `ImageBuffer` + `UndoHistory`; ofrece `resize`, `change_bits_per_pixel` (remapea los índices con un mapping y guarda el bpp anterior para undo), `transform_image` (flip/rotación de píxeles; 90/270 intercambian dimensiones), `commit_draw_buffer`, `apply_pixel_patch`, `replace_image` (New/Open), undo/redo, límites de historial y render de la imagen vía `ICanvasRenderer`. La imagen solo se expone en lectura y `take_dirty_rect()` entrega la región sucia a la vista. `current_state_id()` identifica el estado actual del historial. No conoce tools, selección, portapapeles ni máscaras.
- `FloatingRegionEditor` (`inc/paint/floating_region_editor.h`): estado de edición flotante. Posee la `FloatingRegion` y el portapapeles, y opera sobre un `CanvasDocument`: `begin_selection`, `begin_paste`, `move_floating_region`, `commit/cancel`, `set_floating_transparency`, `copy/cut/delete/paste`, `invert_floating_region_colors` y `transform_floating_region` (flip horizontal/vertical y rotate 90/180/270; no confirma ni cancela la región). El commit aplica hueco + contenido y registra un único `PixelPatch` en el documento. Delete rellena el hueco guardado con `hole_fill` y descarta el contenido sin tocar el portapapeles.
- `PaintSession` (`inc/paint/paint_session.h`): sesión de edición. Posee `CanvasDocument`, `FloatingRegionEditor` y la tool activa. Coordina `undo/redo` (cancelar región flotante, edición pendiente o trazo), `set_tool`, `resize` y `transform_image` (confirmar trazo activo/región/edición pendiente antes; el transform puede dejar dos pasos de historial), y reenvía a la UI la API de documento, selección y `has_pending_edit`/`commit_pending_edit`/`cancel_pending_edit`.
- `FloatingRegion` (`inc/paint/floating_region.h`): contenido movible que todavía no forma parte del canvas. La comparten selección y paste: `content()` (ImageBuffer, solo lectura), `set_content(ImageBuffer)` (reemplazo que siempre incrementa `revision()`), `revision()`, `rect` (posición actual), `hole` (zona original que se rellenará con `hole_fill` al confirmar), `active`, `transparent_background` y `transparent_color`. Solo puede existir una a la vez y la maneja `FloatingRegionEditor`. Un paste no tiene hueco. El commit construye hueco + contenido en memoria y lo aplica como un único `PixelPatch`; los píxeles que la máscara viva marca como transparentes se saltan y lo que cae fuera del canvas se ignora. La transparencia no reescribe `content`: se aplica al dibujar y al confirmar; `invalidate()` incrementa la revisión cuando solo cambia la máscara.
- Portapapeles de `FloatingRegionEditor` (`m_clipboard`): `copy_to_clipboard()`, `cut_to_clipboard()`, `paste_from_clipboard(pos)`, `has_clipboard()` y `clipboard()` mantienen los píxeles copiados en el dominio y sin aplicar la máscara de transparencia. Copy no confirma la región; Cut copia y rellena el hueco con el background; Paste crea una nueva región flotante en `pos` (el menú usa `CanvasArea::visible_image_origin()`, la esquina superior izquierda visible), confirma la anterior si existía y cambia a Selection para aplicar su modo actual. `invert_floating_region_colors()` invierte los índices crudos del contenido con `max_index - index` (`max_index = 2^bpp - 1`, en 2bpp `3 - i`) y reemplaza el contenido con `set_content`, que incrementa la revisión.
- `CanvasArea`: vista ImGui del canvas; recibe `PaintSession&` de `PaintBackend` (no la posee) y la usa para documento, selección y tools. Gestiona hover/active y construye un `ViewTransform` por frame, compartido por input y render. La imagen se sube como tiles de 256×256 (`paint/canvas_tile_grid.h`): solo se cachean los tiles visibles más un margen de uno, se destruyen los que salen y cada frame se consume `take_dirty_rect()` para refrescar los tiles afectados. Un cambio de paleta re-sube todos los tiles en caché y un cambio de tamaño los destruye; si crear un tile falla, se usa el render por píxel. Cuando la paleta tiene un índice transparente, pinta un damero de 8 px en espacio de pantalla anclado al origen de la imagen debajo de los tiles (con `PaintUi::CheckerboardTexture`, o por celdas si la textura falla); el preview de la tool y la huella del cursor muestran damero para los píxeles transparentes (incluidos los que el tile cache todavía tiene como opacos). El resize gráfico lo delega en `CanvasResizeGizmo`. Mientras hay una `FloatingRegion`, intercepta el input antes que los handles y las tools: arrastre con el botón izquierdo, commit al clicar fuera, cancelación con clic derecho y cursor `Move`; dibuja el hueco, el contenido flotante y el marquee (recortados al canvas) y oculta los handles de resize. Con una edición pendiente (p. ej. Curve) hace lo propio: los handles se arrastran con el botón izquierdo, el clic fuera los confirma, el derecho cancela y `Enter`/`Escape` también; pinta guías y handles como overlay y desactiva los handles de resize. También pinta un borde de 2 px con `ImGuiCol_HeaderActive` justo fuera de los bordes derecho e inferior; la última línea de la rejilla cubre el primer píxel y los handles cubren el segundo, sin mover nada.
- `CanvasResizeGizmo`: handles de resize en espacio de pantalla (derecha, abajo y esquina), guía de preview y commit al soltar. Cada handle se ancla al borde real 1 px por fuera y solo aparece cuando ese borde está visible; los de borde se centran en el tramo visible y se ocultan si el tramo es menor que el handle. La esquina aparece solo al alcanzar la esquina real y sirve de fallback para canvas pequeños. `handle_offset` permite un desplazamiento extra.
- `CanvasGrid`: overlay visual de rejilla estilo mspaint. Usa dos texturas 1D con píxeles alternos gris oscuro/gris claro y dibuja un único quad por línea (no segmentos de 1 px). Se activa con `View > Grid`, por defecto marcado, y solo se pinta desde zoom ×6. No toca el `ImageBuffer` ni el historial.
- `PaintFrontend`: vista/controller ImGui. Calcula el layout dinámico y el tamaño mínimo de ventana, pinta menú, toolbar, canvas y panel de paleta, y traduce input/acciones a comandos del backend. No posee el documento, el historial, la paleta ni los colores activos: solo estado transitorio de vista (zoom, rejilla, tema, popups y campos de diálogo). `Settings > Theme` aplica los presets Light/Dark/Classic en caliente y `AppConfig` persiste la elección. El menú `Image > Properties...` abre el diálogo de canvas size (ancho, alto y bpp); si el bpp cambia, muestra un aviso y remapea los píxeles al confirmar. El menú `Palette` selecciona la tonalidad y la inversión; `Selection` incluye Copy (Ctrl+C), Cut (Ctrl+X), Paste (Ctrl+V), Delete (Del), Flip/Rotate... (Ctrl+R, modal activo) e Invert Colors. `Image > Flip/Rotate...` usa el mismo modal con objetivo Canvas; Ctrl+R elige Selection si hay región flotante y, si no, Canvas. El menú `File` ofrece New (Ctrl+N), Open (Ctrl+O), Save (Ctrl+S), Save As... (Ctrl+Shift+S), Export > Bitmap/PNG (BMP 24 bits y PNG indexado), hasta 10 archivos recientes y Exit (Alt+F4), con `ImGuiFileDialog`, modales `New Image`, `Unsaved Changes`, `IDX Options` y `File Error`; Open ofrece los filtros `Indexed Image` y `All files`, y el nombre del archivo y el `*` de modificado se muestran en el título de la ventana (consultados a `PaintBackend`). Las opciones con iconos reparten el alto disponible del hueco (`Rect`/`Circle`, `Selection`, `Fill`); Brush y Eraser conservan sus previews compactos. La barra de estado inferior es un child `Status` siempre visible, incluido en el mínimo de ventana: muestra tamaño del canvas, zoom, coordenadas, color bajo el puntero y `Modified`/`Saved`. Con `ToolBase::drag_anchor()` muestra dos coordenadas durante `Selection`, `Line`, `Curve` (solo al crear la recta), `Rect` y `Circle`; mover una región flotante o editar handles de Curve deja una sola. El zoom (slider, View > Zoom +/- y Ctrl+rueda) se ancla al cursor si está sobre el canvas o al centro visible si no; `ViewTransform::scroll_for_zoom_anchor()` calcula el scroll y el frontend lo aplica en el frame siguiente con content size + `SetNextWindowScroll`. `Help > About QtreGBPaint...` abre un modal con el logo cargado desde `assets/QtreGBPaint.png` mediante `PaintCore::load_png_rgba` (stb_image), “Made by kirbyboy”, la licencia GPL-3.0-or-later, la licencia CC0 de los assets y enlaces al proyecto.
- `IGuiBackend::set_minimum_window_size`: permite al frontend imponer un mínimo de ventana al backend; `SDLRendererBackend` lo aplica con `SDL_SetWindowMinimumSize`.
- `IGuiBackend::poll_events()`/`quit_requested()`: el backend SDL registra el cierre de ventana (`SDL_QUIT`) sin terminar el bucle por sí mismo; `PaintFrontend::render()` decide si cerrar o abrir `Unsaved Changes`, y `main` usa el retorno booleano de `render()` para salir.
- `IGuiBackend::set_cursor(CursorType)`: permite al frontend pedir el cursor de la tool activa. `SDLRendererBackend` posee los cursores del sistema (toolbar e ImGui) y gestiona `SDL_SetCursor`; activa `ImGuiConfigFlags_NoMouseCursorChange` de forma permanente para que ImGui no los sobrescriba.
- `IGuiBackend::set_cursor_image(CursorType, CursorImage)`: sustituye el cursor nativo de una herramienta por un bitmap RGBA. `SDLRendererBackend` lo convierte con `SDL_CreateColorCursor`; si algo falla, conserva el cursor del sistema. `PaintBackend::loadCursor()` tiene dos variantes: por nombre de botón (asocia el `.ipx` a `ToolButton::cursor`) y por `CursorType` (para overrides como `Move`). `assets/cursors/move.ipx` se carga con la segunda; si falta o es inválido, se mantiene `SDL_SYSTEM_CURSOR_SIZEALL`.
- `CursorImage` (`inc/core/cursor.h`): bitmap RGBA independiente de plataforma con `width`, `height`, `hotspot_x`, `hotspot_y` y píxeles.
- `.ipx`: formato de cursor basado en el mismo texto `IndexedImage` que `.idx`, con convención `0`=transparente, `1`=hotspot (exactamente uno), `2`=negro y `3`=blanco. Todos los assets usan exactamente 2 bpp (índices 0..3). El cursor Brush es la única excepción: al cargarse, su índice 2 se pinta como gris oscuro `(96,96,96)`; el archivo sigue usando el mismo 2 que el resto. Lo carga `PaintCore::load_cursor_asset()` (`inc/paint/cursor_asset.h`).
- `CanvasSnapshot` / `UndoHistory`: los pasos normales guardan `PixelPatch` (rect + before + after); los pasos que cambian dimensiones guardan solo el snapshot `before_canvas` y `CanvasResize{width,height,fill}`. Los pasos de cambio de bpp guardan `before_canvas` y `CanvasBitsPerPixel{bits_per_pixel,mapping}`; `redo` restaura el snapshot anterior y vuelve a aplicar el mapping. Los pasos de transformación guardan solo `ImageTransform` (sin copia de píxeles): `undo` aplica la transformación inversa y `redo` la original. `undo`/`redo` restauran dimensiones, bpp, píxeles y dirty rect. Cada paso recibe además un `state_id` monotónico; 0 es el estado inicial y `current_state_id()` identifica el estado actual. Guardar registra ese id para detectar si el documento volvió al punto guardado. `UndoStep::bytes()` cuenta snapshots, patches y mappings; los pasos de transformación ocupan 0 bytes.
- `ToolBase` -> `DrawTool` (dibujo: press/move/release y colores fg/bg) y
  `SelectionTool` (solo marquee: al soltar llama a
  `PaintSession::begin_selection` y no posee píxeles ni estado de movimiento;
  tiene modos Normal y Transparent). Las tools reciben `PaintSession&`, no el
  documento directamente. `ToolBase::set_colors()` permite al
  frontend actualizar los índices de paleta sin conocer la tool concreta.
  `ToolBase::has_marquee()`/`marquee_rect()` exponen el rectángulo transitorio
  que pinta `CanvasArea` mientras se arrastra.
  `ToolBase::stamp_preview()` expone la huella y el estilo de preview que
  `CanvasArea` pinta como overlay bajo el cursor.
  `ToolBase::has_pending_edit()`/`commit_pending_edit()`/`cancel_pending_edit()`
  y `EditOverlay`/`EditGuide`/`EditHandle` permiten tools multipaso con handles
  editables; `PaintSession` las hace exclusivas con la selección flotante y
  `CanvasArea` pinta y enruta los handles. `CurveTool` es el primer usuario.
- `SquareTool`: hereda de `ShapeTool`, dibuja un cuadrado con los colores activos.
- `CircleTool`: hereda de `ShapeTool`, dibuja una elipse inscrita en el bbox del
  arrastre y, con Shift, un círculo perfecto. Rellena por spans horizontales
  (`rasterize_ellipse_spans`) y deriva el contorno de esos mismos spans.
- `BrushTool`: hereda de `FreehandTool` y estampa una de sus 12 huellas en cada
  píxel del recorrido interpolado. Las máscaras viven en
  `PaintCore::make_brush_offsets()` (`inc/core/brush.h`) y el tool expone la
  huella activa con `stamp_preview()`. `stamp_margin()` amplía el `DrawBuffer`
  para que la huella no se recorte.
- `EraserTool`: hereda de `FreehandTool`; cuadrados de 10/8/6/4 px. Izquierdo
  pinta background; derecho solo sustituye píxeles con foreground, comprobando
  cada píxel de la huella contra la imagen principal. Su preview es opaco con
  borde fijo de 1 px de pantalla y su cursor es invisible.
- `FillTool`: hereda de `DrawTool`; bote de pintura con modos `Contiguous` y
  `Global`. Usa `PaintCore::collect_flood_fill_spans()` /
  `collect_global_fill_spans()` (`inc/core/fill.h`), un scanline flood fill
  4-conectado por spans. Aplica y confirma el relleno al pulsar (no hay preview
  de arrastre) para no repintar el área con quads cada frame; el botón derecho
  usa el background. Sus iconos de opción están en `assets/fill_options.idx`.
- `PickColorTool`: hereda de `ToolBase`; cuentagotas. Mientras se mantiene
  pulsado el botón izquierdo o derecho muestra el color bajo el puntero en la
  zona de opciones; al soltar, lee el índice del píxel y lo guarda una vez en
  `ToolBase::ColorPick`. Si el release cae fuera de la imagen no cambia nada;
  cualquier índice es válido, incluido el que la paleta renderiza como
  transparente. El backend lo consume con
  `apply_pending_color_pick()`: izquierdo actualiza foreground, derecho
  background, y después vuelve a la última herramienta de edición.
  `ToolBase::is_operating()` hace que `CanvasArea` enrute el release aunque la
  tool no pinte. Su icono está en
  `assets/pick_*.idx` y su cursor en `assets/cursors/pick.ipx`.
- `CurveTool`: hereda de `DrawTool`; Bézier cúbica con dos handles visibles. El
  primer arrastre fija A/B, con Shift aplicando el mismo `snapped_line_end()` que
  Line; al soltar entra en edición pendiente con P1/P2 en 1/3 y 2/3 de la línea.
  Los handles se mueven con drag y el preview se rasteriza con
  `PaintCore::rasterize_cubic_bezier()` (`inc/core/bezier.h`) mediante
  subdivisión adaptativa (tolerancia 0.5 px) y unión de tramos con
  `rasterize_line()`. Shift ya no afecta a los handles. Enter, clic fuera de los
  handles o cambio de tool confirman (un único paso de historial); Escape,
  botón derecho o ambos botones cancelan. Los colores quedan congelados durante
  la edición y los handles se recortan al canvas. La tool implementa el ciclo
  `has_pending_edit`/`commit_pending_edit`/`cancel_pending_edit` y expone
  `EditOverlay` (guías A-P1-P2-B, anclas A/B y handles P1/P2).
- `ToolOption` / `ToolOptionChoice` (`inc/tools/tool_option.h`): descriptor de una opción de herramienta. `Choice` mantiene siempre un valor seleccionado; `Integer` se recorta a `[min,max]`. `visible_in_options_bar` decide si se pinta en el hueco reservado; `icon` es una clave de icono resuelta por `PaintBackend`; `preview` es una huella opcional (offsets) para listas o cuadrículas; `grid_columns` pinta una opción `Choice` como cuadrícula compacta. `ToolBase` posee las opciones (`add_option`, `options()`, `set_option()`, `option_value()`) y las tools las declaran en su constructor.
- `PaintFrontend::renderToolOptions()`: pinta las opciones visibles de la tool activa. Para `Choice` usa filas verticales de ancho completo con iconos de 60x16 o previews de huella, o una cuadrícula si `grid_columns > 1`; para `Integer`, un slider compacto. No conoce tools concretas.
- `ShapeTool::Mode`: `Outline` (solo borde), `Fill` (todo el interior con el color de fondo) y `Both` (borde + relleno). Por defecto `Both`; Outline deja el interior transparente (también su icono, con índice 0 para que se vea el color del tema). Lo comparten `SquareTool` y `CircleTool`.
- `AppConfig` (`inc/paint/app_config.h`): preferencias globales persistentes. Se carga una vez al arrancar y se guarda al salir en `$XDG_CONFIG_HOME/qtregbpaint/config.ini` (fallback `~/.config/qtregbpaint/config.ini`) con escritura atómica (`.tmp` + rename). Formato plano `key=value`; las rutas se escriben con `std::quoted`. Persiste tema (`dark`/`light`/`classic`), `grid`, `window.x/y/width/height/maximized`, `last_directory` y hasta 10 `recent_file`. Los valores desconocidos o malformados se ignoran y se mantiene el default; las rutas con caracteres de control se descartan para no propagar una configuración corrupta. No persiste herramienta, paleta ni zoom.
- `PaintFrontend` configura los diálogos de Open/Save As con `last_directory`; si la ruta guardada no existe o no se puede leer, cae silenciosamente a `XDG_PICTURES_DIR`, `~/Pictures`, `$HOME` o `"."`, sin modal de error. Recuerda la carpeta tras abrir o guardar correctamente.
- `SDLRendererBackend::init_with_geometry()` crea la ventana oculta, aplica la geometría guardada (si la posición no cae en ningún monitor se centra), maximiza si procede y la muestra. `window_geometry()` devuelve la última geometría no maximizada y el flag de maximizado; `poll_events()` la actualiza cada frame, ignorando los estados maximizado/minimizado.
- `ImGuiManager` fija `io.IniFilename = nullptr`: ImGui no lee ni escribe `imgui.ini`. El layout es fijo y el directorio de trabajo no es fiable en AppImage/.deb.
- `Palette(size_t bpp)`: interpreta el parámetro como bits por píxel; crea `1 << bpp` colores. `palette_usable_color_count()` devuelve todos los índices (8bpp usa 256), `palette_visual_color_count()` limita el panel a 32 swatches y `palette_visual_color_index()` muestrea índices con extremos incluidos (en 8bpp el último es 255). `set_transparent_index(optional)` guarda un índice opcional y `getColor()` devuelve alfa 0 para él sin borrar el color almacenado; `is_transparent()` consulta esa propiedad. `is_visual_palette_index()` valida que un índice se pueda elegir desde el panel. `make_palette(mode, bpp, inverted)` genera DMG, Grayscale, Red, Green o Blue de claro a oscuro; invertir la paleta invierte ese orden.
- `IndexedImage`: imagen indexada (width, height, bpp y vector de índices) y lector/escritor del formato `.idx`. Admite `Index` (texto decimal), `Binary` (un byte por píxel), `Packed` (filas empaquetadas MSB-first en hexadecimal) y `BinaryPacked` (las mismas filas en bytes crudos). Valida bpp 1..8, valores dentro de `[0, 2^bpp)`, tamaño exacto del payload y dígitos/bytes válidos. `format()` devuelve el modo leído; `save_indexed_image(..., IndexedFormat, ...)` escribe el modo elegido. `indexed_image_data_size()` y `indexed_image_estimated_file_size()` calculan el tamaño aproximado para el diálogo de Save As (payload + 64 B de cabecera media).
- `PaintBackend`: estado de aplicación. Recibe `IGuiBackend*`, posee `PaintSession` (documento, historial y selección flotante), la paleta de la aplicación, sus colores foreground/background, el modo de paleta y la inversión, además de tools, botones, iconos y cursores. Expone `session()`, `palette()`, `foreground_index()`, `background_index()`, `transparent_index()`, `has_transparent_color()`, `set_transparent_color()`, `set_transparent_foreground()`, `clear_transparent_color()`, `select_palette()`, `toggle_palette_inverted()`, `selectButton()`, `change_bits_per_pixel()`, `apply_pending_color_pick()`, `loadCursor()`, `setCursorImage()`, `loadOptionIcon()`/`optionIcon()` y `backend()`. `change_bits_per_pixel()` genera la paleta del bpp destino, calcula el índice más cercano por distancia RGB para cada índice antiguo, conserva el color transparente si su índice sigue siendo visible y aplica el cambio al documento. `undo()`/`redo()` regeneran la paleta cuando el paso restaurado cambia el bpp. `set_transparent_color()` solo acepta swatches visibles; regenerar la paleta conserva la transparencia y `New`/`Open` la limpian, sin tocar historial ni `is_modified`. `selectButton()` guarda en `m_last_edit_button` la última tool de edición, excluyendo Selection y Pick Color. `is_modified()` cuenta la edición pendiente y guardar la confirma. También posee `DocumentFile` y ofrece `file()`, `is_modified()`, `new_document()`, `open_document()`, `save_document()` y `save_document_as()`; guardar confirma la región flotante y cancela un trazo en curso. `recent_files()` mantiene hasta 10 rutas en memoria, nuevas primero y sin duplicados, actualizadas al abrir y guardar; `set_recent_files()` carga la lista desde `AppConfig` y `main` la vuelve a guardar al salir.
- `DocumentFile` (`inc/paint/document_file.h`): identidad y savepoint del documento. Guarda el path y el `state_id` guardado; `display_name()` devuelve el nombre o `Untitled`, y `is_modified(current_state, has_floating_region)` compara ambos. No hace I/O.
- `ToolButton`: struct con nombre, tool, cursor e icono único de 32×32.
- `main`: registra el toolbar en el orden Selection, Eraser, Fill, Pick Color,
  Pencil, Brush, Line, Curve, Rect, Circle y selecciona Pencil al arrancar.

## Estado del repo

Repositorio: <https://github.com/kirbyboy/QtreGBPaint>

Rama `main`, HEAD `feed82e` ("Add GitHub Actions CI and version smoke
tests"). Ese commit incluye el trabajo de las sesiones 2026-11-10/11:
título/cierre/atajos, barra de estado/cuentagotas, menús/atajos, zoom anclado,
Select All, Flip/Rotate de selección y de imagen, tema Light/Both/icono
Outline, botones de toolbar sin fondo, transparencia de paleta, configuración
`.ini`/geometría de ventana, recientes pulsables con corrección de rutas
corruptas, cambio de bpp en Properties, exportación BMP/PNG, formatos de
almacenamiento `.idx`, apertura desde `%f`/CLI, MIME por magic, guardado
atómico, preview por tiles, damero con textura, About con logo PNG, la licencia
CC0 de todos los assets, el README, el bootstrap de dependencias (`make deps`),
el empaquetado `.deb`/AppImage, el icono de ventana/class, la separación de
`PaintFrontend`, los smokes versionados en `tests/smoke/` y la CI de GitHub
Actions. Último backup: `tmp/backup/20261111_153859/`.

- `IndexedImage` y formato `.idx` estricto; `ppm_image` eliminado.
- Tests headless con doctest en `tests/` y su propio `Makefile`.
- Makefile debug/release con dependencias en una sola pasada.
- Limpieza de código muerto e idioma unificado a inglés en el código.
- UI final de toolbar, paleta y tamaño mínimo de ventana.
- Assets migrados de `.ppm` a `.idx` y cursores `.ipx` en `assets/cursors/`.
- Selección rectangular, región flotante y menú `Selection` con Copy, Cut,
  Paste e Invert Colors sobre `FloatingRegionEditor`/`PaintSession`.

La release `v0.9.1` está publicada con el `.deb`, el AppImage y
`SHA256SUMS-0.9.1`:
<https://github.com/kirbyboy/QtreGBPaint/releases/tag/v0.9.1>. La CI de GitHub
Actions también está en verde en `feed82e`. El árbol queda limpio. Los
artefactos ignorados son `compile_commands.json`, `deps*/`, `obj*/`, `build/`,
`lib/`, `imgui.ini`, `tmp/` y `temp/`. Los assets `.idx`/`.ipx`, los tests y el
código de rejilla/handles ya estaban versionados desde `7c0d30c`.
`canvas.h`/`canvas.cpp` se renombraron a `canvas_document.h`/
`canvas_document.cpp` con `git mv`.

## Pendiente inmediato

No hay trabajo bloqueante. La arquitectura ya deja el estado de aplicación en
`PaintBackend` y la vista en `PaintFrontend`; el menú Palette está cerrado. La
selección rectangular quedó cerrada en la sesión 2026-11-07: `SelectionTool`
solo crea el marquee, `FloatingRegionEditor` posee la
`FloatingRegion`/portapapeles y `CanvasArea` resuelve arrastre, commit,
cancelación y render. `PaintSession` coordina documento, editor y tool. Ver
“Sesión 2026-11-07 — Selección y región flotante”, “Sesión 2026-11-07 — Menú
Selection y portapapeles” y “Sesión 2026-11-08 — Separación CanvasDocument /
PaintSession”. El menú **File** (New/Open/Save/Save As) ya está implementado con
`DocumentFile`, los IDs de estado del historial, `save_indexed_image` e
ImGuiFileDialog. El prompt al cerrar la ventana, los atajos `Ctrl+N/O/S` y el
título con el nombre del archivo quedaron cerrados el 2026-11-10 (ver “Sesión
2026-11-10 — Título de ventana, cierre y atajos”). Los modos binarios del
`.idx`, el damero con textura, la licencia del icono (CC0 + marca), el
empaquetado `.deb`/AppStream, la separación de `PaintFrontend` y la CI ya están
cerrados; solo quedan los estudios opcionales (portapapeles entre instancias y
separación de overlays de `CanvasArea`). Las herramientas **Line**, **Pencil**,
**Brush**, **Eraser**, **Fill**, **Curve** y **Pick Color** ya están cerradas; Fill usa un scanline
flood fill por spans (`inc/core/fill.h`), Pick Color vuelve a la última tool de
edición tras muestrear y Curve usa una Bézier cúbica con handles y edición
pendiente (`inc/core/bezier.h`, `ToolBase::EditOverlay`). Line y Curve comparten
cinco anchos de trazo (`inc/core/line_stamp.h`, `inc/tools/line_width.h`). Antes
de añadir nuevas
funcionalidades, queda el estudio (no comprometido) del portapapeles entre
instancias de “Decisiones de producto y revisión pendiente — 2026-11-08”; las
opciones en cuadrícula ya están resueltas. Los invariantes de historial,
render/scroll y ASAN/UBSAN ya están resueltos. El menú contextual queda
descartado; los atajos Ctrl+C/X/V ya están enlazados desde el menú Selection.
El tema de ImGui (`Settings > Theme`) se cambia en caliente y se persiste en
`AppConfig`, igual que la rejilla, la geometría de ventana y la última carpeta.

La colocación de los handles de resize también está cerrada: viven en el borde
real, solo aparecen al alcanzarlo y la esquina es el fallback para canvas
pequeños. La rejilla, el recorte al contenido del child y los bordes siguen
validados; no tocar esos dos últimos sin revisar los smokes de scroll y
selección.

Comandos habituales:

```sh
make deps     # primera vez: descarga y compila lib/
make          # debug
make release  # release
make test     # tests headless
make run      # ejecutar debug
make smoke    # smokes (requiere display)
make deb      # paquete Debian 0.9.1
make appimage # AppImage 0.9.1
make release-assets # ambos + SHA256SUMS-0.9.1
make clean    # limpiar todo
```

## Refactor realizado

- Separación `CanvasDocument`/`PaintSession` (dominio) / `CanvasArea` (vista ImGui).
- `CanvasDocument::render(ICanvasRenderer&, ViewTransform, Palette)` para la imagen; `PaintSession` añade el preview de la tool.
- `ViewTransform` en `CanvasArea`, construido una vez por frame en `update_view()`.
- `handle_input(mouse)` en `CanvasArea` usando ese mismo `ViewTransform`.
- `Palette` interpreta constructor como bpp.
- Argumentos de `render` corregidos (size vs scroll).
- Scroll: eliminada doble resta (GetCursorScreenPos ya incluye scroll).
- `PaintBackend` + `ToolButton` para gestión de tools/iconos con fallback a texto.
- `renderToolbar()` opcion B: InvisibleButton + Image, sin bordes ImGui; icono segun estado (normal/hover/pressed). Tooltip con el nombre del boton. Layout en rejilla.
- Icono por defecto (interrogante) hardcodeado cuando falta el archivo.
- Los antiguos botones de prueba `Missing`/`HoverOnly` fueron eliminados en la
  sesión 2026-11-06.
- Iconos 16x16, 3bpp, con efecto 3D en `assets/` (rect, line, pencil, fill, circle).
- Rutas de assets resueltas con `SDL_GetBasePath()` y fallback a `assets`.
- SDL: `SDL_PIXELFORMAT_RGBA32` para coincidir con formato nativo de los colores.

## Sesión 2026-11-06 — UI y paleta

- **Toolbar dinámico.** Dos columnas fijas; el ancho se calcula con tamaño de
  botón, separación y `WindowPadding`. El alto crece según el número de tools.
  Sin scrollbars (`NoScrollbar`/`NoScrollWithMouse`).
- **Opciones de tool.** Recuadro reservado bajo los botones
  (`kToolOptionsHeight = 88`), ahora relleno con las opciones de la tool activa.
  Rect muestra Outline/Fill/Both en vertical; el resto de tools aún no tienen
  instancia. Cada opción es una fila de ancho completo y 24 px de alto, con el
  icono de 60x16 centrado; no hay frames de botón individuales.
- **Textos eliminados.** Ya no se dibujan `Tools` ni `Palette`.
- **Ventana mínima.** El frontend calcula el mínimo a partir del toolbar, la
  paleta y el chrome de la ventana; se aplica con
  `IGuiBackend::set_minimum_window_size()` y `SDL_SetWindowMinimumSize`.
  La ventana principal lleva `NoScrollbar` y `NoScrollWithMouse` para evitar el
  scroll de 1-2 px.
- **Paleta visual.**
  - Control foreground/background con dos cuadrados biselados y gap respecto
    al marco exterior.
  - Contenido fijo equivalente a 10 celdas actuales.
  - 1 fila para 1..16 colores; 2 filas para 17..32.
  - En dos filas, el alto total de la zona de color sigue siendo 50 px: las
    celdas se reducen verticalmente y mantienen una separación pequeña.
  - Todas las celdas llevan bisel 3D dibujado por dentro; el hitbox sigue siendo
    la celda completa, así que el borde también selecciona el color.
  - El recuadro del panel ocupa el ancho disponible de la ventana y se encoge
    con ella; el contenido de la paleta mantiene su ancho fijo.
  - El slider de zoom y su etiqueta ocupan el ancho del contenido de la paleta
    más el control foreground/background.
- **Controles temporales de paleta.** Menú `Palette (temp)`:
  - preset `Game Boy` (4 colores DMG), activo al arrancar;
  - slider de 1 a 32 colores;
  - gradientes `Red`, `Green` y `Blue`;
  - 2 filas a partir de 17 colores (16 + 16), 1 fila hasta 16;
  - clic izquierdo/derecho selecciona foreground/background.
- **Paleta conectada al dibujo.** `PaintFrontend` mantiene una única `m_palette`
  usada tanto por el panel como por el canvas, así que cambiar colores se
  refleja automáticamente en el dibujo (la textura se regenera vía LUT).
  `m_palette` es un contenedor de 32 entradas y `m_palette_color_count` indica
  cuántas están activas. Al arrancar: 4 colores DMG, foreground índice 3
  (oscuro) y background índice 0 (claro). `ToolBase::set_colors()` transmite los
  índices a la tool activa; `DrawTool` los guarda y `SquareTool` los usa al
  dibujar.
- **Importante:** en esta sesión el `ImageBuffer` del canvas seguía siendo 2bpp.
  La paleta de hasta 32 colores era una previsualización temporal; después se
  generalizó el documento a 1..8 bpp y el cambio de bpp pasó a
  `Image > Properties...`.
- **Menús:** se acepta el comportamiento estándar de ImGui; el clic que cierra
  un menú no activa el elemento de debajo.

Verificado con build debug/release, `make test` (53 casos, 787 aserciones),
`temp/smoke_palette.cpp` (inyección de eventos: preset DMG, selección fg/bg,
dibujo con los colores elegidos, paleta de 32 colores en 2 filas y cambio de
colores reflejado en el canvas) y capturas
`temp/smoke_palette_1.png` / `temp/smoke_palette_2.png`.

## Sesión 2026-11-06 — Cursores por herramienta

- **Tipo de cursor en el botón, no en la tool.** Nuevo `inc/core/cursor.h` con
  `PaintCore::CursorType { Arrow, Crosshair, Pencil, Brush, Eraser, Pick, Fill, Rect, Circle, ResizeEW, ResizeNS, ResizeNWSE, Move, Count }`.
  `ToolButton` guarda un `cursor`; `PaintBackend::selectButton()` actualiza
  `m_activeCursor` y lo expone con `activeCursor()`. Las tools no conocen nada
  de cursores.
- **Solo sobre el canvas.** `PaintFrontend::renderCanvas()` llama a
  `IGuiBackend::set_cursor()` con el cursor del botón activo cuando el canvas
  está hovered o activo (incluye arrastre fuera del canvas), y con `Arrow` en
  caso contrario. Menús, toolbar y paleta conservan el cursor normal.
- **Backend SDL dueño del cursor.** `SDLRendererBackend` crea un cursor por
  `CursorType` y otro juego para los `ImGuiMouseCursor`; activa
  `ImGuiConfigFlags_NoMouseCursorChange` y aplica el cursor en
  `set_cursor()`/`begin_imgui_frame()`/`render_imgui()` con caché del último
  cursor. Fuera del canvas, `Arrow` deja que ImGui use su propio cursor (texto,
  sliders, etc.).
- **Assets `.ipx`.** El formato usa 2 bpp: `0` transparente, `1` hotspot,
  `2` negro y `3` blanco. `assets/cursors/cross.ipx` es la cruz compartida por
  Selection/Line/Curve/Rect/Circle; `slim_cross.ipx` es el cursor de Brush;
  `fill.ipx`, `pencil.ipx`, `pick.ipx` y `move.ipx` son los demás. `circle.ipx`
  se eliminó al compartir la cruz.
- **Carga y fallback.** `PaintCore::load_cursor_asset()` valida el `.ipx`
  (exactamente 2 bpp, índices 0..3, exactamente un hotspot) y produce un
  `CursorImage` RGBA. `PaintBackend::loadCursor()` lo asocia al botón;
  `SDLRendererBackend::set_cursor_image()` crea el cursor con
  `SDL_CreateColorCursor`. Si el asset no existe o no es válido, el backend
  conserva el cursor del sistema. Rect, Pencil, Brush, Fill y Pick Color usan
  cursor personalizado; Circle conserva el fallback (flecha).
  `CursorType::Crosshair` sigue disponible como tipo explícito, pero las
  herramientas de dibujo ya no lo usan como fallback.
- **Reset del cursor al entrar en la ventana.** El window manager puede
  resetear el cursor al mapear/entrar en la ventana; `poll_events()` invalida
  `m_last_cursor` en `ENTER`/`FOCUS_GAINED`/`SHOWN` para que se reaplique.
- **Verificación.** Build debug/release, `make test` (59 casos, 808 aserciones;
  incluye `tests/test_cursor_asset.cpp` con 6 casos y 21 aserciones),
  `temp/smoke_palette.cpp`, `temp/smoke_undo.cpp` y `temp/smoke_cursor.cpp`
  (carga de Rect, rechazo de los tres assets sin hotspot, cambio al canvas y
  restauración fuera). Prueba aislada del cursor `.ipx` en
  `temp/test_ipx_cursor.cpp` y captura
  `temp/crosshair_ipx_cursor_screenshot.png`.

## Sesión 2026-11-06 — Opciones de herramienta (Rect)

- **Modelo genérico.** `inc/tools/tool_option.h` define `ToolOption` y
  `ToolOptionChoice`. `ToolBase` guarda las opciones de cada instancia
  (`add_option`, `options`, `set_option`, `option_value`) y ofrece
  `on_option_changed` para futuras reacciones. Una opción `Choice` siempre
  tiene un valor válido (por defecto, la primera elección); una `Integer` se
  recorta a su rango. `visible_in_options_bar` separa opciones presentes de
  opciones visibles en el hueco. Las opciones son por instancia, así que
  persisten al cambiar de herramienta.
- **Rect.** `SquareTool::Mode` tiene `Outline`, `Fill` y `Both`, con
  `Outline` por defecto como en Paint:
  - Outline: solo borde con foreground; interior transparente.
  - Fill: todo el rectángulo con background.
  - Both: borde foreground + interior background.
  No se añadió el ancho de línea; era solo un ejemplo. El tipo `Integer` queda
  soportado por si se necesita más adelante.
- **UI.** `PaintFrontend::renderToolOptions()` pinta las opciones visibles de la
  tool activa en el hueco reservado, ahora `kToolOptionsHeight = 88`. Para
  `Choice` usa filas verticales de ancho completo y 24 px de alto, con el icono
  de 60x16 centrado. No usa `ImageButton` ni frames individuales: dibuja con
  `InvisibleButton` + `ImDrawList`, hover neutro (blanco con alfa bajo) y fondo
  azul de fila completa en la opción seleccionada. Si falta el icono, pinta la
  etiqueta corta centrada. Para `Integer` usa un slider compacto. No hay ningún
  `if` por nombre de tool.
- **Hoja de iconos.** `assets/rect_options.idx` es una hoja vertical de 60x48
  (bpp 3) con tres celdas de 60x16: contorno, relleno y ambos. El índice 0 es
  transparente al cargarla con `PaintBackend::m_option_icon_palette`. `main`
  registra las regiones con `loadOptionIcon()` bajo las claves `rect.outline`,
  `rect.fill` y `rect.both`. `temp/make_rect_options.py` genera la hoja y
  `temp/rect_options_preview.png` / `temp/smoke_options.png` sirven de
  referencia visual.
- **Verificación.** Build debug/release, `make test` (65 casos, 844 aserciones;
  `tests/test_tool_options.cpp` aporta 6 casos y 36 aserciones),
  `temp/smoke_palette.cpp`, `temp/smoke_undo.cpp` y `temp/smoke_cursor.cpp`.
  Los tests de clamp y los smoke tests de dibujo seleccionan `Both`
  explícitamente ahora que el modo por defecto es `Outline`.

## Sesión 2026-11-07 — Resize de canvas

- **Dominio.** `Canvas::resize(width, height, fill)` valida mínimo 1x1, ignora
  el no-op, cancela la tool activa, conserva el contenido anclado arriba-
  izquierda, rellena la zona nueva con el índice de background y registra un
  único paso de historial. No hay máximo de tamaño: la responsabilidad es del
  usuario.
- **Undo/redo.** `ImageBuffer::resize(w,h,fill)` conserva el contenido y
  rellena lo nuevo. `CanvasSnapshot` guarda dimensiones, bpp y píxeles;
  `UndoStep` lleva snapshots completos `before_canvas`/`after_canvas` para los
  pasos que cambian el tamaño. `UndoHistory::undo/redo` restauran dimensiones y
  píxeles y marcan todo dirty; `UndoStep::bytes()` cuenta los snapshots para los
  límites de memoria. Los pasos de píxeles (`PixelPatch`) siguen funcionando
  igual y pueden convivir con los de resize.
- **UI.** Nuevo menú `Image > Canvas Size...` con modal de ancho/alto, nota de
  que la zona nueva usa el color de background y botones OK/Cancel. Al aceptar
  llama a `Canvas::resize()` con el índice de background actual. Un tamaño
  idéntico cierra el diálogo sin crear historial.
- **Verificación.** Build debug/release, `make test` (72 casos, 910 aserciones;
  `tests/test_canvas_resize.cpp` aporta 7 casos y 66 aserciones) y
  `temp/smoke_canvas_size.cpp`, que abre el diálogo, pulsa OK, comprueba
  `320x240`, deshace a `256x256` y rehace. Captura:
  `temp/smoke_canvas_size.png`.

## Sesión 2026-11-07 — Handles de resize y cursores

- **Handles de resize.** `CanvasResizeGizmo` dibuja y hit-testea handles
  totalmente fuera del canvas, con 1 px de separación, en borde derecho, borde
  inferior y esquina inferior derecha. Los hitboxes empiezan fuera del canvas,
  así que dibujar en el borde sigue funcionando. `CanvasArea` les da prioridad
  sobre la tool, dibuja una guía mientras se arrastra y confirma con
  `Canvas::resize()` al soltar.
- **Scroll y handles.** Los handles se calculan sobre el máximo bloque de
  píxeles completos que cabe en el área visible más el espacio del propio
  handle, no sobre el borde real del canvas cuando este queda fuera del
  viewport. Así los handles derecho/inferior/esquina quedan agrupados en el
  extremo inferior-derecho visible y no desaparecen al hacer scroll. El dibujo
  y el hit-test se recortan al área de contenido del child (sin padding ni
  scrollbars).
- **Offset independiente del canvas.** El gizmo tiene un `handle_offset`
  propio (por defecto `0,0`) que se aplica a los handles y a la guía sin
  consultar la rejilla. `CanvasArea::set_resize_handle_offset()` permite
  desplazarlos, por ejemplo `1 px` a la derecha, cuando se implemente la rejilla.
- **Cursores de resize.** `CursorType` incluye `ResizeEW`, `ResizeNS` y
  `ResizeNWSE`; el backend SDL los mapea a `SDL_SYSTEM_CURSOR_SIZEWE`,
  `SIZENS` y `SIZENWSE`. Son cursores del sistema por ahora; se pueden
  sustituir por `.ipx` más adelante.
- **Rediseño posterior (2026-11-07).** La colocación provisional de esta
  sesión (handles agrupados en el extremo inferior-derecho visible) se
  sustituyó por handles anclados al borde real; ver “Sesión 2026-11-07 —
  Rediseño de handles y scroll”. La rejilla y el recorte al contenido no se
  tocaron.
- **Verificación.** `temp/smoke_resize_handles.cpp` arrastra la esquina
  inferior derecha, comprueba el cursor de resize, verifica `306x306`, deshace
  a `256x256` y rehace. Build debug/release y el resto de smoke tests OK.
  Captura durante el drag: `temp/smoke_resize_handles.png`.

## Sesión 2026-11-07 — Rejilla

- **UI.** `Image > Grid` es un checkbox marcado por defecto. La rejilla es un
  overlay puramente visual y solo se dibuja con `zoom >= 6`.
- **Líneas.** Se dibuja una línea por borde de píxel, más la línea extra a la
  derecha y abajo; las líneas superiores/izquierdas cubren el primer píxel de
  cada celda. El píxel extra de la derecha/abajo entra en el margen que ya
  reservamos para los handles, sin redimensionar nada. El canvas, la rejilla,
  los handles y su hit-test se recortan al área de contenido del child, así que
  el padding y los scrollbars no se pintan.
- **Patrón.** Dos texturas 1D (`1×H` y `W×1`) con píxeles alternos
  `(64,64,64)` y `(192,192,192)`. Cada línea es un solo quad texturizado, así
  que en 4K/×6 son ~1002 quads, no los ~2,77 M de segmentos de 1 px que
  requeriría el patrón dibujado a mano. La fase del patrón se ancla al origen
  del canvas, de modo que al hacer scroll las líneas y sus colores siguen
  coincidiendo con los píxeles de la imagen.
- **Coste.** Las texturas se regeneran solo cuando cambia el tamaño visible;
  se dibujan únicamente las líneas visibles. El coste de CPU/GPU es
  despreciable frente al canvas y no depende del número de píxeles del patrón.
- **Verificación.** `temp/smoke_grid.cpp` genera `temp/smoke_grid.png` (zoom 8,
  grid on) y `temp/smoke_grid_off.png` (grid off).
  `temp/smoke_grid_scroll.cpp` hace scroll máximo con un canvas grande y
  `temp/smoke_grid_scroll_min.cpp` reproduce la ventana mínima a zoom 64.
  Capturas: `temp/smoke_grid_scroll.png` y
  `temp/smoke_grid_scroll_min.png`. Build debug/release y `make test` siguen en
  verde (72 casos, 910 aserciones).

## Sesión 2026-11-07 — Rediseño de handles y scroll

- **Scroll 1:1.** `ViewTransform::make()` ya no ajusta el scroll a múltiplos de
  zoom. `origin` sigue a `canvas_pos` (lo que devuelve `GetCursorScreenPos`, que
  ya incluye el scroll) redondeado a píxel entero de pantalla. Desaparece el
  “píxel completo en la esquina superior izquierda”: al hacer scroll puede
  verse un píxel parcial recortado en el borde superior/izquierdo, como en
  Paint. `visible_image_rect()` no cambia. Tests actualizados en
  `tests/test_view_transform.cpp`.
- **Handles en el borde real.** `CanvasResizeGizmo` ya no agrupa los handles en
  el último bloque visible. Cada handle se ancla al borde real del canvas
  (incluido el offset opcional) y está 1 px fuera, así que no toca la rejilla.
  - derecho: aparece cuando el borde derecho real está visible y el handle
    completo cabe en el clip; se centra en el tramo vertical visible del borde.
    Si el tramo es menor que el alto del handle (16 px), desaparece.
  - inferior: misma regla con el tramo horizontal y el ancho del handle.
  - esquina: aparece solo al alcanzar la esquina real (derecha e inferior
    visibles) y no depende del tamaño del canvas, por eso sirve de fallback
    cuando los handles de borde no caben.
  - Los hitboxes siguen empezando fuera del canvas, así que el borde se puede
    dibujar. La guía de preview y el commit al soltar no cambian.
- **Verificación.** Build debug/release, `make test` (73 casos, 912
  aserciones). `temp/smoke_resize_handles.cpp` (drag de esquina, cursor,
  undo/redo) sigue en verde; nuevo `temp/smoke_resize_handles_edges.cpp`, que
  comprueba handles ocultos sin borde visible, derecho solo al llegar al borde
  derecho, los tres al llegar a la esquina, handles de borde ocultos en canvas
  de 8x8 y presentes en 24x24. Capturas:
  `temp/smoke_resize_edges_right.ppm`, `temp/smoke_resize_edges_corner.ppm`,
  `temp/smoke_resize_edges_small.ppm` y
  `temp/smoke_resize_edges_small_all.ppm`. También se reejecutaron
  `smoke_grid_scroll_min`, `smoke_grid_scroll`, `smoke_grid`, `smoke_undo`,
  `smoke_canvas_size`, `smoke_cursor` y `smoke_palette`.

## Sesión 2026-11-07 — Selección y región flotante

- **Modelo compartido con paste.** `FloatingRegion`
  (`inc/paint/floating_region.h`) vive en `Canvas`, no en la tool. Guarda
  `content` (ImageBuffer con el bpp del canvas), `rect` (posición actual),
  `hole` (zona original), `hole_fill`, `active` y `content_revision`. Solo hay
  una región a la vez. `FloatingRegionEditor::begin_selection(rect, hole_fill,
  transparent_background)` recorta el
  rectángulo al canvas, copia el contenido y registra el hueco;
  `FloatingRegionEditor::begin_paste(content, pos)` crea la misma estructura sin hueco.
  `move_floating_region(dx, dy)` mantiene al menos 1 px dentro del canvas;
  un paste puede empezar totalmente fuera y el commit será un no-op.
- **Commit atómico.** `commit_floating_region()` rellena el hueco, escribe el
  contenido visible (ignorando `TRANSPARENT_INDEX` en paste) y registra un
  único `PixelPatch` con el before/after de la unión hueco+contenido. Si no
  cambia ningún píxel no crea historial. `cancel_floating_region()` descarta el
  overlay sin tocar la imagen.
- **Ciclo de vida.** `undo()` cancela una región activa; `can_undo()` es true
  mientras existe y `redo()`/`can_redo()` quedan bloqueados. `set_tool()` y
  `resize()` confirman la región antes de continuar; el resize con el mismo
  tamaño también la confirma (OK del diálogo Canvas Size). `begin_selection`
  y `begin_paste` confirman la región anterior si la hubiera.
- **Tool.** `SelectionTool` solo dibuja el marquee: en press/move guarda las
  esquinas y en release llama a `PaintSession::begin_selection` con el rectángulo
  normalizado, el background actual como `hole_fill` y el flag de modo
  transparente. No guarda píxeles ni estado de movimiento. `ToolBase` añade
  `has_marquee()`/`marquee_rect()` para
  que `CanvasArea` pinte el rectángulo transitorio sin conocer la tool.
- **Interacción y render.** `CanvasArea` intercepta el input de la región
  antes que los handles de resize y las tools. El botón izquierdo dentro del
  contenido inicia el arrastre; un clic fuera confirma; el clic derecho cancela
  aunque no se esté arrastrando. Mientras flota se ocultan los handles y se usa
  `CursorType::Move` (con `assets/cursors/move.ipx` si carga; si no,
  `SDL_SYSTEM_CURSOR_SIZEALL`). El hueco y el contenido se
  dibujan antes de la rejilla; el borde discontinuo (negro/blanco) se dibuja
  después y se recorta al canvas. El contenido flotante usa su propia textura
  con LUT de paleta y `TRANSPARENT_INDEX` como alfa 0.
- **Toolbar.** Selection es el primer botón, con `CursorType::Crosshair` e
  iconos `assets/selection_normal/hover/pressed.idx`. Se generaron como
  placeholders con `temp/make_selection_icons.py`; sustituirlos por el diseño
  definitivo cuando exista.
- **Decisión sobre resize de selección.** No hay handles de selección ni
  redimensionado del contenido flotante; la selección es solo rectangular y
  axis-aligned. El menú `Selection` ya ofrece Copy/Cut/Paste e Invert Colors; el
  clic derecho sigue cancelando y no abre ningún menú contextual.
- **Verificación.** Build debug/release y `make test`: 11 ejecutables, 111 casos
  y 1234 aserciones tras añadir el menú, el portapapeles y el modo transparente.
  `tests/test_floating_region.cpp` aporta 37 casos y 317 aserciones (clamp,
  hueco+contenido atómico, undo/redo, paste, tool, resize con y sin cambio de
  tamaño, copy/cut/paste, inversión, fondo transparente y máscara viva). `temp/smoke_selection.cpp` inyecta
  eventos SDL y comprueba creación, arrastre, hueco, commit al clicar fuera,
  cancelación con clic derecho, commit al cambiar de tool, undo e interacción
  real con el menú `Selection`; usa el origen real de `m_view` en lugar de
  coordenadas fijas. Capturas:
  `temp/smoke_selection_created.png`, `smoke_selection_moved.png`,
  `smoke_selection_committed.png`, `smoke_selection_tool_switch.png`,
  `smoke_selection_menu.png`, `smoke_selection_inverted.png`,
  `smoke_selection_invert_committed.png`, `smoke_selection_cut.png` y
  `smoke_selection_paste.png`. Se reejecutaron `smoke_undo`,
  `smoke_canvas_size`, `smoke_cursor`, `smoke_palette`, `smoke_grid*` y
  `smoke_resize_handles*` sin regresiones.

## Sesión 2026-11-07 — Menú Selection y portapapeles

- **Hueco con background.** El área original de una selección se rellena con el
  índice de background actual, no con foreground. `SelectionTool` guarda el
  background en `set_colors()` y lo pasa como `hole_fill` a
  `FloatingRegionEditor::begin_selection()` (a través de `PaintSession`). Afecta tanto al commit normal como a Cut.
- **Portapapeles en FloatingRegionEditor.** El editor guarda `m_clipboard` como `ImageBuffer`
  con el bpp del canvas. APIs: `copy_to_clipboard()`, `cut_to_clipboard()`,
  `paste_from_clipboard(pos)`, `has_clipboard()`, `clipboard()` e
  `invert_floating_region_colors()`. Copy no confirma ni modifica la región
  flotante; Cut copia y rellena el hueco con background (un paste flotante, sin
  hueco, solo se descarta); Paste crea una nueva región flotante en la esquina
  superior izquierda visible del canvas, confirma la anterior si existía y
  cambia a Selection para aplicar su modo actual. El
  portapapeles conserva `TRANSPARENT_INDEX` intrínseco y no aplica la máscara.
- **Paste en la zona visible.** `CanvasArea::visible_image_origin()` usa
  `ViewTransform::visible_image_rect()` y devuelve el píxel de imagen que queda
  más arriba-izquierda en el viewport. El menú lo pasa a
  `paste_from_clipboard(pos)`, así que con zoom alto y scroll lejano el paste
  aparece junto a la zona que el usuario está viendo, no en `(0,0)`. Si el
  rectángulo visible está vacío, cae a `(0,0)`.
- **Cursor Move `.ipx`.** `assets/cursors/move.ipx` es una conversión del cursor
  `move` del tema Yaru (`/usr/share/icons/Yaru/cursors/move`) hecha con
  `temp/make_move_cursor.py`: 24x24, índices 0/3/4 y hotspot en `(12,11)`. *El
  cursor se redibujó desde cero el 2026-11-11 y ya no deriva de Yaru; está
  cubierto por `assets/LICENSE` (CC0).* El
  script parsea el formato Xcursor, elige la imagen de 24 px y umbraliza alfa y
  luminancia a transparente/negro/blanco. `PaintBackend::loadCursor(CursorType,
  path, error)` carga el `.ipx` directamente en el slot `Move`; si falta o es
  inválido, el cursor de sistema `SIZEALL` sigue funcionando como fallback. La
  misma sobrecarga permite ir añadiendo `.ipx` para el resto de acciones.
- **Flag `modified`.** `FloatingRegion` incorpora un booleano que empieza en
  false al crear una selección. Se pone a true al mover el rectángulo, al crear
  un paste y al invertir colores. `commit_floating_region()` descarta sin tocar
  el historial una selección intacta, y en cambio confirma Invert Colors aunque
  el recuadro no se haya movido. Sigue comparando before/after antes de apilar
  el paso, así que invertir dos veces o volver a la posición original no crea
  historial.
- **Invert Colors.** Para 2bpp el índice invertido es `3 - i`; en general
  `max_index - i` con `max_index = 2^bpp - 1`. Se saltan los píxeles
  `TRANSPARENT_INDEX` y cualquier índice fuera del rango del canvas. La
  inversión actualiza `content_revision` para que la textura flotante se
  regenere.
- **Menú.** Nuevo menú superior `Selection`, en inglés, con Copy, Cut, Paste,
  separador e Invert Colors. Copy/Cut/Invert Colors se habilitan solo mientras
  hay una región flotante; Paste se habilita cuando el portapapeles tiene
  contenido. El clic derecho sigue cancelando y no abre menú contextual.
- **Verificación.** `temp/smoke_selection.cpp` abre el menú con eventos SDL,
  pulsa Invert Colors sin mover la selección, comprueba la inversión y el
  historial, y después usa Copy, Cut y Paste desde el menú real. También carga
  `assets/cursors/move.ipx` y comprueba que Paste a zoom 8 y scroll máximo cae
  en `visible_image_origin()` (`smoke_selection_paste_visible.png`). Capturas
  del menú abierto y de cada estado en `temp/smoke_selection_menu.png`,
  `smoke_selection_inverted.png`, `smoke_selection_invert_committed.png`,
  `smoke_selection_cut.png`, `smoke_selection_paste.png` y
  `smoke_selection_paste_visible.png`. `make test` en verde: 111 casos y 1234
  aserciones; build debug/release OK.

## Sesión 2026-11-08 — Zoom y scroll en los bordes

- **Síntoma.** Con zoom alto y el scroll al máximo, al bajar el zoom se veía un
  frame con una franja vacía a la derecha/abajo. No era un fallo del render:
  ImGui calcula `ScrollMax` en `Begin` a partir del `ContentSize` del frame
  anterior. El slider de zoom vive en la paleta, que se renderiza después del
  canvas, así que el primer frame con el nuevo zoom conservaba el scroll viejo
  (mayor que el nuevo máximo) y la imagen quedaba desplazada; el frame siguiente
  ya se corregía.
- **Fix.** `PaintFrontend` guarda `m_last_canvas_zoom`. Antes de
  `BeginChild("Canvas")`, si el zoom cambió, llama a
  `ImGui::SetNextWindowContentSize()` con el tamaño del nuevo zoom
  (`content_width * zoom + kCanvasResizeMargin`). Así `Begin` calcula el
  `ScrollMax` nuevo y `CalcNextScrollFromScrollTargetAndClamp()` recorta el
  scroll en el mismo frame que el nuevo zoom. No se toca `CanvasArea` ni
  `ViewTransform`.
- **Verificación.** `temp/smoke_zoom_edge.cpp`: zoom 64, scroll a máximo, simula
  el slider cambiando `m_zoom` después de `frontend.render()`, y comprueba que
  el primer frame tras el cambio ya tiene color de canvas en la esquina
  inferior derecha del viewport. Capturas:
  `temp/smoke_zoom_edge_before.png` y `smoke_zoom_edge_after.png`. `make test`
  sigue en 111 casos y 1234 aserciones; build debug/release y el resto de smokes
  en verde. El mismo patrón se puede extender a cambios de tamaño de canvas si
  apareciera el mismo problema.

## Sesión 2026-11-08 — Selección con fondo transparente

- **Dos modos.** `SelectionTool` tiene una opción `mode` (`Mode` en la
  UI) con `Normal` y `Transparent`. Se declara en el constructor con
  `ToolOption::choice` y se muestra en el hueco de opciones con los iconos
  `selection.normal` / `selection.transparent` de
  `assets/selection_options.idx` (generado por
  `temp/make_selection_options.py`). El modo Normal es el de por defecto.
- **Máscara viva, no píxeles convertidos.** El contenido flotante conserva
  siempre los índices originales. `FloatingRegion` guarda
  `transparent_background` y `transparent_color`; al dibujar, los píxeles que
  coinciden con ese color se envían a la textura con alfa 0, y al confirmar se
  saltan. La tool solo pasa el modo inicial a
  `FloatingRegionEditor::begin_selection(rect, hole_fill,
  transparent_background)` a través de `PaintSession`;
  `FloatingRegionEditor::set_floating_transparency(enabled, color)` cambia la máscara de una
  selección o paste ya existente. `SelectionTool::sync_floating_region()` la
  sincroniza cuando cambia la opción o el background, y el frontend la llama
  tras editar opciones/colores. No se rellena ni reescribe el `ImageBuffer`
  desde fuera del Canvas.
- **Paste y portapapeles.** Copy/Cut copian los píxeles crudos (sin aplicar la
  máscara). Paste crea la región con esos píxeles y el menú cambia a Selection
  para aplicar el modo actual; si el modo está activo, los píxeles de background
  se ven transparentes, y si se desactiva, se vuelven opacos. Así una selección
  copiada conserva su información y solo se muestra transparente cuando la
  opción está habilitada.
- **Flag `modified`.** Si se activa la máscara y hay al menos un píxel del color
  transparente, `begin_selection`/`set_floating_transparency` marcan
  `modified = true`. Si no hay coincidencias, el modo no altera nada y
  `modified` sigue false. Como siempre, el commit solo crea historial si los
  píxeles cambian de verdad.
- **Commit.** Primero se rellena el hueco con `hole_fill` (background) y luego
  se escribe el contenido saltando los `TRANSPARENT_INDEX` intrínsecos y los
  píxeles que la máscara marca como transparentes. En la posición original,
  esos píxeles dejan ver el fondo del hueco; si la selección se mueve, dejan ver
  los píxeles del destino. Invert Colors opera sobre los índices crudos: si
  invierte píxeles que coincidían con el background, dejan de coincidir y pasan
  a ser visibles.
- **Blend de texturas.** Para que los píxeles transparentes del overlay no se
  vieran negros, `SDLRendererBackend::createTexture()` configura
  `SDL_BLENDMODE_BLEND` además de `SDL_ScaleModeNearest`. Afecta también a los
  iconos con alpha y a cualquier textura RGBA futura.
- **Verificación.** `temp/smoke_selection_transparent.cpp` activa el modo
  Transparent pulsando la segunda fila de opciones con eventos SDL, dibuja una
  selección con borde opaco e interior de background, cambia a Normal y vuelve
  a Transparent sobre la región ya creada (comprobando que el contenido sigue
  crudo), la mueve sobre una zona de otro color y verifica que el interior
  conserva el destino y el hueco queda con background. Capturas:
  `smoke_selection_transparent.png`, `smoke_selection_transparent_opaque.png`,
  `smoke_selection_transparent_moved.png` y
  `smoke_selection_transparent_committed.png`. Tests: 37 casos y 317
  aserciones en `test_floating_region.cpp`; total 111 casos y 1234 aserciones.
  Build debug/release y el resto de smokes en verde.

## Sesión 2026-11-08 — Separación CanvasDocument / PaintSession

- **Motivo.** `Canvas` había acumulado documento y estado de edición:
  `ImageBuffer` + historial + resize, pero también selección, portapapeles,
  máscara de transparencia, tool activa y coordinación de undo/redo. Se separó
  en tres piezas de dominio.
- **`CanvasDocument`.** Solo datos: `ImageBuffer`, `UndoHistory`, `resize`,
  `commit_draw_buffer`, `push_pixel_patch` y render de la imagen. No incluye
  tools, región flotante, portapapeles ni máscaras. Archivos:
  `inc/paint/canvas_document.h` y `src/paint/canvas_document.cpp` (renombrados
  desde `canvas.h`/`canvas.cpp` con `git mv`).
- **`FloatingRegionEditor`.** Posee `FloatingRegion` y el portapapeles, y
  implementa toda la lógica de selección/paste sobre una referencia a
  `CanvasDocument`: `begin_selection`, `begin_paste`, movimiento, commit/cancel,
  copy/cut/paste, invert y máscara de transparencia. **No** vive dentro del
  documento: lo posee `PaintSession` junto al documento. Archivos nuevos:
  `inc/paint/floating_region_editor.h`, `src/paint/floating_region_editor.cpp`.
- **`PaintSession`.** Posee `CanvasDocument`, `FloatingRegionEditor` y la tool
  activa. Coordina lo que no es documento puro:
  - `undo`: cancela la región flotante o el trazo en curso antes de tocar el
    historial.
  - `can_undo`/`can_redo` y bloqueo de redo mientras flota.
  - `set_tool` y `resize`: confirman la región antes de continuar.
  - Reenvía a la UI la API de documento y de selección.
  Archivos nuevos: `inc/paint/paint_session.h`, `src/paint/paint_session.cpp`.
- **UI y tools.** `CanvasArea` ahora posee `PaintSession m_session` y expone
  `session()` (antes `canvas()`). `CanvasResizeGizmo` y las tools reciben
  `PaintSession&`; `SelectionTool` sigue siendo solo marquee. El documento se
  obtiene con `session.document()` cuando se necesita, pero las tools no lo
  reciben directamente.
- **Nota sobre secciones anteriores.** Donde las sesiones antiguas dicen
  `Canvas::begin_selection`, `Canvas::copy_to_clipboard`, etc., el propietario
  actual es `FloatingRegionEditor` (a través de `PaintSession`). Donde dicen
  `Canvas::undo`, `set_tool` o `resize` con coordinación de selección, ahora es
  `PaintSession`. Donde se refieren a píxeles/historial/resize puro, es
  `CanvasDocument`.
- **Verificación.** Mismo comportamiento: `make test` en verde con 111 casos y
  1234 aserciones; build debug/release y todos los smokes (`canvas_size`,
  `cursor`, `grid*`, `palette`, `resize_handles*`, `selection*`, `undo`,
  `zoom_edge`) OK. Se añadieron tests directos de `CanvasDocument` (imagen e
  historial sin sesión) y de `FloatingRegionEditor` sobre un documento; el resto
  de tests siguen usando `PaintSession` como fachada.

## Sesión 2026-11-08 — Invariantes de edición e historial

- **Motivo.** `CanvasDocument::get_main_buffer()` devolvía un `ImageBuffer&`
  mutable: se podían cambiar píxeles sin pasar por el historial. Además,
  `FloatingRegionEditor` mantenía a mano `modified`, `content_revision` y el
  before/after de cada `PixelPatch`, con riesgo de olvidar alguno al añadir
  operaciones nuevas.
- **Única vía de escritura.** `CanvasDocument::apply_pixel_patch(rect, after)`
  valida que el rect esté dentro del canvas y que `after` contenga exactamente
  `w*h` píxeles, lee el `before`, compara y, si cambió, apila el `PixelPatch`
  antes de escribir. Ese orden es deliberado: si falla la asignación del
  historial, la imagen queda intacta. `commit_draw_buffer` conserva su ruta de
  merge y usa el `record_patch` privado. Ya no existen la variante mutable de
  `get_main_buffer()` ni `push_pixel_patch`; solo queda la vista const y
  `take_dirty_rect()`, que consume la región sucia.
- **Contenido flotante replace-only.** `FloatingRegion` expone `content()`,
  `set_content(ImageBuffer)` y `revision()`. `set_content` siempre incrementa la
  revisión; `invalidate()` hace lo propio cuando solo cambia la máscara de
  transparencia. Se eliminaron `modified` y `content_revision`. Invert copia el
  contenido, lo invierte y lo reemplaza con `set_content`.
- **Commit como parche único.** `FloatingRegionEditor::commit_floating_region`
  construye el bloque final en memoria (hueco + contenido con la máscara) y se
  lo pasa a `apply_pixel_patch`; `cut_to_clipboard` hace lo mismo con el hueco.
  Así el documento es el único que captura before/after y decide si hay
  historial: una selección intacta, un movimiento que deja los mismos píxeles o
  invertir dos veces no crean pasos por sí solos, sin necesidad de un flag.
- **Vista.** `CanvasArea` usa la imagen const, un flag local
  `m_texture_full_upload` (recreación de textura o cambio de paleta) y consume
  `PaintSession::take_dirty_rect()`. La textura flotante se refresca con
  `region.revision()`.
- **Tests y verificación.** Los seeds de los tests ahora usan
  `commit_draw_buffer` + `clear_history()` en lugar de escribir el buffer
  directamente. Se añadieron tests de `apply_pixel_patch` (undo/redo, entrada
  inválida, no-op, invalidación de redo, historial desactivado y dirty rect), de
  commit en el que solo cambia la máscara y del incremento de revisión.
  `make test`: 119 casos y 1278 aserciones. Builds debug y release OK; smokes
  `selection`, `selection_transparent`, `undo` y `zoom_edge` OK.
- **Nota sobre secciones anteriores.** Las menciones a `modified`,
  `content_revision`, `push_pixel_patch` y al `get_main_buffer()` mutable en las
  sesiones previas son históricas: a partir de esta sesión ya no existen.

## Sesión 2026-11-08 — ASAN/UBSAN en tests

- **Objetivos.** `tests/Makefile` acepta `SANITIZER` y `BUILD_SUFFIX`. Los
  atajos `make test-asan`, `make test-ubsan` y `make test-sanitize`
  (address+undefined) recompilan todos los tests en `tests/build-asan`,
  `tests/build-ubsan` y `tests/build-sanitize`; `make test` sigue usando
  `tests/build` sin cambios y cada variante tiene objetos propios. Si se usan
  `SANITIZER`/`BUILD_SUFFIX` a mano, el Makefile exige el sufijo para no
  mezclar objetos.
- **Flags.** `-fsanitize=$(SANITIZER) -fno-sanitize-recover=all
  -fno-omit-frame-pointer`, aplicados tanto a compilación como a enlace.
  `-fno-sanitize-recover=all` hace que UBSAN aborte en el primer hallazgo en
  lugar de imprimir un aviso y terminar con código 0.
- **Limpieza.** `make clean` (y `make -C tests clean`) elimina las tres
  variantes además de `build/`. `.gitignore` ignora `tests/build-*/`.
- **Verificación.** Las tres variantes pasan los 11 binarios: 119 casos y 1278
  aserciones, sin errores de ASAN, UBSAN ni LeakSanitizer. `make test` normal
  sigue en verde.

## Sesión 2026-11-08 — Tiles de render y snapshots de resize

- **Tiles.** `CanvasArea` ya no crea una textura del tamaño completo del canvas.
  La imagen se divide en tiles de 256×256 (`inc/paint/canvas_tile_grid.h`:
  `kCanvasTileSize`, `tile_count`, `tile_rect`, `tiles_covering`, `TileSpan` y
  `tile_key`). Solo se cachean los tiles que cubren la zona visible más un tile
  de margen; los que salen se destruyen. Así el VRAM no depende del tamaño del
  canvas y se supera el límite de textura del backend (16384×16384 en esta
  máquina; una textura de 16384² RGBA ya ocupa ~1 GiB).
- **Actualización.** `update_canvas_tiles()` consume `take_dirty_rect()` una vez
  por frame. Los tiles nuevos se suben completos; los existentes que intersecan
  el rect sucio se vuelven a subir completos (256² conversiones por tile, coste
  acotado). Un cambio de paleta marca todos los tiles en caché para re-subida;
  un cambio de dimensiones destruye el cache. Si crear un tile falla, el frame
  cae al render por píxel de siempre.
- **Bordes y costuras.** El rect visible se expande 1 píxel para absorber
  redondeos de scroll; el margen de un tile evita crear/destruir texturas al
  cruzar una frontera. El smoke comprueba ambos lados de la frontera x=256, un
  parche sucio dentro de un tile ya cacheado, el refresco de paleta y el borde
  derecho tras hacer scroll.
- **Precisión del scroll.** No se cambia el modelo. ImGui guarda el scroll como
  `float`; para un content size `S = ancho_imagen * zoom`, el error en píxeles
  de imagen es aproximadamente `ancho_imagen / 2^23`. Con 17000 px son ~0.002
  px; haría falta un canvas de ~8.4 millones de píxeles de ancho para acercarse
  a 1 px. Con 1 byte/píxel esos tamaños no son realistas, así que el scroll
  actual es suficiente.
- **Snapshots de resize.** `UndoStep` guarda solo `before_canvas` y
  `CanvasResize{width,height,fill}`; desaparece `after_canvas`. `redo`
  reconstruye el estado posterior con `ImageBuffer::resize`, la misma operación
  determinista que usa `CanvasDocument::resize`. El historial de cada resize usa
  la mitad de memoria. La validación rechaza pasos con snapshot sin target, con
  dimensiones no positivas o con patches mezclados.
- **Tests y verificación.** Nuevo `tests/test_canvas_tiles.cpp` (7 casos, 62
  aserciones) y `test_canvas_resize` ampliado (8 casos, 76 aserciones). Totales:
  12 binarios, 127 casos y 1350 aserciones. Builds debug y release OK; las
  variantes ASAN, UBSAN y combinada pasan los mismos 127 casos. Los 13 smokes de
  `temp/` (incluido `smoke_large_canvas` con 17000×64) pasan.
- **Objeto obsoleto.** `obj/paint/canvas.o`, de un `src/paint/canvas.cpp` que ya
  no existe, seguía en el árbol y los smokes que enlazan con `find obj` podían
  preferirlo frente a `canvas_document.o`. Se eliminó; no afecta al binario de
  la aplicación, que solo enlaza fuentes actuales.

## Sesión 2026-11-08 — Estado en PaintBackend y menú Palette

- **Reparto.** `PaintBackend` pasa a ser el estado de la aplicación: posee
  `PaintSession` (documento, historial y selección flotante), la paleta, los
  índices foreground/background, el modo de paleta y la inversión. `CanvasArea`
  recibe `PaintSession&` y deja de poseerla; `PaintFrontend` queda como
  vista/controller con solo estado transitorio (zoom, rejilla, popups y campos
  de diálogo). La rejilla y el zoom se mantienen en la vista por decisión
  explícita. `PaintBackend::selectButton()` sincroniza la tool activa con
  `PaintSession` y aplica los colores actuales.
- **Menú Palette.** Sustituye al antiguo `Palette (temp)`. Ofrece DMG,
  Grayscale, Red, Green, Blue, un separador e `Invert Palette` (con checkmark).
  No hay selector de bpp ni slider de número de colores: el número de swatches
  sale del bpp del documento, limitado a 32.
- **Generación de paletas.** `palette_usable_color_count()` reserva el índice
  255 para transparencia, así que 8bpp usa 255 colores en vez de 256.
  `palette_visual_color_count()` limita el panel a 32 y
  `palette_visual_color_index()` muestrea con extremos incluidos: 64 colores
  muestran aproximadamente 0, 2, 4… y el último swatch es el más oscuro; 8bpp
  termina en 254. `make_palette()` genera DMG, Grayscale, Red, Green y Blue de
  claro a oscuro. Al crear o abrir un documento, el foreground es el último
  swatch visible y el background el primero. Cambiar de tonalidad o invertir
  la paleta conserva esos índices; solo cambian los colores. Seleccionar una
  tonalidad reinicia la inversión.
- **Tests.** Dos casos nuevos en `test_image_model` para el muestreo y la
  generación/inversión. Totales: 129 casos y 1391 aserciones. Builds debug y
  release OK; los 13 smokes de `temp/` pasan.
- **ImGuiFileDialog.** Se copia `aiekick/ImGuiFileDialog` v0.6.8 (commit
  `ee77f19`) en `lib/ImGuiFileDialog/`, con su Makefile propio. El Makefile
  principal añade el include, enlaza `libImGuiFileDialog.a`, incluye `-pthread`
  y construye la librería si falta. Todavía no se usa: el menú File queda para
  la siguiente sesión. `lib/` no se versiona en este repo; para compilar desde
  un clon hay que descargar la dependencia en esa ruta.

## Sesión 2026-11-08 — Menú File

- **Estado de archivo.** `PaintBackend` posee `DocumentFile` (path + `saved_state`)
  y expone `file()`, `is_modified()`, `new_document()`, `open_document()`,
  `save_document()` y `save_document_as()`. `is_modified()` combina el estado
  del historial, la región flotante y un trazo en curso. El nombre y el `*` de
  modificado se muestran en el menú File, consultando al backend.
- **Savepoint.** `UndoStep` recibe un `state_id` monotónico al apilarse; 0 es el
  estado inicial. `undo`/`redo` actualizan `UndoHistory::current_state_id()` sin
  reutilizar ids (`clear()` no reinicia el contador). Guardar registra ese id;
  volver a él con undo/redo deja el documento limpio, y una rama nueva lo deja
  modificado.
- **New/Open.** New usa un modal con ancho/alto (256×256 por defecto) y bpp 1..8; el
  modal muestra la cantidad de colores disponibles (255 para 8bpp) y rellena
  con el background por defecto (el primer swatch visible). Open carga un `.idx` con
  `IndexedImage::loadFromFile`, valida dimensiones/bpp/datos y reemplaza el
  documento con `PaintSession::load_image` + `CanvasDocument::replace_image`,
  que cancelan región/trazo y limpian el historial. Si el bpp cambia, la paleta
  se regenera y se reinician los colores por defecto.
- **Save/Save As.** `save_indexed_image()` añade el escritor del modo `Index`
  (mismo formato que el lector, sin copia intermedia) y valida índices. Save usa
  el path actual; si no existe, el frontend abre Save As. Save As normaliza la
  extensión a `.idx` y confirma sobrescritura. Antes de escribir se confirma la
  región flotante y se cancela un trazo en curso. Un fallo de escritura no
  actualiza path ni savepoint.
- **Diálogos.** Se usa `ImGuiFileDialog` v0.6.8 en modo modal, filtro `.idx` y tamaño mínimo
  720×480 para que el diálogo siempre sea visible.
  `File > New/Open` con cambios pendientes abre `Unsaved Changes` con
  Save/Discard/Cancel; si Save no tiene path, encadena Save As y luego ejecuta la
  acción pendiente. Los errores se muestran en el modal `File Error`. El smoke
  `smoke_file` abre/cierra los diálogos y los tres modales.
- **Tests.** Nuevos `test_document_file` (3/17) y `test_file_session` (8/127),
  más casos de `state_id` en `test_undo` y round-trip/validación del escritor en
  `test_indexed_image`. Totales: 14 binarios, 144 casos y 1575 aserciones.
  Builds debug/release y las tres variantes de sanitizers OK.
- **Pendiente.** Prompt al cerrar la ventana, atajos Ctrl+N/O/S, título de
  ventana con el nombre, y modos `Binary`/`Packed` para archivos grandes. La
  paleta no se persiste (decisión de producto); el `.idx` guarda solo tamaño,
  bpp e índices.

## Sesión 2026-11-08 — Herramienta Line y rasterizador compartido

- **Rasterizador común.** `inc/core/line.h` expone `PaintCore::rasterize_line()`,
  un Bresenham entero con callback `plot(x,y)` que incluye ambos extremos y
  funciona en las ocho direcciones. No depende de DrawBuffer ni de ImGui: sirve
  igual para Line, Pencil, Brush y Eraser.
- **LineTool.** `LineTool` hereda de `DrawTool`, previsualiza la recta desde el
  punto de press hasta la posición actual y la confirma al soltar. El preview se
  recalcula entero en cada frame y se recorta al canvas con `DrawBuffer`.
  Añadido al toolbar en `main`, con iconos `assets/line_normal/hover/pressed.idx`
  y cursor crosshair del sistema; no declara opciones por ahora.
- **Mano alzada.** La estrategia acordada para Pencil/Brush/Eraser es conectar
  la última posición de imagen con la actual usando `rasterize_line()` en cada
  frame; así un movimiento rápido no deja huecos. Para un pincel grueso basta
  con estampar la forma en cada píxel de la recta. Alternativas como capturar
  todos los eventos SDL en una cola o suavizar con splines dan más fidelidad o
  curvas más suaves, pero añaden estado/latencia y no son necesarias para pixel
  art. El rasterizador por callback deja la puerta abierta a ambas.
- **Tests.** Nuevo `tests/test_line.cpp` (4 casos, 668 aserciones) para
  direcciones, continuidad, endpoints, recorte y commit de la tool. Nuevo
  `smoke_line` que simula un salto brusco del ratón y verifica que la recta
  completa queda dibujada. Totales: 15 binarios, 148 casos y 2243 aserciones.
- **Assets.** `line_normal.idx`, `line_hover.idx` y `line_pressed.idx` son iconos
  16×16 3bpp con la misma base que Rect y una diagonal interior.

## Sesión 2026-11-08 — Lápiz y FreehandTool

- **Problema.** Con un movimiento rápido del ratón, ImGui solo entrega la
  posición final del frame; dibujar únicamente ese punto deja huecos. La solución
  es conectar la posición de imagen anterior con la actual mediante
  `PaintCore::rasterize_line()`.
- **Base común.** `FreehandTool` (`inc/tools/freehand_tool.h`) hereda de
  `DrawTool` y centraliza el ciclo: en press inicializa el buffer, guarda la
  última posición y estampa un punto; en move añade el segmento anterior→actual
  sin limpiar el buffer; en release añade el último segmento y confirma. El
  preview se acumula, no se redibuja desde cero.
- **PencilTool.** Solo implementa `stamp()` escribiendo el foreground en el
  `DrawBuffer` (con el offset `buffer.x()/y()`). Brush y Eraser podrán heredar de
  `FreehandTool` cambiando únicamente el stamp; una brocha gruesa deberá ampliar
  el buffer para su huella. No fue necesario tocar `DrawTool`, `CanvasArea` ni el
  sistema de historial.
- **Alternativas descartadas.** Capturar todos los eventos SDL en una cola da el
  recorrido exacto pero exige plomería en el backend; suavizar con splines añade
  latencia y deforma el trazo; subir el polling reduce pero no elimina los
  huecos entre frames. La interpolación por segmentos es la opción estándar para
  pixel art y ya queda compartida.
- **Tests.** Nuevo `tests/test_pencil.cpp` (4 casos, 11 aserciones) para salto
  rápido, acumulación de segmentos, click de un píxel y recorte. Nuevo
  `smoke_pencil` con dos saltos de ratón y comprobación del trazo completo.
  Totales: 16 binarios, 153 casos y 2262 aserciones. El cursor de Pencil es un
  18×18 extraído de `xcursor-themes` (tema redglass, MIT/X11), con hotspot en la
  punta `(2,14)`. *El cursor se redibujó desde cero el 2026-11-11 y ya no usa el
  asset de xcursor-themes; ahora está cubierto por `assets/LICENSE` (CC0).*

## Sesión 2026-11-08 — Botones izquierdo/derecho y cancelación

- **Colores efectivos del trazo.** `DrawTool` guarda `m_stroke_foreground` y
  `m_stroke_background`, calculados al pulsar: izquierdo usa foreground/background
  normales; derecho los intercambia. Las tools de forma (`SquareTool`,
  `CircleTool` y el futuro Fill) usan ambos; Line y las tools de trazo usan el
  foreground efectivo, que con el derecho es el background original.
- **Selección.** `ToolBase::accepts_button()` limita por defecto a izquierdo;
  `DrawTool` acepta ambos. Selection sigue aceptando solo izquierdo, así que el
  botón derecho no crea marquees.
- **Ambos botones.** `CanvasArea` detecta ambos pulsados, cancela el trazo no
  confirmado con `PaintSession::cancel_active_stroke()`, lo descarta sin tocar
  el historial y bloquea nuevos trazos hasta que se suelten los dos. Si se suelta
  el botón activo mientras el otro sigue pulsado, tampoco se inicia otro trazo.
- **Enrutado.** El router ya no depende de `ImGui::IsItemActive()`: mientras un
  trazo está activo, los movimientos se envían a la tool aunque el cursor salga
  del item. El botón que inició el trazo decide el release que confirma.
- **Tests.** Line y Pencil prueban color de background con derecho y cancelación
  con ambos; Square prueba el intercambio outline/fill. Nuevo `smoke_buttons`
  con Pencil derecho, Rect derecho con colores intercambiados y un trazo izquierdo
  cancelado al pulsar derecho. Totales: 16 binarios, 158 casos y 2284
  aserciones; 17 smokes.

## Sesión 2026-11-08 — Modificadores Shift

- **Estado transitorio.** `PaintCore::Modifiers` (`inc/core/modifiers.h`) lleva
  shift/ctrl/alt. `PaintSession` lo expone con `modifiers()`/`set_modifiers()`;
  `CanvasArea` lo actualiza cada frame desde `ImGuiIO` (`KeyShift`, `KeyCtrl`,
  `KeyAlt`). No es una opción clickeable ni entra en el historial: al soltar
  Shift, la siguiente previsualización vuelve al comportamiento normal.
- **Square.** `square_constrained_end()` (`inc/core/shape_constraint.h`) usa el
  menor de |dx|/|dy| y conserva la dirección del arrastre. `SquareTool` y
  `SelectionTool` lo usan con Shift; el marquee de selección también queda
  cuadrado antes de crear la región flotante.
- **Line.** `snapped_line_end()` aplica los umbrales acordados: menos de 30°
  horizontal, 30–60° diagonal a 45° y más de 60° vertical, en las cuatro
  direcciones. Para la diagonal usa el mayor de |dx|/|dy| como lado del cuadrado
  envolvente, de modo que la línea llega al eje dominante del arrastre.
- **Live update.** `CanvasArea` ya enviaba `handle_mouse_move` cada frame
  mientras hay un trazo activo; `DrawTool::draw_preview()` recibe ahora solo
  `const PaintCore::Modifiers&`, así que pulsar o soltar Shift sin mover el ratón
  actualiza el preview inmediatamente sin acoplar la vista al dominio. Pencil ignora Shift.
- **Tests.** Casos nuevos para los helpers, Square con Shift, Line en
  horizontal/diagonal/vertical y Selection cuadrado. Nuevo `smoke_modifiers`
  inyecta Shift real por SDL. Totales: 16 binarios, 162 casos y 2333 aserciones;
  18 smokes.

## Sesión 2026-11-08 — Herramienta Circle

- **Base compartida.** `ShapeTool` (`inc/tools/shape_tool.h`) hereda de
  `DrawTool` y declara el enum `Mode { Outline, Fill, Both }` y la opción
  `mode`. `SquareTool` y `CircleTool` heredan de ella; `SquareTool::Mode` sigue
  siendo válido por herencia. Las claves de icono (`rect.outline`, `rect.fill`,
  `rect.both`) se reutilizan porque la hoja `rect_options.idx` ya representa
  esos tres modos.
- **Rasterizador.** `inc/core/ellipse.h` inscribe la elipse en el rectángulo
  inclusivo `[left..right] x [top..bottom]`. Trabaja con coordenadas de centro
  de píxel: `cx = left + width/2`, `cy = top + height/2`, `rx = width/2`,
  `ry = height/2`; para cada fila calcula
  `dy = (y + 0.5) - cy`, `dx = rx * sqrt(1 - (dy/ry)^2)` y redondea a un span
  inclusivo `[x0, x1]`. En alturas pares las dos filas centrales se tratan como
  el ecuador para que la elipse toque los bordes izquierdo/derecho; las filas
  donde el ancho es menor que un píxel caen al píxel o píxeles centrales, de
  modo que las puntas superior/inferior también tocan el bbox.
- **Relleno por spans.** `rasterize_ellipse_spans()` emite un span horizontal
  por fila y `CircleTool` lo escribe con `set_pixel`. No se dibujan círculos
  concéntricos ni se evalúa la ecuación por píxel: hay O(alto) raíces y O(área)
  escrituras, sin overdraw. Es la misma familia de primitiva que necesitará el
  futuro bote de pintura (scanline flood fill): rellenar una tirada horizontal
  y enlazar las tiradas de las filas contiguas.
- **Contorno.** `rasterize_ellipse_outline()` deriva el borde de los mismos
  spans: tapa superior/inferior completas, píxeles laterales de cada fila y
  tramos horizontales de transición dibujados en la fila más ancha. Así Outline,
  Fill y Both son coherentes y el contorno queda conectado y simétrico.
- **Comportamiento.** Sin Shift la figura es una elipse dentro del bbox del
  arrastre; con Shift usa `square_constrained_end()` (lado menor), igual que
  Rect y Selection. Hereda de `DrawTool` el botón izquierdo/derecho (derecho
  intercambia foreground/background) y la cancelación al pulsar ambos botones.
  El botón Circle de `main` ya instancia `CircleTool`; su cursor sigue siendo el
  asset sin hotspot que prueba el fallback del sistema.
- **Tests.** Nuevo `tests/test_circle.cpp` (12 casos, 5843 aserciones) con spans,
  simetría, toque del bbox, coherencia contorno/relleno, Outline/Fill/Both,
  Shift, botón derecho, cancelación, recorte fuera del canvas y tamaños
  1x1/2x2. Nuevo `smoke_circle` con elipse 16x16 y círculo 9x9 con Shift por
  SDL. Totales: 17 binarios, 174 casos y 8176 aserciones; 19 smokes.

## Sesión 2026-11-08 — Herramienta Brush

- **Tool y huellas.** `BrushTool` hereda de `FreehandTool`; cada píxel del
  recorrido interpolado estampa la huella activa. `PaintCore::make_brush_offsets()`
  (`inc/core/brush.h`) define las 12 huellas: círculos de radio 3, 2 y 1 (7x7,
  5x5 y 3x3); cuadrados 8, 5 y 2; diagonales 45 de 8, 5 y 2; diagonales 135 de
  8, 5 y 2. En tamaños pares el hotspot es el píxel central superior izquierdo,
  de modo que la fila/columna extra queda a la derecha/abajo. Los círculos se
  calculan por cobertura de área (60%): radio 1 es un plus 3x3, radio 2 un
  círculo 5x5 y radio 3 un círculo 7x7.
- **Opciones en cuadrícula.** `ToolOption::grid_columns` permite pintar una
  opción `Choice` como cuadrícula; Brush usa 3 columnas y 4 filas con el orden
  anterior, de mayor a menor, y Circle r3 por defecto. Cada
  `ToolOptionChoice::preview` lleva su huella, y `renderToolOptions()` pinta una
  miniatura nítida (escala entera) centrada en la celda. `kToolOptionsHeight`
  sube de 88 a 116 px para las cuatro filas.
- **Cursor.** `CursorType::Brush` usa una cruz de 17x17 con patrón alterno
  blanco/gris oscuro y hotspot central transparente. En esta sesión se generaba
  con `PaintCore::make_brush_cursor()`; después se externalizó a
  `assets/cursors/slim_cross.ipx` (ver “Sesión 2026-11-09 — Cursor Brush externo”).
  `PaintBackend::setCursorImage()` / `loadCursor()` la instala al arrancar; si
  falla, queda el crosshair del sistema. El cursor del SO queda siempre por
  encima del overlay de la huella.
- **Preview en el canvas.** `ToolBase::stamp_preview_offsets()` expone la huella
  activa. `CanvasArea` guarda el píxel bajo el puntero y, tras el canvas y la
  rejilla, pinta la huella semitransparente con el color de foreground,
  centrada en ese píxel y recortada al canvas. Es un overlay de vista: no toca
  documento, historial ni selección.
- **Recorte del buffer.** `FreehandTool::stamp_margin()` devuelve el margen de
  la huella y `append_segment()` llama a `ensure_fit()` con las cuatro esquinas
  del stamp en ambos extremos. Así una brocha de 8x8 no se recorta al dibujar
  cerca de los bordes del canvas.
- **Huecos en diagonales.** Cuando una brocha diagonal de 1 px se mueve por la
  diagonal contraria, las líneas estampadas en píxeles consecutivos quedan
  separadas por 1 px. `FreehandTool::stamp_segment_step()` recibe cada paso
  unitario del recorrido; `BrushTool` añade un estampado en la esquina
  `(to.x, from.y)` solo cuando la brocha y el paso son perpendiculares. Así se
  rellena el hueco sin engordar los trazos paralelos ni afectar a círculos,
  cuadrados o lápiz.
- **Tests.** Nuevo `tests/test_brush.cpp` (6 casos, 141 aserciones) para
  máscaras, orden/opciones, estampado, margen, huecos diagonales, botón derecho
  y cancelación; el cursor procedural se prueba en `test_cursor_asset.cpp`.
  Nuevo `smoke_brush` con preview, selección real en la cuadrícula, trazo de
  círculo radio 3, cuadrado 2 y diagonal 5. Totales: 18 binarios, 181 casos y
  8329 aserciones; 20 smokes.

## Sesión 2026-11-09 — Herramienta Eraser

- **Tool.** `EraserTool` hereda de `FreehandTool` y usa cuadrados de 4, 6, 8 y
  10 px (opción `size`, lista vertical de menor a mayor, 4 por defecto). El
  botón izquierdo pinta el background; el derecho solo sustituye los píxeles de
  la huella que tengan el foreground. La comprobación se hace por cada píxel del
  cuadrado contra la imagen principal, no solo en el píxel del recorrido.
- **Colores del trazo.** Al pulsar se congelan `m_erase_color` (background) y
  `m_erase_mask_color` (foreground), así que un cambio de paleta a mitad del
  trazo no altera lo que se está borrando. Ambos botones cancelan la operación.
- **Preview.** `ToolBase::StampPreview` amplía el preview de Brush con `opaque`
  y `border`: la goma pinta un cuadrado opaco con el background y un borde azul
  de 1 px de pantalla, fijo e independiente del zoom. El borde se dibuja fuera
  de la huella para no tapar ningún píxel borrado.
- **Cursor.** `CursorType::Eraser` usa `PaintCore::make_invisible_cursor()`, un
  cursor de 1x1 totalmente transparente. La vista solo aplica el cursor de la
  tool cuando el puntero está sobre la imagen; en el margen gris vuelve la
  flecha. Si la creación del cursor falla, queda el cursor del sistema.
- **Buffer.** `stamp_margin()` devuelve el margen del cuadrado (5 px para el
  tamaño 10) y `FreehandTool::append_segment()` expande el `DrawBuffer` con las
  cuatro esquinas del stamp. `FreehandTool::on_mouse_release()` estampa el
  último segmento antes de limpiar `m_active_button`, necesario para que el
  botón derecho conserve su modo de filtrado en el release.
- **Tests.** Nuevo `tests/test_eraser.cpp` (5 casos, 73 aserciones) para
  opciones, izquierdo, derecho por píxel, cancelación, margen y undo; el cursor
  invisible se prueba en `test_cursor_asset.cpp`. Nuevo `smoke_eraser` con
  preview, comprobación del cursor invisible, selección real de tamaño en la
  cuadrícula, borrado izquierdo y derecho selectivo. Totales: 19 binarios, 187
  casos y 8411 aserciones; 21 smokes.

## Sesión 2026-11-09 — Herramienta Fill

- **Rasterizador.** `inc/core/fill.h` expone `FillSpan` y dos funciones puras:
  `collect_flood_fill_spans()` para la región 4-conectada que contiene la
  semilla y `collect_global_fill_spans()` para todas las tiradas del color. El
  flood fill es scanline: expande cada semilla horizontalmente, emite la tirada,
  marca los píxeles visitados y busca nuevas semillas en las filas superior e
  inferior. Usa un `std::vector<bool>` de visitados porque las tiradas se
  aplican después de recolectarlas (no depende de que el callback escriba), y
  ordena el resultado por fila y `x0`. No conoce `ImageBuffer` ni `DrawBuffer`.
- **Tool.** `FillTool` (`inc/tools/fill_tool.h`) hereda de `DrawTool` y lee el
  color de la semilla de la imagen principal. Calcula el bbox de las tiradas,
  dimensiona el `DrawBuffer` una sola vez y escribe el color de trazo. El commit
  es inmediato en `on_mouse_press` y la tool queda fuera de `is_drawing()`; así
  el release no crea un preview con un quad por píxel y el historial recibe un
  único paso. El botón derecho usa el background, como el resto de tools.
- **Opciones.** `mode` es una opción `Choice` con `Contiguous` (por defecto) y
  `Global`. Sus iconos viven en `assets/fill_options.idx` (60x32, bpp 3), una
  región conectada frente a varias regiones separadas; se generan con
  `temp/make_fill_options.py`. `main` registra `fill.contiguous` y
  `fill.global` y asigna `FillTool` al botón que antes tenía `nullptr`.
- **Cursor.** `fill.ipx` sigue sin hotspot a propósito, así que sobre la imagen
  se mantiene el fallback al cursor por defecto del sistema (flecha). Los
  cursores `.ipx` siguen aparcados.
- **Tests.** Nuevo `tests/test_fill.cpp` (11 casos, 124 aserciones) para spans,
  borde envolvente, conectividad 4 vs global, no-op/semilla inválida, opciones,
  relleno izquierdo/derecho, undo/redo, modo global y pulsación fuera del
  canvas. Totales: 20 binarios, 198 casos y 8535 aserciones. Nuevo
  `temp/smoke_fill.cpp`, que ejercita el frontend real con dos regiones
  desconectadas: contiguo (solo una), Global (ambas) y botón derecho
  (background). Capturas: `smoke_fill_contiguous.ppm`,
  `smoke_fill_global.ppm` y `smoke_fill_right.ppm`. Build debug/release y
  `smoke_fill` en verde; 22 smokes.

## Sesión 2026-11-09 — Temas de ImGui

- **Menú.** Nuevo `Settings > Theme` con los tres presets de color de ImGui:
  `Dark` (por defecto), `Light` y `Classic`. Cada entrada es un `MenuItem` con
  checkmark; al pulsarla se llama a `ImGui::StyleColorsDark/Light/Classic`.
- **Estado y aplicación.** `PaintFrontend` guarda `Theme` y
  `m_theme_applied`. `set_theme()` aplica el estilo inmediatamente si ya existe
  un contexto ImGui; si no (smokes que construyen el frontend antes de
  `ImGuiManager::init()`), el primer `render()` lo aplica. No hay persistencia:
  al reiniciar se vuelve a Dark.
- **Layout.** Los presets de ImGui solo cambian `ImGuiStyle::Colors`; no tocan
  `WindowPadding`, `ItemSpacing`, `FramePadding` ni `GetFrameHeight()`. El
  smoke lo comprueba explícitamente, así que no cambian los tamaños de toolbar,
  opciones ni paleta. La imagen, la paleta de la app y los iconos no dependen
  del tema; sí cambian los detalles que usan `ImGuiCol_HeaderActive` (borde del
  canvas, preview de Eraser/Resize).
- **Smoke.** Nuevo `temp/smoke_theme.cpp`: captura Dark, cambia a Light y
  Classic, verifica el tema activo, que las métricas de layout no cambian y que
  el color de fondo sí. Capturas: `temp/smoke_theme_dark.ppm`,
  `smoke_theme_light.ppm` y `smoke_theme_classic.ppm`. Build debug/release y
  smoke en verde; 23 smokes.
- **Pendiente.** Guardar el tema y otras preferencias en un archivo de
  configuración; por decisión de esta sesión se deja para el futuro.

## Sesión 2026-11-09 — Herramienta Pick Color

- **Tool.** `PickColorTool` (`inc/tools/pick_color_tool.h`) hereda de `ToolBase`
  y no escribe en el documento ni crea historial. En `on_mouse_press` lee el
  índice del píxel: izquierdo lo expone para foreground y derecho para
  background. Ignora clicks fuera del canvas, el índice transparente y botones
  distintos de izquierdo/derecho.
- **Entrega al backend.** `ToolBase::ColorPick` + `take_color_pick()` permiten
  que la tool entregue el color una sola vez sin conocer `PaintBackend`.
  `PaintFrontend::renderCanvas()` llama a `apply_pending_color_pick()` después
  de enrutar el input. El backend actualiza foreground/background y después
  vuelve a la última herramienta de edición.
- **Última tool de edición.** `PaintBackend::selectButton()` guarda
  `m_last_edit_button` con cualquier botón que tenga tool, excepto `Selection` y
  `Pick Color`. Si todavía no se ha elegido ninguna tool de edición, Pick Color
  se queda activo tras muestrear. Cambiar a Pick Color confirma la región
  flotante igual que cualquier otro botón; no hay excepción.
- **UI.** Nuevo botón `Pick Color` entre Fill y Circle, con iconos
  `assets/pick_normal/hover/pressed.idx` generados por
  `temp/make_pick_icons.py`. El toolbar pasa a 9 botones (5 filas en 2
  columnas), así que el alto mínimo de ventana crece una fila. Cursor propio
  `assets/cursors/pick.ipx` (24x24, hotspot 5,21) convertido del tema Yaru con
  `temp/make_move_cursor.py`; el fallback es crosshair. *El cursor se redibujó
  desde cero el 2026-11-11 (16×16, hotspot `(0,15)`) y ya no deriva de Yaru;
  está cubierto por `assets/LICENSE` (CC0).*
- **Tests.** Nuevo `tests/test_pick_color.cpp` (5 casos, 39 aserciones):
  selección izquierda/derecha, entrega única, fuera de canvas/transparente/
  cancelación, restauración de Fill tras Selection, ausencia de tool previa y
  commit de la selección flotante al elegir Pick Color. Totales: 21 binarios,
  203 casos y 8574 aserciones. Nuevo `temp/smoke_pick_color.cpp` con clicks
  izquierdo/derecho reales, comprobación de foreground/background y retorno a
  Fill; capturas `temp/smoke_pick_color_left.ppm` y
  `smoke_pick_color_right.ppm`. Build debug/release, test-full y 24 smokes en
  verde.

## Sesión 2026-11-09 — Herramienta Curve

- **Rasterizador.** `inc/core/bezier.h` expone `CubicBezier`,
  `cubic_bezier_point()` y `rasterize_cubic_bezier()`. El trazado se subdivide
  adaptativamente con tolerancia de 0.5 px: los tramos rectos o suaves se
  dibujan con un único `rasterize_line()` y solo se subdividen las zonas con
  curvatura fuerte. Esto evita el engrosamiento que producía un muestreo fijo
  denso con segmentos cortos solapados. El coste es proporcional al número de
  tramos planos, no al área.
- **Tool.** `CurveTool` hereda de `DrawTool`. El primer arrastre fija los
  extremos A/B; con Shift aplica `snapped_line_end()`, igual que Line. Al soltar
  entra en edición pendiente con P1/P2 en 1/3 y 2/3 de la recta. Los handles se
  arrastran con el botón izquierdo y Shift ya no interviene. El preview se
  reconstruye en cada movimiento. Enter, clic fuera de los handles o cambio de
  tool confirman; Escape, botón derecho o ambos botones cancelan. Los colores
  quedan congelados durante la edición y los handles se recortan al canvas.
- **Ciclo de edición pendiente.** `ToolBase` añade `has_pending_edit`,
  `commit_pending_edit`, `cancel_pending_edit`, `EditOverlay`/`EditGuide`/
  `EditHandle` y el enrutado de handles. `PaintSession` lo hace exclusivo con
  la selección flotante: `set_tool`, `begin_selection`/`begin_paste`, `resize`
  y guardar confirman; `undo` cancela la edición pendiente antes del historial;
  `can_undo` la incluye y `can_redo` la bloquea; `load_image` la descarta.
  `PaintBackend::is_modified()` también la cuenta.
- **Vista.** `CanvasArea` pinta las guías del polígono de control y los handles
  como overlay fijo en pantalla (las anclas A/B no son arrastrables), dibuja el
  handle activo resaltado, usa `CursorType::Move` sobre ellos y desactiva los
  handles de resize mientras hay una edición pendiente. Los clics en el margen
  del canvas confirman; los clics en toolbar/paleta no interfieren.
- **UI.** Botón `Curve` entre Line y Pencil, con iconos
  `assets/curve_normal/hover/pressed.idx` generados por
  `temp/make_curve_icons.py` y cursor crosshair del sistema. El toolbar pasa a
  10 botones, que siguen ocupando 5 filas en 2 columnas; el alto mínimo no
  cambia. No declara opciones.
- **Tests.** Nuevo `tests/test_curve.cpp` (11 casos, 353 aserciones):
  endpoints, recta con controles colineales, bbox, ausencia de bloques 2x2,
  handles por defecto, reshape, Shift solo en el tramo inicial, botón derecho,
  commit al cambiar de tool, undo que cancela la edición antes del historial,
  cancel y resize. Totales: 22 binarios, 214 casos y 8927 aserciones. Nuevo
  `temp/smoke_curve.cpp` (zoom 8) que dibuja, comprueba
  handles, arrastra P1, confirma con clic en el margen, cambia de tool,
  cancela con Escape y confirma con Enter. Capturas:
  `temp/smoke_curve_pending.ppm`, `smoke_curve_handles.ppm`,
  `smoke_curve_committed.ppm`, `smoke_curve_tool_switch.ppm` y
  `smoke_curve_enter.ppm`. Build debug/release, test-full y 25 smokes en verde.

## Sesión 2026-11-09 — Afinado de toolbar, Delete y opciones

- **Orden y default.** `main` registra el toolbar en el orden Selection, Eraser,
  Fill, Pick Color, Pencil, Brush, Line, Curve, Rect, Circle (2 columnas, 5
  filas) y llama a `selectButton("Pencil")` al arrancar. Pencil queda también
  como última tool de edición, así que el primer uso de Pick Color vuelve a él.
- **Selection > Delete.** `FloatingRegionEditor::delete_floating_region()`
  rellena el hueco original con `hole_fill` y descarta el contenido sin copiarlo
  al portapapeles; si la selección se había movido, el destino no se escribe.
  Un paste se limita a descartarse. Se añadió la entrada `Delete` (atajo `Del`)
  al menú Selection y la tecla `ImGuiKey_Delete` en los atajos globales, con
  comprobación de `WantTextInput` y popups. Entra en el historial como un único
  paso cuando el hueco cambia.
- **Opciones dinámicas.** Las listas de opciones con iconos (`Rect`/`Circle`,
  `Selection`, `Fill`) reparten el alto disponible del hueco entre sus filas, y
  el icono se dibuja al tamaño de la celda. Brush y Eraser conservan sus
  previews compactos de 24 px. `assets/fill_options.idx` se regeneró con celdas
  de 60x46 (región conectada y cinco regiones separadas) para que el icono no se
  deforme al ocupar las dos filas; `main` carga la nueva altura.
- **Filtro de Open.** El diálogo usa `Indexed Image{.idx},All files{.*}`, de modo
  que se pueden seleccionar `.ipx` u otros archivos; la validación sigue en
  `IndexedImage::loadFromFile` y el error se muestra en `File Error`.
- **Tests.** `test_floating_region` añade delete con selección movida, delete de
  paste y delete sin región (3 casos, 25 aserciones; total del binario 43 casos
  y 353 aserciones). Totales: 22 binarios, 217 casos y 8952 aserciones. Nuevo
  `temp/smoke_selection_delete.cpp` (delete directo y tecla Delete); los smokes
  de Selection se actualizaron para el nuevo layout y las coordenadas del menú.
  Build debug/release, test-full y 26 smokes en verde.

## Sesión 2026-11-09 — Anchos de Line/Curve y opciones al pulsar

- **Cinco anchos.** `LineTool` y `CurveTool` declaran la opción compartida
  `width` (`inc/tools/line_width.h`). Los valores 1..5 se representan en el
  panel con cinco líneas horizontales de 1, 2, 3, 4 y 5 px de grosor, con
  etiqueta `1`..`5` a la derecha y tooltip `Width N px`.
- **Huellas del trazo.** `PaintCore::make_line_stamp_offsets()`
  (`inc/core/line_stamp.h`) define: 1 = un píxel; 2 = cuadrado 2x2; 3 = cruz 3x3
  de brazos de 1 px; 4 = cruz 4x4 de brazos de 2 px; 5 = cruz 5x5 de brazos de
  3 px. Los tamaños pares siguen la convención del brush (hotspot arriba a la
  izquierda de los dos píxeles centrales, el extra cae a la derecha/abajo). El
  trazo barre la huella con `rasterize_line()` o `rasterize_cubic_bezier()` y
  expande el `DrawBuffer` por el margen de la huella para no recortar los
  extremos; el ancho se congela al pulsar, igual que los colores. Los handles y
  guías de Curve no reciben grosor.
- **Opciones al pulsar.** Las filas y celdas de opciones usan
  `ImGui::IsItemClicked()` tras su `InvisibleButton`, así que el valor cambia en
  el frame del botón abajo, no al soltar. Los smokes de Line y Brush comprueban
  el valor antes del release.
- **Layout.** Las listas de previews mantienen 24 px de fila y solo se reducen
  cuando no caben; Line/Curve usan cinco filas de ~19.6 px. Brush conserva su
  cuadrícula 4x3 y Eraser sus cuatro filas de 24 px.
- **Tests.** `test_line` añade huellas, opciones, un caso por ancho, clipping
  grueso, congelación del ancho y botón derecho grueso (14 casos, 794
  aserciones). `test_curve` añade opciones, barrido de los cinco anchos y
  congelación (14 casos, 412 aserciones). Totales: 22 binarios, 226 casos y
  9086 aserciones. Smokes de Line, Curve y Brush actualizados; 26 smokes en
  verde. Build debug/release y test-full en verde.

## Sesión 2026-11-09 — Cursor Brush externo

- **Asset.** `assets/cursors/slim_cross.ipx` (originalmente `brush.ipx`) contiene
  la cruz de 17x17: hotspot central en el índice 1, brazos alternos índice 3
  (blanco) e índice 2 (negro, que el loader pinta como gris oscuro para Brush);
  el resto es transparente.
  El centro sigue transparente para no tapar la huella de preview del brush.
  La primera versión de esta sesión usó 3/4 y 3 bpp; después se normalizó a
  2 bpp (ver “Sesión 2026-11-09 — Cursor .ipx en 2 bpp”).
- **Interpretación del índice 2.** En esta sesión el negro pasó a gris oscuro
  para el cursor Brush; posteriormente la excepción se limitó a Brush y el
  resto de cursores recuperó el negro. La representación en disco usa el mismo
  índice 2 en todos los `.ipx`.
- **Código.** `main` carga Brush con `loadCursor("Brush", ...)` como Rect,
  Pencil, Fill, Pick Color y Circle; se eliminó `PaintCore::make_brush_cursor()`
  de `inc/core/cursor.h`. Eraser sigue usando `make_invisible_cursor()`.
- **Tests.** `test_cursor_asset` actualiza el color esperado del índice 3 y carga
  `assets/cursors/slim_cross.ipx` (9 casos, 49 aserciones). Totales: 226 casos y 9088
  aserciones. El smoke de Brush carga el asset. Build debug/release, test-full y
  26 smokes en verde.

## Sesión 2026-11-09 — Cursor Fill de 2 bpp

- **Asset.** `assets/cursors/fill.ipx` es un dibujo de 14x11 en 2 bpp con el
  cubo inclinado, hotspot en `(0,10)` (la punta por la que vierte) y el resto
  transparente. El tamaño no tiene que ser fijo; basta con que sea positivo y
  tenga un único hotspot.
- **Formato (estado intermedio).** En ese momento el loader aceptó bpp >= 2 y
  pintó el índice 2 como gris oscuro para validar el asset. Después se normalizó
  todo a 2 bpp con `2`=negro y `3`=blanco (ver “Sesión 2026-11-09 — Cursor .ipx
  en 2 bpp”).
- **Conexión.** `main` ya cargaba `assets/cursors/fill.ipx` con
  `loadCursor("Fill", ...)`; el cambio de formato hace que el asset sea válido.
  `smoke_cursor` ya no espera el fallback de Fill: comprueba que carga.
- **Tests.** `test_cursor_asset` añade 2 bpp con índice 2, rechazo de 1 bpp y
  carga del asset Fill (11 casos, 68 aserciones). Totales: 228 casos y 9107
  aserciones. Build debug/release, test-full y 26 smokes en verde.

## Sesión 2026-11-09 — Cursor .ipx en 2 bpp

- **Convención final.** Todos los `assets/cursors/*.ipx` usan exactamente 2 bpp:
  `0`=transparente, `1`=hotspot, `2`=negro y `3`=blanco. No hay índices 4..7.
- **Archivos.** `brush`, `circle`, `move`, `pencil`, `pick` y `rect` pasaron de
  3 bpp a 2 bpp remapeando `3`→`2` (negro) y `4`→`3` (blanco). `fill.ipx` ya
  estaba en la convención nueva (`2` negro, `3` blanco, con cuatro píxeles de
  brillo) y se dejó intacto. `circle.ipx` se eliminó después al compartir
  `cross.ipx`; `rect.ipx` y `brush.ipx` se renombraron a `cross.ipx` y
  `slim_cross.ipx`.
- **Excepción Brush.** `load_cursor_asset(path, image, error, black_as_dark_gray)`
  pinta el índice 2 como `{96,96,96}` cuando `PaintBackend::loadCursor()` carga
  `CursorType::Brush`; `slim_cross.ipx` usa el mismo `2` que el resto. Las demás
  tools pintan el índice 2 negro.
- **Validación.** El loader exige exactamente 2 bpp; `IndexedImage` ya impide
  valores fuera de 0..3, así que la conversión de color solo distingue 0, 1, 2
  y 3.
- **Tests.** `test_cursor_asset` cubre la convención 0/1/2/3, el modo Brush,
  bpp distinto de 2, hotspot único y los assets reales Brush/Fill/Pencil
  (10 casos, 75 aserciones). Totales: 227 casos y 9114 aserciones. Build
  debug/release, test-full y 26 smokes en verde.

## Sesión 2026-11-09 — Cursores compartidos

- **Cruz compartida.** `rect.ipx` pasó a `cross.ipx`; Selection, Line, Curve,
  Rect y Circle usan `CursorType::Crosshair` y comparten esa cruz de precisión.
- **Brush.** `brush.ipx` pasó a `slim_cross.ipx`; mantiene
  `CursorType::Brush` y la excepción de pintar el índice 2 como gris oscuro.
- **Limpieza.** Se eliminó `circle.ipx` y se quitaron `CursorType::Rect` y
  `CursorType::Circle`; el backend ya no crea un cursor por herramienta.
  `fill.ipx`, `pencil.ipx`, `pick.ipx` y `move.ipx` se mantienen.
- **Tests.** Se actualizaron las dimensiones/hotspot actuales de Fill (19x18,
  hotspot `(2,16)`) y Pencil (hotspot `(4,16)`). Verificación mínima:
  `test_cursor_asset` (10 casos, 75 aserciones) y smokes `cursor` y `brush` en
  verde; no se ejecutó la matriz completa. Totales sin cambios: 227 casos y
  9114 aserciones.

## Sesión 2026-11-10 — Nuevo modelo de botones

- **Assets.** 10 `assets/*_button.idx` de 32×32 y 3 bpp: índice 0
  transparente y el resto escala de grises. Sustituyen a los pares
  normal/hover/pressed del toolbar, que se eliminaron de `assets/` (copia en
  `temp/old_toolbar_icons/`).
- **Modelo.** `ToolButton` guarda un único `icon`; `addButton()` acepta una
  sola ruta. `m_icon_palette` deja el índice 0 transparente.
- **Render.** `renderToolbar()` usa `ImageButton` con `FramePadding = 0` para
  mantener el área de 32×32. El botón activo fuerza `ImGuiCol_ButtonActive` en
  `Button`, `ButtonHovered` y `ButtonActive`, así que se queda pulsado;
  hover/pressed usan los colores de ImGui y siguen el tema en caliente.
- **Limpieza.** Eliminados los 4 botones de prueba,
  `loadTestButtonIcon()`/`renderTestButtons()`, `m_test_button_icon` y
  `assets/circle_test.idx` (copia temporal en `temp/circle_test.idx`).
- **Tests.** Actualizados `test_pick_color` y los smokes a la nueva firma de
  `addButton()`. Verificados `test_pick_color` (5 casos, 39 aserciones) y los
  smokes `buttons` y `cursor` en verde; no se ejecutó la suite completa.

## Sesión 2026-11-10 — Empaquetado AppImage

- **Objetivo.** `make appimage` ejecuta `packaging/build_appimage.sh` y deja
  `build/QtreGBPaint-x86_64.AppImage`. El script compila release, prepara el
  AppDir en `temp/appimage/`, copia assets a `usr/assets`, usa `linuxdeploy`
  para SDL2/dependencias y `appimagetool` para el empaquetado.
- **Herramientas.** `linuxdeploy` y `appimagetool` se descargan a
  `temp/appimage-tools/` si faltan. `packaging/` contiene el `.desktop`, el
  icono 256×256 y el script.
- **Portabilidad.** Se copian también `libstdc++.so.6` y `libgcc_s.so.1`, que
  `linuxdeploy` omite por defecto. Tamaño resultante: **10.381.816 bytes
  (~9,9 MiB)**; sin esas dos librerías eran **9.435.640 bytes (~9,0 MiB)**.
  Probado en el equipo: arranca y carga los assets.
- **Pendiente para una entrega real.** AppStream metadata (resuelto el
  2026-11-11), compilar en una base más antigua (Ubuntu 22.04) para
  compatibilidad de glibc y revisar licencias de terceros. El antiguo pendiente
  de mover `imgui.ini` a `$XDG_CONFIG_HOME` quedó resuelto en la sesión de
  configuración `.ini`.

## Sesión 2026-11-10 — Título de ventana, cierre y atajos

- **Título de ImGui.** La ventana principal usa `PaintCore::make_window_title()`
  (`inc/paint/window_title.h`): `*<nombre> - QtreGBPaint`, con el `*` delante
  cuando el documento tiene cambios sin guardar. El nombre sale de
  `DocumentFile::display_name()` (`Untitled` si no hay path). El frontend añade
  el sufijo estable `###QtreGBPaintMain` para que el ID de la ventana no cambie
  con el título, y centra el texto con
  `ImGuiStyleVar_WindowTitleAlign = (0.5, 0.5)`. Se añadió
  `ImGuiWindowFlags_NoSavedSettings` para no acumular entradas por nombre en
  `imgui.ini`. El nombre de archivo ya no se muestra dentro del menú File.
- **Título del SO.** `SDL_CreateWindow` usa `QtreGBPaint` (antes `QtreGB`), que
  es lo que muestra el gestor de ventanas.
- **Cierre con cambios.** `IGuiBackend` expone `quit_requested()` y
  `clear_quit_request()`; `poll_events()` ya no termina el bucle por sí mismo,
  solo registra la petición. `PaintFrontend::render()` devuelve `bool` (false =
  salir) y decide: sin cambios cierra en el acto; con cambios abre
  `Unsaved Changes` con `FileAction::Quit` y espera a Save/Discard/Cancel.
  `main` usa el retorno para terminar el bucle.
- **Atajos.** `Ctrl+N`, `Ctrl+O` y `Ctrl+S` reutilizan el mismo camino que el
  menú y respetan los popups abiertos. Save solo actúa si hay cambios o el
  documento todavía no tiene path (en ese caso abre Save As).
- **Tests.** Nuevo `tests/test_window_title.cpp` (2 casos, 4 aserciones) para el
  formato del título; totales: 23 binarios, 229 casos y 9118 aserciones. Nuevos
  smokes `temp/smoke_close.cpp` (título, cierre limpio, prompt y resolución) y
  `temp/smoke_shortcuts.cpp` (Ctrl+N/O/S con eventos SDL reales). Smokes: 28.
  Builds debug/release, `test-full` (ASAN/UBSAN) y todos los smokes en verde.
- **Smokes existentes.** Se actualizaron a `poll_events()` sin parámetro, al
  título SDL `QtreGBPaint` y a localizar los hijos por `/Canvas_` y `/Palette`,
  porque el nombre de la ventana principal ya no es `PaintGB`.

## Sesión 2026-11-10 — Barra de estado y cuentagotas

- **Barra de estado.** Child `Status` al fondo de la ventana principal, con
  borde como el resto de paneles y siempre visible. `compute_status_bar_height()`
  reserva su alto y el mínimo de ventana lo incluye; el canvas y la paleta se
  reparten el espacio restante. Contenido: `Canvas: WxH`, `Zoom: Nx`,
  coordenadas `X/Y`, swatch + índice del color bajo el puntero y estado
  `Modified`/`Saved`.
- **Dos coordenadas.** `ToolBase::drag_anchor()` expone el punto fijo de las
  operaciones de dos puntos. `LineTool`, `ShapeTool` (Rect/Circle),
  `CurveTool` (solo en `Phase::Endpoints`) y `SelectionTool` lo implementan
  mientras el arrastre está activo. La barra muestra `X: inicio Y: inicio ->
  X: cursor Y: cursor` hasta confirmar o cancelar. Mover una región flotante,
  editar handles de Curve, Fill, Pencil/Brush/Eraser y el resize (fuera de la
  imagen) muestran solo la coordenada actual o `--`.
- **Cuentagotas al soltar.** `PickColorTool` ya no muestrea en
  `on_mouse_press`: guarda el botón pulsado, actualiza un preview con
  `update_preview()` y muestrea en `on_mouse_release()` en la posición de
  suelta. Si se suelta fuera de la imagen o sobre un píxel transparente no
  cambia el color; si se pulsan ambos botones, `CanvasArea` cancela con
  `cancel_active_stroke()`. `ToolBase::is_operating()` permite enrutar moves y
  release a tools que no pintan; `PaintSession::cancel_active_stroke()`,
  `undo()` y `redo()` usan ese estado.
- **Preview en opciones.** `ToolBase::color_pick_preview()` expone el color
  mientras el botón está pulsado y `renderToolOptions()` rellena todo el
  recuadro de opciones con ese color (dejando el borde del child visible). El
  preview conserva el último color válido si el puntero sale de la imagen.
  Pick no declara opciones, así que el relleno no tapa iconos.
- **Tests.** `test_pick_color` pasa a 7 casos y 63 aserciones (muestreo al
  soltar, preview, release fuera, transparencia, cancelación y backend). Se
  añadieron casos de `drag_anchor` a Line, Curve, Circle (ShapeTool), Square y
  Selection. Totales: 23 binarios, 236 casos y 9181 aserciones. Nuevo
  `temp/smoke_status.cpp`, que comprueba la barra, las dos coordenadas, el
  preview de opciones, el release fuera, la cancelación con ambos botones y la
  visibilidad al tamaño mínimo; capturas `temp/smoke_status_idle.png`,
  `smoke_status_drag.png`, `smoke_status_pick.png` y `smoke_status_min.png`.
  Smokes: 29. Builds debug/release, `test-full` y todos los smokes en verde.

## Sesión 2026-11-10 — Menús y atajos

- **Estructura.** Orden de menús: File, Edit, View, Selection, Image, Palette,
  Settings y Help. Los elementos sin implementación se muestran deshabilitados;
  sus aceleradores quedan visibles en el menú pero sin comportamiento.
- **File.** New (Ctrl+N), Open (Ctrl+O), Save (Ctrl+S), Save As... (Ctrl+Shift+S),
  separador, Export > Bitmap/PNG, separador, hasta 10 archivos recientes y,
  solo si hay recientes, otro separador antes de Exit (Alt+F4). Export sigue
  siendo un marcador; los recientes eran marcadores en esta sesión y pasaron a
  abrirse directamente en la sesión de configuración `.ini`.
- **Archivos recientes.** `PaintBackend::recent_files()` guarda hasta 10 rutas
  en memoria, nuevas primero y sin duplicados. Se actualiza al abrir y al
  guardar; en esta sesión no se persistía y las entradas eran de solo lectura.
  Se probó en `test_file_session` (orden, deduplicación, fallos y límite).
- **Edit/View.** Edit añade Select All (Ctrl+A); quedó como placeholder en esta
  sesión y se activó en “Sesión 2026-11-10 — Select All”. View (nuevo)
  contiene Grid (Ctrl+G, checkbox real); Zoom + / Zoom - quedaron como
  placeholders en esta sesión y se activaron en “Sesión 2026-11-10 — Zoom
  anclado y Ctrl+rueda”.
- **Selection.** Copy/Cut/Paste/Delete muestran Ctrl+C/X/V/Del y ahora están
  enlazados mediante `copy_selection()`/`cut_selection()`/`paste_selection()`;
  después, Flip/Rotate... (Ctrl+R), que quedó como placeholder en esta sesión y
  se activó en “Sesión 2026-11-10 — Flip/Rotate de selección”; separador e
  Invert Colors.
- **Image.** Flip/Rotate... (Ctrl+R); quedó como placeholder en esta sesión y
  se activó en “Sesión 2026-11-10 — Flip/Rotate de imagen e historial por
  transformación”. Properties... (Ctrl+E): el antiguo `Canvas Size` pasa a
  llamarse Properties; el diálogo conserva ancho, alto, nota de background y
  OK/Cancel. El miembro es `m_open_properties_popup`.
- **Help.** About QtreGBPaint... (placeholder en esta sesión; activado el
  2026-11-11 con logo PNG, autor y licencia).
- **Atajos nuevos.** Ctrl+Shift+S (Save As), Ctrl+G (Grid), Ctrl+E (Properties)
  y Ctrl+C/X/V usan acciones ya existentes. Ctrl+A y Ctrl+R se activaron en las
  sesiones “Select All” y “Flip/Rotate de selección”; Ctrl+± en “Zoom anclado y
  Ctrl+rueda”; Alt+F4 lo gestiona el gestor de ventanas. Todos los
  atajos respetan los popups abiertos.
- **Tests.** `test_file_session` añade archivos recientes; totales: 23
  binarios, 237 casos y 9213 aserciones. `smoke_shortcuts` amplía la cobertura
  a Ctrl+Shift+S, Ctrl+G, Ctrl+E y Ctrl+C/X/V. Nuevo `temp/smoke_menus.cpp` con
  capturas `temp/smoke_menus_{file,export,edit,view,selection,image,palette,settings,help}.png`.
  Smokes: 30. Builds debug/release, `test-full` y todos los smokes en verde.
- **Smokes existentes.** `smoke_selection` cambia la coordenada de Invert
  Colors por el nuevo elemento Flip/Rotate; `smoke_canvas_size` usa el popup
  `Properties`.

## Sesión 2026-11-10 — Zoom anclado y Ctrl+rueda

- **Matemática.** `ViewTransform::scroll_for_zoom_anchor(anchor, new_zoom)`
  devuelve el scroll que mantiene el punto de imagen bajo `anchor`:
  `scroll' = scroll + (anchor - origin) * (new_zoom / zoom - 1)`. Se probó en
  `test_view_transform` (punto conservado, centro, scroll relativo y zoom
  inválido).
- **Aplicación diferida.** El frontend acumula las peticiones (View > Zoom +/−,
  Ctrl++/Ctrl+−, Ctrl+rueda y slider) y las resuelve al final del frame con la
  vista actual: calcula el scroll y lo guarda en `m_pending_canvas_scroll`. El
  frame siguiente aplica el `SetNextWindowContentSize()` existente y
  `SetNextWindowScroll()`. Así no hay un frame con el zoom nuevo y el scroll
  viejo.
- **Anclaje.** Si el cursor está sobre el canvas y sobre la imagen, el ancla es
  el cursor; si no, el centro de la parte visible de la imagen
  (`CanvasArea::visible_image_center()`). El slider usa siempre el centro porque
  el ratón está en la paleta. Ctrl+rueda acumula fracciones para touchpads y
  comparte cola con Zoom +/−.
- **Atajos.** View > Zoom + y Zoom - quedan activos; Ctrl++ (Ctrl+= o
  Ctrl+Shift+=), Ctrl+- (incluido keypad) y Ctrl+rueda. Límites 1..64.
- **Barra de estado.** `CanvasArea::mouse_over_canvas()` (item hover/active)
  evita que la barra muestre coordenadas o color de imagen cuando el puntero
  está sobre la propia barra, la paleta o los scrollbars; el ancla por defecto
  usa el mismo estado.
- **Interacción con tools.** Al anclar en el cursor, el punto de imagen bajo el
  ratón no cambia y un trazo activo no se desplaza. En los bordes el scroll se
  recorta a `[0, ScrollMax]`, así que el anclaje puede ceder cuando no hay
  margen (por ejemplo, zoom out cerca de abajo-derecha).
- **Tests.** `test_view_transform` añade 3 casos. Totales: 23 binarios, 240
  casos y 9223 aserciones. Nuevo `temp/smoke_zoom.cpp` (teclado, rueda, centro y
  límites); capturas `temp/smoke_zoom_before.png` y `smoke_zoom_after.png`.
  Smokes: 31. Builds debug/release, `test-full` y todos los smokes en verde.

## Sesión 2026-11-10 — Select All

- **Tool.** `ToolBase::select_all()` es una capacidad opcional (por defecto
  devuelve false). `SelectionTool` la implementa con
  `PaintSession::begin_selection(Rect{0,0,w,h}, m_background, transparent)`, es
  decir, el mismo camino que un arrastre de esquina a esquina: respeta el modo
  Normal/Transparent, el background guardado para el hueco y confirma cualquier
  región flotante anterior.
- **UI.** Edit > Select All y Ctrl+A activan Selection y crean la región
  flotante de todo el canvas. El tool pasa a Selection igual que si se pulsara
  su botón; la última herramienta de edición no cambia.
- **Tests.** `test_floating_region` añade un caso (Normal, Transparent y
  reemplazo de una región existente). Totales: 23 binarios, 241 casos y 9242
  aserciones. Nuevo `temp/smoke_select_all.cpp` con Ctrl+A y la entrada del
  menú Edit, en modo Normal y Transparent; capturas
  `temp/smoke_select_all.png`, `smoke_select_all_transparent.png` y
  `smoke_select_all_menu.png`. Smokes: 32. Builds debug/release, `test-full` y
  todos los smokes en verde.

## Sesión 2026-11-10 — Flip/Rotate de selección

- **Transformación.** `FloatingRegionEditor::transform_floating_region()` acepta
  FlipHorizontal, FlipVertical, Rotate90, Rotate180 y Rotate270. Reasigna los
  píxeles en un `ImageBuffer` nuevo (mismo bpp) y llama a `set_content()`, que
  incrementa la revisión de la textura.
- **Geometría.** Flips y 180 conservan el `rect`. 90/270 intercambian ancho y
  alto y recentran el rect sobre el centro anterior; el resultado se limita para
  dejar al menos 1 px visible, igual que `move_floating_region()`. El `hole`
  original no cambia.
- **UI.** Selection > Flip/Rotate... y Ctrl+R solo se habilitan con una región
  flotante. El modal `Flip/Rotate` ofrece radios Flip horizontal, Flip vertical
  y Rotate; al elegir Rotate aparecen 90/180/270 grados. OK aplica la
  transformación y Cancel no hace nada; ninguno confirma ni cancela la región.
  En esta sesión el modal solo apuntaba a la selección; el objetivo Canvas se
  añadió en “Sesión 2026-11-10 — Flip/Rotate de imagen e historial por
  transformación”.
- **Tests.** `test_floating_region` añade un caso con los cinco transforms,
  revisión, hole, transparencia y ausencia de región. Totales: 23 binarios, 242
  casos y 9300 aserciones. Nuevo `temp/smoke_flip_rotate.cpp` (menú, Ctrl+R,
  Cancel y OK con Rotate 90); capturas `temp/smoke_flip_rotate_modal.png`,
  `smoke_flip_rotate_rotate.png` y `smoke_flip_rotate_result.png`. Smokes: 33.
  Builds debug/release, `test-full` y todos los smokes en verde.

## Sesión 2026-11-10 — Flip/Rotate de imagen e historial por transformación

- **Transformación compartida.** `PaintCore::ImageTransform`
  (`inc/core/image_transform.h`) y `PaintCore::transform_image()`
  (`inc/paint/image_transform.*`) centralizan las cinco permutaciones;
  `FloatingRegionEditor` y `CanvasDocument` las usan. El enum incluye
  `image_transform_swaps_axes`, `is_valid_image_transform` e
  `inverse_image_transform` (90↔270; flips y 180 son involutivos).
- **Canvas.** `CanvasDocument::transform_image()` genera la imagen transformada,
  evita historial si dimensiones y píxeles no cambian, apila un
  `UndoStep::canvas_transform` y sustituye el buffer. `PaintSession::transform_image()`
  confirma antes un trazo activo, la edición pendiente o la región flotante y
  cancela la tool activa; como máximo deja dos pasos: el pendiente y la
  transformación. `PaintBackend::transform_canvas()` lo reenvía.
- **Historial.** Los pasos de transformación guardan solo el enum (0 bytes):
  `undo` aplica la transformación inversa y `redo` la original. No hay pérdida
  de información porque son permutaciones exactas de píxeles, también con
  dimensiones impares. La validación rechaza pasos de transformación con
  parches, snapshots mezclados o un enum inválido.
- **UI.** Image > Flip/Rotate... (Ctrl+R) abre el mismo modal con «Transform
  image». Ctrl+R elige Selection si hay región flotante y Canvas si no; el menú
  Image siempre apunta al canvas. OK aplica y Cancel no hace nada.
- **Tests.** Nuevo `test_canvas_transform` (7 casos, 177 aserciones):
  permutaciones, inversas, undo/redo de los cinco transforms, flip simétrico sin
  historial, pasos de 0 bytes, validación y dos pasos con trazo/selección
  pendientes.
  Totales: 24 binarios, 249 casos y 9477 aserciones. Nuevo
  `temp/smoke_image_flip_rotate.cpp` (Cancel, OK Rotate 90, undo/redo y
  pending+flip); capturas `temp/smoke_image_flip_rotate_modal.png`,
  `smoke_image_flip_rotate_rotate.png`, `smoke_image_flip_rotate_result.png` y
  `smoke_image_flip_rotate_pending.png`. Smokes: 34. Builds debug/release,
  `test-full` y todos los smokes en verde.

## Sesión 2026-11-10 — Tema Light, Both por defecto e icono Outline

- **Tema.** `Settings > Theme` enumera Light, Dark y Classic; `Light` es el
  preset por defecto (`PaintFrontend::m_theme`). Los presets siguen cambiando
  solo colores y no se persisten.
- **Formas.** `ShapeTool` declara `mode` con `Both` por defecto, así que Rect y
  Circle arrancan con contorno y relleno. `Outline` mantiene el interior
  transparente en el buffer; `Fill` y `Both` no cambian.
- **Icono Outline.** `assets/rect_options.idx` se regeneró con el interior del
  icono Outline en índice 0 (transparente), de modo que se ve el color del tema
  en la zona de opciones; el borde sigue en índice 7. El script
  `temp/make_rect_options.py` queda actualizado.
- **Tests.** `test_tool_options` actualiza el default y añade un caso de Outline
  con interior transparente; `test_circle` fija Outline donde comprueba el
  hueco. Totales: 24 binarios, 250 casos y 9481 aserciones. `smoke_theme` se
  actualiza (Light por defecto y capturas Light/Dark/Classic) y `smoke_circle`
  fija Outline en su primera elipse. Smokes: 34. Builds debug/release,
  `test-full` y todos los smokes en verde.

## Sesión 2026-11-10 — Botones de toolbar sin fondo

- **Render.** `renderToolbar()` ya no usa el fondo de botón del tema para las
  tools no activas: `ImGuiCol_Button` se pinta transparente y hover/pressed usan
  un overlay neutro derivado de `ImGuiCol_Text` (alfa 0.08/0.14). Solo la tool
  activa conserva `ImGuiCol_ButtonActive` (azul).
- **Motivo.** Era el estilo de `ImageButton`, no un comportamiento fijo de
  ImGui; se personaliza con `PushStyleColor`. Los iconos de 32×32 y el tooltip
  siguen igual y el hover mantiene feedback.
- **Verificación.** Smokes `status`, `menus`, `theme` y `buttons` en verde;
  capturas `temp/smoke_status_idle.png` y `smoke_theme_light.png`/`dark.png`
  muestran el botón activo azul y el resto sin fondo.

## Sesión 2026-11-10 — Transparencia de paleta y fin de TRANSPARENT_INDEX

> Esta sesión sustituye las referencias históricas a `TRANSPARENT_INDEX` de
> apartados anteriores: el centinela ya no existe en el código actual.

- **Decisión.** La transparencia deja de ser un índice centinela y pasa a ser
  una propiedad opcional de la paleta. Cualquier swatch visible (incluido el
  255 en 8bpp) puede marcarse como transparente. Todos los bpp se tratan
  igual; `PaintCore::TRANSPARENT_INDEX` se eliminó de `inc/core/color.h`, del
  código y de los tests. En 8bpp los 256 índices son colores normales y el
  último swatch del panel es 255.
- **Paleta.** `Palette` guarda `std::optional<ColorIndex>` y `getColor()`
  devuelve `{0,0,0,0}` para ese índice sin borrar el color almacenado;
  `is_transparent()` consulta la propiedad. `palette_usable_color_count(8)`
  devuelve 256 y `is_visual_palette_index()` valida los índices muestreados
  (en 6bpp, por ejemplo, 61/62 no son swatches y no se pueden marcar).
- **Backend y menú.** `PaintBackend` expone `transparent_index()`,
  `has_transparent_color()`, `set_transparent_color()`,
  `set_transparent_foreground()` y `clear_transparent_color()`. El menú
  `Palette` añade `Transparent Foreground` (actúa sobre el foreground actual)
  y `Clear Transparent Color`. La transparencia no toca historial ni
  `is_modified`; `select_palette()` e `toggle_palette_inverted()` la
  conservan, y `new_document()`/`open_document()` la reinician. El diálogo
  `New Image` muestra `Colors: 256` en 8bpp.
- **DrawBuffer.** Se añadió una máscara de cobertura `std::vector<bool>`
  (`is_painted()`, `painted()`) que distingue píxeles escritos de no escritos.
  Los datos no pintados ya no usan 255; el preview y `blend_onto()` consultan
  la máscara. Pintar 255 o el índice transparente es una escritura normal, y
  las herramientas (Fill, Pencil, Eraser, selección, portapapeles, invert)
  funcionan igual en todos los bpp. `PickColorTool` puede muestrear cualquier
  índice, incluido el transparente.
- **Damero.** `inc/paint/checkerboard.h` (`PaintUi::draw_checkerboard`) pinta
  celdas de tamaño fijo en pantalla ancladas al origen de la imagen, con el
  primer píxel en el centro de una celda. `CanvasArea` lo dibuja bajo los tiles
  con celdas de 8 px; el preview del trazo y la huella del cursor lo muestran
  para píxeles transparentes, y el swatch foreground/background, los swatches
  del panel y el color de la barra de estado usan celdas de 4 px. El damero no
  cambia con el zoom y no forma parte del documento.
- **Verificación.** Suite normal: 251 casos y 9529 aserciones. Matriz
  `test-full` (normal + ASAN + UBSAN + ASAN+UBSAN): 1004 casos y 38116
  aserciones. Los 35 smokes pasan, incluido el nuevo
  `temp/smoke_transparency.cpp` (8bpp con 255 visible, fill/pencil/eraser con
  índices transparentes, damero en canvas y preview, undo/redo, save/open y
  reinicio de la transparencia). Builds debug y release OK. Capturas:
  `temp/smoke_transparency_fill.ppm` y
  `temp/smoke_transparency_preview.ppm`.
- **Pendiente.** El damero se pinta con rectángulos de ImGui por celda; si en
  ventanas grandes resulta costoso, la optimización natural es una textura de
  damero repetida en espacio de pantalla. *Resuelto el 2026-11-11 con
  `PaintUi::CheckerboardTexture`; ver “Sesión 2026-11-11 — Assets CC0, marca y
  damero con textura”.* No hay atajo de teclado para marcar el transparente.

## Sesión 2026-11-10 — Configuración .ini y geometría de ventana

- **Archivo y ubicación.** Nuevo `AppConfig` (`inc/paint/app_config.h`,
  `src/paint/app_config.cpp`) con formato plano `key=value`. Se guarda en
  `$XDG_CONFIG_HOME/qtregbpaint/config.ini` o, si no existe la variable,
  `~/.config/qtregbpaint/config.ini`. La escritura es atómica (`.tmp` +
  `rename`) y los errores de guardado solo se registran en `stderr`; nunca
  bloquean el cierre. Es la ubicación correcta tanto para un `.deb` (instalación
  de solo lectura) como para un AppImage (montaje temporal): no se escribe junto
  al ejecutable ni en el directorio de trabajo.
- **Contenido persistido.** `theme` (`dark`/`light`/`classic`), `grid`,
  `window.x/y/width/height/maximized`, `last_directory` y hasta 10
  `recent_file`. Las rutas se codifican con `std::quoted`. La herramienta activa
  no se guarda: siempre arranca Pencil. Tampoco se guardan la paleta ni el zoom,
  por decisión de producto.
- **Recientes.** Las entradas del menú `File` ya se pueden pulsar: abren el
  archivo directamente. Si hay cambios pendientes, pasan por el modal
  `Unsaved Changes` (Save/Discard/Cancel) y conservan la ruta elegida hasta
  reanudar la acción; Cancelar la descarta. Si la apertura falla, se muestra
  solo `File Error` y el documento actual queda intacto, sin cambiar la ruta ni
  el estado modificado.
- **Ventana.** `SDLRendererBackend::init_with_geometry()` crea la ventana oculta,
  valida que la posición guardada caiga dentro de algún monitor (si no,
  `SDL_WINDOWPOS_CENTERED`), aplica tamaño/posición, maximiza si el flag está
  activo y la muestra. `window_geometry()` devuelve la última geometría **no
  maximizada** y el flag de maximizado; `poll_events()` actualiza esa geometría
  cada frame e ignora los estados maximizado/minimizado. Así, cerrar maximizado
  no guarda el tamaño de pantalla como si fuera el tamaño normal.
- **Última carpeta.** `PaintFrontend` recuerda el directorio padre tras abrir o
  guardar correctamente y lo usa como valor inicial de Open/Save As. Si la ruta
  guardada desapareció o no se puede leer, no muestra ningún error: prueba
  `XDG_PICTURES_DIR`, luego `~/Pictures`, luego `$HOME` y, como último recurso,
  `"."`. También se evita usar el directorio de un documento actual si ya no es
  accesible.
- **ImGui ini.** `ImGuiManager::init()` fija `io.IniFilename = nullptr`, de modo
  que ImGui no lee ni escribe `imgui.ini`. El layout es fijo y
  `ImGuiWindowFlags_NoSavedSettings` por sí solo no impedía que otras ventanas o
  el file dialog crearan el archivo. Se eliminó el `imgui.ini` obsoleto que
  quedaba en la raíz del repo.
- **Verificación.** Nuevo `tests/test_app_config.cpp` (defaults, round-trip con
  rutas y espacios, valores malformados, límite de recientes, escritura atómica)
  y `temp/smoke_config.cpp` (config en XDG temporal, restauración de geometría,
  ausencia de `imgui.ini`), además de `temp/smoke_recent.cpp` (apertura directa
  de un reciente, error sin tocar el documento y flujo con cambios pendientes).
  Suite normal: 257 casos y 9576 aserciones; `test-full`: 1028 casos y 38304
  aserciones. Los 37 smokes pasan. Builds debug y release OK.

## Sesión 2026-11-10 — Corrección de recientes y cambio de bpp

- **Bug de rutas corruptas en recientes.** La causa era una referencia
  inválida: al abrir un reciente, `PaintFrontend` pasaba al backend una
  referencia a un elemento de `recent_files`; `open_document()` llamaba a
  `add_recent_file()`, que hacía `erase()` de ese mismo elemento y después
  copiaba la referencia ya liberada. El resultado eran prefijos binarios en la
  configuración (a veces visibles como `i`, `8` o similares) y entradas que
  desaparecían al recargar. Se corrigió en dos capas:
  - `PaintBackend::add_recent_file(std::string)` recibe una copia estable antes
    de tocar el vector.
  - `request_open_path(std::string)` y `open_recent_path(std::string)` también
    copian la ruta.
  - `AppConfig::load` descarta rutas con caracteres de control, de modo que una
    configuración ya corrupta se limpia sola al siguiente guardado.
  - La deduplicación ya existente se mantiene: si el archivo ya estaba, se
    mueve al principio en vez de duplicarse.
- **Cambio de bpp desde `Image > Properties...`.** El diálogo añade un slider
  de 1..8 bpp, el número de colores y un aviso cuando el valor difiere del
  documento. Al pulsar OK se aplica primero el resize (si lo hay) y luego el
  cambio de bpp. `ImageBuffer::remap_bits_per_pixel()` valida el mapping y
  reescribe todos los índices; `CanvasDocument::change_bits_per_pixel()` guarda
  un `CanvasBitsPerPixel` (snapshot anterior + mapping) en el historial, así que
  undo/redo restauran también el bpp y los índices.
- **Remapeo de tonos.** `PaintBackend::change_bits_per_pixel()` genera la
  paleta del bpp destino con el modo e inversión actuales y, para cada índice
  antiguo, elige el índice nuevo con menor distancia RGB al color almacenado.
  Si el índice transparente antiguo sigue siendo visible en el nuevo bpp se
  conserva y los colores normales nunca se mapean a él; si no, se pierde la
  transparencia y se usa el color almacenado. `undo`/`redo` regeneran la paleta
  cuando cambia el bpp del documento y clampan foreground/background.
- **Verificación.** Nuevos casos en `test_file_session` (apertura de un reciente
  por referencia, remapeo 2→8, transparencia y undo/redo) y en
  `test_image_model` (remapeo de `ImageBuffer` y color almacenado). Nuevo
  `temp/smoke_bpp.cpp` que abre Properties, comprueba el aviso, pulsa OK y
  valida documento/paleta/undo/redo. Suite normal: 262 casos y 9639
  aserciones; `test-full`: 1048 casos y 38556 aserciones. Los 38 smokes pasan.
  Builds debug y release OK.

## Sesión 2026-11-10 — Exportación BMP y PNG

- **BMP 24 bits.** `export_bmp()` escribe `BITMAPFILEHEADER` +
  `BITMAPINFOHEADER` (little-endian), filas de abajo arriba, píxeles BGR y
  padding a múltiplo de 4. Usa `Palette::getStoredColor()` para todos los
  índices, de modo que el índice transparente sale con su color almacenado
  porque BMP no tiene canal alfa.
- **PNG indexado.** `export_png()` escribe un PNG de color type 3 (indexed),
  profundidad 8: firma, `IHDR`, `PLTE` con los colores RGB de la paleta,
  `tRNS` si hay índice transparente, `IDAT` y `IEND`. Los datos son filas con
  filtro 0 y un byte por índice, comprimidos con zlib (`compress2`); los CRC
  usan `crc32`. Es pequeño, conserva exactamente la paleta y la transparencia.
- **UI.** `File > Export > Bitmap.../PNG...` abre `ImGuiFileDialog` con
  extensión forzada `.bmp`/`.png`, nombre base del documento actual o
  `untitled`, y la última carpeta usada. Los errores usan el modal `File
  Error`. No se añade el archivo exportado a recientes ni se toca el savepoint
  del `.idx`.
- **Estado transitorio.** `PaintBackend::export_image()` confirma la región
  flotante, la edición pendiente o el trazo activo antes de exportar, igual que
  Save, para que la copia coincida con lo que se ve.
- **Dependencia.** Makefile y tests enlazan `-lz`; `run_smokes.sh` también.
  zlib está disponible en el sistema y es una dependencia estándar de AppImage
  y `.deb`.
- **Verificación.** `tests/test_image_export.cpp` valida cabeceras BMP, padding,
  orden BGR, chunks PNG, CRC, `PLTE`, `tRNS` y el `IDAT` descomprimido.
  `temp/smoke_export.cpp` comprueba que los diálogos de exportación se abren y
  que la ruta del frontend genera ambos archivos. Suite normal: 265 casos y
  9717 aserciones; `test-full`: 1060 casos y 38868 aserciones. Los 39 smokes
  pasan. Builds debug y release OK.

## Sesión 2026-11-10 — Formatos de almacenamiento .idx

- **Dónde se elige.** El formato se elige en `Save As...` (o en `Save` cuando
  el documento aún no tiene ruta), no en `Image > Properties...`. Es una
  propiedad de escritura del archivo, no de la imagen, y se aplica en el
  momento de guardar.
- **Modos implementados.** `Index` (texto decimal), `Binary` (un byte por
  píxel), `Packed` (filas empaquetadas MSB-first en hexadecimal) y
  `BinaryPacked` (las mismas filas en bytes crudos). El lector detecta el modo
  desde la cabecera `IndexedImage <Mode>` y `IndexedImage::format()` lo expone.
- **Flujo.** `Save` con ruta existente usa el formato actual del documento;
  `Save As` abre `IDX Options` después de elegir el archivo. El modal muestra
  un combo de formato, una explicación del modo seleccionado, una tabla con el
  tamaño aproximado de los cuatro modos para el canvas actual y el tamaño de
  los datos del modo elegido (payload + ~64 B de cabecera). Al guardar, el
  backend recuerda el formato (`file_format()`); al abrir un `.idx`, lo toma del
  archivo; `New` vuelve a `Index`.
- **Tamaños.** `indexed_image_data_size()` calcula el payload: `Binary` =
  píxeles, `BinaryPacked` = `height*ceil(width*bpp/8)`, `Packed` = bytes
  empaquetados ×2 + saltos de línea, e `Index` una cota superior usando el
  máximo número de dígitos del bpp. `indexed_image_estimated_file_size()` suma
  64 B de cabecera media.
- **Verificación.** Roundtrip de los cuatro modos y tamaño aproximado en
  `test_indexed_image`; persistencia del formato en `test_file_session`; nuevo
  `temp/smoke_idx_options.cpp` que abre el modal, cambia a `BinaryPacked`,
  pulsa Save y comprueba el archivo y el formato del backend. Suite normal: 269
  casos y 9780 aserciones; `test-full`: 1076 casos y 39120 aserciones. Los 40
  smokes pasan. Builds debug y release OK.

## Decisiones de producto y revisión pendiente — 2026-11-08

### Decisiones cerradas

- **Ventana única.** Sin pestañas ni múltiples documentos. El zoom y la paleta
  son únicos/globales a propósito.
- **Sin capas, sin transformaciones de selección y sin selecciones no
  rectangulares.** Fuera del scope de esta aplicación.
- **Sin portapapeles externo del sistema.** No habrá conversión RGBA↔índices.
  Se estudiará, sin compromiso, un portapapeles entre instancias de la misma
  aplicación (formato propio; solo podría cambiar el número de colores de
  paleta).
- **Paletas no persistentes en esta app.** Es intencionado. En la futura app
  Qt la paleta será parte del documento. El elemento de canvas debe seguir
  siendo portable (dominio sin ImGui) y se adaptará a RGB cuando toque; se
  espera que la adaptación sea pequeña.
- **Semántica de transparencia de selección.** Es la deseada: `hole_fill`
  conserva el background del momento de crear la selección; `transparent_color`
  sigue al background actual si se cambia en la paleta. No cambiarlo.
- **Cambios de paleta.** Cambiar solo los colores no entra en el historial.
  Cambiar el bpp sería una propiedad del documento y habría que decidir si
  entra en el historial.
- **`kToolOptionsHeight`** es 116 px y `ToolOption::grid_columns` soporta
  cuadrículas compactas. Brush usa 4 filas x 3 columnas; el resto de opciones
  mantiene la lista vertical de ancho completo.
- **`CanvasArea`** ya pinta también el overlay de edición de Curve (guías y
  handles). No añadir más overlays sin estudiar su separación en un
  `CanvasOverlay`/renderer específico.
- **Toolbar.** Orden fijo en dos columnas: Selection, Eraser, Fill, Pick Color,
  Pencil, Brush, Line, Curve, Rect, Circle. La aplicación arranca con Pencil
  seleccionado (y como última tool de edición).
- **Selection > Delete.** Rellena el hueco original con el background guardado al
  crear la selección y descarta el contenido; no copia al portapapeles. El
  destino de una selección movida no se modifica. Atajo `Del`; entra en el
  historial como un paso si el hueco cambia.
- **Opciones dinámicas.** Las opciones con iconos reparten el alto disponible
  entre sus filas y escalan el icono a la celda; Brush y Eraser conservan sus
  previews compactos. Los iconos de Fill usan celdas 60x46 para no deformarse.
- **Abrir archivos.** El diálogo de Open ofrece `Indexed Image` y `All files`;
  el filtro no decide la validez, que sigue en `IndexedImage::loadFromFile`.
- **Curve.** Bézier cúbica con dos handles visibles (P1/P2); los extremos A/B
  quedan fijos tras el arrastre inicial, con Shift aplicando la misma
  restricción que Line solo en ese tramo. Los handles se recortan al canvas para
  que no se pierdan. Se confirma con Enter, clic fuera de los handles o cambio
  de tool; se cancela con Escape, botón derecho o ambos botones. La edición
  pendiente es exclusiva con la selección flotante y entra en el historial como
  un único paso al confirmar. No declara opciones.
- **Cursores `.ipx`.** El índice 3 (el hueco de “negro” del formato) se
  interpreta como gris oscuro `(96,96,96)` para poder representar el cursor
  Brush con el mismo patrón que tenía por código; la representación en el
  archivo sigue usando 3. Afecta a todos los `.ipx`, no solo a Brush.

### Revisión pendiente antes de nuevas funcionalidades

1. **Invariantes de edición e historial — resuelto el 2026-11-08.**
   `CanvasDocument::apply_pixel_patch` es la única vía de escritura sobre la
   imagen y `FloatingRegionEditor` ya no mantiene flags de cambio; ver la sesión
   “Invariantes de edición e historial”. Único resto a revisar si se busca
   simetría total: `commit_draw_buffer` escribe antes de apilar el paso, así que
   una excepción al reservar historial dejaría píxeles sin undo.

2. **Escalabilidad de render y scroll — resuelto el 2026-11-08.**
   - La imagen ya no se sube como una única textura del tamaño del canvas: se
     usa un cache de tiles de 256×256 solo para la zona visible (más un margen
     de un tile). El VRAM no depende del tamaño del canvas y se superan los
     límites de textura del backend. Ver la sesión “Tiles de render y snapshots
     de resize”.
   - El scroll sigue siendo `float` de ImGui; la medición queda documentada en
     esa sesión. El error se mantiene por debajo de un píxel de imagen para
     canvases realistas (incluido 17000 px a zoom 64), así que no se cambia el
     modelo.
   - Los pasos de resize guardan solo el snapshot previo y reconstruyen el
     posterior con `ImageBuffer::resize`; el historial de resize usa la mitad de
     memoria.

3. **ASAN/UBSAN en tests — resuelto el 2026-11-08.**
   Nuevos objetivos `make test-asan`, `make test-ubsan` y `make test-sanitize`
   (ASAN+UBSAN), con directorios de build separados y
   `-fno-sanitize-recover=all`. Ver la sesión “ASAN/UBSAN en tests”. No hay CI
   configurada; si se añade, los mismos objetivos sirven tal cual.

4. **Portapapeles entre instancias (estudio, no comprometido).**
   - Sin portapapeles externo de imágenes; solo tendría sentido entre
     instancias de QtreGBPaint.
   - Idea: usar el portapapeles del sistema con un formato propio (texto
     serializado con cabecera, bpp, tamaño, número de colores e índices), sin
     conversión a RGBA. Habría que decidir qué hacer si cambia el número de
     colores de paleta.
   - No implementar hasta que se decida.

5. **Layout de opciones en cuadrícula — resuelto el 2026-11-08.**
   - `ToolOption::grid_columns` pinta una opción `Choice` como cuadrícula
     compacta y `ToolOptionChoice::preview` permite dibujar la huella de cada
     brush. Brush usa 4 filas x 3 columnas; `kToolOptionsHeight` subió a 116 px.

6. **Separación de overlays de `CanvasArea` (baja prioridad).**
   - Hoy `CanvasArea` pinta textura, hueco, contenido flotante, marquee,
     rejilla y handles. No se prevén más overlays, pero si aparecen conviene
     extraer un `CanvasOverlay`/renderer de overlays para no engordar la vista.

## Sesión 2026-11-11 — Apertura desde archivo, MIME y preview por tiles

- **CLI y `%f`.** `main(int argc, char** argv)` abre `argv[1]` al arrancar con
  `PaintFrontend::open_initial_file()`. El `.desktop` usa
  `Exec=QtreGBPaint %f`.
- **MIME por magic.** `packaging/qtregbpaint.xml` define
  `application/x-qtregbpaint-index` con magic `IndexedImage` en el offset 0,
  sin glob `*.idx`, para no reclamar la extensión genérica. El `.desktop`
  declara `MimeType=application/x-qtregbpaint-index;`. El AppImage copia el XML
  en `usr/share/mime/packages`, pero la asociación automática depende de la
  integración del AppImage; el `.deb` la instalará con `update-mime-database`.
- **Guardado atómico.** `save_indexed_image()` escribe en `.tmp` y renombra al
  destino; si falla, elimina el temporal. Las exportaciones BMP/PNG siguen
  escribiendo directamente por decisión de producto.
- **Dependencias.** Eliminada `lib/tinyfiledialogs` y sus referencias en
  Makefile y `run_smokes.sh`; no se usaba desde el código.
- **Preview por tiles.** `CanvasArea` cachea texturas de 256×256 píxeles de
  imagen para el `DrawBuffer` activo, solo en la zona visible. `DrawBuffer`
  expone `revision()` y `has_painted_pixels()`; cada tile se re-sube cuando
  cambia la revisión y se destruye al terminar el trazo, cambiar la paleta o
  salir de la zona visible. Si crear la textura falla, se cae al preview por
  píxeles de `PaintSession::render_preview()`.
- **Limpieza.** Eliminados `ICanvasRenderer::draw_rect()`/`draw_line()` (sin
  llamadas). `AGENTS.md`/`HANDOFF.md`, fuentes, tests, `packaging/`,
  `docs/screenshots/` y los documentos de licencia/marca quedan staged.
- **Verificación.** Sin cambios en tests: 269 casos y 9780 aserciones;
  `test-full` 1076/39120; los 40 smokes siguen pasando. Builds debug y release
  OK.

## Sesión 2026-11-11 — Assets CC0, marca y damero con textura

- **Icono y assets.** `assets/QtreGBPaint.png` se generó con ChatGPT (OpenAI)
  en 2026 a partir de indicaciones del proyecto. `assets/LICENSE` dedica al
  dominio público bajo CC0 1.0 todos los assets visuales (icono, botones,
  hojas de opciones y los seis cursores, incluido `move.ipx`), con traducción
  informativa en `assets/LICENSE.es.md`. Todos los cursores, también Move, se
  redibujaron desde cero. La dedicatoria cubre solo copyright y derechos
  conexos; CC0 no renuncia ni concede derechos de marca. El icono y el
  logotipo siguen siendo Marcas y su uso se rige por `TRADEMARK.md` (versión
  1.1, apartado 7). Se actualizaron `TRADEMARK.md` y `TRADEMARK.es.md` para
  separar copyright y marca.
- **Damero con textura.** Nuevo `PaintUi::CheckerboardTexture`
  (`inc/paint/checkerboard.h`, `src/paint/checkerboard.cpp`): textura de
  256×256 con celdas de 8 px en el canvas y de 4 px en la paleta y la barra de
  estado, dibujada como quads alineados al origen de la imagen y recortada al
  área visible mediante UVs. Sustituye el `AddRectFilled` por celda en el fondo
  del canvas, el hueco flotante transparente, el preview del trazo y la huella
  del cursor, además de los swatches y el preview del cuentagotas. Si
  `createTexture` falla, se conserva el dibujo por celdas de
  `PaintUi::draw_checkerboard`.
- **Coste.** En 1080p el fondo del canvas pasa de ~32.000 celdas a ~54 quads;
  en 4K, de ~130.000 a ~160. El patrón sigue anclado al origen de la imagen
  (scroll y zoom) y la paridad se conserva porque el lado de la textura es
  múltiplo de dos celdas.
- **Verificación.** Build debug y release, suite normal y `test-full` en verde;
  41 smokes, incluido el nuevo `temp/smoke_checkerboard.cpp`: documento
  256×256 transparente a zoom 16, textura del canvas y del frontend válidas, y
  muestras claras/oscuras a ambos lados del límite de tile x=256. La primera
  pasada completa tuvo fallos transitorios de foco/ratón en `modifiers`,
  `pencil` y `zoom`; aislados y en una segunda pasada completa quedaron verdes.
  Captura: `temp/smoke_checkerboard.ppm`.

## Sesión 2026-11-11 — Help > About y carga de PNG

- **Pregunta PNG.** ImGui no decodifica imágenes: solo dibuja texturas. SDL2
  básico tampoco lee PNG (solo BMP); SDL_image sería otra dependencia. El repo
  ya trae `stb_image` en `lib/ImGuiFileDialog/stb/`, así que se usa
  directamente sin añadir nada.
- **Loader.** Nuevo `inc/paint/png_image.h` y `src/paint/png_image.cpp`:
  `PaintCore::load_png_rgba()` decodifica a RGBA8 con `stb_image` (compilado
  estático en esa unidad para no chocar con las thumbnails de
  ImGuiFileDialog), valida dimensiones y devuelve un `RgbaImage`.
- **About.** `Help > About QtreGBPaint...` abre un modal centrado con el logo
  (`assets/QtreGBPaint.png`, 160 px), “Made by kirbyboy”, copyright,
  `GPL-3.0-or-later`, `CC0 1.0` para los assets, enlaces a la licencia y al
  repositorio, y botón Close. `PaintFrontend` guarda la ruta y carga el PNG de
  forma perezosa la primera vez; `main` le pasa
  `assetBase + "/QtreGBPaint.png"`. La textura se destruye en el destructor del
  frontend.
- **Notices.** `THIRD_PARTY_NOTICES` actualiza la entrada de stb (v2.29, usado
  por la app para PNG) y AGENTS documenta que vive en
  `lib/ImGuiFileDialog/stb/`.
- **Verificación.** Build debug/release; `make test` y `test-full`; 42 smokes
  en verde, incluido `temp/smoke_about.cpp`, que abre el modal, comprueba que
  la textura se cargó (256×256) y captura `temp/smoke_about.ppm`.

## Sesión 2026-11-11 — README, capturas y limpieza de data

- **README.** Nuevo `README.md` en inglés con descripción, características,
  build, tests, uso, alcance, estructura, licencia y créditos. Capturas en
  `docs/screenshots/main.png` (ventana principal con una consola portátil
  dibujada) y `about.png` (modal About). `assets/LICENSE` amplía su alcance a
  `docs/screenshots/`.
- **Limpieza.** Eliminado `data/` (no contenía nada útil); `HANDOFF.md` ya no
  lo menciona.

## Sesión 2026-11-11 — Bootstrap de dependencias

- **Problema.** `lib/` está ignorado por git, así que un clon nuevo no tenía
  ImGui, ImGuiFileDialog ni doctest y no podía compilar sin descargarlos a mano.
- **Script.** Nuevos `scripts/bootstrap_deps.sh` y `scripts/deps/`:
  `make deps` clona ImGui `v1.92.4-docking`, ImGuiFileDialog `v0.6.8` y doctest
  `v2.4.12` en `lib/`, copia los Makefiles del proyecto y construye
  `libimgui.a` y `libImGuiFileDialog.a`. Es idempotente: si las fuentes y las
  librerías existen, no descarga ni recompila nada.
- **ImGui docking.** El proyecto usa `ImGuiConfigFlags_DockingEnable`, así que
  el bootstrap debe usar el tag de la rama docking, no el tag normal. El
  Makefile de ImGui que se copia compila solo los backends que usa la
  aplicación (SDL2 y SDL_Renderer2), sin OpenGL3.
- **Makefile.** `make deps` ejecuta el script. El target principal construye
  `lib/imgui/libimgui.a` si falta (antes solo se construía
  `libImGuiFileDialog.a`) y muestra “Run 'make deps' first” si `lib/` está
  vacío. `packaging/build_appimage.sh` ejecuta `make deps` antes del release.
  `.gitignore` ancla `/deps/` y `/deps-release/` para que `scripts/deps/` sí se
  versione.
- **Documentación.** README, AGENTS y HANDOFF explican el flujo
  `make deps` → `make`; `make deps` queda en los comandos habituales.
- **Verificación.** `make deps` es idempotente con el `lib/` actual. En una
  copia limpia sin `lib/`: `make deps`, `make` y `make -C tests test-circle`
  pasan; el binario se enlaza con los dos `.a` generados.

## Sesión 2026-11-11 — Paquete .deb, AppStream y fallback de assets

- **Versión.** Paquete `qtregbpaint` **0.9.0**, app-id
  `io.github.kirbyboy.QtreGBPaint`, maintainer
  `kirbiboy <kirbiboy@users.noreply.github.com>`. Es un paquete binario para
  adjuntar a una release de GitHub, no para el archivo oficial de Debian.
- **Empaquetado.** Nuevo `packaging/build_deb.sh` y target `make deb`: usa
  `dpkg-deb` directo (sin debhelper), construye el release, instala binario,
  assets, `.desktop`, icono, MIME XML, metadata AppStream y documentación en
  `/usr/share/doc/qtregbpaint`. Añade scripts `postinst`/`postrm` para
  actualizar MIME, escritorio e iconos.
- **AppStream.** Nuevo
  `packaging/io.github.kirbyboy.QtreGBPaint.metainfo.xml` con resumen,
  descripción, licencias, URLs, capturas, categorías y release 0.9.0. Valida
  con `appstreamcli validate --no-net`.
- **Assets.** `main` resuelve ahora `assets` en este orden: junto al
  ejecutable, `../assets` (build/AppImage), `../share/qtregbpaint/assets`
  (instalación en `/usr/bin`) y rutas absolutas de `/usr/share` y
  `/usr/local/share`, con `assets` como último recurso. El `.deb` instala en
  `/usr/share/qtregbpaint/assets`.
- **AppImage.** Copia el `.desktop` con el ID de AppStream y añade la metadata
  a `usr/share/metainfo`.
- **Icono de ventana.** `IGuiBackend::set_window_icon` recibe el logo RGBA y
  `SDLRendererBackend` lo convierte en una superficie de 64×64 (los iconos
  mayores no llegan a `_NET_WM_ICON` en X11) y llama a `SDL_SetWindowIcon`.
  `SDL_VIDEO_X11_WMCLASS` y `SDL_VIDEO_WAYLAND_WMCLASS` fijan el class/app_id
  a `io.github.kirbyboy.QtreGBPaint`, y el `.desktop` declara
  `StartupWMClass=io.github.kirbyboy.QtreGBPaint`. Así el dock asocia la
  ventana con el icono del `.desktop`; en Wayland se comprobó con
  `WAYLAND_DEBUG=1` que se envía el `set_app_id` correcto.
- **Verificación.** `make deb` genera
  `build/qtregbpaint_0.9.0_amd64.deb`; se extrajo con `dpkg-deb -x`, se
  revisaron los archivos instalados y el binario extraído arrancó con
  `SDL_VIDEODRIVER=dummy` durante 2 s. `desktop-file-validate` y
  `appstreamcli validate --no-net` pasan.

## Sesión 2026-11-11 — Separación de PaintFrontend

- **Motivo.** `paint_frontend.cpp` había llegado a ~2.370 líneas y mezclaba
  layout/render, menús, modales, operaciones de archivo, exportación y
  acciones de selección/zoom.
- **Reparto.** La clase y la API pública no cambian; solo se reparten las
  implementaciones en `src/paint/`:
  - `paint_frontend.cpp`: ciclo de vida, tema, layout, canvas, toolbar,
    opciones, paleta, barra de estado y helpers de render.
  - `paint_frontend_menu.cpp`: barra de menús.
  - `paint_frontend_dialogs.cpp`: modales (About, Properties, Flip/Rotate),
    atajos globales y diálogos de archivo.
  - `paint_frontend_file.cpp`: New/Open/Save/Save As, exportación, recientes,
    cierre y errores.
  - `paint_frontend_actions.cpp`: selección, Select All, Properties,
    Flip/Rotate y zoom.
  - `paint_frontend_helpers.cpp` y `paint_frontend_internal.h`: helpers
    compartidos (extensiones, rutas, tamaños y descripciones de formato).
- **Render.** `render()` llama a `renderMenuBar()`, luego a `renderDialogs()` y
  mantiene `renderFileDialogs()` al final, con el mismo orden de ejecución que
  antes. El comportamiento de menús, atajos y modales no cambia.
- **Verificación.** Build debug/release; suite normal y `test-full`; **42
  smokes** en verde, incluidos `about`, `menus`, `shortcuts`, `file`, `export`,
  `idx_options` y `selection`.

## Sesión 2026-11-11 — Release 0.9.1

- **Versión.** Release `0.9.1`: AppStream, `README`, `AGENTS`, `HANDOFF`,
  `build_deb.sh` y `build_appimage.sh` actualizados. La primera release
  empaquetada fue `0.9.0`.
- **Artefactos.** Nuevo `packaging/build_release_assets.sh` y target
  `make release-assets`: construye el `.deb`, el AppImage versionado y
  `build/SHA256SUMS-0.9.1`. El AppImage pasa a llamarse
  `QtreGBPaint-0.9.1-x86_64.AppImage`.
- **Notas.** `packaging/RELEASE_NOTES_0.9.1.md` contiene descargas,
  instalación, verificación y cambios para publicar en GitHub.
- **Verificación.** `make release-assets` genera los tres archivos;
  `appstreamcli validate --no-net`, `desktop-file-validate`, `make test` y los
  42 smokes siguen en verde.

## Sesión 2026-11-11 — CI y smokes versionados

- **Smokes en el repo.** Los 42 `smoke_*.cpp` y el runner se movieron de
  `temp/` (ignorado) a `tests/smoke/`. El runner sigue escribiendo binarios,
  logs y capturas en `temp/`, y ahora acepta `SMOKE_RETRIES` para reintentar
  fallos transitorios. `make smoke` ejecuta la suite completa.
- **GitHub Actions.** Nuevo `.github/workflows/ci.yml`: en push a `main` y en
  pull requests instala `g++`, SDL2, zlib, Xvfb y xauth, ejecuta `make deps`,
  `make -j`, `make -j test-full` y `xvfb-run ... ./tests/smoke/run.sh` con
  `SDL_VIDEODRIVER=x11`, `SDL_RENDER_DRIVER=software` y `SMOKE_RETRIES=2`.
  Timeout de 45 minutos y cancelación de ejecuciones antiguas.
- **Documentación.** README incluye el badge de CI y el comando de smokes;
  `tests/smoke/README.md`, AGENTS y HANDOFF actualizados.
- **Verificación.** `make test-full` y los 42 smokes (con el runner ya movido)
  pasan. La primera ejecución real de GitHub Actions terminó en verde en
  `feed82e`; el badge del README queda activo.

## Plan de cierre — 2026-11-10

Orden acordado para las próximas sesiones:

1. **Integración y distribución**
   - [x] Abrir archivos desde terminal/gestor: `main(int argc, char** argv)`,
     `PaintFrontend::open_initial_file()` y `Exec=QtreGBPaint %f`.
   - [x] MIME: tipo propio `application/x-qtregbpaint-index` detectado por
     **magic** (`IndexedImage` al inicio), sin reclamar `*.idx`. XML en
     `packaging/qtregbpaint.xml`; el `.desktop` lo declara.
   - [x] Empaquetado `.deb` y metadata AppStream. Versión actual `0.9.1`
     (primera release `0.9.0`); `app-id`
     `io.github.kirbyboy.QtreGBPaint`; maintainer
     `kirbiboy <kirbiboy@users.noreply.github.com>`. `make release-assets`
     genera `build/qtregbpaint_0.9.1_amd64.deb`,
     `build/QtreGBPaint-0.9.1-x86_64.AppImage` y
     `build/SHA256SUMS-0.9.1`; los assets se instalan en
     `/usr/share/qtregbpaint/assets` y `main` los resuelve también desde ahí.
   - [x] Licencia de código: `GPL-3.0-or-later` en `LICENSE.txt`, con aviso de
     marca separado en `TRADEMARK.md` y avisos de terceros en
     `THIRD_PARTY_NOTICES.md`. Titular público: «the QtreGBPaint project»;
     contacto por issues del repositorio, sin email ni datos privados.
2. **Guardado**
   - [x] Escritura atómica para el archivo destino `.idx` (`.tmp` + `rename`),
     conservando el original si falla la escritura.
   - Exportaciones BMP/PNG no atómicas por decisión explícita.
3. **Documentación**: mantener `HANDOFF.md` al día (esta entrada es el plan) y
   `README.md` en inglés con características, build, uso, licencia y capturas.
4. **Repositorio**
   - [x] `AGENTS.md` y `HANDOFF.md` marcados para el repositorio; el resto de
     fuentes, tests, `packaging/` y documentación también.
   - [x] Ignorar `deps/`, `deps-release/`, `obj/`, `lib/`,
     `compile_commands.json`, `tmp/` y artefactos de build.
   - [x] Icono final `assets/QtreGBPaint.png` y copia en
     `packaging/qtregbpaint.png`; ambos staged.
   - [x] Eliminar `lib/tinyfiledialogs` y sus referencias en Makefiles/smokes.
5. **Rendimiento y calidad**
   - [x] Eliminar `ICanvasRenderer::draw_rect`/`draw_line` y sus
     implementaciones.
   - [x] Preview por tiles de **256×256** píxeles de imagen (cache por tile,
     revisión del `DrawBuffer`, solo zona visible).
   - [x] Damero mediante textura repetida: `PaintUi::CheckerboardTexture`
     (`inc/paint/checkerboard.h`, `src/paint/checkerboard.cpp`) dibuja quads
     de 256×256 con el patrón anclado al origen de la imagen; si falla la
     textura, se conserva el dibujo por celdas.
   - [x] Separar `PaintFrontend` (menús, diálogos y exportación) en archivos:
     `paint_frontend.cpp` (núcleo/render), `paint_frontend_menu.cpp`,
     `paint_frontend_dialogs.cpp`, `paint_frontend_file.cpp`,
     `paint_frontend_actions.cpp` y `paint_frontend_helpers.cpp`.
   - [x] Añadir CI (`test-full` + smokes): `.github/workflows/ci.yml` instala
     SDL2/zlib/Xvfb, ejecuta `make deps`, `make`, `make test-full` y todos los
     smokes versionados en `tests/smoke/` bajo Xvfb, con `SMOKE_RETRIES=2`
     para absorber fallos transitorios de foco/ratón.
   - Sin autosave/recovery por decisión explícita.

## Próximos pasos sugeridos

> Orden acordado (2026-11-08): antes de añadir nuevas funcionalidades, cerrar
> la revisión pendiente de la sección anterior. Las opciones en cuadrícula ya
> están resueltas; queda el estudio (no comprometido) del portapapeles entre
> instancias. Los invariantes de historial, render/scroll y ASAN/UBSAN ya están
> resueltos.

- **Rejilla:** ajustar colores/opacidad si hace falta; el checkbox y el patrón
  1D ya están implementados. El offset de handles sigue disponible por si la
  rejilla cambia el borde visual.
- **Cursores (aparcado):** no hay un cursor por herramienta; `cross.ipx` lo
  comparten Selection, Line, Curve, Rect y Circle, y todos los `.ipx` son 2 bpp.
  Valorar ampliar tamaños si los cursores pequeños se ven mal; el fallback al
  sistema ya está implementado.
- **Iconos de toolbar.** Implementado el 2026-11-10: un único
  `assets/*_button.idx` de 32×32 por herramienta, con índice 0 transparente y
  el resto en gris. El botón activo usa `ImGuiCol_ButtonActive` y se queda
  pulsado; los no activos tienen fondo transparente (hover/pressed neutro). Los
  antiguos `normal/hover/pressed` se eliminaron; copia en
  `temp/old_toolbar_icons/`.
- **Formato de paleta y bpp.** En esta app la paleta no se persiste a
  propósito. Para la futura app Qt, la paleta será parte del documento. El
  canvas ya admite 1..8 bpp, el cambio de bpp se hace desde Properties y entra
  en el historial con remapeo de tonos.
- **Opciones de las herramientas restantes.** Rect y Circle tienen
  Outline/Fill/Both; Selection tiene Normal/Transparent; Fill tiene
  Contiguous/Global; Line y Curve tienen Width 1..5 (píxel, cuadrado 2x2 y
  cruces 3/4/5); Pencil y Pick Color no declaran opciones; Brush usa una
  cuadrícula 4x3 con 12 huellas; Eraser una lista vertical 4/6/8/10.
- **Formas editables (futuro).** `ToolBase::has_pending_edit`/`EditOverlay` y
  el enrutado de handles de `CanvasArea` dejan preparado el patrón para tools
  multipaso con tamaño, posición o bordes editables (por ejemplo, un cuadrado
  con handles). No está en el alcance de esta aplicación.
- **Configuración persistente.** Implementada con `AppConfig` (tema, rejilla,
  geometría de ventana, última carpeta y recientes) en
  `$XDG_CONFIG_HOME/qtregbpaint/config.ini`.
- **Atajos de selección.** Ctrl+C/X/V ya están enlazados; queda valorar Ctrl+I
  respetando los
  campos de texto. No habrá menú contextual; el clic derecho mantiene su
  comportamiento actual.
- **Cursores `.ipx` para el resto de acciones (aparcado).** El cargador por
  `CursorType` ya existe. Ir sustituyendo los cursores del sistema (Crosshair
  de Selection, Fill, Circle, ResizeEW/NS/NWSE) por `.ipx` propios, como el
  ya redibujado `move.ipx`.
- **Save/load y modos `Binary`, `Packed` y `BinaryPacked`.** Implementados,
  incluida la elección de formato en `Save As...`.
- **`ImGuiCanvasRenderer::draw_rect`/`draw_line`.** Se eliminan: no tienen
  llamadas y los overlays usan `draw_rect_filled`, texturas o `ImDrawList`.
- **Preview del trazo.** Evaluar una textura/tiles de 256×256 si el preview por
  píxeles se nota; no es prioritario para trazos pequeños.

> Ver la revisión de 2026-11-03 (siguiente sección) para el orden recomendado.

## Revisión general de código — 2026-11-03

Revisión estática de `src/`, `inc/`, `Makefile` y el `git diff` pendiente
(sin compilar). Tareas ordenadas por prioridad: P0 son bugs reales o bloquean
trabajo posterior; P1 arquitectura con coste alto si se aplaza; P2 limpieza.

### P0 — Bugs y coherencia

- [x] **Unificar el contexto de ImGui.** `SdlRendererBackend` y `ImGuiManager`
  llaman los dos a `ImGui::CreateContext()` y `main` inicializa ambos; el
  segundo contexto deja el backend SDL atado al primero (`BackendUserData ==
  nullptr` en `NewFrame`). Decidir un único dueño: o el backend gestiona ImGui,
  o `ImGuiManager` lo hace todo y el backend solo pinta.
  *Hecho (2026-11-03):* opción B con el reparto correcto. `ImGuiManager` es el
  único dueño del contexto (`CreateContext`/`DestroyContext`) y no incluye nada
  de SDL; `SDLRendererBackend` instala y libera sus `ImGui_ImplSDL2_*` /
  `ImGui_ImplSDLRenderer2_*` en `init()`/`shutdown()` y expone
  `begin_imgui_frame()`/`render_imgui(ImDrawData*)`; `ImGuiManager::begin_frame(IGuiBackend&)`
  se apoya en la interfaz, no en SDL. `poll_events(bool&)` absorbe el bucle
  `SDL_PollEvent` + `ImGui_ImplSDL2_ProcessEvent` + `SDL_QUIT`. Build limpio y
  arranque verificado.
- [x] **Un solo dueño de la tool activa.** `Canvas` y `PaintBackend` guardan
  copias del mismo `shared_ptr<ToolBase>` y se desincronizan: pulsar
  Pencil/Fill/Circle cambia el botón (con `tool == nullptr`) pero `Canvas`
  conserva `SquareTool`. `PaintBackend` debe ser el dueño; `Canvas` guarda
  `ToolBase*` y `set_tool(nullptr)` desactiva el dibujo.
  *Hecho (2026-11-03):* `PaintBackend` es el dueño (`shared_ptr<ToolBase>
  m_activeTool`), `Canvas::set_tool(ToolBase*)` es no-propietario y
  `PaintFrontend::select_tool()` es el único punto que sincroniza ambos.
  Al pasar de `shared_ptr` a puntero crudo, `m_active_tool` quedó **sin
  inicializar** y `render_draw_buffer` lo desreferenciaba en el primer frame
  (SIGSEGV en `canvas.cpp:65`): corregido con `ToolBase* m_active_tool =
  nullptr;`. Verificado con `make clean && make` (0 errores) y un test de
  regresión (Rect → dibuja, Pencil/`nullptr` → no dibuja, cambio a mitad de
  trazo → `on_cancel`, re-selección → OK).
- [x] **Un solo `Color` canónico con conversores.** Se define `0xFFe0f8d0`
  (AARRGGBB) pero ImGui y `SDL_PIXELFORMAT_RGBA32` esperan AABBGGRR: R y B
  quedan intercambiados. Además los swatches del panel son `ImVec4`
  hardcodeados que ni siquiera coinciden con `m_palette`. Crear `toImGui()` /
  `toRGBA32()` en la frontera y pintar los swatches desde `m_palette`.
  *Hecho (2026-11-03):* `PaintCore::Color` es un struct RGBA con canales
  explícitos y la única frontera con ImGui vive en `inc/paint/color_imgui.h`
  (`toImU32`/`toImVec4`). `Palette::dmg()` y `Palette::grayscale()` son las
  únicas factorías: desaparecen las tres copias de la paleta gris y los
  `ImVec4` hardcodeados de los swatches. `ICanvasRenderer` y
  `IGuiBackend::updateTexture` reciben `const PaintCore::Color*`; el backend
  SDL escribe R,G,B,A byte a byte respetando el `pitch` de `SDL_LockTexture`.
  Centinela de `Palette::getColor` → `{255, 0, 255, 255}`.
  Verificado: `make clean && make` (0 errores, los 3 warnings previos) y test
  headless de la tubería de color: `toImU32` = `0xFFD0F8E0` que decodifica a
  (0.878, 0.973, 0.816); `SDL_PIXELFORMAT_RGBA32` produce los bytes
  `E0 F8 D0 FF`; `Palette::dmg`, `grayscale(3)` y el centinela OK.
- [x] **Límite del buffer de dibujo, no del puntero.** El puntero debe poder
  salir del canvas (y de la ventana) para dibujar figuras parciales, estilo
  Paint: un círculo enorme del que solo entra un trozo, o un cuadrado que sale
  por abajo-izquierda y solo pinta la parte visible. Por eso
  `CanvasArea::screen_to_image` queda **sin clamp** (solo documentado).
  Lo que se acota es el `DrawBuffer`: `set_bounds()` guarda el tamaño del
  canvas y `begin()`/`ensure_fit()` clampean el punto antes de crecer, así que
  el buffer nunca supera `width*height` (64 KiB en 256x256). Las tools siguen
  calculando la geometría con las posiciones reales y `set_pixel()` descarta
  los píxeles fuera del área recortada; `blend_onto()` ya recorta contra el
  `ImageBuffer`. Las tools que no dibujan una caja `start→current` (p. ej. un
  círculo) deberán llamar `ensure_fit()` con las esquinas de su bbox, no solo
  con la posición actual, para que el área recortada cubra toda la parte
  visible. `DrawTool` fija los límites desde `canvas.getWidth()/getHeight()`
  al pulsar y en cada preview. Verificado con `make` y un test headless
  (cuadrado arrastrado fuera por abajo-izquierda y por abajo-derecha: sin
  bordes falsos en el límite del canvas y memoria acotada).

### P1 — Arquitectura

- [x] **Render por textura con LUT y rect sucio.** Hoy se dibuja un
  `AddRectFilled` por píxel (~65.000 quads/frame a 256x256). `createTexture` /
  `updateTexture` existen y no se usan: subir el `ImageBuffer` como textura
  índice→RGBA, actualizar solo el rect sucio del último trazo y pintar un único
  `ImGui::Image` con `Nearest`. Mejor relación esfuerzo/beneficio del proyecto.
  *Hecho (2026-11-04):* `ImageBuffer` acumula un `PaintCore::Rect` sucio
  (`set_pixel`, `fill`, `allocate`, `resize`; `copy_from` pasa por `set_pixel`)
  y `CanvasArea` mantiene una textura RGBA del canvas con LUT de 256 colores.
  Solo sube el rect sucio vía `IGuiBackend::updateTextureRegion`
  (`SDL_LockTexture` con región) y pinta un único `AddImage` con
  `SDL_ScaleModeNearest`; si falla la textura, cae al render por píxel. El
  preview del trazo sigue por píxeles encima de la textura. La ruta de textura
  también deja que ImGui recorte el píxel parcial de abajo/derecha. Verificado
  con `make clean && make`, test headless del dirty rect y arranque de la app.
- [x] **Undo/redo ahora.** *Hecho (2026-11-05):* `Canvas` posee
  `UndoHistory` (`inc/paint/undo_history.h`, `src/paint/undo_history.cpp`) con
  pilas undo/redo y límites configurables (64 MB / 200 pasos por defecto; cero
  desactiva el historial). Cada paso es una transacción con varios `PixelPatch`
  (`rect` + `before` + `after`); deshacer mueve el paso a la pila de redo sin
  liberar memoria y una edición nueva descarta esa rama. El recorte evicta
  primero los pasos más antiguos de undo y luego los de redo, conservando el
  paso más reciente aunque él solo supere el presupuesto de bytes (límite
  blando). `Canvas::commit_draw_buffer` centraliza la confirmación de las tools
  y `ImageBuffer::read_region` / `write_region` copian por filas y marcan el
  rect sucio al restaurar, así que la textura sigue subiendo solo la zona
  tocada. `DrawTool::on_mouse_release` ya no llama a `blend_onto`; el menú Edit
  y `Ctrl+Z`/`Ctrl+Y` consultan `can_undo`/`can_redo`. Los límites se pueden
  cambiar en caliente desde fuera con `Canvas::set_history_limits()`.
  Verificado con `tests/test_undo.cpp` (144 comprobaciones: push/undo/redo,
  multi-parche, orden con solapes, invalidación de redo, límites de pasos y
  bytes, `set_limits`, no-op/recorte en Canvas y dirty rect) y con
  `temp/smoke_undo.cpp`, que inyecta eventos SDL en el frontend real: dibujar,
  `Ctrl+Z`, `Ctrl+Y`, edición nueva que invalida redo y rehacer en dos pasos.
- [x] **Una sola transformación de coordenadas.** *Hecho (2026-11-05):*
  `PaintCore::ViewTransform` (`inc/core/view_transform.h`,
  `src/core/view_transform.cpp`) es la única fórmula imagen ↔ pantalla.
  `CanvasArea::update_view()` construye una instancia por frame y la comparten
  `handle_input()` y `render()`; `Canvas::render` recibe la transformación y
  usa `visible_image_rect()` para el fallback por píxel. `ImGuiCanvasRenderer`
  ya no guarda `canvas_pos` ni duplica el zoom. Se eliminaron
  `VisualViewport`, `PaintCore::Viewport`, `inc/core/viewport.h` y los helpers
  `screen_to_image`/`image_to_screen`/`pixel_aligned_canvas_pos` de
  `CanvasArea`. `ViewTransform::make` conserva el alineado a píxel entero y el
  zoom 1..64. Verificado con `tests/test_view_transform.cpp` (426
  comprobaciones: alineado, round-trip, `screen_to_image_pixel`, rectángulo
  visible con zoom/scroll y recortes en los bordes).
- [x] **Modelo `ImageBuffer`/`Palette`.** *Hecho (2026-11-05):*
  `ImageBuffer` guarda `m_width`, `m_height`, `m_bits_per_pixel` y `m_data`
  como privados; expone `width()`, `height()`, `bitsPerPixel()` y `data()`
  (solo lectura), y `allocate()` pasó a ser un helper privado. El constructor
  valida dimensiones no negativas y bpp en 1..8, y `resize()` valida las nuevas
  dimensiones. `Palette` valida el mismo rango antes de construir
  `size_t{1} << bpp`, así que desaparece el UB con bpp >= 32 y ya no hay
  divergencia entre los dos modelos. `Canvas`, `CanvasArea`, `DrawBuffer` y los
  tests usan los accesores. Verificado con `tests/test_image_model.cpp` (69
  comprobaciones: accesores, rechazo de dimensiones/bpp inválidos, y
  `sub_buffer`/`copy_from`/`resize`, `data()` de solo lectura y paletas
  DMG/grayscale), además de `tests/test_dirty`, `tests/test_undo`,
  `tests/test_clamp`, `tests/test_view_transform` y `smoke_undo`.

### P2 — Limpieza y calidad

- [x] **Código muerto que confunde.** *Hecho (2026-11-05):* eliminada
  `PaintFrontend::update()` (solo estaba declarada) y los métodos sin uso
  `has_preview()`, `get_normalized_rect()` y `get_name()` (virtual pura en
  `ToolBase`, overrides en `DrawTool`, `SquareTool` y `SelectionTool`, y el
  `FakeTool` del test). También se quitó el include de `core/util.h` en
  `Canvas`, que no usaba `Vec2Max`/`Vec2Min`. Por decisión explícita se
  conservan `SelectionTool` (sin `.cpp`, como recordatorio de la futura
  herramienta de selección; implementada finalmente en la sesión 2026-11-07 —
  Selección y región flotante) e `inc/core/util.h` (a la espera de decidir su
  utilidad). Verificado con build limpio y todos los tests headless, además de
  `smoke_undo`.
- [x] **Tests (doctest ya está en `lib/`, sin usar).** *Hecho (2026-11-05):*
  los tests headless viven versionados en `tests/` y usan doctest
  (`lib/doctest/doctest.h`). `tests/Makefile` es autónomo: compila los objetos
  de producción en `tests/build/`, enlaza cada ejecutable con su propio
  `doctest_main.o` y no toca `obj/` ni el build de la aplicación. El Makefile
  principal solo delega con `make test` (y `make clean` limpia también
  `tests/build/`). Cobertura actual: `ImageBuffer` (accesores, validación,
  dirty rect, `sub_buffer`, `copy_from`, `resize`), `DrawBuffer`
  (`ensure_fit`, `blend_onto` con origen negativo, recorte y transparencia),
  `Palette`, `ViewTransform`, undo/redo sobre `Canvas` y parser `IndexedImage`
  (cabecera, modos, dimensiones, bpp, datos insuficientes/sobrantes y rango). Total:
  10 ejecutables, 72 casos y 910 aserciones, todas en verde con
  `make test`. El smoke test del frontend sigue en `temp/` porque necesita
  SDL, ventana y los objetos de la aplicación.
- [x] **Formato y nombres.** *Hecho (2026-11-05):* `PPMImage` pasó a
  `IndexedImage`, la extensión a `.idx` y la cabecera a `IndexedImage [Index]`.
  El modo `Index` es el único implementado; `Binary`, `Packed` y
  `BinaryPacked` están documentados. La validación es estricta (bpp 1..8,
  índices en `[0, 2^bpp)`, recuento exacto y sin datos extra). El escritor/Save
  y los modos restantes quedan para el trabajo de funcionalidades y afinado de
  la aplicación.
- [x] **Idioma.** *Hecho (2026-11-05):* código, comentarios, mensajes de error
  y textos de UI en inglés; la documentación de proyecto (`AGENTS.md`,
  `HANDOFF.md`, `temp/README.md`) se mantiene en español. Se eliminó el
  `readme.txt` antiguo. Verificado con build, tests y smoke.
- [x] **UI y `main`.** *Hecho (2026-11-05):* `New`, `Open` y `Save` quedan
  deshabilitados hasta implementarlos; eliminados los botones de prueba
  `Missing`/`HoverOnly`; la paleta de iconos vive una sola vez en
  `PaintBackend` y se expone con `iconPalette()`; la ruta de assets usa
  `SDL_GetBasePath()` con fallback a `assets`; eliminado el busy-wait y el
  frame rate queda limitado por `PRESENTVSYNC`. Verificado con build, tests y
  smoke.
- [x] **Backend SDL.** *Hecho (2026-11-05):* `SDL_Init`, `CreateWindow`,
  `CreateRenderer` y los backends de ImGui comprueban el retorno;
  `m_window`/`m_renderer` ya están inicializados a `nullptr`; `updateTexture`
  respeta el `pitch` de `SDL_LockTexture`; `PaintBackend` destruye sus texturas
  (incluido el icono por defecto) en su destructor; se quitó
  `SDL_INIT_AUDIO`; se comprueba `SDL_SetTextureScaleMode`; ya no existe
  `handle_event(const void*)`. No se añadió `try`/`catch`: un fallo de
  inicialización es fatal y las excepciones no tienen coste en el camino
  normal. Verificado con build, tests y smoke.
- [x] **Makefile.** *Hecho (2026-11-05):* las dependencias se generan en la
  misma pasada con `-MMD -MP -MF`; `ROOTDIR` se obtiene de la ruta del propio
  Makefile y el build funciona fuera del árbol; añadidos `run`, `release`,
  `run-release` y el perfil release separado (`obj-release/`,
  `deps-release/`, `build/QtreGBPaint-release`). `test` sigue delegando en
  `tests/Makefile`. Verificado con build debug, build release, tests y smoke.

### Orden de trabajo propuesto

1. P0 completo: contexto ImGui, ownership de tools, `Color`+conversores,
   clamp. (Media sesión, son bugs.)
2. Render por textura con LUT + rect sucio.
3. Historial de trazos (undo/redo) sobre `on_mouse_release` + tests de
   buffers/paleta/parser.

Save/load, recorte, selección, relleno y línea encajan sin fricción una vez
hechos esos tres, porque las costuras ya estarían limpias.

## Notas para agentes

- No comandos git destructivos; solo consultas.
- No escribir en el proyecto sin permiso, excepto AGENTS.md/HANDOFF.md acordados.
- Artefactos persistentes del agente en `temp/` (ignorada por git).
- Temporales efímeros en /tmp/opencode.
- Al leer este handoff, ejecutar backup.
