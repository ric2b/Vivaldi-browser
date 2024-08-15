---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: firmware-updating
title: Just-In-Time compilation in ChromeOS
---

This document covers the requirements for Just-in-time (JIT) compilation on
ChromeOS. These requirements ensure that JIT engines don't undermine the
security guarantees of ChromeOS.

The main problem with JIT-ing is that it bypasses Verified boot. The objective
of ChromeOS’s Verified boot is to ensure that all code executing on the device
has been verified as coming from Google. Code generated on-the-fly by a JIT
engine cannot be verified as coming from Google, because it doesn't exist at the
time the ChromeOS image is signed.

This is most clearly shown by the fact that JIT-ing requires memory regions to
be marked as both writable and executable -- the very thing that would defeat
Verified boot. If the process writing into these memory regions were
compromised, or if another process gained access to these memory regions,
Verified boot could be bypassed by just writing to these memory regions and then
jumping to this code.

Unfortunately it’s not currently possible to fully get rid of JIT engines since
the Chrome browser uses V8 to process Javascript, and V8 uses JIT compilation.
What we can do is restrict the way that JIT engines work in ChromeOS to try to
keep them as safe as possible:

*   The JIT engine should be embedded into the executable (e.g. linked as a
    library) rather than being a standalone interpreter present on the system as
    an executable.
*   The input provided to the JIT engine has to come from a partition covered by
    Verified boot:
    *   The rootfs.
    *   A component downloaded via component updater or the
        [DLC infrastructure].
*   The verification of the provenance of the input should be done on a file
    descriptor and not on a file path, since file paths can be modified and the
    checks can be raced.
*   More concretely, check that the `st_dev` member of the struct populated by
    the `fstat()` syscall, both on the file descriptor of the input, and on a
    file descriptor for the root path "/", are the same. This is based on the
    fact that files on the rootfs are on the partition mounted at “/”.
*   Ideally, modify the JIT engine to remove as much system interaction
    functionality as possible: if the JIT engine has a way to execute other
    binaries, disable that.

[DLC infrastructure]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/dlcservice/docs/developer.md
