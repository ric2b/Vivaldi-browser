# IPCAudioDecoder

## [**Back to top**](../README.md)

Location : vivaldi/platform_media/renderer/decoders/ipc_audio_decoder.h

IPCAudioDecoder is the second user of the [**Platform Media Pipeline**][1], it uses it to decode audio used in the WebAudio JavaScript API.

In AudioFileReader::Open() in Chromium an IPCAudioDecoder is allocated and is then used to service this class when it is given a file that cannot be handled by ffmpeg. This delegation is a part of the Media Patch Set

[1]: gpu_pipeline.md