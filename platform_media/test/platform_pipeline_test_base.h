#ifndef PLATFORM_MEDIA_TEST_PLATFORM_PIPELINE_TEST_BASE_H_
#define PLATFORM_MEDIA_TEST_PLATFORM_PIPELINE_TEST_BASE_H_

#include "platform_media/common/feature_toggles.h"

#include "base/files/file_path.h"
#include "base/test/scoped_task_environment.h"
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

  Demuxer * CreatePlatformDemuxer(std::unique_ptr<DataSource> & data_source,
                                  base::test::ScopedTaskEnvironment & task_environment_,
                                  MediaLog* media_log);

  void AppendPlatformAudioDecoders(std::vector<std::unique_ptr<AudioDecoder>> & audio_decoders,
          const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner);

  void AppendPlatformVideoDecoders(std::vector<std::unique_ptr<VideoDecoder>> & video_decoders,
          const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
          MediaLog* media_log);

  std::unique_ptr<MockGpuVideoAcceleratorFactories>
      mock_video_accelerator_factories_;
  base::FilePath filepath_;
};

}

#endif // PLATFORM_MEDIA_TEST_PLATFORM_PIPELINE_TEST_BASE_H_
