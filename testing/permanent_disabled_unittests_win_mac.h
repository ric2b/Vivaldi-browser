// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows and Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// never stop loading; disabled widevine pepper support(?)
//DISABLE(PepperContentSettingsSpecialCasesPluginsBlockedTest, WidevineCdm)

#if defined(OFFICIAL_BUILD)
DISABLE_ALL(End2EndTest)

DISABLE_MULTI(VideoEncoderTest, CanBeDestroyedBeforeVEAIsCreated)
DISABLE_MULTI(VideoEncoderTest, EncodesVariedFrameSizes)
DISABLE_MULTI(VideoEncoderTest, GeneratesKeyFrameThenOnlyDeltaFrames)
#endif  // OFFICIAL_BUILD

// This test assumes proprietary media support built into FFmpeg, which is not
// the case when USE_SYSTEM_PROPRIETARY_CODECS is defined.
//DISABLE(MediaFileCheckerTest, MP3)
// This test assumes proprietary media support built into FFmpeg, which is not
// the case when USE_SYSTEM_PROPRIETARY_CODECS is defined.
//DISABLE(AudioFileReaderTest, MP3)
//DISABLE(AudioFileReaderTest, CorruptMP3)
//DISABLE(AudioFileReaderTest, MidStreamConfigChangesFail)
// USE_SYSTEM_PROPRIETARY_CODECS requires a different test setup, see
// PlatformMediaPipelineIntegrationTest.
//DISABLE(FFmpegDemuxerTest, NoID3TagData)
//DISABLE(FFmpegDemuxerTest, Mp3WithVideoStreamID3TagData)
DISABLE(FFmpegDemuxerTest, MP4_ZeroStszEntry)
//DISABLE_MULTI(Mp3SeekFFmpegDemuxerTest, TestFastSeek)
DISABLE(FFmpegDemuxerTest, IsValidAnnexB)
DISABLE(FFmpegDemuxerTest, Rotate_Metadata_0)
DISABLE(FFmpegDemuxerTest, Rotate_Metadata_90)
DISABLE(FFmpegDemuxerTest, Rotate_Metadata_180)
DISABLE(FFmpegDemuxerTest, Rotate_Metadata_270)
DISABLE(FFmpegDemuxerTest, NaturalSizeWithoutPASP)
DISABLE(FFmpegDemuxerTest, NaturalSizeWithPASP)
//DISABLE(FFmpegDemuxerTest, HEVC_in_MP4_container)
//DISABLE(FFmpegDemuxerTest, Read_AC3_Audio)
//DISABLE(FFmpegDemuxerTest, Read_EAC3_Audio)
//DISABLE(FFmpegDemuxerTest, Read_Flac_192kHz_Mp4)
//DISABLE(FFmpegDemuxerTest, Read_Flac_Mp4)
DISABLE(FFmpegDemuxerTest, Read_Mp4_Media_Track_Info)
DISABLE(FFmpegDemuxerTest, Read_Mp4_Multiple_Tracks)
DISABLE(FFmpegDemuxerTest, Read_Mp4_Crbug657437)
DISABLE(FFmpegDemuxerTest, Read_AudioNegativeStartTimeAndOpusDiscardH264Mp4_Sync)
// USE_SYSTEM_PROPRIETARY_CODECS requires a different test setup, see
// PlatformMediaPipelineIntegrationTest.
//DISABLE(FFmpeg/AudioDecoderTest, Decode/0)
DISABLE(FFmpeg/AudioDecoderTest, Decode/1)
DISABLE(FFmpeg/AudioDecoderTest, Decode/2)
//DISABLE(FFmpeg/AudioDecoderTest, Initialize/0)
DISABLE(FFmpeg/AudioDecoderTest, Initialize/1)
DISABLE(FFmpeg/AudioDecoderTest, Initialize/2)
//DISABLE(FFmpeg/AudioDecoderTest, NoTimestamp/0)
DISABLE(FFmpeg/AudioDecoderTest, NoTimestamp/1)
DISABLE(FFmpeg/AudioDecoderTest, NoTimestamp/2)
//DISABLE(FFmpeg/AudioDecoderTest, ProduceAudioSamples/0)
DISABLE(FFmpeg/AudioDecoderTest, ProduceAudioSamples/1)
DISABLE(FFmpeg/AudioDecoderTest, ProduceAudioSamples/2)
//DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterDecode/0)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterDecode/1)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterDecode/2)
//DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterInitialize/0)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterInitialize/1)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterInitialize/2)
//DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterReset/0)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterReset/1)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterReset/2)
//DISABLE(FFmpeg/AudioDecoderTest, Reset/0)
DISABLE(FFmpeg/AudioDecoderTest, Reset/1)
DISABLE(FFmpeg/AudioDecoderTest, Reset/2)
// Previously ifdeffed out
DISABLE(CBRSeek_HasTOC/Mp3FastSeekIntegrationTest, FastSeekAccuracy_MP3/0)
DISABLE(CBRSeeks_NoTOC/Mp3FastSeekIntegrationTest, FastSeekAccuracy_MP3/0)
DISABLE(VBRSeeks_HasTOC/Mp3FastSeekIntegrationTest, FastSeekAccuracy_MP3/0)
DISABLE(VBRSeeks_NoTOC/Mp3FastSeekIntegrationTest, FastSeekAccuracy_MP3/0)
DISABLE_MULTI(BasicPlaybackTest, PlayToEnd)
//DISABLE_MULTI(BasicMSEPlaybackTest, PlayToEnd)

