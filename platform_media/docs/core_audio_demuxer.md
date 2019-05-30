# CoreAudioDemuxer

## [**Back to top**](../README.md)

Location : vivaldi/platform_media/renderer/decoders/mac/core_audio_demuxer.h

The CoreAudioDemuxerStream (which is used by CoreAudioDemuxer) uses the Apple AudioToolbox [AudioQueueOfflineRender][1] API

The [CoreAudioDemuxer][2] is really a Mac audio decoder running in the renderer process. It is only used for non-MSE codepaths and only if the [**Platform Media Pipeline**][3] did not handle the audio, see [WebMediaPlayerImpl::StartPipeline()][4] in Chromium. It will even then only be used if the *CoreAudioDemuxer::IsSupported* returns true. The mimetypes *IsSupported* accepts are defined in kSupportedMimeTypes in the core_audio_demuxer.cc file.

[1]: https://developer.apple.com/documentation/audiotoolbox/1502237-audioqueueofflinerender?language=objc
[2]: ../renderer/decoders/mac/core_audio_demuxer.h
[3]: gpu_pipeline.md
[4]: ../../chromium/media/blink/webmediaplayer_impl.cc