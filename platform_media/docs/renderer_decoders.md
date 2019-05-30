# Decoders in the Renderer process

## [**Back to top**](../README.md)

The AudioDecoder and VideoDecoder APIs are Chromium APIs that are implemented to support the Chromium ChunkDemuxer which is used with Media Source Extension, a JavaScript API.

## Important Notes

**NOTE** : When Hardware Accelerated (HWA) Decoding is available the GpuVideoDecoder, which is a Chromium class, will do h264 decoding for MSE. However when HWA Decoding is not available on Windows we will fall back to WMFVideoDecoder.

**IMPORTANT NOTE** : There is NO implementation of VideoDecoder on Mac. Therefore there is NO fallback when HWA Decoding is unavailable.

## Platform Media Implementations

* WMFAudioDecoder (Windows) : AAC
* ATAudioDecoder (Mac) : AAC
* WMFVideoDecoder (Windows) : h.264

## High Level Design

The VideoDecoder and AudioDecoder APIs are push based and in addition the metadata has already been extracted and is therefore passed in *Initialize*. *Initialize* also passes in a callback that is used to notify the caller of each decoded buffer that is ready. The encoded buffer is passed in to *Decode* together with a callback that is called once the encoded buffer is decoded.

## **Not implemented** : IPCVideoDecoder

Patricia Aas started to implement a cross-platform IPCVideoDecoder as a solution for Mac not having a VideoDecoder implementation. This class was similar in how it used the [**Platform Media Pipeline**][1] to [**IPCAudioDecoder**][2] which is used for WebAudio.

However, the Mac Gpu pipeline expects to receive a container stream, and the container has already been stripped by the [MP4StreamParser][3] when the VideoDecoder receives its encoded buffers.

Also Chromium converts the stream to AnnexB format, and in doing so overwrites some bytes per frame.

To present a pulling type interface to the [**Platform Media Pipeline**][1] this solution also introduced a vector of encoded buffers and a class to read from them as if they were one long stream.

To be able to get more than one encoded buffer at a time, the *VideoDecoder::GetMaxDecodeRequests()* function was overridden, and in fact made dynamic based on experimental heuristics.

This was also intended to work with the GStreamer platform code, see [**Pipeline Implementations**][6], and there you will even find the start of an IPCAudioDecoder(Adapter) (the name was taken for the class used by WebAudio).

### Experimental work

**NOTE** : This is a very rough beginning, but it is probably useful if someone wants to look at this again. Saying it's 40% finished is being generous.

The code as it was left after Christmas 2016 is here (rebased to July 2017 but not tested since January 2016):

[Vivaldi patches - top two commits][4]

[Chromium patches - top three commits][5] The commit 'Very rough stuff' is just a series of hacks so that platform_media can try to reassemble the mp4 file again.

[1]: gpu_pipeline.md
[2]: ipc_audio_decoder.md
[3]: ../../chromium/media/formats/mp4/mp4_stream_parser.h
[4]: http://git.viv.int/?p=vivaldi.git;a=shortlog;h=refs/heads/patricia/ipc-video-decoder-1
[5]: http://git.viv.int/?p=chromium/src.git;a=shortlog;h=refs/heads/patricia/ipc-video-decoder-1
[6]: pipeline_impl.md