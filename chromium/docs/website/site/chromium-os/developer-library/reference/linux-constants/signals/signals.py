#!/usr/bin/env python3
# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Print signal table as markdown."""

import argparse
import ctypes
import ctypes.util
from pathlib import Path
import re
import subprocess
import sys
from typing import Optional


THIS_FILE = Path(__file__).resolve()

# The directory where all the code & markdown files live.
THIS_DIR = THIS_FILE.parent

sys.path.insert(0, str(THIS_DIR.parent))

# pylint: disable=import-error,wrong-import-position
import constants


# The C library header to find symbols.
HEADER = "signal.h"

# The markdown file where we store this table.
MARKDOWN = "index.md"

# The string we use to start the table header.
START_OF_TABLE = "| number |"


def strsignal(num: int) -> str:
    """Until Python supports this, do it ourselves."""
    # Handle internal glibc details.
    if num in (32, 33):
        return (
            "Real-time signal reserved by the C library for "
            "[NPTL][pthreads(7)]; see [signal(7)]"
        )

    libc = ctypes.CDLL(ctypes.util.find_library("c"))
    proto = ctypes.CFUNCTYPE(ctypes.c_char_p, ctypes.c_int)
    func = proto(("strsignal", libc), ((1,),))
    return func(num).decode("utf-8")


def find_symbols(target: str) -> dict[str, str]:
    """Find all the symbols using |target|."""
    cc = f"{target}-clang"
    source = f"#include <{HEADER}>\n"
    ret = subprocess.run(
        [cc, "-E", "-dD", "-P", "-"],
        check=True,
        input=source,
        stdout=subprocess.PIPE,
        encoding="utf-8",
    )

    table = {}

    # Find all the symbols that are known.  We have to do two passes as the
    # headers like to #undef & redefine names.
    matcher = re.compile(r"^#define\s+(SIG[^_]+)\s+[0-9]")
    symbols = set()
    for line in ret.stdout.splitlines():
        m = matcher.match(line)
        if m:
            sym = m.group(1)
            if sym not in ("SIGSTKSZ",):
                symbols.add(sym)

    source += "\n".join(f"_{x} {x}" for x in symbols)

    # Pull out any aliases.
    matcher = re.compile(r"#define\s+(SIG[^_]+)\s(SIG[A-Z]+)")
    for line in ret.stdout.splitlines():
        m = matcher.match(line)
        if m:
            table[m.group(1)] = m.group(2)

    # Parse our custom code and extract the symbols.
    ret = subprocess.run(
        [cc, "-E", "-P", "-"],
        check=True,
        input=source,
        stdout=subprocess.PIPE,
        encoding="utf-8",
    )

    for line in ret.stdout.splitlines():
        if line.startswith("_SIG"):
            sym, val = line.strip().split()
            sym = sym[1:]
            assert sym not in table, f"sym {sym} already found"
            table[sym] = val
    return table


def load_table() -> tuple[dict[str, str], dict[str, list[str]]]:
    """Return a table of all the symbol values (and aliases)."""
    all_tables = {}
    for target in constants.TARGETS:
        all_tables[target] = find_symbols(target)

    # Check that all the tables are the same.
    basetarget = constants.TARGETS[0]
    baseline = all_tables[basetarget]
    for target, table in all_tables.items():
        assert baseline == table

    # Sometimes values have multiple names.
    aliases = {}
    for sym, val in baseline.items():
        try:
            int(val)
        except ValueError:
            aliases.setdefault(val, []).append(sym)

    # Deal with dynamic realtime signals.
    baseline["SIGRTMIN-2"] = "32"
    baseline["SIGRTMIN-1"] = "33"
    assert "SIGRTMIN" not in baseline
    baseline["SIGRTMIN"] = "34"
    assert "SIGRTMAX" not in baseline
    baseline["SIGRTMAX"] = "64"
    for i in range(1, 16):
        num = 34 + i
        baseline[f"SIGRTMIN+{i}"] = str(num)
    for i in range(1, 15):
        num = 64 - i
        baseline[f"SIGRTMAX-{i}"] = str(num)

    return (baseline, aliases)


def sort_table(table: dict[str, str]) -> list[tuple[str, str]]:
    """Return a sorted table."""

    def sorter(element):
        try:
            num = int(element[1])
        except ValueError:
            num = 0
        return (num, element[0])

    return sorted(table.items(), key=sorter)


def get_md_table(
    table: dict[str, str], aliases: dict[str, list[str]]
) -> list[str]:
    """Return the table in markdown format."""
    ret = []
    last_num = 0
    for sym, val in sort_table(table):
        try:
            num = int(val)
        except ValueError:
            continue

        # Fill in holes in the table so it's obvious to the user when searching.
        for stub in range(last_num + 1, num):
            ret.append("| %i | 0x%02x | | *not implemented* ||" % (stub, stub))
        last_num = num

        desc = strsignal(num)
        ret.append("| %i | 0x%02x | %s | %s |" % (num, num, sym, desc))
        for alias in aliases.get(sym, []):
            ret.append(
                "| %i | 0x%02x | %s | *(Same value as %s)* %s |"
                % (num, num, alias, sym, desc)
            )
    return ret


def get_parser() -> argparse.ArgumentParser:
    """Return a command line parser."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-i",
        "--inplace",
        action="store_true",
        help="Update the markdown file directly.",
    )
    return parser


def main(argv: list[str]) -> Optional[int]:
    """The main func!"""
    parser = get_parser()
    opts = parser.parse_args(argv)

    baseline, aliases = load_table()
    md_data = get_md_table(baseline, aliases)

    if opts.inplace:
        md_file = THIS_DIR / MARKDOWN
        old_data = md_file.read_text().splitlines(keepends=True)

        i = None
        for i, line in enumerate(old_data):
            if line.startswith(START_OF_TABLE):
                break
        else:
            print(
                f"ERROR: could not find table in {md_file}",
                file=sys.stderr,
            )
            sys.exit(1)

        old_data = old_data[0 : i + 2]
        with md_file.open("w") as fp:
            fp.writelines(old_data)
            fp.write("\n".join(md_data) + "\n")
    else:
        print("\n".join(md_data))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
