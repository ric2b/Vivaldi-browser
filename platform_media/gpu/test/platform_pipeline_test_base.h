#ifndef MEDIA_TEST_PLATFORM_PIPELINE_TEST_BASE_H
#define MEDIA_TEST_PLATFORM_PIPELINE_TEST_BASE_H

#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "media/base/data_source.h"
#include "media/base/demuxer.h"

namespace media {

class AudioDecoder;
class VideoDecoder;
class MockGpuVideoAcceleratorFactories;

class PlatformPipelineTestBase {
public:
 PlatformPipelineTestBase();
 virtual ~PlatformPipelineTestBase();
protected:
  class DecodingMockVDA;

  Demuxer * CreatePlatformDemuxer(std::unique_ptr<DataSource> & data_source,
                                  base::MessageLoop & message_loop_,
                                  MediaLog* media_log);

  void AppendPlatformAudioDecoders(std::vector<std::unique_ptr<AudioDecoder>> & audio_decoders,
          const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner);

  void AppendPlatformVideoDecoders(std::vector<std::unique_ptr<VideoDecoder>> & video_decoders,
          const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
          MediaLog* media_log);

  void EnableMockVDA();
  void DestroyMockVDA();
  void ResumeMockVDA();

  std::unique_ptr<MockGpuVideoAcceleratorFactories>
      mock_video_accelerator_factories_;
  std::unique_ptr<DecodingMockVDA> mock_vda_;
  base::FilePath filepath_;
};

}

#endif // MEDIA_TEST_PLATFORM_PIPELINE_TEST_BASE_H
