---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: rust-on-cros
title: Rust on ChromeOS
---

This provides information on creating [Rust] projects for installation within
ChromeOS and ChromeOS SDK. All commands and paths given are from within the
SDK's chroot.

[TOC]

## Usage

[cros-rust.eclass] is an eclass that supports first-party and third-party crate
dependencies. Rust project's ebuild should inherit the eclass to support Rust
building in ChromeOS build system.

All the Rust projects using `cros-rust.eclass` will get dependent crates from
`/build/${BOARD}/usr/lib/cros_rust_registry` instead of [crates.io].
You'll learn how to publish first-party and third-party crates to
`cros_rust_registry` in the following sections.

### Third-party ebuild crates

To add a third-party crate, follow the
[instructions][rust_crates_updating] in the `rust_crates` repository. No
ebuild is needed for third-party crates.

### First-party crates

You can create your Rust project in anywhere in the ChromeOS system and its
ebuild in a suitable place in `chromiumos-overlay` with name
`<category>/<crate_name>-9999.ebuild`. Here is an ebuild for
an example **first-party** crate:

```bash
# Copyright <copyright_year> The ChromiumOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI="7"

CROS_WORKON_INCREMENTAL_BUILD=1
CROS_WORKON_LOCALNAME="example"
CROS_WORKON_PROJECT="path/to/project/repository"
# We don't use CROS_WORKON_OUTOFTREE_BUILD here since project's Cargo.toml is
# using "provided by ebuild" macro which supported by cros-rust.
CROS_WORKON_SUBTREE="path/to/project/subtree"

inherit cros-workon cros-rust

DESCRIPTION="An example first party project"
HOMEPAGE="home_page_url"

LICENSE="BSD-Google"
SLOT="0/${PVR}"
KEYWORDS="~*"
IUSE="test"

DEPEND="
	dev-rust/third-party-crates-src:=
"
# (crbug.com/1182669): build-time only deps need to be in RDEPEND so they are pulled in when
# installing binpkgs since the full source tree is required to use the crate.
RDEPEND="${DEPEND}"

# Only include this if you need to install binaries or multiple crates.
src_install() {

	# 1. Publish this library for other first-party crates.
	#    This is needed if other crates depend on this crate.
	cros-rust_src_install
	# 2. Install the binary to image. This isn't needed for libraries.
	dobin "$(cros-rust_get_build_dir)/example_bin"
}
```

Then follow the [instructions][rust_crates_updating] in the
`rust_crates` repository for adding a first-party crate.

> **WARNING**: Please make sure your project could be built by both steps for
> engineering productivity:
>
> 1.  From chroot with `emerge-${BOARD} CRATE-EBUILD-NAME`
>
>     **Tips**: You can set `USE=-lto` to speed up build times when using
>     emerge. This turns off link time optimization, which is useful for
>     release builds but significantly increases build times and isn't really
>     needed during development.
>
> 2.  From project root directory with `cargo build`
>
> We add two macros to resolve conflicts between these two build system. Check
> details from the following section.

#### Ebuild versioning

Ebuild versions should match the version in `Cargo.toml`. To keep the versions
in sync, add a `files/chromeos-version.sh` script like this:

```sh
#!/bin/sh
#
# Copyright <copyright_year> The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Assumes the first 'version =' line in the Cargo.toml is the version for the
# crate.
awk '/^version = / { print $3; exit }' "$1/<path/to/Cargo.toml>" | tr -d '"'
```

Replace `<path/to/Cargo.toml>` with the path of `Cargo.toml` relative
to the root of the repository it's in.

### Cargo.toml macros

The `cros-rust` eclass supports building crates in the ChromeOS build system,
but it breaks `cargo build` in some situations. We add two macros which are
recognized by the eclass to keep both build systems working. The macros take
the form of special comments in `Cargo.toml`.

-   `# provided by ebuild`

    This macro introduces a replacement of:

    ```toml
    [dependencies]
    data_model = { path = "../data_model" }  # provided by ebuild
    ```

    with:

    ```toml
    [dependencies]
    data_model = { version = "*" }
    ```

    **Example usage**: Add dependency to first-party crate

    1.  Add the dependent crate to DEPEND section in ebuild.
    2.  Use relative path for imported crates in `Cargo.toml` but with
        `# provided by ebuild` macro:

    ```toml
    [dependencies]
    data_model = { path = "../data_model" }  # provided by ebuild
    ```

-   `# ignored by ebuild`

    We will use this to discard parts of `[patch.crates-io]` which should be
    applied to local developer builds but not to ebuilds.

    This macro introduces a replacement of:

    ```toml
    audio_streams = { path = "../../third_party/adhd/audio_streams" } # ignored by ebuild
    ```

    with empty line while building with emerge.

    **Example usage**: Add dependency to first-party crate for sub-crates from
    root crate.

    1.  Add the dependent crate to DEPEND section in root crate's ebuild.
    2.  Use relative path for imported crates in `[patch.crates-io]` section in
        root crate's `Cargo.toml` but with `# ignored by ebuild` macro:

    ```toml
    [patch.crates-io]
    audio_streams = { path = "../../third_party/adhd/audio_streams" } # ignored by ebuild
    ```

