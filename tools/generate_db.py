#!/usr/bin/env python3
"""Generate compact native tables from the pinned upstream hashid.py."""
from __future__ import annotations

import pathlib
import sre_parse
import sys

ROOT = pathlib.Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "upstream-hashid"))
import hashid  # type: ignore  # noqa: E402

nodes: list[tuple[str, int, int, int]] = []
sequences: list[tuple[int, int]] = []
class_atoms: list[tuple[str, int, int]] = []
classes: list[tuple[int, int, int]] = []
branch_sequences: list[int] = []


def add_class(items) -> int:
    negate = 0
    first = len(class_atoms)
    for op, arg in items:
        name = str(op)
        if name == "NEGATE":
            negate = 1
        elif name == "LITERAL":
            class_atoms.append(("HidClassLiteral", arg, arg))
        elif name == "RANGE":
            class_atoms.append(("HidClassRange", arg[0], arg[1]))
        elif name == "CATEGORY":
            category = str(arg)
            mapping = {
                "CATEGORY_DIGIT": "HidClassDigit",
                "CATEGORY_SPACE": "HidClassSpace",
                "CATEGORY_WORD": "HidClassWord",
            }
            class_atoms.append((mapping[category], 0, 0))
        else:
            raise ValueError(f"unsupported class opcode {op}")
    classes.append((first, len(class_atoms) - first, negate))
    return len(classes) - 1


def add_sequence(parsed) -> int:
    local: list[tuple[str, int, int, int]] = []
    for op, arg in parsed:
        name = str(op)
        if name == "AT":
            at = str(arg)
            local.append(("HidNodeBegin" if "BEGINNING" in at else "HidNodeEnd", 0, 0, 0))
        elif name == "LITERAL":
            local.append(("HidNodeLiteral", arg, 0, 0))
        elif name == "NOT_LITERAL":
            local.append(("HidNodeNotLiteral", arg, 0, 0))
        elif name == "ANY":
            local.append(("HidNodeAny", 0, 0, 0))
        elif name == "IN":
            local.append(("HidNodeClass", add_class(arg), 0, 0))
        elif name == "SUBPATTERN":
            local.append(("HidNodeSubsequence", add_sequence(arg[-1]), 0, 0))
        elif name in ("MAX_REPEAT", "MIN_REPEAT"):
            minimum, maximum, child = arg
            maximum = 0xFFFF if maximum == sre_parse.MAXREPEAT else maximum
            local.append(("HidNodeRepeat", minimum, maximum, add_sequence(child)))
        elif name == "BRANCH":
            first = len(branch_sequences)
            branch_sequences.extend(add_sequence(part) for part in arg[1])
            local.append(("HidNodeBranch", first, len(arg[1]), 0))
        else:
            raise ValueError(f"unsupported opcode {op}")
    first = len(nodes)
    nodes.extend(local)
    sequences.append((first, len(local)))
    return len(sequences) - 1


def c_string(value: str | None) -> str:
    if value is None:
        return "NULL"
    data = value.encode("utf-8")
    escaped = "".join(chr(b) if 32 <= b < 127 and chr(b) not in '\\"' else f"\\x{b:02x}" for b in data)
    return f'"{escaped}"'


def main() -> None:
    prototypes = []
    candidates = []
    for prototype in hashid.prototypes:
        sequence = add_sequence(sre_parse.parse(prototype.regex.pattern, prototype.regex.flags))
        first = len(candidates)
        for mode in prototype.modes:
            candidates.append((mode.name, mode.hashcat, mode.john, mode.extended))
        prototypes.append((sequence, first, len(prototype.modes)))

    header = """/* Generated from upstream HashID; do not edit manually. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { HidNodeLiteral, HidNodeNotLiteral, HidNodeAny, HidNodeClass,
    HidNodeSubsequence, HidNodeRepeat, HidNodeBranch, HidNodeBegin, HidNodeEnd } HidNodeType;
typedef enum { HidClassLiteral, HidClassRange, HidClassDigit, HidClassSpace, HidClassWord } HidClassType;
typedef struct { uint8_t type; uint16_t a, b, child; } HidNode;
typedef struct { uint16_t first, count; } HidSequence;
typedef struct { uint8_t type; uint8_t a, b; } HidClassAtom;
typedef struct { uint16_t first; uint8_t count, negate; } HidClass;
typedef struct { uint16_t sequence, first_candidate; uint8_t candidate_count; } HidPrototype;
typedef struct { const char* name; int32_t hashcat; const char* john; bool extended; } HidCandidate;

extern const HidNode hid_nodes[];
extern const HidSequence hid_sequences[];
extern const HidClassAtom hid_class_atoms[];
extern const HidClass hid_classes[];
extern const uint16_t hid_branch_sequences[];
extern const HidPrototype hid_prototypes[];
extern const HidCandidate hid_candidates[];
extern const size_t hid_prototype_count, hid_candidate_count;
"""
    lines = ['/* Generated from psypanda/hashID commit 7e8473a823060e56d4b6090a98591e252bd9505e. */', '#include "hashid_db.h"', ""]
    lines.append("const HidNode hid_nodes[] = {")
    lines.extend(f"    {{{t}, {a}, {b}, {c}}}," for t, a, b, c in nodes)
    lines.append("};\nconst HidSequence hid_sequences[] = {")
    lines.extend(f"    {{{a}, {b}}}," for a, b in sequences)
    lines.append("};\nconst HidClassAtom hid_class_atoms[] = {")
    lines.extend(f"    {{{t}, {a}, {b}}}," for t, a, b in class_atoms)
    lines.append("};\nconst HidClass hid_classes[] = {")
    lines.extend(f"    {{{a}, {b}, {c}}}," for a, b, c in classes)
    lines.append("};\nconst uint16_t hid_branch_sequences[] = {")
    lines.extend(f"    {value}," for value in branch_sequences)
    lines.append("};\nconst HidPrototype hid_prototypes[] = {")
    lines.extend(f"    {{{a}, {b}, {c}}}," for a, b, c in prototypes)
    lines.append("};\nconst HidCandidate hid_candidates[] = {")
    lines.extend(
        f"    {{{c_string(name)}, {mode if mode is not None else -1}, {c_string(john)}, {'true' if ext else 'false'}}},"
        for name, mode, john, ext in candidates
    )
    lines.append("};")
    lines.append(f"const size_t hid_prototype_count = {len(prototypes)}U;")
    lines.append(f"const size_t hid_candidate_count = {len(candidates)}U;")
    (ROOT / "hashid_db.h").write_text(header, encoding="utf-8", newline="\n")
    (ROOT / "hashid_db.c").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"generated {len(prototypes)} prototypes, {len(candidates)} candidates, {len(nodes)} nodes")


if __name__ == "__main__":
    main()
