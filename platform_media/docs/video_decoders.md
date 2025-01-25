# Video Decoders

## [**Back to top**](../README.md)

## Overview

The video codecs for h.264 in Vivaldi is mostly based on the original
patches from Opera.

As the expiration date for the last relevant h.264 patent is around
2027, it will be a while before Vivaldi can stop maintaining this code.

## Chromium changes

The basic change is a call to `VivaldiDecoderConfig::AddVideoDecoders()`
from video decoder factory and media tests in Chromium. This ensures
that our Mac and Windows h.264 decoders are called after Chromium tied
other decoders.

In addition on Mac we patch
`chromium/media/gpu/mac/vt_video_decode_accelerator_mac.cc` not to
insist that it requires hardware decoder. In theory this change makes
the original code from Opera unnecessary on Mac, but we keep and
maintain it in case Chromium will switch to another API on Mac where
patching to enable software decoding mode will be problematic.

On Windows Chromium provides `DXVAVideoDecodeAccelerator` class from
`chromium/media/gpu/windows/dxva_video_decode_accelerator_win.h` that
implements video decoding using Media Foundation Classes API. In
principle as on Mac this class can be patched not to insist on hardware
acceleration. But Chromium deprecated this decoder in favour of Direct
3D implementation in `D3D11VideoDecoderImpl` class and related code and
that assumes video hardware acceleration is always available.

## Code details

### Windows

The Windows decoder is provided by `WMFVideoDecoder` class from
`platform_media/decoders/win/wmf_video_decoder.h`. The original Opera
patches implemented it via a common template shared between audio and
video. But since the audio implementation is done in FFmpeg, the video
code was moved into own class.

Another important change was to make the class to override
`NeedsBitstreamConversion()` method from `media::VideoDecoder` to repack
the video into a stream with meta-information. Without this change the
original Opera code fails to decode many video streams. This method is
only called if `USE_PROPRIETARY_CODECS` is defined in Chromium meaning
that it was essential to set `use_proprietary_codecs` in GN build config
to make this work.
