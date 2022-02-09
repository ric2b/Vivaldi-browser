#include "platform_pipeline_test_base.h"

#include <limits>
#include <queue>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/base/test_data_util.h"

#include "media/base/limits.h"
#if defined(OS_MAC)
#include "platform_media/renderer/decoders/mac/at_audio_decoder.h"
#elif defined(OS_WIN)
#include "platform_media/renderer/decoders/win/wmf_audio_decoder.h"
#include "platform_media/renderer/decoders/win/wmf_video_decoder.h"
#endif
#include "platform_media/common/platform_mime_util.h"
#include "platform_media/renderer/decoders/ipc_demuxer.h"
#include "platform_media/renderer/decoders/ipc_factory.h"
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

std::unique_ptr<IPCMediaPipelineHost> CreateIPCMediaPipelineHost() {
  return std::make_unique<TestPipelineHost>();
}

// IPCDemuxer expects that the pipeline host is already initialized when
// Chromium calls its Initialize from Demuxer interface. This subclass overrides
// Initialize to init the host first as this provides a convinient place to
// perform an asynchronous init when running the tests.
class TestIPCDemuxer : public IPCDemuxer {
 public:
  TestIPCDemuxer(std::unique_ptr<IPCMediaPipelineHost> ipc_media_pipeline_host,
                 std::string mime_type,
                 MediaLog* media_log)
      : IPCDemuxer(std::move(ipc_media_pipeline_host), media_log),
        mime_type_(mime_type),
        weak_ptr_factory_(this) {}

  void Initialize(DemuxerHost* host,
                  PipelineStatusCallback status_cb) override {
    DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());
    ipc_media_pipeline_host_->Initialize(
        mime_type_, base::BindOnce(&TestIPCDemuxer::OnHostInitialized,
                                   weak_ptr_factory_.GetWeakPtr(), host,
                                   std::move(status_cb)));
  }

  void OnHostInitialized(DemuxerHost* host,
                         PipelineStatusCallback status_cb,
                         bool success) {
    if (!success) {
      std::move(status_cb).Run(
          PipelineStatus::PIPELINE_ERROR_INITIALIZATION_FAILED);
      return;
    }
    IPCDemuxer::Initialize(host, std::move(status_cb));
  }

  absl::optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override {
    return absl::nullopt;
  }

 private:
  std::string mime_type_;
  base::WeakPtrFactory<TestIPCDemuxer> weak_ptr_factory_;
};

}  // namespace

PlatformPipelineTestBase::PlatformPipelineTestBase()
    : mock_video_accelerator_factories_(new MockGpuVideoAcceleratorFactories(nullptr))
{}
PlatformPipelineTestBase::~PlatformPipelineTestBase() {}

Demuxer* PlatformPipelineTestBase::CreatePlatformDemuxer(
    std::unique_ptr<DataSource>& data_source,
    base::test::TaskEnvironment& task_environment_,
    MediaLog* media_log) {
  const std::string content_type;
  const GURL url("file://" + filepath_.AsUTF8Unsafe());
  IPCFactory::Preinitialize(base::BindRepeating(&CreateIPCMediaPipelineHost),
                            task_environment_.GetMainThreadTaskRunner(),
                            task_environment_.GetMainThreadTaskRunner());
  std::string adjusted_mime_type = IPCDemuxer::CanPlayType(content_type, url);
  if (!adjusted_mime_type.empty()) {
    auto pipeline_host = IPCFactory::CreateHostOnMainThread();
    pipeline_host->init_data_source(data_source.get());
    return new TestIPCDemuxer(std::move(pipeline_host), adjusted_mime_type,
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
  std::string adjusted_mime_type = IPCDemuxer::CanPlayType(content_type, url);
  if (!adjusted_mime_type.empty()) {
    audio_decoders.push_back(
        std::make_unique<PassThroughAudioDecoder>(media_task_runner));
  }

#if defined(OS_MAC)
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
  std::string adjusted_mime_type = IPCDemuxer::CanPlayType(content_type, url);
  if (!adjusted_mime_type.empty()) {
    video_decoders.push_back(
        std::make_unique<PassThroughVideoDecoder>(media_task_runner));
  }

#if defined(OS_WIN)
  video_decoders.push_back(
      std::make_unique<WMFVideoDecoder>(media_task_runner));
#endif

  VideoDecodeAccelerator::Capabilities capabilities;
  capabilities.supported_profiles = GetSupportedProfiles();

  EXPECT_CALL(*mock_video_accelerator_factories_, GetTaskRunner())
      .WillRepeatedly(testing::Return(media_task_runner));
}
}
