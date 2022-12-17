# FFmpeg configuration changes

## [**Back to top**](../README.md)

## Overview

To build without patented codecs Chromium provides a GN variable
`proprietary_codecs` that can be set to false. However, besides the
codecs itself this disable a lot of code related to MPEG parsing and
handling codec metadata which has been patent-free at least since 2017.
These code is necessary so we can use FFmpeg demuxer to extract audio or
video streams from the media files.

So Vivaldi does not disable that variable. Instead we set
`proprietary_codecs` to true and patch during the build time FFmpeg
config to either disable AAC and h.264 decoders or replace them with
ones implemented using OS API, see [**Audio**][1]

## FFmpeg config changes

During the build FFmpeg requires a few configuration-like C header files
to specify a particular components that the build has to include.
Chromium generates those sources with a special script and then commit
the resulting files under `chromium/third_party/ffmpeg/chromium/config`
directory. Then the compiler is instructed to look for the include files
in that directory first.

To patch the files out build script from
`platform_media/ffmpeg/BUILD.gn` reads the relevant files, replaces the
lines with our changes and write the result to a build directory. Then
the compilation settings are adjusted to use those files, not the ones
in `chromium/third_party/ffmpeg/chromium/config`.

In addition `platform_media/source_update.gni` adds the relevant OS
libraries that we use to implement audio decoders using OS API to FFmpeg
build targets.

[1]: audio_decoders.md