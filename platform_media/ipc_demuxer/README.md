# IPC Demuxer

## [**Back to top**](../README.md)

## Note

This is about older and no longer used media demuxer unless Vivaldi starts with
`--enable-ipc-demuxer` flag. As the implementation requires a lot of patches to
Chromium, the suggestion is to remove the code the moment it causes a
non-trivial conflict during Chromium uptake.

The relevant code can be disabled at the compile time via setting
`vivaldi_use_system_media_demuxer` in `gn/config/features.gni`. All relevant
changes in Chromium sources are annotated with
`VIVALDI_USE_SYSTEM_MEDIA_DEMUXER` preprocessor define.

## Overview

The original Opera patches implemented the whole media demuxing and decoding
pipeline using a high-level platform media API and integrated them into Chromium
via implementing media::Demuxer interface as an alternative to
media::FFmpegDemuxer and media::MediaUrlDemuxer. The demuxer is called IPC one
since it uses inter-process communication to call platform API from GPU process
while providing the renderer process with decoded streams.

## Renderer process part

The main class that the renderer process instantiates is `media::IPCDemuxer`
from `platform_media/ipc_demuxer/renderer/ipc_demuxer.h`. Its instance is
created in WebMediaPlayerImpl after the code detects or sniffs that the source
is MP4 container and that `--enable-ipc-demuxer` is present in the command line
options, see the code inside `VIVALDI_USE_SYSTEM_MEDIA_DEMUXER` preprocessor
define in
`chromium/third_party/blink/renderer/platform/media/web_media_player_impl.cc`.
The class is not used for any other container types.

The implementation then proceeds to
instantiate `media::IPCMediaPipelineHost` that in turn establishes a Mojo
connection to the GPU process and implements a Mojo service that the GPU process
uses to get media data. If the initialization of the decoding fails, the
implementation falls back to demuxers in Chromium.

As the GPU process both demuxes the media container and decodes it, to integrate
with the decoding pipeline in Chromium in the renderer process the code uses a
pass through decoders. Those implements `media::AudioDecoder` and
`media::VideoDecoder` interfaces to simply copy or pass-through data the decoder
instances receive from GPU process. See
platform_media/ipc_demuxer/renderer/pass_through_decoder.cc for details.

The initialization of Mojo connection to the GPU process is done in
media::IPCFactory. The factory instance is created during initialization of the
renderer process.

## GPU process part

The Mojo service in the GPU process is implemented by `media::IPCMediaPipeline`
class. The platform-specific demuxing and decoding is done in
`media::PlatformMediaPipeline` subclasses.

Both Mac and Windows implementations use a separated thread to run OS demuxing
and decoding while the IPC runs on the thread that runs Mojo IPC for the GPU
channel. On both Mac and Windows this thread is the main process thread. While
Windows code does depend on it, Mac implementation assumes that the IPC thread
is the main thread to be able to post to it messages using OS-specific queue API
in addition for calling Chromium's `PostTasK()`. This caused a complication for
the unit tests in
`platform_media/test/platform_media_pipeline_integration_test.cc` as the main
thread for the test cannot be used to post messages both with Chromium and OS
API. A workaround is `DispatchQueueRunner` helper class from
`platform_media/test/ipc_pipeline_test_setup_mac.mm`

Another complication is that during GPU pipeline initialization our changes to
`gpu::GpuChannel` (see `VivaldiCreateMediaPipelineFactory` method) cannot
directly call into `media::IPCMediaPipeline` due to linking restrictions. This
is worked around using a function pointer which is initialized in
`GpuServiceImpl::GpuServiceImpl` constructor.

To simplify implementation of IPC protocol helper classes `IPCDataSource` and
`IPCDecodingBuffer` are used extensively. They wrap the shared memory handles
for the media source and decoding buffers into move-only classes. Instances of
those classes are moved around during various stages of decoding.