## Panic Signatures

The default crash handler is not able to generate useful signatures for crashes
in Rust binaries. To get better crash signatures, you need to install a custom
panic handler which integrates with ChromeOS's crash collector.

All you need to do is to call the following function at the top of `main()`:
```rust
fn main() {
    libchromeos::panic_handler::install_memfd_handler();
    ...
}
```

Ensure that your package & ebuild depend on libchromeos:

Cargo.toml
```toml
[dependencies]
libchromeos = "0.1.0"
...

[patch.crates-io] # ignored by ebuild
crosvm-base = { path = "../../../chroot/usr/lib/cros_rust_registry/registry/crosvm-base-0.1.0/" } # ignored by ebuild
libchromeos = { path = "../../../chroot/usr/lib/cros_rust_registry/registry/libchromeos-0.1.0/" } # ignored by ebuild
```

Ebuild:
```bash
DEPEND="
	${COMMON_DEPEND}
	...
	dev-rust/libchromeos:=
	...
"
```

## Workflows

There are a few different ways for building and testing first party Rust code.

### cros_workon_make

Using [cros_workon_make] is the same for Rust as with typical platform2 packages
on ChromeOS. The downsides for using it with Rust on CrOS are **it modifies the
Cargo.toml** file and can be slower than running cargo directly. You will want
to locally commit any Cargo.toml changes before running [cros_workon_make]. If
you try to run cargo directly with the `cros_workon_make`-modified Cargo.toml,
things will likely be broken, so be sure to checkout the unmodified copy before
calling cargo.

### cargo

The primary downside of using cargo directly is it will fetch dependencies from
crates.io though the internet. These often do not match with the versions used
by ChromeOS, leading to undesired consequences. One workaround is to have an
up-to-date Cargo.lock file checked in for your project. Another is to use the
same cargo config as cros_workon_make. There is a helper script that set this up
for you:

```bash
BOARD=<board> ~/chromiumos/src/platform/dev/contrib/setup_cros_cargo_home
```

This can be applied outside the ChromeOS chroot by copying the
`~/.cargo/config` from the chroot outside and updating the paths to be correct
for outside the chroot.

***note
**Note** the resulting `~/.cargo/config` depends on the cros_rust_registry for
the specified ${BOARD}. You may need to run `build_packages` for the board or
`emerge-${BOARD} dev-rust/<dependency>` to install or update dependencies.
***

#### Cross-compiling

This is taken care of in the `~/.cargo/config` if you preform the setup above.

The toolchain that is installed by default is targetable to the following triples:

| Target Triple                 | Description                                                                         |
|-------------------------------|-------------------------------------------------------------------------------------|
| `x86_64-pc-linux-gnu`         | **(default)** Used exclusively for packages installed in the chroot                 |
| `armv7a-cros-linux-gnueabihf` | Used by 32-bit usermode ARM devices                                                 |
| `aarch64-cros-linux-gnu`      | Used by 64-bit usermode ARM devices (none of these exist as of November 30th, 2018) |
| `x86_64-cros-linux-gnu`       | Used by x86_64 devices                                                              |

When building Rust projects for development, a non-default target can be
selected as follows:

```bash
cargo build --target=<target_triple>
```

If a specific board is being targeted, that board's sysroot can be used for
compiling and linking purposes by setting the `SYSROOT` environment variable as
follows:

```bash
export SYSROOT="/build/<board>"
```

If C files are getting compiled with a build script that uses the `cc` or `gcc`
crates, you may also need to set the `TARGET_CC` environment variable to point
at the appropriate C compiler.

```bash
export TARGET_CC="<target_triple>-clang"
```

If a C/C++ package is being pulled in via `pkg-config`, the
`PKG_CONFIG_ALLOW_CROSS` environment
variable should be exposed. Without this, you might see `CrossCompilation`
as part of an error message during build script execution.

```bash
export PKG_CONFIG_ALLOW_CROSS=1
```

[rust]: https://www.rust-lang.org
[cargo]: https://crates.io/
[crates.io]: https://crates.io
[cros-rust.eclass]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/eclass/cros-rust.eclass
[cros_workon_make]: https://chromium.googlesource.com/chromiumos/docs/+/main/developer_guide.md#Make-your-changes
[localmirror]: archive_mirrors.md
[link time optimizations]: https://en.wikipedia.org/wiki/Interprocedural_optimization
[rust_crates_updating]: https://chromium.googlesource.com/chromiumos/third_party/rust_crates/+/HEAD/README.md#updating-packages
[semver]: https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html
