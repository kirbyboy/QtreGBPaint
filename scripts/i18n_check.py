#!/usr/bin/env python3
"""Check lang/es.po against the i18n::tr() keys used by the sources.

The check is intentionally conservative:
- It scans literal first arguments of i18n::tr("...") in src/, inc/ and
  tests/smoke/. Calls whose first argument is a variable are ignored because
  their keys are dynamic (tool names, option labels, etc.).
- It validates missing keys, duplicate keys, empty msgstr, and printf-style
  placeholders. It does not report unused entries: dynamic keys are not
  discoverable from the source.
- Plural entries are reported because the loader does not implement them.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PO_PATH = ROOT / "lang" / "es.po"
SOURCE_DIRS = [
    ROOT / "src",
    ROOT / "inc",
    ROOT / "tests" / "smoke",
]
SOURCE_SUFFIXES = {".cpp", ".h"}

# Calls whose first literal argument is a translation key. PaintUi's modal
# helpers translate the title themselves, so their first argument is a key too.
TRANSLATED_CALLS = (
    "i18n::tr",
    "PaintUi::open_modal",
    "PaintUi::begin_modal",
    "PaintUi::is_modal_open",
)

FORMAT_RE = re.compile(
    r"%(?:(?P<position>[0-9]+)\$)?"
    r"[-+ #0]*(?:[0-9]+)?(?:\.[0-9]+)?"
    r"(?:hh|h|ll|l|j|z|t|L)?"
    r"(?P<conv>[diuoxXfFeEgGaAcsp%])"
)


def parse_quoted(line: str) -> str:
    """Return the unescaped contents of the first quoted string in a line."""
    start = line.find('"')
    if start < 0:
        return ""

    out = []
    index = start + 1
    while index < len(line):
        character = line[index]
        if character == "\\":
            index += 1
            if index >= len(line):
                break
            escaped = line[index]
            out.append(
                {
                    "n": "\n",
                    "t": "\t",
                    "r": "\r",
                    '"': '"',
                    "\\": "\\",
                }.get(escaped, escaped)
            )
            index += 1
        elif character == '"':
            return "".join(out)
        else:
            out.append(character)
            index += 1
    return "".join(out)


def parse_po(path: Path):
    """Parse the subset of .po used by the project."""
    entries = []
    block = []
    block_line = 0

    def flush():
        nonlocal block, block_line
        if not block:
            return

        context = ""
        msgid = ""
        msgstr = ""
        plural = False
        field = None

        for raw in block:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("msgctxt"):
                field = "context"
                context = parse_quoted(line)
            elif line.startswith("msgid_plural"):
                plural = True
                field = None
            elif line.startswith("msgid"):
                field = "id"
                msgid = parse_quoted(line)
            elif line.startswith("msgstr["):
                plural = True
                field = None
            elif line.startswith("msgstr"):
                field = "str"
                msgstr = parse_quoted(line)
            elif line.startswith('"'):
                continuation = parse_quoted(line)
                if field == "context":
                    context += continuation
                elif field == "id":
                    msgid += continuation
                elif field == "str":
                    msgstr += continuation

        if msgid:
            entries.append(
                {
                    "context": context,
                    "msgid": msgid,
                    "msgstr": msgstr,
                    "plural": plural,
                    "line": block_line,
                }
            )
        block = []
        block_line = 0

    with path.open(encoding="utf-8-sig") as stream:
        for line_number, raw in enumerate(stream, 1):
            line = raw.rstrip("\n").rstrip("\r")
            if not line.strip():
                flush()
            else:
                if not block:
                    block_line = line_number
                block.append(line)
    flush()
    return entries


def skip_ws_and_comments(text: str, index: int) -> int:
    while index < len(text):
        if text[index].isspace():
            index += 1
        elif text.startswith("//", index):
            newline = text.find("\n", index)
            index = len(text) if newline < 0 else newline + 1
        elif text.startswith("/*", index):
            end = text.find("*/", index + 2)
            index = len(text) if end < 0 else end + 2
        else:
            break
    return index


def parse_string_literal(text: str, index: int):
    """Parse a C++ string literal starting at text[index] == '"'."""
    index += 1
    out = []
    while index < len(text):
        character = text[index]
        if character == "\\":
            index += 1
            if index >= len(text):
                break
            escaped = text[index]
            out.append(
                {
                    "n": "\n",
                    "t": "\t",
                    "r": "\r",
                    '"': '"',
                    "\\": "\\",
                }.get(escaped, escaped)
            )
            index += 1
        elif character == '"':
            return "".join(out), index + 1
        else:
            out.append(character)
            index += 1
    return "".join(out), index


def scan_source(path: Path):
    """Return (key, line) pairs for literal i18n::tr() calls in one file."""
    text = path.read_text(encoding="utf-8")
    results = []
    index = 0
    in_line_comment = False
    in_block_comment = False
    in_string = False
    in_character = False

    while index < len(text):
        if in_line_comment:
            if text[index] == "\n":
                in_line_comment = False
            index += 1
            continue
        if in_block_comment:
            if text.startswith("*/", index):
                in_block_comment = False
                index += 2
            else:
                index += 1
            continue
        if in_string:
            if text[index] == "\\":
                index += 2
            elif text[index] == '"':
                in_string = False
                index += 1
            else:
                index += 1
            continue
        if in_character:
            if text[index] == "\\":
                index += 2
            elif text[index] == "'":
                in_character = False
                index += 1
            else:
                index += 1
            continue

        if text.startswith("//", index):
            in_line_comment = True
            index += 2
            continue
        if text.startswith("/*", index):
            in_block_comment = True
            index += 2
            continue
        if text[index] == '"':
            in_string = True
            index += 1
            continue
        if text[index] == "'":
            in_character = True
            index += 1
            continue

        matched = None
        for token in TRANSLATED_CALLS:
            if not text.startswith(token, index):
                continue
            before = text[index - 1] if index > 0 else ""
            after = (
                text[index + len(token)]
                if index + len(token) < len(text)
                else ""
            )
            if not (before.isalnum() or before == "_") and not (
                after.isalnum() or after == "_"
            ):
                matched = token
                break

        if matched:
            cursor = skip_ws_and_comments(text, index + len(matched))
            if cursor < len(text) and text[cursor] == "(":
                cursor = skip_ws_and_comments(text, cursor + 1)
                if cursor < len(text) and text[cursor] == '"':
                    value, cursor = parse_string_literal(text, cursor)
                    while True:
                        next_cursor = skip_ws_and_comments(text, cursor)
                        if next_cursor < len(text) and text[next_cursor] == '"':
                            continuation, cursor = parse_string_literal(
                                text, next_cursor
                            )
                            value += continuation
                        else:
                            break
                    line = text.count("\n", 0, index) + 1
                    results.append((value, line))
                    index = cursor
                    continue
        index += 1
    return results


def format_specs(text: str):
    """Return ordered (position, conversion) pairs, ignoring %%."""
    specs = []
    for match in FORMAT_RE.finditer(text):
        conversion = match.group("conv")
        if conversion == "%":
            continue
        specs.append((match.group("position"), conversion))
    return specs


def specs_match(source, translated) -> bool:
    """Compare format specs, allowing positional reordering."""
    if len(source) != len(translated):
        return False
    if not source:
        return True

    translated_positions = [position for position, _ in translated]
    if all(position is None for position in translated_positions):
        return [conversion for _, conversion in source] == [
            conversion for _, conversion in translated
        ]

    positions = {}
    for position, conversion in translated:
        if position is None:
            return False
        index = int(position)
        if index in positions:
            return False
        positions[index] = conversion
    if sorted(positions) != list(range(1, len(translated) + 1)):
        return False

    if all(position is None for position, _ in source):
        return all(
            positions[index + 1] == source[index][1]
            for index in range(len(source))
        )

    source_positions = {
        int(position): conversion
        for position, conversion in source
        if position is not None
    }
    return source_positions == positions


def relative(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def main() -> int:
    errors = []

    if not PO_PATH.is_file():
        print(f"i18n check: missing catalog: {relative(PO_PATH)}", file=sys.stderr)
        return 1

    try:
        entries = parse_po(PO_PATH)
    except OSError as error:
        print(f"i18n check: could not read {relative(PO_PATH)}: {error}",
              file=sys.stderr)
        return 1

    translations = {}
    for entry in entries:
        key = entry["msgid"]
        if entry["context"]:
            key = entry["context"] + "\x1f" + key

        if entry["plural"]:
            errors.append(
                f"{relative(PO_PATH)}:{entry['line']}: plural entries are not "
                f"supported by the loader: {entry['msgid']!r}"
            )
        if key in translations:
            errors.append(
                f"{relative(PO_PATH)}:{entry['line']}: duplicate msgid: "
                f"{entry['msgid']!r}"
            )
        translations[key] = entry

        if not entry["msgstr"]:
            errors.append(
                f"{relative(PO_PATH)}:{entry['line']}: empty msgstr for "
                f"{entry['msgid']!r}"
            )
            continue

        source_specs = format_specs(entry["msgid"])
        translated_specs = format_specs(entry["msgstr"])
        if not specs_match(source_specs, translated_specs):
            errors.append(
                f"{relative(PO_PATH)}:{entry['line']}: placeholder mismatch "
                f"for {entry['msgid']!r}: {source_specs} != {translated_specs}"
            )

    source_keys = {}
    for directory in SOURCE_DIRS:
        if not directory.is_dir():
            continue
        for path in sorted(directory.rglob("*")):
            if path.suffix not in SOURCE_SUFFIXES:
                continue
            for key, line in scan_source(path):
                source_keys.setdefault(key, []).append((path, line))

    for key, locations in sorted(source_keys.items()):
        if key in translations:
            continue
        for path, line in locations:
            errors.append(
                f"{relative(path)}:{line}: missing translation for {key!r}"
            )

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        print(
            f"i18n check failed: {len(errors)} error(s), "
            f"{len(source_keys)} source key(s), {len(translations)} translation(s)",
            file=sys.stderr,
        )
        return 1

    print(
        f"i18n check OK: {len(source_keys)} source key(s), "
        f"{len(translations)} translation(s)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
