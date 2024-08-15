---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: libchrome
title: libchrome
---

[TOC]

## Background

The Chromium project has an general utility library referred to as
[libbase]. Because it is standalone and does not depend on any other parts
of Chromium, it has been been picked up by other Google related projects so
people don't have to reinvent these things.

Along those lines, a package in ChromiumOS is provided so that projects
specific to us can share the code without having to bundle it multiple
times over. Currently, there are over 20 such projects in ChromiumOS.
To keep people on their toes, the package is named `libchrome` as
`libbase` by itself is too confusing. Granted, `libchrome` isn't exactly
clear itself, but it's a lose-lose situation, and we tried our best.
Please still love us.

## Using libchrome in ChromiumOS

### In an ebuild (non-platform2)

**Note:** If your package is integrated into the [platform2] ebuild,
then this is already handled for you in the common platform2 ebuild
and you can skip this section.

There are 4 things to make sure the ebuild does when building a Chromium
OS package that uses libchrome:

1.  inherit the `cros-debug` eclass
1.  inherit the `libchrome-version` eclass and depend on
    `chromeos-base/libchrome:=` with the right
    USE=cros-debug setting, or inherit the `libchrome` eclass.
1.  call `cros-debug-add-NDEBUG` in one of `src_prepare`, `src_configure`,
    or `src_compile`.
1.  make sure to have `-DBASE_VER=${v}` for C++ compile flag where
    `local v="$(libchrome_ver)"`. Sometimes `export BASE_VER=$(libchrome_ver)`
    may also be used so makefile can use `BASE_VER` environment variable
    directly.

Below you can find copy & paste snippets that should work for any ebuild
in the ChromiumOS tree. All you should need to change is the `125070` as
the number is updated.

```
...
inherit cros-debug libchrome-version
...
RDEPEND="chromeos-base/libchrome:=[cros-debug=]"
...
src_compile() {
  ...
  tc-export PKG_CONFIG
  cros-debug-add-NDEBUG
  export BASE_VER="$(libchrome_ver)"
  ...
}
```

### In the platform2 build system

If your package has been upgraded to [platform2] (if not, why not?),
then it's simple.

In your package's `BUILD.gn`, list libchrome as a dependency as needed
(e.g. either in the project-common `target_defaults` or the target-specific
section). For example, `portier` takes the former approach:

```
pkg_config("target_defaults") {
  pkg_deps = [
    "libbrillo",
    "libchrome",
    "protobuf",
  ]
}
```

At time of writing, the platform2 build system automatically takes care
of the rest for you.

### In common.mk (deprecated)

In a standard `common.mk` ChromiumOS platform project, you can use these
snippets in your Makefile:

```
# If the build env has exported $PKG_CONFIG to a wrapper, use that, else use
# the default pkg-config wrapper (so we can "make" in place for testing).
PKG_CONFIG ?= pkg-config

# If the build env has exported $BASE_VER, use that. Else, use the installed
libchrome.
BASE_VER ?= $(shell cat /usr/share/libchrome/BASE_VER)

# You can add as many or as view pkg-config libraries to this PC_DEPS
# value. Here we just use a specific version of libbase.
PC_DEPS = libchrome

# Look up the compiler flags and linker settings via the pkg-config
# wrapper once. That is why we use a dedicated variable and the :=
# operator -- if we use =, then make will end up executing the
# pkg-config wrapper many times.
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
```

### In a package's source code

In your source, include files from `base/` like normal. So if you want to
use the string printf API, do:

```c
#include <base/stringprintf.h>
```

## Internal Details

Over time, we've evolved how we package up the base source tree. Here
we'll cover the lessons we've learned, and why we do the things that we
do. This way, future decisions can take into consideration all the
factors without missing something.

