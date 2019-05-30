# Linux Platform Media

## [**Back to top**](../README.md)

## Introduction

For Linux we build Chromium's patched FFMpeg as a seperate library
(libffmpeg.so), that is bundled within the installable package. This bundled
library will only load open media formats.

All the “magic” on Linux happens within the startup wrapper (a shell script
that is used to launch the main binary). The wrapper is located here, within
our git repository:

**vivaldi/chromium/chrome/installer/linux/common/wrapper**

## Alternative libffmpeg.so libraries

Some Linux distributions provide copies of libffmpeg.so with proprietary media
enabled. Usually this is for use with the copy of Chromium that the
distribution maintains but occasionally these files are even provided for us
or other Chromium based browsers (Opera, Brave,…).

The reasons for third parties being able to include a copy of with proprietary
media enabled this vary but include:

* They have paid the appropriate licenses to do so (e.g. Ubuntu)
* The library is provided from a external repository where licensing issues do
  not apply (e.g. community repository hosted and developed entirely out of
  Europe, where software patents are not enforced).
* A distribution where all or large parts are compiled from source on the
  user's local machine (e.g. Gentoo). This is OK as patents do not prevent
  distribution of source code, only binaries.

## Loading alternative libffmpeg.so libraries

The entire trick on Linux is to look for alternative libraries (within the
startup wrapper) and determine if they are suitable for our usage. If they are
we load them (via LD_PRELOAD) instead of our bundled libffmpeg.so.

### Tests to confirm they are suitable

The following tests are performed to determine if a library is suitable:

1. We have a list of hardcoded locations where we expect to find suitable
   libraries. This has been built up over time based on our knowledge of
   distros and where they are likely to store something that could be used.
2. We check for the version of the library. Often a copy of libffmpeg.so that
   was built for a Chromium that is too old (or two new) can causes crashes
   and other problems. The lower and upper version limits are usually
   discovered during testing and then the numbers hardcoded into the wrapper.
3. We check that the library has all of its dependencies fullfilled. This is
   needed because users on distributions which do not provide a suitable
   libffmpeg.so will sometimes copy the file over from a repository intended
   for a different distribution.

The first matching libffmpeg.so found that passes these tests will be loaded
instead of our bundled copy.

If no suitable library is found we just use the bundled libffmpeg.so and
print a warning in the terminal that only open media formats will load. This
warning also includes a link to [HTML5 Proprietary Audio and Video on Linux][1],
which explains the packages you could install to get an alternative
libffmpeg.so with proprietary media support)

## Test libs for Chromium updates

When we first bump Chromium, a suitable version (with proprietary media
support) of libffmpeg.so might not yet be available from any distro. For the
purposes of testing media during the chromium update cycle, Ruarí maintains an
[internal respository of libffmepg.so files][2] for different Chromium
versions and architectures.

[1]: https://help.vivaldi.com/article/html5-mp4-h-264-aac-support-under-linux/
[2]: http://byggmakker.viv.int/testfiles/linux/media/proprietary/
