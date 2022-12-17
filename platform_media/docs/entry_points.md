# Entry Points

## [**Back to top**](../README.md)

## High Level Overview

![Entry Points](images/entry_points.svg)

## Entry Point Classes

* [**IPCDemuxer**][1] (Demuxer, Cross platform)
* [**ATAudioDecoder**][2] (AudioDecoder, Mac)
* [**VivVideoDecoder**][2] (VideoDecoder, Mac)
* [**WMFAudioDecoder**][2] (AudioDecoder, Windows)
* [**WMFVideoDecoder**][2] (VideoDecoder, Windows)

## Implementing Chromium APIs

There are three APIs that are implemented in platform_media that fulfill decoding functionality: Demuxer, AudioDecoder and VideoDecoder.

These are really two different models, with the Demuxer there is a DataSource available that can be read from, so the actual decoders pull data from it, and the Demuxer pulls decoded media from the decoders. This is clearly a pull model. Throttling is achieved by not pulling.

However, the AudioDecoder and VideoDecoder follow a push model, here the caller pushes encoded buffers to the decoders and the decoders call back when decoded data is ready. Here throttling is done by not pushing more data. These are used by the ChunkDemuxer.

[1]: ipc_demuxer.md
[2]: renderer_decoders.md