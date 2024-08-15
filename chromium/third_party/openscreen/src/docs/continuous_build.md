# Continuous build and try jobs

Open Screen uses [LUCI builders](https://ci.chromium.org/p/openscreen/builders)
to monitor the build and test health of the library.

Current builders include:

| Name                   | Arch   | OS                     | Toolchain | Build   | Notes                  | CQ? |
|------------------------|--------|------------------------|-----------|---------|------------------------|-----|
| linux_x64              | x86-64 | Ubuntu Linux 20.04     | clang     | debug   | ASAN enabled           |  Y  |
| linux_arm64            | arm64  | Ubuntu Linux 20.04 [\*] | clang    | debug   |                        |  N  |
| linux_x64_gcc          | x86-64 | Ubuntu Linux 20.04     | gcc-9     | debug   |                        |  Y  |
| linux_x64_msan_rel     | x86-64 | Ubuntu Linux 20.04     | clang     | release | MSAN enabled           |  N  |
| linux_x64_tsan_rel     | x86-64 | Ubuntu Linux 20.04     | clang     | release | TSAN enabled           |  N  |
| linux_x64_coverage     | x86-64 | Ubuntu Linux 20.04     | clang     | debug   | used for code coverage |  N  |
| linux_arm64_cast_receiver | arm64  | Ubuntu Linux 20.04 [\*] | clang | debug   | Builds cast sender/receiver |  N  |
| mac_x64                | x86-64 | Mac OS 13              | clang     | debug   |                        |  Y  |
| win_x64                | x86-64 | Windows 10             | clang     | debug   |                        |  N  |
| chromium_linux_x64     | x86-64 | Ubuntu Linux 20.04     | clang     | debug   | built with chromium    |  Y  |
| chromium_mac_x64       | x86-64 | MacOS 13               | clang     | debug   | built with chromium    |  Y  |
| chromium_win_x64       | x86-64 | Windows 10             | clang     | debug   | built with chromium    |  N  |
<br />

[*] Tests run on Ubuntu 20.04, but are cross-compiled to arm64 with a debian
sysroot.

The chromium_ builders compile against Chromium top-of-tree to ensure that
changes can be autorolled into Chromium.

The CQ builders are part of the commit queue and must pass before CLs can be
landed.

You can run a patch through all builders using `git cl try` or the Gerrit Web
interface.  All builders are run as part of the commit queue and are also run
continuously in our CI.
