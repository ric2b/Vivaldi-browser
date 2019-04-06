#include "platform_pipeline_test_base.h"

#include <limits>
#include <queue>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "media/base/test_data_util.h"

#include "media/base/limits.h"
#if defined(OS_MACOSX)
#include "platform_media/renderer/decoders/mac/at_audio_decoder.h"
#elif defined(OS_WIN)
#include "platform_media/renderer/decoders/win/wmf_audio_decoder.h"
#include "platform_media/renderer/decoders/win/wmf_video_decoder.h"
#endif
#include "media/filters/gpu_video_decoder.h"
#include "platform_media/common/platform_mime_util.h"
#include "platform_media/renderer/decoders/ipc_demuxer.h"
#include "platform_media/renderer/decoders/pass_through_audio_decoder.h"
#include "platform_media/renderer/decoders/pass_through_video_decoder.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_decode_accelerator.h"


#include "platform_media/test/test_pipeline_host.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;

namespace media {

namespace {

VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles() {
  VideoDecodeAccelerator::SupportedProfile profile_prototype;
  profile_prototype.max_resolution.SetSize(std::numeric_limits<int>::max(),
                                           std::numeric_limits<int>::max());

  VideoDecodeAccelerator::SupportedProfiles all_profiles;
  for (int i = VIDEO_CODEC_PROFILE_MIN + 1; i <= VIDEO_CODEC_PROFILE_MAX; ++i) {
    profile_prototype.profile = static_cast<VideoCodecProfile>(i);
    all_profiles.push_back(profile_prototype);
  }

  return all_profiles;
}

}  // namespace

PlatformPipelineTestBase::PlatformPipelineTestBase()
    : mock_video_accelerator_factories_(new MockGpuVideoAcceleratorFactories(nullptr))
{}
PlatformPipelineTestBase::~PlatformPipelineTestBase() {}

Demuxer * PlatformPipelineTestBase::CreatePlatformDemuxer(
    std::unique_ptr<DataSource> & data_source,
    base::test::ScopedTaskEnvironment & task_environment_,
    MediaLog* media_log) {
  const std::string content_type;
  const GURL url("file://" + filepath_.AsUTF8Unsafe());
  if (IPCDemuxer::CanPlayType(content_type, url)) {
    std::unique_ptr<IPCMediaPipelineHost> pipeline_host(
          new TestPipelineHost(data_source.get()));
    return new IPCDemuxer(
                     task_environment_.GetMainThreadTaskRunner(), data_source.get(),
                     std::move(pipeline_host), content_type, url,
                     media_log);
  }

  return nullptr;
}

void PlatformPipelineTestBase::AppendPlatformAudioDecoders(
        std::vector<std::unique_ptr<AudioDecoder>> & audio_decoders,
        const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner)
{
  const std::string content_type;
  const GURL url("file://" + filepath_.AsUTF8Unsafe());
  if (IPCDemuxer::CanPlayType(content_type, url)) {
    audio_decoders.push_back(
        std::make_unique<PassThroughAudioDecoder>(media_task_runner));
  }

#if defined(OS_MACOSX)
  audio_decoders.push_back(std::make_unique<ATAudioDecoder>(media_task_runner));
#elif defined(OS_WIN)
  audio_decoders.push_back(
      std::make_unique<WMFAudioDecoder>(media_task_runner));
#endif
}

void PlatformPipelineTestBase::AppendPlatformVideoDecoders(
        std::vector<std::unique_ptr<VideoDecoder>> & video_decoders,
        const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
        MediaLog* media_log)
{
  const std::string content_type;
  const GURL url("file://" + filepath_.AsUTF8Unsafe());
  if (IPCDemuxer::CanPlayType(content_type, url)) {
    video_decoders.push_back(
        std::make_unique<PassThroughVideoDecoder>(media_task_runner));
  }

#if defined(OS_WIN)
  video_decoders.push_back(
      std::make_unique<WMFVideoDecoder>(media_task_runner));
#endif

  video_decoders.push_back(std::make_unique<GpuVideoDecoder>(
      mock_video_accelerator_factories_.get(), RequestOverlayInfoCB(),
      gfx::ColorSpace(), media_log));

  VideoDecodeAccelerator::Capabilities capabilities;
  capabilities.supported_profiles = GetSupportedProfiles();

  EXPECT_CALL(*mock_video_accelerator_factories_, GetTaskRunner())
      .WillRepeatedly(testing::Return(media_task_runner));
  EXPECT_CALL(*mock_video_accelerator_factories_,
              GetVideoDecodeAcceleratorCapabilities())
      .WillRepeatedly(testing::Return(capabilities));
  EXPECT_CALL(*mock_video_accelerator_factories_, WaitSyncToken(_))
      .Times(testing::AnyNumber());
}
}
