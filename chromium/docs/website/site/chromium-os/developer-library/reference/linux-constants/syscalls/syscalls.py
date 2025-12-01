#!/usr/bin/env python3
# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Print syscall table as markdown."""

import argparse
import collections
import functools
import os
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
HEADER = "syscall.h"

# The markdown file where we store this table.
MARKDOWN = "index.md"

# The string we use to start the table header.
START_OF_TABLE = "## Tables"

# We order things based on expected usage.
ORDER = collections.OrderedDict(
    (
        ("x86_64", "x86_64 (64-bit)"),
        ("arm", "arm (32-bit/EABI)"),
        ("aarch64", "arm64 (64-bit)"),
        ("i686", "x86 (32-bit)"),
    )
)

# Which register is used for the syscall NR.
REGS = {
    "arm": {"nr": "r7", "args": ["r0", "r1", "r2", "r3", "r4", "r5"]},
    "aarch64": {"nr": "x8", "args": ["x0", "x1", "x2", "x3", "x4", "x5"]},
    "i686": {"nr": "eax", "args": ["ebx", "ecx", "edx", "esi", "edi", "ebp"]},
    "x86_64": {"nr": "rax", "args": ["rdi", "rsi", "rdx", "r10", "r8", "r9"]},
}


def manpage_link(name: str) -> str:
    """Get a link to the man page for this symbol."""
    # There are man pages for almost every syscall, so blindly link them.
    return f"[man/](https://man7.org/linux/man-pages/man2/{name}.2.html)"


def cs_link(name: str) -> str:
    """Get a link to code search for this symbol."""
    return (
        "[cs/](https://source.chromium.org/search?ss=chromiumos&"
        f"q=f:third_party/kernel+SYSCALL_DEFINE[^,]*\\b{name}\\b)"
    )


def kernel_version(target: str) -> tuple[int]:
    """Figure out what version of the kernel we're looking at."""
    cc = f"{target}-clang"
    source = "#include <linux/version.h>\nLINUX_VERSION_CODE\n"
    ret = subprocess.run(
        [cc, "-E", "-P", "-"],
        check=True,
        input=source,
        stdout=subprocess.PIPE,
        encoding="utf-8",
    )
    line = ret.stdout.splitlines()[-1]
    ver = int(line)
    return (ver >> 16 & 0xFF, ver >> 8 & 0xFF, ver & 0xFF)


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
    matcher = re.compile(r"^#define\s+(SYS_[^\s]+)\s+__NR_")
    symbols = set()
    for line in ret.stdout.splitlines():
        m = matcher.match(line)
        if m:
            sym = m.group(1)
            symbols.add(sym)

    # Because they can ...
    matcher = re.compile(r"^#define\s+(__ARM_NR_[^\s]+)\s+")
    for line in ret.stdout.splitlines():
        m = matcher.match(line)
        if m:
            sym = m.group(1)
            symbols.add(sym)

    source += "\n".join(f"__{x} {x}" for x in symbols)

    # Parse our custom code and extract the symbols.
    ret = subprocess.run(
        [cc, "-E", "-P", "-"],
        check=True,
        input=source,
        stdout=subprocess.PIPE,
        encoding="utf-8",
    )

    for line in ret.stdout.splitlines():
        if line.startswith("__"):
            sym, val = line.strip().split(" ", 1)
            if sym.startswith("____ARM"):
                sym = "ARM_" + sym[11:]
                if sym == "ARM_BASE":
                    continue
            else:
                sym = sym[6:]
            if val.startswith("("):
                val = constants.math_eval(val)
            val = int(val)
            assert sym not in table, f"sym {sym} already found"
            table[sym] = val

    return table


def load_table() -> dict[str, dict[str, str]]:
    """Return a table of all the symbol values (and aliases)."""
    all_tables = {}
    for target in constants.TARGETS:
        all_tables[target] = find_symbols(target)
    return all_tables


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
    table: dict[str, str], prototypes: dict[str, list[str]], arch: str
) -> list[str]:
    """Return the table in markdown format."""
    ret = []
    last_num = 0
    for sym, num in sort_table(table):
        # Fill in holes in the table so it's obvious to the user when searching.
        # Unless it's an obviously large gap for ABI reasons.  We pick 100 as an
        # arbitrary constant that seems to work.
        if num - last_num < 100:
            for stub in range(last_num + 1, num):
                anchor = f"{arch}_{stub}"
                ret.append(
                    f'| <a name="{anchor}"></a> [{stub}](#{anchor}) '
                    f"| *not implemented* "
                    "| "
                    f"| 0x{stub:02x} "
                    "||"
                )
        last_num = num

        anchor = f"{arch}_{num}"
        line = '| <a name="%s"></a> [%i](#%s) | %s | %s %s | 0x%02x | ' % (
            anchor,
            num,
            anchor,
            sym,
            manpage_link(sym),
            cs_link(sym),
            num,
        )
        prototype = prototypes.get(sym, ["?"] * 6)
        prototype = (prototype + ["-"] * 6)[0:6]
        line += " | ".join(
            x.replace("*", r"\*").replace("_", r"\_") for x in prototype
        )
        line += " |"

        ret.append(line)
    return ret