For some more background (and specifics), see
[this thread](https://groups.google.com/a/chromium.org/d/topic/chromium-os-dev/daH97iacQeQ/discussion).

### Dynamic Libraries

We provide shared library libbase.so (and libmojo.so for mojo, etc) instead of
static library so the binary code can be reused among ChromiumOS projects.

Exceptions: For unittest test related libraries, .a static library is provided
instead.

The advantages here:

1.  There is only one copy of all libbase functions and state at runtime
    -- when we call a logging function, we know that function has one copy
    of its state, and know that everyone will be using that same function.
1.  We know exactly what version of libbase an application was linked with
    after it's been built (by looking at `DT_NEEDED` ELF tags).
1.  We can track down exactly what packages are implicitly using libbase
    without declaring it in its ebuild.

It's not all peaches and apple pie though -- there are some trade-offs here:

1.  All programs using libbase load all of libbase at runtime, even parts
    they don't use (functions and state).
    *   Arguably, this overhead is small, but it is not zero.
1.  All third-party libraries that libbase needs get pulled in at runtime,
    even if they're only needed by parts of libbase the application doesn't
    use (and those libraries could pull in other libraries!).
    *   This overhead is much more noticeable than the previously-mentioned
        issue.
1.  The libchrome ebuild has to be complete in terms of what files it
    builds and links together as any undefined references will cause a
    shared library link error.
    *   This is actually a good thing as it means we can be confident
        that when updating, our package has included all the objects we
        care about. Otherwise, we have to try compiling all the other
        projects and see if they all pass (without undefined references)
        before we know the new package built all the necessary files.
1.  Static libraries (other than project-specific ones -- e.g. convenience
    libraries) are no longer allowed to use libbase (e.g. a project provides
    `libfoo.a` which uses libbase and multiple other projects use
    `libfoo.a`) as this is hard to track, and leads to similar problems as
    slotted static libraries in terms of mismatch of ABI.
    *   Not a big deal as we have few libraries in ChromiumOS that use
        libbase, and making them all shared has been okay.
1.  Projects that link against a shared library that links against
    libbase must use the same version of libbase. Otherwise, at runtime,
    an application will load `libbase.so` and `libfoo.so`, and
    `libfoo.so` will load `libbase.so`, and both libbases
    will attempt to satisfy symbol references leading to runtime ABI
    conflicts and possibly random crashes.
    *   To be safe, this improves the set of packages that need to be
        upgraded simultaneously from all consumers of libbase to consumers
        of a specific library. So anyone who links against libmetrics
        (which uses libbase) has to use the same version of libbase as
        libmetrics. When an upgrade occurs, they should all upgrade
        together. For standalone projects which don't link against any
        libraries that use libbase, they can safely upgrade/downgrade
        independently.

The last point here is the only real show stopper.
Fortunately, two things work in our favor. Generally, the ABI is stable
with libbase (across the version ranges we upgrade between), so the
runtime "mostly works". This means we can tolerate a period of time
where we are upgrading to a newer version of libbase but some
applications are actually (runtime) linked against multiple versions. By
the time we actually release, the upgrade will have completed, so there
will once again only be one version live at a time. Further, since we
can detect exactly what versions of a library an ELF has been linked
against, we can confidently detect the cases where a program uses one
version of libbase, but links against a shared library which pulls in a
different one and act appropriately (i.e. update all the packages).

At this point, we have a workable solution. But we can still do better.
Onwards!

### Using pkg-config

The advantages of providing a `.pc` file for projects to query are
significant!

1.  We can hide all of libbases' dependencies from projects that just
    want to use libbase. Rather than having the projects manually specify
    `-lrt` or `-lpthread` or `-I/usr/include/...` or anything else, the
    `.pc` file declares everything it depends on. Projects then just ask
    for the CFLAGS and libraries that libbase needs, and it gets expanded
    as needed.
1.  If we want to make any library changes in the future (moving or
    renaming the files or the install paths), we only have to update the
    `.pc` file. No end projects need change.

For the nit-pickers out there, there are disadvantages:

*   You have to depend on the pkg-config package, and execute `pkg-config`
    at build time.
    *   This is required by many other projects already (like glib), so
        not really unique to libbase problem.

So this cleans up the compiling/linking process nicely, and integrates
with existing pkg-config framework that other libraries depend on.

### Optimized Dynamic Libraries

The biggest disadvantage to shared libraries is that libbase isn't
really one API, but rather a large collection of different APIs.
Some require third party libraries to work (like pcre or glib or
pthreads), while others require very little. So forcing one project
that wants just the simple APIs (like string functions) to pull in more
complicated APIs which pull in other third party libraries (like pcre)
even though it won't use them is a waste of runtime resources.

We can combat this though by leveraging some linker tricks. When you
specify a library like `-lfoo`, it doesn't have to be just a static
archive (`libfoo.a`) or a shared ELF (`libfoo.so`), it could even be a
[linker script]! Combined with the useful `AS_NEEDED` directive, we can
create an arbitrary number of smaller shared libraries (like
`libbase-core` and `libbase-pcre` and `libbase-foo`) where
each one has its own additional library requirements and provides different
sets of APIs. Then when people link against `-lbase`, the linker will
look at all of the smaller libbase shared libraries and only pull in the
ones we actually use. This is all transparent to the user of the libbase
API.

So we have all of the advantages of slotted dynamic libraries, and only
one of the downsides: we still have possible runtime conflicts where a
program uses one version, but a library it links against uses a
different version. As noted previously, this is an acceptable trade off
for now, and makes the upgrade situation significantly more manageable.

### cros-debug and NDEBUG

Some of libbase's headers define structs or classes that include or
exclude members based on whether the `NDEBUG` macro is defined or not.
If libbase is built with `NDEBUG` defined, but then a program that
dynamically links against libbase includes those headers with `NDEBUG`
undefined (or vice versa), resulting in disagreements about the sizes
of objects, hard-to-debug segfaults can occur. We define a `cros-debug`
`USE` flag to try to ensure that `NDEBUG` is set or unset consistently
across different packages.

### Future Work

Here are some random thoughts that might be worth investigating to try and
improve the current situation:

*   Put all of libbase into a version-specific C++ namespace.

## Building libchrome

See the
[gn build recipe](https://chromium.googlesource.com/chromiumos/platform/libchrome/+/HEAD/BUILD.gn)
.

It maintains a list of all the files which go into a shared library
fragment (such as 'core' and 'glib' and 'event'). It's largely split
along the lines of what third party libraries will get pulled in (so the
'core' only requires C libraries, 'glib' additionally requires glib,
'event' additionally requires libevent, etc...). The fragments could
conceivably be split further, but the trade-offs in terms of runtime
overhead were found to not warrant it (generally in the range of "system
noise").

This build file is also responsible for generating the pkg-config `.pc`
file and linker script.

## Upgrade/Release Plans

See [Internal
Doc](http://g3doc/chrome_platform/eng_velocity/g3doc/libchrome_maintenance.md).


## Making changes locally

*   **NOTE**: This is intended for testing *local* changes only, not for changes
intended to be committed (e.g. a patch or a cherry-pick).

*   **NOTE**: If the interface (ABI) is changed in any way, the entire image
will need to be rebuilt.

To make local changes (for testing an update or adding debugging) you can do
the following.

1.  Create a local branch in the libchrome repository.
    *   `cd src/platform/libchrome`
    *   `repo start ${branch_name}`
1.  Locally modify the files, and create a local comit.
    *   `git commit -am "libchrome: {change description}"`
1.  Find the git commit hash
    *   `git log -n 1`
1.  Run `cros_workon-${BAORD} start libchrome` to use local HEAD.
1.  Build and deploy libchrome
    *   `emerge-${BOARD} libchrome {plus dependent workon packages, e.g. shill}`
    *   `cros deploy ${IP} libchrome {other packages}`

## See also

[libbase] on Google Git

[libbase]: https://chromium.googlesource.com/chromium/src/base/
[platform2]: ../platform2_primer.md
[pkg-config]: https://www.freedesktop.org/wiki/Software/pkg-config/
[linker script]: https://sourceware.org/binutils/docs/ld/Implicit-Linker-Scripts.html
[chromium/src/base]: https://chromium.googlesource.com/chromium/src/base/
[chromium/src/build]: https://chromium.googlesource.com/chromium/src/build/
[chromium/src/components/timers]: https://chromium.googlesource.com/chromium/src/components/timers/
[chromium/src/crypto]: https://chromium.googlesource.com/chromium/src/crypto/
[chromium/src/dbus]: https://chromium.googlesource.com/chromium/src/dbus/
[chromium/src/sandbox]: https://chromium.googlesource.com/chromium/src/sandbox/
