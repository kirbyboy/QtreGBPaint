# Sistema de diseño de la UI

Guía corta para la UI ImGui de QtreGBPaint. No es un framework de widgets: son
tokens, roles semánticos y unas pocas reglas para que el chrome y lo que se
dibuja a mano (toolbar, opciones, paleta, canvas y diálogos) compartan el mismo
lenguaje visual.

## Capas

1. **Tema**: los presets de ImGui (`Light`, `Dark`, `Classic`) definen los
   colores base. `PaintFrontend::set_theme()` los aplica en caliente.
2. **Tokens** (`inc/paint/ui_tokens.h`): espaciado, tamaños y alfas. No
   dependen del tema.
3. **Roles** (`inc/paint/ui_roles.h`): colores con nombre semántico. Se leen
   del estilo actual en cada llamada, así que no hay caché que refrescar al
   cambiar de tema.
4. **Widgets** (`inc/paint/ui_widgets.h`): piezas reutilizables para los
   controles que se dibujan a mano.

## Tokens

| Grupo | Token | Valor | Uso |
|---|---|---|---|
| `Space` | `kXxs` | 2 px | Separaciones mínimas |
| `Space` | `kXs` | 4 px | Padding de iconos, separaciones cortas |
| `Space` | `kSm` | 8 px | Espaciado estándar |
| `Space` | `kMd` | 12 px | Separaciones amplias |
| `Space` | `kLg` | 16 px | Padding de diálogos, separadores de estado |
| `Space` | `kXl` | 24 px | Bloques grandes |
| `Size` | `kToolButton` | 32 px | Botón de herramienta |
| `Size` | `kToolOptionsHeight` | 116 px | Zona de opciones |
| `Size` | `kToolOptionRow` | 24 px | Fila/celda de opción |
| `Size` | `kToolOptionIconWidth` | 60 px | Icono de opción |
| `Size` | `kToolOptionIconHeight` | 16 px | Icono de opción |
| `Size` | `kPaletteColor` | 50 px | Swatch foreground/background |
| `Size` | `kSwatch` | 28 px | Swatch pequeño |
| `Size` | `kSwatchGap` | 6 px | Separación fg/bg |
| `Size` | `kSwatchBevel` | 2 px | Bisel Paint |
| `Size` | `kDialogButton` | 80 px | Botón de diálogo |
| `Alpha` | `kHover` | 0.08 | Hover neutro |
| `Alpha` | `kPressed` | 0.14 | Pressed neutro |
| `Alpha` | `kSelected` | 0.65 | Fila/celda seleccionada |

Los tokens no sustituyen a `ImGuiStyle`: `ItemSpacing`, `WindowPadding` y
compañía siguen siendo la base de ImGui. Los tokens se usan para lo que se
dibuja a mano.

## Roles

| Rol | Color base | Uso |
|---|---|---|
| `accent()` | `ImGuiCol_ButtonActive` | Herramienta activa, handle arrastrado |
| `selection()` | `ImGuiCol_HeaderActive` | Marquee, borde del canvas, handles |
| `guide()` | `ImGuiCol_TextDisabled` al 85 % | Guías de Curve |
| `border()` | `ImGuiCol_Border` | Bordes de swatches y miniaturas |
| `surface()` | `ImGuiCol_FrameBg` | Fondo del swatch fg/bg |
| `text()` | `ImGuiCol_Text` | Texto y previews |
| `text_muted()` | `ImGuiCol_TextDisabled` | Texto secundario |
| `hover_overlay()` | `ImGuiCol_Text` al 8 % | Hover neutro |
| `pressed_overlay()` | `ImGuiCol_Text` al 14 % | Pressed neutro |
| `selected_fill()` | `ImGuiCol_HeaderActive` al 65 % | Opción seleccionada |
| `bevel_light()` / `bevel_dark()` | blanco / negro | Bisel fijo de la paleta |

Regla: si un color tiene un significado de UI, se usa un rol; si es un patrón
de contraste fijo (damero, borde negro de handles, guiones del marquee), puede
quedarse como color literal y documentarse aquí.

## Widgets

### `PaintUi::tool_button`

