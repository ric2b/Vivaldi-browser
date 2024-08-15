---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: shell
title: Shell style guidelines [go/cros-shstyle]
---

[TOC]

## Introduction

This guide is for all shell source code that is created as part of the
ChromiumOS project i.e. code that is used on the device and `src/scripts/`. You
may not like some bits of it, but you still need to follow it. In doing so, you
ensure that others can work with your code. Our focus is on long term
maintenance rather than one person writing/owning things alone. Inevitably, that
one person will move on leaving a code base for others to pick up.

This guide may be ignored if you are working on code that is part of an open
source project that already conforms to a different style i.e. generally all
of the ebuilds & eclasses.

## Official Style Guide

The [Google Shell Style guide] is the official shell style guide for ChromiumOS
original code. Note that the guide often has "soft" recommendations that make it
sound like it is permissible to ignore its recommendations. This is due to the
guide attempting to cater to many groups (who have varying preferences on what
is "correct") and does not apply to the ChromiumOS project. If the guide covers
a particular bit of code, then you should be following it.

## Standard Selection

There are two standards that are permissible: [GNU Bash] and
[POSIX shell][POSIX standard].
When writing code that is used on developer systems or dev/test ChromiumOS
images, always use bash. For scripts that are used on the release ChromiumOS
image, you should be using POSIX shell.

Upstart jobs (`.conf` files in `/etc/init/`) require the POSIX shell standard.

The shebang (the first line of the file) should always reflect the language.
GNU Bash scripts must use `#!/bin/bash`; POSIX shell scripts must use
`#!/bin/sh`.

### POSIX Shell Restrictions

Various bash constructs can't be used in POSIX-conforming shell scripts.
If a construct isn't described in the POSIX shell standard, it isn't allowed in
POSIX shell scripts. Below is a list of bash-specific constructs that have been
sources of trouble in the past:

*   Tests using the double-bracket test operator,
    e.g. `[[ "${SOMEVAR}" == "test value" ]]`
*   Arrays, e.g. `foo=( 1 2 3 )`
*   The `declare` built-in command
*   Using the `function` keyword to define a shell function (you should never
    use this though, even with bash)
*   Brace expansion, e.g. `echo {a,b}{c,d}`

The list above isn't comprehensive; to be sure, you must consult the
[POSIX standard].

## Beyond The Guide

The Google Shell Style guide leaves a lot of things unspecified. Here we will
cover many things it does not.

### Arithmetic

Never use the `let` command, nor use the `$[...]` syntax, nor use the shell
helper `expr`. Instead, use `$((...))` to perform all math operations in
conjunction with the colon null utility.

When using variables, avoid using the `${var}` form when possible. The shell
knows to look up `var` as `${var}` for you and omitting the `${...}` leads to
cleaner code. Note: this does not mean you should avoid using `${var}` in
non-arithmetic code.

```bash
# To increment the variable "i" by one.  We avoid the ++ operator
# as it is not as portable, and this version isn't much longer.
# Note that:
#  - We do not write ${i} or $i.
#  - We put a space after the (( and before the )).
: $(( i += 1 ))

# To decrement the variable "i" by one:
: $(( i -= 1 ))

# Do some complicated math.
min=5
sec=30
echo $(( (min * 60) + sec ))
```

### Extended Printing (echo)

People often want to print out a string but omit the trailing new line.
Or print a string with escape sequences (like colors or tabs). You should
never use `echo` for this. Instead, use `printf`. In other words, when you
use `echo`, avoid all options like `-e` or `-n`. The `printf` command is both
powerful & portable, and has well defined behavior in all shell environments.

```bash
# Print out a string without a trailing newline.
printf '%s' "${some_var}"

# Print out a string and interpret escape sequences it contains.
printf '%b\n' "${some_var}"

# Print escape sequences in place.
printf '\tblah: run it and believe\n'
```

### Default Assignments

