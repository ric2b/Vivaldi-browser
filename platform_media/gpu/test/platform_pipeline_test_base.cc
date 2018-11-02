#include "platform_pipeline_test_base.h"

#include <limits>
#include <queue>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "media/base/test_data_util.h"

#if defined(PLATFORM_MEDIA_HWA)
#include "gpu/GLES2/gl2extchromium.h"
#endif
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


#include "platform_media/gpu/test/test_pipeline_host.h"

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

#if defined(PLATFORM_MEDIA_HWA)
const int kNumPictureBuffers = limits::kMaxVideoFrames + 1;
const int kMaxPictureWidth = 1920;
const int kMaxPictureHeight = 1080;

bool CreateTextures(int32_t count,
                    const gfx::Size& size,
                    std::vector<uint32_t>* texture_ids,
                    std::vector<gpu::Mailbox>* texture_mailboxes,
                    uint32_t texture_target) {
  CHECK_EQ(count, kNumPictureBuffers);
  for (int i = 0; i < count; ++i) {
    texture_ids->push_back(i + 1);
    texture_mailboxes->push_back(gpu::Mailbox());
  }
  return true;
}
#endif

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

#if defined(PLATFORM_MEDIA_HWA)
// A MockVideoDecodeAccelerator that pretends it reallly decodes.
class PlatformPipelineTestBase::DecodingMockVDA
    : public MockVideoDecodeAccelerator {
 public:
  DecodingMockVDA() : client_(nullptr), enabled_(false) {
    EXPECT_CALL(*this, Initialize(_, _))
        .WillRepeatedly(testing::Invoke(this, &DecodingMockVDA::DoInitialize));
  }

  void Enable() {
    enabled_ = true;

    EXPECT_CALL(*this, AssignPictureBuffers(_))
        .WillRepeatedly(
            testing::Invoke(this, &DecodingMockVDA::SetPictureBuffers));
    EXPECT_CALL(*this, ReusePictureBuffer(_))
        .WillRepeatedly(
            testing::Invoke(this, &DecodingMockVDA::DoReusePictureBuffer));
    EXPECT_CALL(*this, Decode(_))
        .WillRepeatedly(testing::Invoke(this, &DecodingMockVDA::DoDecode));
    EXPECT_CALL(*this, Flush())
        .WillRepeatedly(testing::Invoke(this, &DecodingMockVDA::DoFlush));
  }

 private:
  enum { kFlush = -1 };

  bool DoInitialize(const Config &config, Client* client) {
    // This makes this VDA and GpuVideoDecoder unusable by default and will
    // require opt-in (see |Enable()|).
    if (!enabled_)
      return false;

    if (config.profile < VideoCodecProfile::H264PROFILE_MIN ||
      config.profile > VideoCodecProfile::H264PROFILE_MAX)
      return false;

    client_ = client;
    client_->ProvidePictureBuffers(
        kNumPictureBuffers,
        VideoPixelFormat::PIXEL_FORMAT_UNKNOWN,
        1,
        gfx::Size(kMaxPictureWidth, kMaxPictureHeight),
        GL_TEXTURE_RECTANGLE_ARB);
    return true;
  }

  void SetPictureBuffers(const std::vector<PictureBuffer>& buffers) {
    CHECK_EQ(buffers.size(), base::checked_cast<size_t>(kNumPictureBuffers));
    CHECK(available_picture_buffer_ids_.empty());

    for (const PictureBuffer& buffer : buffers)
      available_picture_buffer_ids_.push(buffer.id());
  }

  void DoReusePictureBuffer(int32_t id) {
    available_picture_buffer_ids_.push(id);
    if (!finished_bitstream_buffers_ids_.empty())
      SendPicture();
  }

  void DoDecode(const BitstreamBuffer& bitstream_buffer) {
    finished_bitstream_buffers_ids_.push(bitstream_buffer.id());

    if (!available_picture_buffer_ids_.empty())
      SendPicture();
  }

  void SendPicture() {
    CHECK(!available_picture_buffer_ids_.empty());
    CHECK(!finished_bitstream_buffers_ids_.empty());

    const int32_t bitstream_buffer_id = finished_bitstream_buffers_ids_.front();
    finished_bitstream_buffers_ids_.pop();

    client_->PictureReady(
        Picture(available_picture_buffer_ids_.front(), bitstream_buffer_id,
                gfx::Rect(kMaxPictureWidth, kMaxPictureHeight), gfx::ColorSpace(), false));
    available_picture_buffer_ids_.pop();

    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&VideoDecodeAccelerator::Client::NotifyEndOfBitstreamBuffer,
                   base::Unretained(client_), bitstream_buffer_id));

    if (!finished_bitstream_buffers_ids_.empty() &&
        finished_bitstream_buffers_ids_.front() == kFlush) {
      finished_bitstream_buffers_ids_.pop();
      base::MessageLoop::current()->task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&VideoDecodeAccelerator::Client::NotifyFlushDone,
                     base::Unretained(client_)));
    }
  }

  void DoFlush() {
    // Enqueue a special "flush marker" picture.  It will be picked up in
    // |SendPicture()| when all the pictures enqueued before the marker have
    // been sent.
    finished_bitstream_buffers_ids_.push(kFlush);

    while (!finished_bitstream_buffers_ids_.empty() &&
           !available_picture_buffer_ids_.empty())
      SendPicture();
  }

  VideoDecodeAccelerator::Client* client_;
  std::queue<int32_t> available_picture_buffer_ids_;
  std::queue<int32_t> finished_bitstream_buffers_ids_;
  bool enabled_;
};
#endif
PlatformPipelineTestBase::PlatformPipelineTestBase()
    : mock_video_accelerator_factories_(new MockGpuVideoAcceleratorFactories(nullptr))
