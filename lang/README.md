# Traducciones

Los textos de la UI están en inglés en el código y son la clave y el fallback
del catálogo. Las traducciones viven en un archivo `.po` por idioma:

- `es.po`: español.

## Formato

Cada entrada usa el texto inglés como `msgid` y la traducción como `msgstr`:

```po
msgid "Open"
msgstr "Abrir"
```

El cargador de la aplicación implementa el subconjunto de `.po` que usa el
proyecto: `msgctxt` opcional, `msgid`, `msgstr`, comentarios con `#`, cadenas
multilínea y escapes `\n`, `\t`, `\"` y `\\`. No se usan plurales por ahora.

`en_XA` es un pseudo-locale solo para tests: no tiene archivo `.po`; el cargador
genera las cadenas en tiempo de ejecución con `[!! ... !!]` y vocales
acentuadas para detectar desbordamientos. No aparece en `Settings > Language`.

Los textos internos de ImGuiFileDialog también están en `es.po`. La copia
vendorizada de `third_party/ImGuiFileDialog` los recibe mediante
`IGFD::FileDialog::SetStrings()`; ver `LOCAL_CHANGES.md`.

## Añadir o cambiar textos

1. Envolver el texto visible en el código con `i18n::tr("...")`.
2. Añadir o actualizar la entrada en `lang/es.po`.
3. Ejecutar `make i18n-check` y los smokes de UI.

## Comprobación

`make i18n-check` valida que cada `i18n::tr("...")` literal tiene entrada, que
no hay `msgid` duplicados ni `msgstr` vacíos y que los placeholders (`%s`,
`%d`, etc.) coinciden. No comprueba claves dinámicas ni entradas huérfanas: los
nombres de tools y las etiquetas de opciones se pasan como variables. Si una
traducción necesita reordenar placeholders, usa la forma posicional
(`%1$s`, `%2$d`) para que el chequeo siga siendo válido.

Los identificadores internos (nombres de herramientas, IDs de ImGui y
`stable_id` de los modales) se quedan en inglés. Nunca se traduce un ID ni se
usa una traducción como format string.

## Licencia

Las traducciones se distribuyen bajo GPL-3.0-or-later, la misma licencia que el
resto de la aplicación. No están cubiertas por la dedicatoria CC0 de `assets/`.
