#!/usr/bin/env python3
"""Generate the packed save-mode table and audit legacy reset data.

The reset/IRQ compatibility database moved to plain-text files under
``SD-Card-Addons/SYSTEM/PATCHES/GBA`` and is no longer compiled into the kernel.
The preserved ``reset_table_legacy.h`` remains an archival/export input only.
"""
from __future__ import annotations

import argparse
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
LEGACY_DIR = ROOT / "tools" / "compatibility_tables"
SOURCE_DIR = ROOT / "source"

SAVE_MODE_VALUES = (0x00, 0x11, 0x21, 0x22, 0x23, 0x31, 0x32, 0x33)
SAVE_MODE_ALPHABETS = (
    "ABFKMPRTUVZx",
    "023456789ABCDEFGHIJKLMNOPQRSTUVWXYZx",
    "23456789ABCDEFGHIJKLMNOPQRSTUVWXYZx",
    "ACDEFHIJKPQSUXYZx",
)


def c_bytes(data: bytes, per_line: int = 16) -> str:
    lines = []
    for pos in range(0, len(data), per_line):
        chunk = data[pos : pos + per_line]
        lines.append("\t" + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    return "\n".join(lines)


def parse_reset_table(path: pathlib.Path):
    text = path.read_text(encoding="utf-8", errors="strict")
    body = text.split("{", 1)[1].rsplit("}", 1)[0]
    values = [int(token, 16) for token in re.findall(r"0x([0-9A-Fa-f]+)", body)]

    rows = []
    index = 0
    while index < len(values):
        game_code = values[index]
        index += 1
        if game_code == 0xFFFFFFFF:
            break
        if index >= len(values):
            raise ValueError("reset table ends before the count field")
        count = values[index]
        index += 1
        if count > 0xFF:
            raise ValueError(f"reset count {count} does not fit in one byte")
        offsets = values[index : index + count]
        if len(offsets) != count:
            raise ValueError("reset table ends inside an offset list")
        index += count
        if any(offset > 0xFFFFFF for offset in offsets):
            raise ValueError("a reset offset does not fit in 24 bits")
        rows.append((game_code, offsets))

    if index != len(values):
        raise ValueError("unexpected data after reset sentinel")
    return rows


def pack_reset_table(rows):
    # The legacy lookup stops at the first matching game code, so later
    # duplicates are unreachable. Keep the first row to preserve exact behavior.
    unique_rows = []
    seen = set()
    duplicate_count = 0
    for game_code, offsets in rows:
        if game_code in seen:
            duplicate_count += 1
            continue
        seen.add(game_code)
        unique_rows.append((game_code, offsets))

    packed = bytearray()
    for game_code, offsets in unique_rows:
        packed += game_code.to_bytes(4, "little")
        packed.append(len(offsets))
        for offset in offsets:
            packed += offset.to_bytes(3, "little")
    packed += (0xFFFFFFFF).to_bytes(4, "little")
    return bytes(packed), unique_rows, duplicate_count


def parse_save_modes(path: pathlib.Path):
    text = path.read_text(encoding="utf-8", errors="strict")
    rows = [(code, int(mode, 16)) for code, mode in re.findall(
        r'\{"(.{4})",0x([0-9A-Fa-f]{2})\}', text
    )]
    if not rows or rows[-1] != ("FFFF", 0):
        raise ValueError("save-mode sentinel is missing")
    return rows[:-1]


def encode_game_code(code: str) -> int:
    if len(code) != 4:
        raise ValueError(f"invalid game code {code!r}")
    indexes = []
    for position, (alphabet, char) in enumerate(zip(SAVE_MODE_ALPHABETS, code)):
        try:
            indexes.append(alphabet.index(char))
        except ValueError as exc:
            raise ValueError(
                f"game-code character {char!r} is not valid at position {position}"
            ) from exc
    key = indexes[0]
    key = key * len(SAVE_MODE_ALPHABETS[1]) + indexes[1]
    key = key * len(SAVE_MODE_ALPHABETS[2]) + indexes[2]
    key = key * len(SAVE_MODE_ALPHABETS[3]) + indexes[3]
    if key >= (1 << 18):
        raise ValueError("encoded game code exceeds 18 bits")
    return key


def pack_save_modes(rows):
    first_modes: dict[str, int] = {}
    duplicate_count = 0
    for code, mode in rows:
        if mode not in SAVE_MODE_VALUES:
            raise ValueError(f"unsupported save mode 0x{mode:02X}")
        previous = first_modes.get(code)
        if previous is None:
            first_modes[code] = mode
        else:
            duplicate_count += 1
            if previous != mode:
                raise ValueError(
                    f"duplicate game code {code} changes mode "
                    f"0x{previous:02X} -> 0x{mode:02X}"
                )

    packed_records = []
    for code, mode in first_modes.items():
        key = encode_game_code(code)
        mode_index = SAVE_MODE_VALUES.index(mode)
        packed_records.append((key, key | (mode_index << 18), code, mode))
    packed_records.sort(key=lambda record: record[0])

    for left, right in zip(packed_records, packed_records[1:]):
        if left[0] == right[0]:
            raise ValueError(f"encoding collision: {left[2]} and {right[2]}")

    packed = bytearray()
    for _, value, _, _ in packed_records:
        packed += value.to_bytes(3, "little")
    return bytes(packed), packed_records, duplicate_count


def write_reset_header(rows, unique_rows, duplicate_count: int, packed: bytes, output: pathlib.Path):
    old_bytes = 4 * (1 + sum(2 + len(offsets) for _, offsets in rows))
    content = (
        "#ifndef RESET_TABLE_H\n"
        "#define RESET_TABLE_H\n\n"
        "/* Auto-generated by tools/generate_compatibility_tables.py. */\n"
        f"#define RESET_TABLE_GAME_COUNT {len(unique_rows)}u\n"
        f"#define RESET_TABLE_OFFSET_COUNT {sum(len(o) for _, o in unique_rows)}u\n"
        f"#define RESET_TABLE_PACKED_SIZE {len(packed)}u\n\n"
        "static const u8 __attribute__((aligned(4))) reset_table_packed[] = {\n"
        + c_bytes(packed)
        + "\n};\n\n"
        + f"/* {len(rows)} legacy rows, {duplicate_count} unreachable duplicates removed.\n"
        + f" * Legacy table: {old_bytes} bytes; packed table: {len(packed)} bytes. */\n"
        + "#endif\n"
    )
    output.write_text(content, encoding="utf-8", newline="\n")


def write_save_header(rows, packed: bytes, records, duplicate_count: int, output: pathlib.Path):
    old_bytes = (len(rows) + 1) * 5
    alphabets = "\n".join(
        f'static const char save_mode_alphabet_{i}[] = "{alphabet}";'
        for i, alphabet in enumerate(SAVE_MODE_ALPHABETS)
    )
    modes = ", ".join(f"0x{mode:02X}" for mode in SAVE_MODE_VALUES)
    content = (
        "#ifndef SAVE_MODE_H\n"
        "#define SAVE_MODE_H\n\n"
        "/* Auto-generated by tools/generate_compatibility_tables.py. */\n"
        f"#define SAVE_MODE_PACKED_ENTRY_COUNT {len(records)}u\n"
        "#define SAVE_MODE_PACKED_KEY_MASK 0x0003FFFFu\n"
        "#define SAVE_MODE_PACKED_MODE_SHIFT 18u\n\n"
        f"static const u8 save_mode_values[8] = {{ {modes} }};\n"
        + alphabets
        + "\n\n"
        + "static const u8 __attribute__((aligned(4))) save_mode_packed_table[] = {\n"
        + c_bytes(packed)
        + "\n};\n\n"
        + f"/* {len(rows)} legacy rows, {duplicate_count} identical duplicates removed.\n"
        + f" * Legacy table: {old_bytes} bytes; packed records: {len(packed)} bytes. */\n"
        + "#endif\n"
    )
    output.write_text(content, encoding="utf-8", newline="\n")

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify generated files are current")
    args = parser.parse_args()

    reset_rows = parse_reset_table(LEGACY_DIR / "reset_table_legacy.h")
    reset_packed, reset_unique_rows, reset_duplicate_count = pack_reset_table(reset_rows)
    save_rows = parse_save_modes(LEGACY_DIR / "saveMODE_legacy.h")
    save_packed, save_records, duplicate_count = pack_save_modes(save_rows)

    temporary = ROOT / ".compatibility-table-generation"
    temporary.mkdir(exist_ok=True)
    try:
        save_temp = temporary / "saveMODE.h"
        write_save_header(save_rows, save_packed, save_records, duplicate_count, save_temp)
        destination = SOURCE_DIR / "saveMODE.h"
        if args.check:
            if not destination.exists() or destination.read_bytes() != save_temp.read_bytes():
                raise SystemExit(f"generated file is stale: {destination.relative_to(ROOT)}")
        else:
            destination.write_bytes(save_temp.read_bytes())
    finally:
        for child in temporary.glob("*"):
            child.unlink()
        temporary.rmdir()

    print(
        f"reset archive: {len(reset_rows)} rows -> {len(reset_unique_rows)} first-code rows, "
        f"{sum(len(o) for _, o in reset_unique_rows)} offsets; runtime data is external FORMAT=2"
    )
    print(
        f"save mode: {len(save_rows)} rows -> {len(save_records)} unique, "
        f"{len(save_packed)} bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
