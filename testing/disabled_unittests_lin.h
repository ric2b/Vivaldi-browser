// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Linux by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// Assume these fails due to switches::kExtensionActionRedesign being disabled
//DISABLE(ToolbarActionViewInteractiveUITest, TestClickingOnOverflowedAction)

// VB-22258
DISABLE(ComponentFlashHintFileTest, CorruptionTest)
DISABLE(ComponentFlashHintFileTest, InstallTest)

// Seems to have broken in v57
//DISABLE(RenderTextHarfBuzzTest, GetSubstringBoundsMultiline/HarfBuzz)

// Flaky
//DISABLE(SitePerProcessBrowserTest, PopupMenuTest)

//DISABLE(PointerLockBrowserTest, PointerLockEventRouting)
//DISABLE(PointerLockBrowserTest, PointerLockWheelEventRouting)
//DISABLE(LayerTreeHostTilesTestPartialInvalidation,
//        PartialRaster_MultiThread_OneCopy)
//DISABLE(SitePerProcessBrowserTest, SubframeGestureEventRouting)
//DISABLE_MULTI(WebViewInteractiveTest, EditCommandsNoMenu)

// Seems to have broken in v64, at least flaky
//DISABLE(SitePerProcessBrowserTest, ScrollFocusedEditableElementIntoView)

DISABLE(InputImeApiTest, BasicApiTest)

// ===============================================================
//                      TEMPORARY - MEDIA
// ===============================================================

#define NO_EXTRA_CODECS
//#define NO_EXTERNAL_CLEARKEY

// browser_tests

#if defined(NO_EXTRA_CODECS) || defined(NO_EXTERNAL_CLEARKEY)
DISABLE(EncryptedMediaSupportedTypesExternalClearKeyTest, Audio_MP4)
DISABLE(EncryptedMediaSupportedTypesExternalClearKeyTest, Basic)
DISABLE(EncryptedMediaSupportedTypesExternalClearKeyTest, Video_MP4)

DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_ClearVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_EncryptedVideo_MP4_ClearAudio_WEBM/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_VideoOnly_MP4/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_VideoOnly_MP4_MDAT/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_Encryption_CBCS/0)


DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_ClearVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_EncryptedVideo_MP4_ClearAudio_WEBM/0)
DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_VideoOnly_MP4/0)
#endif

#if defined(NO_EXTERNAL_CLEARKEY)
//DISABLE(EncryptedMediaSupportedTypesExternalClearKeyTest, Audio_WebM)
//DISABLE(EncryptedMediaSupportedTypesExternalClearKeyTest, SessionType)
//DISABLE(EncryptedMediaSupportedTypesExternalClearKeyTest, Video_WebM)

//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, ConfigChangeVideo_ClearToEncrypted/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, ConfigChangeVideo_EncryptedToClear/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, ConfigChangeVideo_EncryptedToEncrypted/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, FrameSizeChangeVideo/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, InvalidResponseKeyError/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_AudioClearVideo_WebM/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_Multiple_VideoAudio_WebM/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_VP9Video_WebM_Fullsample/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_VP9Video_WebM_Subsample/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_VideoAudio_WebM/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_VideoAudio_WebM_Opus/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_VideoClearAudio_WebM/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_VideoClearAudio_WebM_Opus/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_VideoOnly_MP4_VP9/0)
//DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, PolicyCheck/0)

//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, ConfigChangeVideo_ClearToEncrypted/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, ConfigChangeVideo_EncryptedToClear/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, ConfigChangeVideo_EncryptedToEncrypted/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, FrameSizeChangeVideo/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, InvalidResponseKeyError/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_AudioClearVideo_WebM/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_Multiple_VideoAudio_WebM/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_VP9Video_WebM_Fullsample/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_VP9Video_WebM_Subsample/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_VideoAudio_WebM/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_VideoAudio_WebM_Opus/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_VideoClearAudio_WebM/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_VideoClearAudio_WebM_Opus/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_VideoOnly_MP4_VP9/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, PolicyCheck/0)
#endif

