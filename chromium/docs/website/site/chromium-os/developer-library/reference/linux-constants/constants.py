# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common code for scripts in here."""

import ast
import operator


# All the toolchains we care about.
TARGETS = (
    "aarch64-cros-linux-gnu",
    "armv7a-cros-linux-gnueabihf",
    "i686-cros-linux-gnu",
    "x86_64-cros-linux-gnu",
)


def math_eval(expr: str) -> str:
    """Evaluate a arithmetic expression."""
    # Only bother listing operators that actually get used.
    operators = {ast.Add: operator.add}

    def _eval(node):
        if isinstance(node, ast.Num):
            return node.n
        elif isinstance(node, ast.BinOp):
            return operators[type(node.op)](_eval(node.left), _eval(node.right))
        else:
            raise TypeError(node)

    return _eval(ast.parse(expr, mode="eval").body)
