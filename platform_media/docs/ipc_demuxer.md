# IPCDemuxer

## [**Back to top**](../README.md)

Location : vivaldi/platform_media/renderer/decoders/ipc_demuxer.h

In WebMediaPlayerImpl::StartPipeline() in Chromium either an IPCDemuxer, an FFmpegDemuxer, a [**CoreAudioDemuxer**][1] or a ChunkDemuxer is created. The ChunkDemuxer is used if the “load type” is Media Source Extension, the others are used for everything else. The selection of the right Demuxer is a part of the Media Patch Set and in StartPipeline() all mp4 like files are routed to the IPCDemuxer.

The IPCDemuxer then uses many classes inside platform_media to send the encoded bytes over to the Gpu process, decoding using the platform SDK and sending the decoded bytes back. The majority of the files in platform_media are concerned with achieving this task. This code is referred to as the [**Platform Media Pipeline**][2].

[1]: core_audio_demuxer.md
[2]: gpu_pipeline.md