// Maybe remove these? They have been patched in but ifdeffed out?
//DISABLE(PipelineIntegrationTest, MediaSource_MP3_a440)
//DISABLE(PipelineIntegrationTest, MediaSource_ConfigChange_MP4_Resampled)

// Wrong hash
DISABLE(PipelineIntegrationTest, BasicPlaybackHashed_MP3)
// PIPELINE_ERROR_DECODE
DISABLE(PipelineIntegrationTest, MidStreamConfigChangesFail)

// Enable these tests when TURN_ON_MSE_VIVALDI is turned on
 //DISABLE(PipelineIntegrationTest, MediaSource_MP3_Icecast)
// //DISABLE(PipelineIntegrationTest, MediaSource_ADTS)
// //DISABLE(PipelineIntegrationTest, MediaSource_ADTS_TimestampOffset)
 //DISABLE(PipelineIntegrationTest, MediaSource_MP3_TimestampOffset)

// Wrong hash - related with TURN_ON_MSE_VIVALDI
//DISABLE(PipelineIntegrationTest, MediaSource_MP3)

// Passes on Mac when TURN_ON_MSE_VIVALDI is turned on
//DISABLE(PipelineIntegrationTest, MediaSource_MP3_Seek)

// VB-33521 FLAC in MP4 is not working on Win and Mac
DISABLE(PipelineIntegrationTest, BasicPlaybackHashed_FlacInMp4)
//DISABLE(AudioVideoMetadataExtractorTest, AudioFLACInMp4)
DISABLE(File/MediaTest, AudioBearFlac192kHzMp4/0)
DISABLE(File/MediaTest, AudioBearFlacMp4/0)
DISABLE(Http/MediaTest, AudioBearFlac192kHzMp4/0)
DISABLE(Http/MediaTest, AudioBearFlacMp4/0)

// Failing media tests since proprietary media code was imported
//DISABLE(AudioVideoMetadataExtractorTest, AndroidRotatedMP4Video)
//DISABLE(AudioVideoMetadataExtractorTest, AudioMP3)
//DISABLE(MediaGalleriesPlatformAppBrowserTest, GetMetadata)
DISABLE_MULTI(MediaTest, VideoBearMovPcmS16be)
DISABLE_MULTI(MediaTest, VideoBearMovPcmS24be)
//DISABLE_MULTI(MediaTest, VideoBearMp4)
DISABLE_MULTI(MediaTest, VideoBearSilentMp4)
//DISABLE(MediaTest, VideoBearRotated0)
//DISABLE(MediaTest, VideoBearRotated180)
DISABLE(MediaColorTest, Yuv420pHighBitDepth)
DISABLE(MediaColorTest, Yuv422pH264)
DISABLE(MediaColorTest, Yuv444pH264)
DISABLE(MediaColorTest, Yuvj420pH264)
DISABLE(MediaColorTest, Yuv420pH264)
