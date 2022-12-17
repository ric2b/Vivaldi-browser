#include "platform_pipeline_test_base.h"

#include <limits>
#include <queue>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/base/test_data_util.h"

#include "media/base/limits.h"
#if BUILDFLAG(IS_MAC)
#include "platform_media/renderer/decoders/mac/at_audio_decoder.h"
#elif BUILDFLAG(IS_WIN)
#include "platform_media/renderer/decoders/win/wmf_audio_decoder.h"
#include "platform_media/renderer/decoders/win/wmf_video_decoder.h"
#endif
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_decode_accelerator.h"

#include "platform_media/renderer/decoders/ipc_demuxer.h"
#include "platform_media/renderer/decoders/ipc_factory.h"
#include "platform_media/test/ipc_pipeline_test_setup.h"

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

// IPCDemuxer expects that the pipeline host is already initialized when
// Chromium calls its Initialize from Demuxer interface using StartIPC() call.
// This subclass overrides Initialize to call StartIPC() first as this provides
// a convinient place to perform an asynchronous init.
class TestIPCDemuxer : public IPCDemuxer {
 public:
  TestIPCDemuxer(DataSource* data_source,
                 scoped_refptr<base::SequencedTaskRunner> media_task_runner,
                 std::string mime_type,
                 MediaLog* media_log)
      : IPCDemuxer(std::move(media_task_runner), media_log),
        data_source_(data_source),
        mime_type_(std::move(mime_type)) {}

  void Initialize(DemuxerHost* host,
                  PipelineStatusCallback status_cb) override {
    DataSource* data_source = data_source_;
    data_source_ = nullptr;

    // Unretained() is safe as the callback can only be called before the parent
    // class destructor is called.
    StartIPC(
        data_source, std::move(mime_type_),
        base::BindOnce(&TestIPCDemuxer::OnHostInitialized,
                       base::Unretained(this), host, std::move(status_cb)));
  }

  void OnHostInitialized(DemuxerHost* host,
                         PipelineStatusCallback status_cb,
                         bool success) {
    if (!success) {
      std::move(status_cb).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
      return;
    }
    IPCDemuxer::Initialize(host, std::move(status_cb));
  }

 private:
  DataSource* data_source_ = nullptr;
  std::string mime_type_;
};

}  // namespace

PlatformPipelineTestBase::PlatformPipelineTestBase()
    : mock_video_accelerator_factories_(
          new MockGpuVideoAcceleratorFactories(nullptr)) {}

PlatformPipelineTestBase::~PlatformPipelineTestBase() {}

Demuxer* PlatformPipelineTestBase::CreatePlatformDemuxer(
    std::unique_ptr<DataSource>& data_source,
    base::test::TaskEnvironment& task_environment_,
    MediaLog* media_log) {
  const std::string content_type;
  const GURL url("file://" + filepath_.AsUTF8Unsafe());
  std::string adjusted_mime_type = IPCDemuxer::CanPlayType(content_type, url);
  if (!adjusted_mime_type.empty()) {
    return new TestIPCDemuxer(data_source.get(),
                              task_environment_.GetMainThreadTaskRunner(),
                              std::move(adjusted_mime_type), media_log);
  }
  return nullptr;
}

void PlatformPipelineTestBase::AppendPlatformAudioDecoders(
    std::vector<std::unique_ptr<AudioDecoder>>& audio_decoders,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner) {
#if BUILDFLAG(IS_MAC)
  audio_decoders.push_back(std::make_unique<ATAudioDecoder>(media_task_runner));
#elif BUILDFLAG(IS_WIN)
  if (WMFAudioDecoder::IsEnabled()) {
    audio_decoders.push_back(
        std::make_unique<WMFAudioDecoder>(media_task_runner));
  }
#endif
}

void PlatformPipelineTestBase::AppendPlatformVideoDecoders(
    std::vector<std::unique_ptr<VideoDecoder>>& video_decoders,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    MediaLog* media_log) {
#if BUILDFLAG(IS_WIN)
  video_decoders.push_back(
      std::make_unique<WMFVideoDecoder>(media_task_runner));
#endif

  VideoDecodeAccelerator::Capabilities capabilities;
  capabilities.supported_profiles = GetSupportedProfiles();

  EXPECT_CALL(*mock_video_accelerator_factories_, GetTaskRunner())
      .WillRepeatedly(testing::Return(media_task_runner));
}
}  // namespace media
