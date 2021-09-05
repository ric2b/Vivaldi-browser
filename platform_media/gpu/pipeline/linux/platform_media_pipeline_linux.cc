#include "platform_media/gpu/pipeline/platform_media_pipeline_factory.h"

#include "platform_media/gpu/pipeline/linux/gstreamer_media_pipeline.h"

namespace media {

namespace {

class GStreamerMediaPipelineFactory : public PlatformMediaPipelineFactory {
 public:
  std::unique_ptr<PlatformMediaPipeline> CreatePipeline() override {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
    return std::make_unique<GStreamerMediaPipeline>();
  }
};

}  // namespace

// static
std::unique_ptr<PlatformMediaPipelineFactory>
PlatformMediaPipelineFactory::Create() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  return std::make_unique<GStreamerMediaPipelineFactory>();
}

}  // namespace media
