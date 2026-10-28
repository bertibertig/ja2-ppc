#!/usr/bin/env python3
"""Create include aliases for the original JA2 sources' mixed-case includes.

The x86 sources were built on a case-insensitive file system and spell the
same header several ways ("types.h", "Types.h", "DEBUG.H", "debug.h").
For every quoted #include in the given source directories that resolves
to an existing header only case-insensitively, this writes a symlink with
the exact spelling into the output directory. Add that directory to the
include path after the real source directories.

    gen_case_includes.py OUTPUT_DIR SOURCE_DIR [SOURCE_DIR ...]
"""

import os
import re
import sys

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


def skipped(root):
    parts = os.path.normpath(root).split(os.sep)
    return any(part.startswith(("build", ".")) or part == "third_party" for part in parts)


def main():
    output, sources = sys.argv[1], sys.argv[2:]
    headers = {}
    for directory in sources:
        for root, _, files in os.walk(directory):
            if skipped(root):
                continue
            for name in files:
                if name.lower().endswith((".h", ".c", ".i")):
                    headers.setdefault(name.lower(), os.path.join(root, name))
    os.makedirs(output, exist_ok=True)
    wanted = set()
    for directory in sources:
        for root, _, files in os.walk(directory):
            if skipped(root):
                continue
            for name in files:
                if not name.lower().endswith((".c", ".h")):
                    continue
                with open(os.path.join(root, name), encoding="latin-1") as handle:
                    wanted.update(INCLUDE.findall(handle.read()))
    created = 0
    for spelling in sorted(wanted):
        base = os.path.basename(spelling.replace("\\", "/"))
        target = headers.get(base.lower())
        if target is None or os.path.basename(target) == base:
            continue
        link = os.path.join(output, base)
        if os.path.lexists(link):
            if os.readlink(link) == os.path.abspath(target):
                continue
            os.remove(link)
        os.symlink(os.path.abspath(target), link)
        created += 1
    print(f"case include aliases: {created} new in {output}")


if __name__ == "__main__":
    main()
