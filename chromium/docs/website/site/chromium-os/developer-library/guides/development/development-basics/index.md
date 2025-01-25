---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: development-basics
title: ChromiumOS Development Basics
---

This document covers guidelines to keep in mind when working on the ChromiumOS
project.

[TOC]

## Documentation

### General guidelines

ChromiumOS follows the Chromium project's [documentation guidelines] and
[documentation best practices].

### Design documents

Design documents typically describe the problem that you're trying to solve,
different approaches that you considered, and the path that you're planning to
take. They're useful for soliciting opinions from others before you start on the
implementation, but they also help your code reviewers get their bearings and
can answer other developers' "why was this done like that?" questions after the
work is complete (even after you've forgotten the reasons why!). On top of all
that, a design doc often helps you solidify a design in your own mind and
convince yourself that you're not missing anything.

Most non-trivial additions of new functionality, and particularly ones that
require adding new daemons or new interactions between different processes,
should have their own design docs. For smaller changes, documenting what you're
doing and why in the issue tracker is often enough.

Share your design docs with your team mailing list, if not with the full
[chromium-os-dev] mailing list. See the existing [ChromiumOS design docs] and
[Chromium design docs] for inspiration.

## Programming languages and style

### Rust

The newest userspace code in ChromiumOS is usually written in Rust to take
advantage of its improved security and ergonomics. Being a memory safe language
with a runtime overhead similar to C/C++ makes it uniquely suited for new code
with reduced incidence of security and stability bugs.

The Rust style guide is currently in development by the ChromiumOS Authors, but
it basically follows that of Rust's default rustfmt configuration. Additionally
clippy is used a linter, usually with certain lints suppressed. For an example
of what lints might be disabled, look at the [set from crosvm]. The [Rust on
ChromeOS] document has more information.

In certain cases C++ is the better choice for userspace code. In particular,
existing C++ projects should not be converted to Rust, except in exceptional
circumstances. However, new modules to existing C++ projects have been
successfully written in Rust. Other cases where C++ should be considered are
projects reliant on Mojo, which has no production ready Rust bindings.

### C++

Most userspace code in the ChromiumOS project is written in C++, whether it's
part of the Chrome browser itself or a system daemon exposing new functionality.

The Chromium project, and by extension the ChromiumOS project, follow the
[Google C++ style guide], with the [Chromium C++ style guide] layered on top to
provide additional guidelines and clarify ambiguities.

The [Modern C++ use in Chromium] document lists which new C++ features are
allowed. ChromiumOS follows the list.

New C++ programs should go into new directories in the [platform2 repository],
which is checked out to `src/platform2`.

### C

C should only be used for code that is part of the Linux kernel or ChromeOS
firmware.

Both kernel and firmware code should follow the [Linux kernel coding style]. The
[Kernel Development] guide has more details about ChromeOS kernel development, and the
[EC Development page] discusses firmware.

Usage of C in new first-party userspace projects is strongly discouraged. Unless
there is a critical, external to ChromeOS reason to write a new userspace
project in C, usage of C will be disallowed.

`platform` and `platform2` OWNERS are encouraged to reject new C code in
first-party userspace ChromeOS components.

### Shell

Sometimes shell scripting can be the best way to perform lightweight,
non-persistent tasks that need to run periodically on ChromeOS systems, but
there are also big downsides: it's much more difficult to write tests for shell
scripts than for C++ code, and scripts have a tendency to grow to the point
where they become difficult to maintain. If you're planning to add a shell
script that is likely to grow in complexity, consider instead using C++ from the
start to save yourself the trouble of rewriting it down the road.

Shell scripts are mainly used for a few categories of tasks in ChromeOS:

*   Upstart initialization scripts, as in [src/platform2/init]. See the
    [init STYLE file] and [init README file] for guidelines.
*   Portage `.ebuild` files, as in the [chromiumos-overlay repository]. We
    follow the upstream guidelines; see the [Ebuild Writing] page and
    specifically the [Ebuild File Format].
*   Miscellaneous development-related tasks

Read the [ChromiumOS shell style guidelines] before writing scripts, with the
caveat that the Upstart or Portage guidelines take precedence when writing
those types of scripts.

For shell scripts that ship with the OS image, be extra careful. The shell
provides powerful features, but the flip side is that security pitfalls are
tricky to avoid. Think twice whether your shell statements can have unintended
side effects, in particular if your script runs with full privileges (as is the
case with init scripts). As a guideline, keep things simple and move more
complex processing to a properly sandboxed environment in an C++ daemon.

### Python

The Python interpreter is not included in production ChromeOS system images,
but Python is used heavily for development and testing.

We largely follow the [Google Python style guide], but see the
[ChromiumOS Python style guidelines] for important differences, particularly
around indenting and naming. For tests, see the [autotest coding style].

## Testing

The [ChromiumOS testing site] is the main repository of information about
testing.

### Unit tests

Unit tests should be added alongside new code. It's important to design your
code with testability in mind, as adding tests after-the-fact often requires
heavy refactoring.

Good unit tests are fast, lightweight, reliable, and easy to run within the
chroot as part of your development workflow. We use [Google Test] (which is
comprised of the GoogleTest unit testing framework and the GoogleMock mocking
framework) to test C++ code. [Why Google C++ Testing Framework?] and the
[Google Test FAQ] are good introductions, and the [unit testing document] has
more details about how unit tests get run.

See the [Best practices for writing ChromeOS unit tests] document for more
guidance on writing good unit tests.

### Autotest

[Autotest] is used to run tests on live ChromeOS systems. Autotests are useful
for performing integration testing (e.g. verifying that two processes are able
to communicate with each other over IPC), but they have heavy costs:

*   Autotests are harder to run than unit tests: you need either a dedicated
    test device or a virtual machine.
*   Autotests are much slower than unit tests: even a no-op test can take 30
    seconds or longer per run.
*   Since autotests involve at least a controlling system and a test device,
    they're susceptible to networking issues and hardware flakiness.
*   Since autotests run on full, live systems, failures can be caused by issues
    in components unrelated to the one that you're trying to test.

Whenever you can get equivalent coverage from either unit tests or autotests,
use unit tests. Design your system with unit testing in mind.

## Code reviews

The ChromiumOS project follows the [Chromium code review policy]: all code and
data changes must be reviewed by another project member before being committed.
Note that ChromiumOS's review process has some differences; see the
[Developer Guide's code review instructions].

OWNERS files are not (yet) enforced for non-browser parts of the ChromiumOS
codebase, but please act as if they were. If there's an OWNERS file in the
directory that you're modifying or a parent directory, add at least one
developer that's listed in it to your code review and wait for their approval
before committing your change.

Owners may want to consider setting up notifications for changes to their code.
To receive notifications of changes to `src/platform2/debugd`, open your
[Gerrit notification settings] and add a new entry for project
`chromiumos/platform2` and expression `file:"^debugd/.*"`.

## Issue trackers

Public ChromiumOS bugs and feature requests are tracked using the
[chromium issue tracker]. Note that this tracker is shared with the Chromium
project; most OS-specific issues are classified under an `OS>`-prefixed
component and have an `OS=Chrome` label. The `crbug.com` redirector makes it
easy to jump directly to an issue with a given ID; `https://crbug.com/123` will
redirect to issue #123, for instance.

Keep discussion in the issue tracker instead of in email or over IM. Issues
remain permanently available and are viewable by people not present for the
original discussion; email and IM exchanges are harder to find after the fact
and may even be deleted automatically. `BUG=chromium:123` lines in commit
descriptions and `https://crbug.com/123` mentions in code comments also make it
easy to link back to the issue that describes the original motivation for a
change.

Avoid discussing multiple problems in a single issue. It's not possible to split
an existing issue into multiple new issues, and it can take a long time to read
through an issue's full history to figure out what is currently being discussed.

Similarly, do not reopen an old, closed issue in response to the reoccurrence of
a bug: the old issue probably contains now-irrelevant milestone and merge labels
and outdated information. Instead, create a new issue and refer to the prior
issue in its initial description. Text of the form `issue 123` will
automatically be turned into a link.

There is much more information about filing bugs and using labels in the
[Chromium bug reporting guidelines].

## Mailing lists

See the [contact] document for more details.

## Developing in the open

ChromiumOS is an open-source project. Whenever possible (i.e. when not
discussing private, partner-related information), use the public issue tracker
and mailing lists rather than the internal versions.

[contact]: /chromium-os/developer-library/guides/who-do-i-notify/contact/
[documentation guidelines]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/documentation_guidelines.md
[documentation best practices]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/documentation_best_practices.md
[ChromiumOS design docs]: https://www.chromium.org/chromium-os/chromiumos-design-docs
[Chromium design docs]: https://www.chromium.org/developers/design-documents
[set from crosvm]: https://chromium.googlesource.com/chromiumos/platform/crosvm/+/HEAD/bin/clippy
[Rust on ChromeOS]: https://www.chromium.org/chromium-os/developer-library/guides/rust/rust-on-cros
[Google C++ style guide]: https://google.github.io/styleguide/cppguide.html
[Chromium C++ style guide]: https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/c++/c++.md
[Modern C++ use in Chromium]: https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/c++/c++-features.md
[platform2 repository]: /chromium-os/developer-library/guides/development/platform2-primer/
[Linux kernel coding style]: https://github.com/torvalds/linux/blob/HEAD/Documentation/process/coding-style.rst
[Kernel Development]: /chromium-os/developer-library/guides/kernel/kernel-development/
[EC Development page]: https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/README.md
[src/platform2/init]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/init/
[init STYLE file]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/init/STYLE
[init README file]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/init/README
[chromiumos-overlay repository]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay
[Ebuild Writing]: https://devmanual.gentoo.org/ebuild-writing/index.html
[Ebuild File Format]: https://devmanual.gentoo.org/ebuild-writing/file-format/index.html
[ChromiumOS shell style guidelines]: /chromium-os/developer-library/reference/style-guides/shell/
[Google Python style guide]: https://google.github.io/styleguide/pyguide.html
[ChromiumOS Python style guidelines]: /chromium-os/developer-library/reference/style-guides/python/
[autotest coding style]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/HEAD/docs/coding-style.md
[ChromiumOS testing site]: https://www.chromium.org/chromium-os/testing
[Google Test]: https://github.com/google/googletest
[Why Google C++ Testing Framework?]: https://github.com/google/googletest/blob/HEAD/googletest/docs/primer.md
[Google Test FAQ]: https://github.com/google/googletest/blob/HEAD/googletest/docs/faq.md
[unit testing document]: https://www.chromium.org/chromium-os/testing/adding-unit-tests-to-the-build
[Best practices for writing ChromeOS unit tests]: /chromium-os/developer-library/guides/testing/unit-tests/
[Autotest]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/HEAD/docs/user-doc.md
[Chromium code review policy]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/code_reviews.md
[Developer Guide's code review instructions]: /chromium-os/developer-library/guides/development/developer-guide/#Upload-your-changes-and-get-a-code-review
[Gerrit notification settings]: https://chromium-review.googlesource.com/settings/#Notifications
[chromium issue tracker]: https://crbug.com/
[Chromium bug reporting guidelines]: https://www.chromium.org/for-testers/bug-reporting-guidelines
[chromium-os-dev]: https://groups.google.com/a/chromium.org/forum/#!forum/chromium-os-dev