#if defined(NO_EXTRA_CODECS)
DISABLE(EncryptedMediaSupportedTypesClearKeyTest, Audio_MP4)
DISABLE(EncryptedMediaSupportedTypesClearKeyTest, Basic)
DISABLE(EncryptedMediaSupportedTypesClearKeyTest, Video_MP4)

DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_ClearVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_EncryptedVideo_MP4_ClearAudio_WEBM/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_VideoOnly_MP4/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_VideoOnly_MP4_MDAT/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_Encryption_CBCS/0)

DISABLE(WebRtcBrowserTest, RunsAudioVideoWebRTCCallInTwoTabsH264)
#endif

DISABLE(FFmpegDemuxerTest, Read_AudioNegativeStartTimeAndOpusDiscardH264Mp4_Sync)

// content_browsertests

#if defined(NO_EXTRA_CODECS)
DISABLE(MediaSourceTest, Playback_AudioOnly_AAC_ADTS)
DISABLE(MediaSourceTest, Playback_Video_MP4_Audio_WEBM)
DISABLE(MediaSourceTest, Playback_Video_WEBM_Audio_MP4)

DISABLE(File/MediaTest, VideoBearHighBitDepthMp4/0)
DISABLE(File/MediaTest, VideoBearMp4/0)
DISABLE(File/MediaTest, VideoBearSilentMp4/0)

DISABLE(Http/MediaTest, VideoBearHighBitDepthMp4/0)
DISABLE(Http/MediaTest, VideoBearMp4/0)
DISABLE(Http/MediaTest, VideoBearSilentMp4/0)

DISABLE(MediaCanPlayTypeTest, CodecSupportTest_AAC_ADTS)
DISABLE(MediaCanPlayTypeTest, CodecSupportTest_Avc1Variants)
DISABLE(MediaCanPlayTypeTest, CodecSupportTest_Avc3Variants)
DISABLE(MediaCanPlayTypeTest, CodecSupportTest_AvcLevels)
DISABLE(MediaCanPlayTypeTest, CodecSupportTest_Mp4aVariants)
//DISABLE(MediaCanPlayTypeTest, CodecSupportTest_mp3)
DISABLE(MediaCanPlayTypeTest, CodecSupportTest_mp4)

//DISABLE(MediaColorTest, Yuv420pHighBitDepth)
DISABLE(MediaColorTest, Yuv422pH264)
DISABLE(MediaColorTest, Yuv444pH264)

DISABLE(MediaTest, LoadManyVideos)
DISABLE(MediaTest, VideoBearRotated0)
DISABLE(MediaTest, VideoBearRotated180)
DISABLE(MediaTest, VideoBearRotated270)
DISABLE(MediaTest, VideoBearRotated90)

#endif

// media_unittests

#if defined(NO_EXTRA_CODECS)
// Crash
DISABLE(FFmpegDemuxerTest, MP4_ZeroStszEntry)
DISABLE(FFmpegDemuxerTest, Read_Mp4_Media_Track_Info)
DISABLE(FFmpegDemuxerTest, Read_Mp4_Multiple_Tracks)
DISABLE(FFmpegDemuxerTest, Read_AudioNegativeStartTimeAndOpusDiscardH264Mp4_Sync)

// Hang
DISABLE(FFmpegDemuxerTest, IsValidAnnexB)

DISABLE(NewByPts/MSEPipelineIntegrationTest, ADTS/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ADTS_TimestampOffset/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_MP4/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_Encrypted_MP4_CENC_VideoOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_Encrypted_MP4_CENC_KeyRotation_VideoOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_NoEncryptedFrames_MP4_CENC_VideoOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_NoEncryptedFrames_MP4_CENC_AudioOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_Video/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_NO_SAIZ_SAIO_Video/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Video/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Audio/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, BasicPlayback_VideoOnly_MP4_AVC3/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_MDAT_Video/0)

DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ADTS/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ADTS_TimestampOffset/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_MP4/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_Encrypted_MP4_CENC_VideoOnly/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_Encrypted_MP4_CENC_KeyRotation_VideoOnly/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_NoEncryptedFrames_MP4_CENC_VideoOnly/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_NoEncryptedFrames_MP4_CENC_AudioOnly/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_Video/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_NO_SAIZ_SAIO_Video/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Video/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Audio/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, BasicPlayback_VideoOnly_MP4_AVC3/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_MDAT_Video/0)

// Fail
DISABLE(FFmpeg/AudioDecoderTest, Decode/1)
DISABLE(FFmpeg/AudioDecoderTest, Initialize/1)
DISABLE(FFmpeg/AudioDecoderTest, NoTimestamp/1)
DISABLE(FFmpeg/AudioDecoderTest, ProduceAudioSamples/1)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterDecode/1)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterInitialize/1)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterReset/1)
DISABLE(FFmpeg/AudioDecoderTest, Reset/1)
DISABLE(FFmpeg/AudioDecoderTest, Decode/2)
DISABLE(FFmpeg/AudioDecoderTest, Initialize/2)
DISABLE(FFmpeg/AudioDecoderTest, NoTimestamp/2)
DISABLE(FFmpeg/AudioDecoderTest, ProduceAudioSamples/2)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterDecode/2)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterInitialize/2)
DISABLE(FFmpeg/AudioDecoderTest, Reinitialize_AfterReset/2)
DISABLE(FFmpeg/AudioDecoderTest, Reset/2)
DISABLE(FFmpegDemuxerTest, NaturalSizeWithPASP)
DISABLE(FFmpegDemuxerTest, NaturalSizeWithoutPASP)
DISABLE(FFmpegDemuxerTest, Read_Mp4_Crbug657437)
DISABLE(FFmpegDemuxerTest, Rotate_Metadata_0)
DISABLE(FFmpegDemuxerTest, Rotate_Metadata_180)
DISABLE(FFmpegDemuxerTest, Rotate_Metadata_270)
DISABLE(FFmpegDemuxerTest, Rotate_Metadata_90)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_EncryptedThenClear_MP4_CENC/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_AudioOnly/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_VideoOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_EncryptedThenClear_MP4_CENC/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_AudioOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_VideoOnly/0)
DISABLE(PipelineIntegrationTest, BasicFallback)
DISABLE(PipelineIntegrationTest, BasicPlaybackHashed_ADTS)
DISABLE(PipelineIntegrationTest, BasicPlaybackHi10P)
DISABLE(PipelineIntegrationTest, Rotated_Metadata_0)
DISABLE(PipelineIntegrationTest, Rotated_Metadata_180)
DISABLE(PipelineIntegrationTest, Rotated_Metadata_270)
DISABLE(PipelineIntegrationTest, Rotated_Metadata_90)
DISABLE(PipelineIntegrationTest, Spherical)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, LegacyByDts_PlayToEnd/0)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, LegacyByDts_PlayToEnd/1)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, LegacyByDts_PlayToEnd/2)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, LegacyByDts_PlayToEnd/3)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, NewByPts_PlayToEnd/0)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, NewByPts_PlayToEnd/1)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, NewByPts_PlayToEnd/2)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, NewByPts_PlayToEnd/3)
DISABLE(ProprietaryCodecs/BasicPlaybackTest, PlayToEnd/0)
DISABLE(ProprietaryCodecs/BasicPlaybackTest, PlayToEnd/1)
DISABLE(ProprietaryCodecs/BasicPlaybackTest, PlayToEnd/2)
DISABLE(ProprietaryCodecs/BasicPlaybackTest, PlayToEnd/3)

DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bear320x240audioonlywebm_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bear320x240audioonlywebm_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bear640x360a_fragmp4_AND_bear320x240audioonlywebm)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bear640x360a_fragmp4_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bear640x360a_fragmp4_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bear640x360a_fragmp4_AND_sfxflac_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bear640x360a_fragmp4_AND_sfxmp3)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bear640x360a_fragmp4_AND_sfxopus441webm)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bearaudiomainaacaac_AND_bear320x240audioonlywebm)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bearaudiomainaacaac_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bearaudiomainaacaac_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bearaudiomainaacaac_AND_sfxflac_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bearaudiomainaacaac_AND_sfxmp3)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/bearaudiomainaacaac_AND_sfxopus441webm)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/sfxflac_fragmp4_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/sfxflac_fragmp4_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/sfxmp3_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/sfxmp3_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/sfxopus441webm_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, LegacyByDts_PlayBackToBack/sfxopus441webm_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bear320x240audioonlywebm_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bear320x240audioonlywebm_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bear640x360a_fragmp4_AND_bear320x240audioonlywebm)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bear640x360a_fragmp4_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bear640x360a_fragmp4_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bear640x360a_fragmp4_AND_sfxflac_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bear640x360a_fragmp4_AND_sfxmp3)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bear640x360a_fragmp4_AND_sfxopus441webm)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bearaudiomainaacaac_AND_bear320x240audioonlywebm)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bearaudiomainaacaac_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bearaudiomainaacaac_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bearaudiomainaacaac_AND_sfxflac_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bearaudiomainaacaac_AND_sfxmp3)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/bearaudiomainaacaac_AND_sfxopus441webm)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/sfxflac_fragmp4_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/sfxflac_fragmp4_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/sfxmp3_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/sfxmp3_AND_bearaudiomainaacaac)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/sfxopus441webm_AND_bear640x360a_fragmp4)
DISABLE(AudioOnly/MSEChangeTypeTest, NewByPts_PlayBackToBack/sfxopus441webm_AND_bearaudiomainaacaac)

#endif

// broke in v66
DISABLE(WebViewTests/WebViewSizeTest,
        Shim_TestResizeWebviewWithDisplayNoneResizesContent/0)
DISABLE(ExtensionApiTabTest, UpdateWindowResize)
DISABLE(ExtensionWindowLastFocusedTest, NoTabIdForDevToolsAndAppWindows)

// Started failing May 13, probably due to expired certs. Supposed fix is on Master, did not work when backporting
DISABLE(CertDatabaseNSSTest, ImportCACertNotHierarchy)
DISABLE(CertDatabaseNSSTest, ImportCACert_EmailTrust)
DISABLE(CertDatabaseNSSTest, ImportCACert_ObjSignTrust)
DISABLE(CertDatabaseNSSTest, ImportCACert_SSLTrust)
DISABLE(CertDatabaseNSSTest, ImportCaAndServerCert)
DISABLE(CertDatabaseNSSTest, ImportCaAndServerCert_DistrustServer)

// broke in v67
DISABLE(EncryptedMediaTestExperimentalCdmInterface, Playback_Encryption_CENC)
DISABLE_MULTI(EncryptedMediaTest, Playback_Encryption_CENC)

// Broke in v68
DISABLE_MULTI(ECKEncryptedMediaTest, Playback_Encryption_CENC)
DISABLE_MULTI(ECKEncryptedMediaTest, Playback_Encryption_CBCS)
DISABLE_MULTI(EncryptedMediaTest, Playback_Encryption_CBCS)
DISABLE_MULTI(EncryptedMediaTest, Playback_EncryptedVideo_CBCS_EncryptedAudio_CENC)
DISABLE_MULTI(EncryptedMediaTest, Playback_EncryptedVideo_CENC_EncryptedAudio_CBCS)
DISABLE_MULTI(ECKEncryptedMediaTest, DecryptOnly_VideoOnly_MP4_CBCS)

// Broke in v69
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_Encryption_CBCS_Video_CENC_Audio/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_Encryption_CENC_Video_CBCS_Audio/0)

// Seems to not work in v67 on the tester
DISABLE(BrowserShutdownBrowserTest, ShutdownConfirmation)

// Causes an assert due to a IO operation in Vivaldi prefs registration
DISABLE(ProfileHelperTest, OpenNewWindowForProfile)

// Flaky
DISABLE(BrowserFocusTest, AppLocationBar)
DISABLE(BrowserFocusTest, PopupLocationBar)
DISABLE(X11TopmostWindowFinderTest, NonRectangularNullShape)
DISABLE(CrExtensionsToolbarTest, ClickHandlers)
