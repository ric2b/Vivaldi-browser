# Audio Decoders

## [**Back to top**](../README.md)

## Overview

The initial implementation of audio codecs for AAC sound from Opera
implemented `media::AudioDecoder` interface in Chromium. This was
similar to how [*Video*][1] decoding was implemented. However these days
Chromium calls FFmpeg API directly to decode sound in few places without
using `media::AudioDecoder`. So to cover those cases the decoders were
changed to implement FFmpeg API and ported to C from C++. They replace
the default FFmpeg AAC decoder from
`chromium/third_party/ffmpeg/libavcodec/aacdec.c`.

Vivaldi probably needs to maintain this code only until 2024. FFmpeg
presently implements only parts of AAC standard that are published in
2003 or earlier so relevant patents should expire in 2024. But this has
to be confirmed.

## Code details

All references to source files below without directory names refer to
files from `platform_media/ffmpeg` directory.

### Logging

FFmpeg has logging hooks that allows to provide custom logging. But
those hooks does not allow to use Chromiums --vmodule support. To
workaround that the hooks are replaced using a preprocessor define to
point to utilities from `ffviv_log.h` and `ffviv_log.cc` that in turn
provide integration with Chromium logging.

With this changes passing
`--enable-logging=stderr --vmodule=filename=verbose_level` works with
all files in `platform_media/ffmpeg`. The maximum level of verboseness
is 7 and gives rather big output.

### ADTS

Audio streams sometimes are packed into ADTS (Audio Data Transport
Stream) where each packet to decode are preceded with a header
describing the nature of audio. FFmpeg provides so-called filtering API
to convert ADTS into raw data and the meta information called extra data
in FFmpeg sources. This extra information replaces the original meta
derived from the container itself. However it turned out that to support
audio as found on the Internet ADTS config cannot be always trusted and
the original extra data should be used if OS API rejects extra_data
generated from ADTS.

To address that helpers from `ffviv_audio_kit.h` allows to check for
ADTS headers while preserving the original extra data. The code follows
`chromium/third_party/ffmpeg/libavcodec/aac_adtstoasc_bsf.c`.

### Mac

The Mac AAC decoder is implemented using AudioToolbox API from Apple and
is located at `audiotoolboxdec.c` is a fork of
`chromium/third_party/ffmpeg/libavcodec/audiotoolboxdec.c`. Most of the
changes there were to make the decoder to deal better with missing or
invalid meta data and be more robust to errors, see comments and the git
log for the file for details.

### Windows

The Windows AAC decoder is implemented using MFC (Media Foundation
Classes) API and is located in `wmfaacdec.c`. As the code is in C it
uses a few helper macros to call the relevant methods of MFC objects. It
also follows a strict policy of not using an early return and calls
`goto cleanup` instead where the label marks all the necessary cleanup
code that in C++ is done in destructors.

As with Mac, dealing with inconsistent meta or extra data was the most
problematic aspect of the code. In addition there is a mismatch between
MFC and FFmpeg expectations on how API has to used resulting in somewhat
complex state machine transitions. Comments and the git log for the file
provide details.

## Other implementations

### Firefox

To decode AAC audio Mozilla uses the same approach that Vivaldi uses and
calls into platform-specific audio API. Their [*Mac*][2] and
[*Windows*][3] provides details.

### Chromium

FFmpeg does not support xHE-AAC decoder. To decode such audio Chromium
uses platform API.

Mac implementation is provided by [*AudioToolboxAudioDecoder*][4] class.
Although it is tailored specifically for xHE-AAC, the implementation is
similar to what we have in our fork of the original `audiotoolboxdec.c`
from FFmpeg.

Windows code is in [*MediaFoundationAudioDecoder*][5] and in the related
classes.

In theory Chromium code can be patched to support all AAC variations.
Such change will remove the need to maintain sandbox relaxations in
Vivaldi. However until Chromium fixes their audio code and starts using
`media::AudioDecoder` API everywhere instead of calling FFmpeg audio
decoders directly, such patch will be of no use for Vivaldi.

[1]: video_decoders.md
[2]: https://searchfox.org/mozilla-central/source/dom/media/platforms/apple/AppleATDecoder.cpp
[3]: https://searchfox.org/mozilla-central/source/dom/media/platforms/wmf/WMFAudioMFTManager.cpp
[4]: ../../chromium/media/filters/mac/audio_toolbox_audio_decoder.h
[5]: ../../chromium/media/filters/win/media_foundation_audio_decoder.h