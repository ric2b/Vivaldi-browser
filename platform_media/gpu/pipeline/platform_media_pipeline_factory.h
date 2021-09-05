#ifndef PLATFORM_MEDIA_GPU_PIPELINE_PLATFORM_MEDIA_PIPELINE_FACTORY_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_PLATFORM_MEDIA_PIPELINE_FACTORY_H_

#include "platform_media/common/feature_toggles.h"

#include "media/base/media_export.h"

#include <memory>

namespace media {

class PlatformMediaPipeline;

class PlatformMediaPipelineFactory {
 public:
  // Instantiates a concrete implementation of this interface.  Each platform
  // is expected to define its version of this method.  When hardware
  // accelerated video decoding mode is preferred but not available media
  // pipeline should attempt to fall back to software decoding.
  static MEDIA_EXPORT std::unique_ptr<PlatformMediaPipelineFactory> Create();

  virtual ~PlatformMediaPipelineFactory() = default;

  // Create a new pipeline instance or null on failure.
  virtual std::unique_ptr<PlatformMediaPipeline> CreatePipeline() = 0;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_PLATFORM_MEDIA_PIPELINE_FACTORY_H_