def get_md_common(tables: dict[str, dict[str, str]]) -> list[str]:
    all_syscalls = set()
    for table in tables.values():
        all_syscalls.update(table.keys())

    desc = [x.split(" ")[0] for x in ORDER.values()]
    targets = [
        [x for x in constants.TARGETS if x.startswith(frag)][0]
        for frag in ORDER.keys()
    ]

    ret = [
        "",
        "### Cross-arch Numbers",
        "",
        (
            "This shows the syscall numbers for (hopefully) the same syscall "
            "name across architectures."
        ),
        "Consult the [Random Names](#naming) section for common gotchas.",
        "",
        "| syscall name | %s |" % (" | ".join(desc),),
        "|---|" + (":---:|" * len(desc)),
    ]
    for syscall in sorted(all_syscalls):
        ret.append(
            "| %s | %s |"
            % (
                syscall,
                " | ".join(str(tables[x].get(syscall, "-")) for x in targets),
            )
        )
    return ret


@functools.cache
def find_source_root() -> Path:
    """Find the root dir of the CrOS source checkout."""
    for d in THIS_DIR.parents:
        if (d / ".repo").is_dir():
            return d
    raise RuntimeError(f"Unable to find CrOS source checkout: {THIS_DIR}")


def load_prototypes() -> dict[str, list[str]]:
    """Parse out prototypes from the kernel headers."""
    # Find the path to the kernel trees in the checkout.
    kernels = find_source_root() / "src/third_party/kernel"
    # Load all the kernel versions that have the header file we need.
    versions = [
        x
        for x in os.listdir(kernels)
        if (
            re.match("^v[0-9.]+$", x)
            and (kernels / x / "include/linux/syscalls.h").exists()
        )
    ]
    # Sort the versions to find the latest (which should be best?).
    versort = sorted([int(x) for x in v[1:].split(".")] for v in versions)
    latest = versort[-1]

    syscalls = (
        kernels
        / ("v%s" % ".".join(str(x) for x in latest))
        / "include/linux/syscalls.h"
    )
    ret = {}
    data = syscalls.read_text()
    entries = re.findall(
        r"^asmlinkage long sys_([^(]*)\s*\(([^)]+)\)", data, flags=re.M
    )
    for name, prototype in entries:
        # Clean up the prototype a bit.
        params = [x.strip() for x in prototype.split(",")]
        if params == ["void"]:
            params = []
        for i, param in enumerate(params):
            keywords = [x for x in param.split() if x not in ("__user",)]
            params[i] = " ".join(keywords)
        ret[name] = params
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

    tables = load_table()
    prototypes = load_prototypes()

    md_data = []

    for frag, desc in ORDER.items():
        target = [x for x in constants.TARGETS if x.startswith(frag)][0]

        ver = kernel_version(target)
        sver = ".".join(str(x) for x in ver)

        table = tables[target]
        reg_nr = REGS[frag]["nr"]
        reg_args = REGS[frag]["args"]
        md_data += [
            "",
            f"### {desc}",
            "",
            f"Compiled from [Linux {sver} headers][linux-headers].",
            "",
            (
                "| NR | syscall name | references | %%%s | arg0\u00a0(%%%s) | "
                "arg1\u00a0(%%%s) | arg2\u00a0(%%%s) | arg3\u00a0(%%%s) | "
                "arg4\u00a0(%%%s) | arg5\u00a0(%%%s) |"
            )
            % (reg_nr, *reg_args),
            "|:---:|---|:---:|:---:|---|---|---|---|---|---|",
        ]
        md_data += get_md_table(table, prototypes, frag)

    md_data += get_md_common(tables)

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

        old_data = old_data[0 : i + 1]
        with md_file.open("w") as fp:
            fp.writelines(old_data)
            fp.write("\n".join(md_data) + "\n")
    else:
        print("\n".join(md_data))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