Sometimes you want to set a variable to something if it isn't already set.
People will try to test for this case using the `-z` operator
(`[[ -z ${foo} ]]`). This leads to duplicated/multiline code when it can all
be accomplished in one line. It might also not be correct if you want to
accept an empty string as a valid input.

```bash
# Assign "bar" to the variable "foo" if it is not set, or if it is set to "".
: "${foo:=bar}"

# Assign "bar" to the variable "foo" only if it is not set.
# If bar is already set to "", do nothing.
: "${foo=bar}"
```

### Argument/Option Parsing

Often times you want your script to accept flags like --foo or -q.
There are three options depending on how much flag parsing you need to do:

1.  Parse the arguments yourself and scan for options
    *   Should be avoided for anything beyond one or two simple flags
1.  Use the [getopts] built-in helper
    *   Preferred when you only have short options (e.g. -q and -v and -h vs
        --quiet and --version and --help)
    *   You have to implement the help (usage) flag yourself
1.  Use the [shflags] package
    *   Already shipped in the sdk and in board images
    *   Provides clean API for supporting short and long options
    *   Automatic support for help output

*** promo
There is a getopt program that is provided by the util-linux package,
but it should be avoided. For short options, it doesn't provide any advantage
over getopts, and for long options, it doesn't offer the level of functionality
that shflags does.
***

#### getopts

Here is an example of using getopts. Note the number of things required to be
implemented by you.

```bash
#!/bin/sh

die() {
  echo "${0##*/}: error: $*" >&2
  exit 1
}

usage() {
  echo "Usage: foo [options] [args]

This does something useful!

Options:
  -o <file>   Write output to <file>
  -v          Run verbosely
  -h          This help screen"
  exit 0
}

main() {
  local flag
  local verbose="false"
  local out="/dev/stdout"

  while getopts 'ho:v' flag; do
    case ${flag} in
      h) usage ;;
      o) out="${OPTARG}" ;;
      v) verbose="true" ;;
      *) die "invalid option found" ;;
    esac
  done

  if [[ ${verbose} == "true" ]]; then
    echo "verbose mode is enabled!"
  else
    echo "will be quiet"
  fi

  if [[ -z ${out} ]]; then
    die "-o flag is missing"
  fi
  echo "writing output to: '${out}'"

  # Now remaining arguments are in "$@".
  ...
}
main "$@"
```

#### shflags

Here is an example of using shflags. Options are declared & processed up top,
but then the layout is like normal.

*** note
**Warning**: shflags uses `eval` command against the input string to parse
arguments. To avoid introducing security vulnerabilities, you must make sure
that unprivileged processes cannot execute arbitrary commands as a privileged
user by sending malicious strings to your script as command line input.
***

```bash
#!/bin/sh

# This is the path in the sdk and in CrOS boards.
. /usr/share/misc/shflags

DEFINE_string out '/dev/stdin' 'Write output to this file' 'o'
DEFINE_boolean verbose ${FLAGS_FALSE} 'Enable verbose output' 'v'

FLAGS_HELP='Usage: foo [options] [args]

This does something useful!
'

# Parse command line.
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Only after this point should you enable `set -e` as
# shflags does not work when that is turned on first.
set -e

die() {
  echo "${0##*/}: error: $*" >&2
  exit 1
}

main() {
  if [[ ${FLAGS_verbose} -eq ${FLAGS_TRUE} ]]; then
    echo "verbose mode is enabled!"
  else
    echo "will be quiet"
  fi

  if [[ -z ${FLAGS_out} ]]; then
    die "--out flag is missing"
  fi
  echo "writing output to: '${FLAGS_out}'"

  # Now remaining arguments are in "$@".
  ...
}
main "$@"
```

[Google Shell Style guide]: https://google.github.io/styleguide/shellguide.html
[GNU Bash]: https://www.gnu.org/software/bash/
[POSIX standard]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
[getopts]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/getopts.html
[shflags]: https://code.google.com/p/shflags/wiki/Documentation10x
