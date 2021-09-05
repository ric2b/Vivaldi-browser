# GN

GN is a meta-build system that generates build files for
[Ninja](https://ninja-build.org).

Related resources:

  * Documentation in [docs/](https://gn.googlesource.com/gn/+/master/docs/). In
    particular [GN Quick Start
    guide](https://gn.googlesource.com/gn/+/master/docs/quick_start.md)
    and the [reference](https://gn.googlesource.com/gn/+/master/docs/reference.md)
    (the latter is all builtin help converted to a single file).
  * An introductory [presentation](https://docs.google.com/presentation/d/15Zwb53JcncHfEwHpnG_PoIbbzQ3GQi_cpujYwbpcbZo/edit?usp=sharing).
  * The [mailing list](https://groups.google.com/a/chromium.org/forum/#!forum/gn-dev).

## Getting a binary

You can download the latest version of GN binary for
[Linux](https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/latest),
[macOS](https://chrome-infra-packages.appspot.com/dl/gn/gn/mac-amd64/+/latest) and
[Windows](https://chrome-infra-packages.appspot.com/dl/gn/gn/windows-amd64/+/latest).

Alternatively, you can build GN from source:

    git clone https://gn.googlesource.com/gn
    cd gn
    python build/gen.py
    ninja -C out
    # To run tests:
    out/gn_unittests

On Windows, it is expected that `cl.exe`, `link.exe`, and `lib.exe` can be found
in `PATH`, so you'll want to run from a Visual Studio command prompt, or
similar.

On Linux and Mac, the default compiler is `clang++`, a recent version is
expected to be found in `PATH`. This can be overridden by setting `CC`, `CXX`,
and `AR`.

## Examples

There is a simple example in [examples/simple_build](examples/simple_build)
directory that is a good place to get started with the minimal configuration.

To build and run the simple example with the default gcc compiler:

    cd examples/simple_build
    ../../out/gn gen -C out
    ninja -C out
    ./out/hello

For a maximal configuration see the Chromium setup:
  * [.gn](https://cs.chromium.org/chromium/src/.gn)
  * [BUILDCONFIG.gn](https://cs.chromium.org/chromium/src/build/config/BUILDCONFIG.gn)
  * [Toolchain setup](https://cs.chromium.org/chromium/src/build/toolchain/)
  * [Compiler setup](https://cs.chromium.org/chromium/src/build/config/compiler/BUILD.gn)

and the Fuchsia setup:
  * [.gn](https://fuchsia.googlesource.com/fuchsia/+/refs/heads/master/.gn)
  * [BUILDCONFIG.gn](https://fuchsia.googlesource.com/fuchsia/+/refs/heads/master/build/config/BUILDCONFIG.gn)
  * [Toolchain setup](https://fuchsia.googlesource.com/fuchsia/+/refs/heads/master/build/toolchain/)
  * [Compiler setup](https://fuchsia.googlesource.com/fuchsia/+/refs/heads/master/build/config/BUILD.gn)

## Reporting bugs

If you find a bug, you can see if it is known or report it in the [bug
database](https://bugs.chromium.org/p/gn/issues/list).

## Sending patches

GN uses [Gerrit](https://www.gerritcodereview.com/) for code review. The short
version of how to patch is:

    Register at https://gn-review.googlesource.com.

    ... edit code ...
    ninja -C out && out/gn_unittests

Then, to upload a change for review:

    git commit
    git push origin HEAD:refs/for/master

The first time you do this you'll get an error from the server about a missing
change-ID. Follow the directions in the error message to install the change-ID
hook and run `git commit --amend` to apply the hook to the current commit.

When revising a change, use:

    git commit --amend
    git push origin HEAD:refs/for/master

which will add the new changes to the existing code review, rather than creating
a new one.

We ask that all contributors
[sign Google's Contributor License Agreement](https://cla.developers.google.com/)
(either individual or corporate as appropriate, select 'any other Google
project').

## Community

You may ask questions and follow along with GN's development on Chromium's
[gn-dev@](https://groups.google.com/a/chromium.org/forum/#!forum/gn-dev)
Google Group.