#if defined(PLATFORM_MEDIA_HWA)
    , mock_vda_(new DecodingMockVDA)
#endif
{
}
PlatformPipelineTestBase::~PlatformPipelineTestBase() {
}

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
        base::MakeUnique<PassThroughAudioDecoder>(media_task_runner));
  }

#if defined(OS_MACOSX)
  audio_decoders.push_back(
      base::MakeUnique<ATAudioDecoder>(media_task_runner));
#elif defined(OS_WIN)
  audio_decoders.push_back(
      base::MakeUnique<WMFAudioDecoder>(media_task_runner));
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
          base::MakeUnique<PassThroughVideoDecoder>(media_task_runner));
  }

#if defined(OS_WIN)
  video_decoders.push_back(
      base::MakeUnique<WMFVideoDecoder>(media_task_runner));
#endif

  video_decoders.push_back(
      base::MakeUnique<GpuVideoDecoder>(mock_video_accelerator_factories_.get(),
                                        RequestOverlayInfoCB(),
                                        gfx::ColorSpace(),
                                        media_log));

  VideoDecodeAccelerator::Capabilities capabilities;
  capabilities.supported_profiles = GetSupportedProfiles();

  EXPECT_CALL(*mock_video_accelerator_factories_, GetTaskRunner())
      .WillRepeatedly(testing::Return(media_task_runner));
  EXPECT_CALL(*mock_video_accelerator_factories_,
              GetVideoDecodeAcceleratorCapabilities())
      .WillRepeatedly(testing::Return(capabilities));
#if defined(PLATFORM_MEDIA_HWA)
  EXPECT_CALL(*mock_video_accelerator_factories_,
              DoCreateVideoDecodeAccelerator())
      .WillRepeatedly(testing::Return(mock_vda_.get()));
  EXPECT_CALL(*mock_video_accelerator_factories_, CreateTextures(_, _, _, _, _))
      .WillRepeatedly(testing::Invoke(&CreateTextures));
  EXPECT_CALL(*mock_video_accelerator_factories_, DeleteTexture(_))
      .Times(testing::AnyNumber());
#endif
  EXPECT_CALL(*mock_video_accelerator_factories_, WaitSyncToken(_))
      .Times(testing::AnyNumber());
#if defined(PLATFORM_MEDIA_HWA)
  DCHECK(mock_vda_);
  EXPECT_CALL(*mock_vda_, Destroy())
      .WillRepeatedly(
          testing::Invoke(this, &PlatformPipelineTestBase::DestroyMockVDA));
#endif
}

void PlatformPipelineTestBase::EnableMockVDA() {
#if defined(PLATFORM_MEDIA_HWA)
  mock_vda_->Enable();
#endif
}

void PlatformPipelineTestBase::DestroyMockVDA() {
#if defined(PLATFORM_MEDIA_HWA)
  mock_vda_.reset();
#endif
}

void PlatformPipelineTestBase::ResumeMockVDA() {
#if defined(PLATFORM_MEDIA_HWA)
if (!mock_vda_)
  mock_vda_.reset(new DecodingMockVDA);
#endif
}

}
