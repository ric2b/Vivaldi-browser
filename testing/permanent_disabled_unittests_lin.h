// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Linux by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)


  // Disabled in Google Chrome official builds
  DISABLE(FirstRunMasterPrefsImportBookmarksFile, ImportBookmarksFile)

  // Disabled in Google Chrome official builds
  DISABLE(FirstRunMasterPrefsImportDefault, ImportDefault)

  // Disabled in Google Chrome official builds
  DISABLE(FirstRunMasterPrefsImportNothing, ImportNothingAndShowNewTabPage)

  // Disabled in Google Chrome official builds
  DISABLE_MULTI(FirstRunMasterPrefsWithTrackedPreferences,
                TrackedPreferencesSurviveFirstRun)

  // In Vivaldi this will always break since the relevant code has been modified
  DISABLE(HelpPageWebUITest, testUpdateState)
  DISABLE(HelpPageWebUITest, testUpdateStateIcon)

  // Disabled video tests on linux, until the tests can be fixed
  DISABLE(MediaSourceTest, Playback_AudioOnly_AAC_ADTS)
  DISABLE(MediaSourceTest, Playback_Video_MP4_Audio_WEBM)

  // Disabled because the Widevine lib is not included in the package build,
  // as it crashes vivaldi
  DISABLE(PepperContentSettingsSpecialCasesPluginsBlockedTest, WidevineCdm)

  // Does not work for vivaldi
  DISABLE(ProfileSigninConfirmationHelperBrowserTest, HasNotBeenShutdown)
  DISABLE(ProfileSigninConfirmationHelperBrowserTest, HasNoSyncedExtensions)

  // Does not work for vivaldi
  DISABLE(StartupBrowserCreatorFirstRunTest,
          FirstRunTabsContainNTPSyncPromoAllowed)
  DISABLE(StartupBrowserCreatorFirstRunTest,
          FirstRunTabsContainNTPSyncPromoForbidden)
  DISABLE(StartupBrowserCreatorFirstRunTest, FirstRunTabsContainSyncPromo)
  DISABLE(StartupBrowserCreatorFirstRunTest, FirstRunTabsPromoAllowed)
  DISABLE(StartupBrowserCreatorFirstRunTest, FirstRunTabsSyncPromoForbidden)
  DISABLE(StartupBrowserCreatorFirstRunTest,
          RestoreOnStartupURLsPolicySpecified)
  DISABLE(StartupBrowserCreatorFirstRunTest, SyncPromoAllowed)
  DISABLE(StartupBrowserCreatorFirstRunTest, SyncPromoForbidden)

  // Proprietary media codec tests
  DISABLE(AudioFileReaderTest, AAC)
  DISABLE(AudioFileReaderTest, CorruptMP3)
  DISABLE(AudioFileReaderTest, MP3)
  DISABLE_MULTI(AudioDecoderTest, Decode)
  DISABLE_MULTI(AudioDecoderTest, Initialize)
  DISABLE_MULTI(AudioDecoderTest, NoTimestamp)
  DISABLE_MULTI(AudioDecoderTest, ProduceAudioSamples)
  DISABLE_MULTI(AudioDecoderTest, Reset)
  DISABLE_MULTI(AudioDecoderTest, Reinitialize_AfterInitialize)
  DISABLE_MULTI(AudioDecoderTest, Reinitialize_AfterDecode)
  DISABLE_MULTI(AudioDecoderTest, Reinitialize_AfterReset)
  DISABLE(FFmpegDemuxerTest, HEVC_in_MP4_container)
  DISABLE(FFmpegDemuxerTest, IsValidAnnexB)
  DISABLE(FFmpegDemuxerTest, Mp3WithVideoStreamID3TagData)
  DISABLE(FFmpegDemuxerTest, NaturalSizeWithPASP)
  DISABLE(FFmpegDemuxerTest, NaturalSizeWithoutPASP)
  DISABLE(FFmpegDemuxerTest, Read_AC3_Audio)
  DISABLE(FFmpegDemuxerTest, Read_EAC3_Audio)
  DISABLE(FFmpegDemuxerTest, Rotate_Metadata_0)
  DISABLE(FFmpegDemuxerTest, Rotate_Metadata_180)
  DISABLE(FFmpegDemuxerTest, Rotate_Metadata_270)
  DISABLE(FFmpegDemuxerTest, Rotate_Metadata_90)
  DISABLE(FFmpegDemuxerTest, Read_Mp4_Media_Track_Info)
  DISABLE(FFmpegDemuxerTest, Read_Mp4_Multiple_Tracks)
  DISABLE(FFmpegDemuxerTest, MP4_ZeroStszEntry)
  DISABLE(FFmpegDemuxerTest, NoID3TagData)
  DISABLE(MediaFileCheckerTest, MP3)
  DISABLE_MULTI(Mp3SeekFFmpegDemuxerTest, TestFastSeek)
  DISABLE(AudioFileReaderTest, MidStreamConfigChangesFail)
  DISABLE(EncryptedMediaSupportedTypesClearKeyTest, Audio_MP4)
  DISABLE(EncryptedMediaSupportedTypesClearKeyTest, Video_MP4)
  DISABLE(EncryptedMediaSupportedTypesExternalClearKeyTest, Audio_MP4)
  DISABLE(EncryptedMediaSupportedTypesExternalClearKeyTest, Video_MP4)
  DISABLE_MULTI(EncryptedMediaTest, Playback_ClearVideo_WEBM_EncryptedAudio_MP4)
  DISABLE_MULTI(EncryptedMediaTest, Playback_EncryptedVideo_MP4_ClearAudio_WEBM)
  DISABLE_MULTI(EncryptedMediaTest, Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4)
  DISABLE_MULTI(EncryptedMediaTest, Playback_VideoOnly_MP4)
  DISABLE_MULTI(EncryptedMediaTest, Playback_AudioOnly_MP4)
  DISABLE_ALL(MediaCanPlayTypeTest)
  DISABLE(MediaColorTest, Yuv420pH264)