`tool_button(id, icon, active, tooltip)` pinta un botón de 32×32. El `id` debe
ser estable (el nombre interno de la herramienta); el `tooltip` es el texto
visible. La herramienta activa usa `accent()`; hover y pressed usan los
overlays neutros. El fondo inactivo es transparente.

### `PaintUi::option_cell`

`option_cell(id, min, size, selected)` coloca un `InvisibleButton`, pinta el
fondo de selección/hover y devuelve `clicked`/`hovered`. El llamante dibuja
encima el icono, preview o etiqueta. Funciona igual para la cuadrícula 4×3 de
Brush y para las listas verticales.

### `PaintUi::swatch_frame`

Dibuja el bisel y el borde de un swatch. El llamante rellena el interior o
pinta el damero de transparencia antes. Se usa en la paleta y en el
foreground/background.

### Diálogos

- `open_modal(title, stable_id)` abre.
- `begin_modal(title, stable_id)` aplica 16 px de padding y devuelve si el
  modal está abierto.
- `end_modal()` cierra y restaura el padding.
- `is_modal_open(title, stable_id)` consulta el estado.
- `dialog_button(label)` pinta un botón de 80 px.

El identificador estable va después de `###` en el nombre visible
(`"File Error###file_error"`). Así el título se puede traducir sin cambiar el
ID de ImGui.

## Reglas por zona

- **Toolbar**: 2 columnas, botones 32×32, iconos con índice 0 transparente.
  Solo la herramienta activa lleva fondo `accent()`.
- **Opciones**: filas de 24 px; los iconos dejan 4 px de margen. La opción
  seleccionada usa `selected_fill()`; el hover usa `hover_overlay()`.
- **Paleta**: fg/bg de 50 px, swatches de 28 px, gap de 6 px, bisel de 2 px.
  El damero de transparencia se dibuja antes del bisel.
- **Canvas**: el borde de 2 px, el marquee y los handles usan `selection()`;
  las guías usan `guide()`. El damero y los guiones de contraste son fijos.
- **Estado**: separadores a 16 px; la miniatura de color usa `border()` y un
  gap de 4 px.
- **Diálogos**: padding de 16 px, botones de 80 px, OK/Cancel o
  Save/Discard/Cancel. No anidar modales.

## Temas

`StyleColorsLight/Dark/Classic` solo cambian colores. Los tokens y las reglas
no cambian con el tema; los roles se recalculan solos. Si un widget nuevo
necesita un color que no existe, se añade un rol, no un `ImGuiCol_*` suelto.

## i18n

Los textos visibles se envuelven con `i18n::tr("...")`. El inglés es la clave y
el fallback; `lang/<locale>.po` contiene las traducciones (por ahora `es.po`).
`main` llama a `i18n::set_language_directory(<assets>/../lang)` y
`Settings > Language` cambia entre System default/English/Español en caliente;
`AppConfig::language` persiste `system`/`en`/`es`. El `.deb` y el AppImage
instalan `lang/` junto a los assets.

Reglas:

- Los IDs internos de herramientas y los `stable_id` de los diálogos se quedan
  en inglés. `PaintUi::open_modal`/`begin_modal` traducen el título y añaden
  `###stable_id`; el ID no cambia al cambiar de idioma.
- Nunca se usa una traducción como ID de ImGui ni como format string.
  `ImGui::Text("%s", ...)`, `TextUnformatted` o `snprintf` para los textos con
  placeholders.
- No se concatenan fragmentos traducidos; se traduce la frase completa.
- El file dialog de ImGuiFileDialog sigue en inglés porque sus textos son
  macros de compilación.
- Los detalles técnicos de error que devuelve el backend (`std::string& error`)
  siguen en inglés; el título y el mensaje genérico sí se traducen. Traducirlos
  del todo requiere códigos de error estructurados.

## UI kit

Pendiente: una ventana o smoke de desarrollo que muestre todos los widgets en
sus estados `default`/`hover`/`pressed`/`disabled`/`selected`, en los tres
temas y con un pseudo-locale para detectar desbordamientos. Mientras tanto,
`tests/smoke/smoke_i18n.cpp` guarda capturas en inglés y español para revisar
el layout con textos traducidos.
