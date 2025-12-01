#!/usr/bin/env python3
# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Print errno table as markdown."""

import argparse
import os
from pathlib import Path
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
HEADER = "errno.h"

# The markdown file where we store this table.
MARKDOWN = "index.md"

# The string we use to start the table header.
START_OF_TABLE = "| number |"


def find_symbols(target: str) -> dict[str, str]:
    """Find all the symbols using |target|."""
    cc = f"{target}-clang"
    ret = subprocess.run(
        [cc, "-E", "-dD", "-P", "-"],
        check=True,
        input=f"#include <{HEADER}>\n",
        stdout=subprocess.PIPE,
        encoding="utf-8",
    )
    table = {}
    for line in ret.stdout.splitlines():
        # If we need to make this smarter, signals.py handles things.
        assert not line.startswith("#undef E"), "#undef not handled"
        if line.startswith("#define E"):
            sym, val = line.strip().split()[1:3]
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

        desc = os.strerror(num)
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
