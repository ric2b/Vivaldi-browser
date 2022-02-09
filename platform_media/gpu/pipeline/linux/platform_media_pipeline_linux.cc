#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

#include "platform_media/gpu/pipeline/linux/gstreamer_media_pipeline.h"

namespace media {

/* static */
std::unique_ptr<PlatformMediaPipeline> PlatformMediaPipeline::Create() {
  return std::make_unique<GStreamerMediaPipeline>();
}

}  // namespace media
