# Vivaldi Platform Media

## Terminology

* Demuxer - the code to read and parse a media file to extract
  individual encoded audio and video streams and relevant
  meta-information.
* Decoder - the code that decodes individual audio or video stream.

## Introduction

Because of patents Vivaldi cannot ship with certain codecs turned on in
ffmpeg (the AAC audio codec and the h264 video codec as of 09.2022), so
to still be able to play proprietary media we have two solutions:

* On Mac and Windows we use platform decoders in their SDKs (the code in
  this directory).
* On [**Linux**][1] we depend on the user installing another ffmpeg
  (extra)

## The Code

There are three parts to getting proprietary media to work in Vivaldi,
the startup script on Linux, the `platform_media` directory and the
media patches to Chromium.

`platform_media` contains the following subdirectories.

* `decoders` - Video decoders that implement Chromium's
  `media::VideoDecoder` interface using OS API, see [**Video**][4]
* `docs` - Documentation files
* `ffmpeg` - Build scripts to adjust ffmpeg build configuration and
  audio decoders that implement FFmpeg decoding interface using OS API,
  see [**FFmpeg**][2] and [**Audio**][3]
* `sandbox` - Sandbox changes to enable access to OS media API, see
  [**Sandbox**][5]

The directory contains the following files:

* OPERA_LICENSE.txt : The license file for Opera
* source_updates.gni : The file that controls how the code is compiled
* README.md : This document

All changes to Chromium are inside C++ preprocessor #ifdef blocks with
either `USE_SYSTEM_PROPRIETARY_CODECS` define. In
addition GN build scripts use `system_proprietary_codecs` flags controlling
if the corresponding changes are enabled during compilation.

## References

* [**Linux**][1]
* [**FFmpeg**][2]
* [**Audio**][3]
* [**Video**][4]
* [**Sandbox**][5]
* [**IPCDemuxer**][6]

[1]: docs/linux.md
[2]: docs/ffmpeg.md
[3]: docs/audio_decoders.md
[4]: docs/video_decoders.md
[5]: docs/sandbox_media_changes.md
