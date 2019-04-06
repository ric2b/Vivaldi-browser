#ifndef PLATFORM_MEDIA_GPU_PIPELINE_PLATFORM_MEDIA_PIPELINE_CREATE_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_PLATFORM_MEDIA_PIPELINE_CREATE_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/gpu/pipeline/platform_media_pipeline.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "media/base/media_export.h"

namespace media {

// Instantiates a concrete implementation of this interface.  Each platform
// is expected to define its version of this method.  When hardware
// accelerated video decoding mode is preferred but not available media
// pipeline should attempt to fall back to software decoding.
MEDIA_EXPORT PlatformMediaPipeline* PlatformMediaPipelineCreate(
    IPCDataSource* data_source,
    const PlatformMediaPipeline::AudioConfigChangedCB& audio_config_changed_cb,
    const PlatformMediaPipeline::VideoConfigChangedCB& video_config_changed_cb);

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_PLATFORM_MEDIA_PIPELINE_CREATE_H_
