#include "platform_media/gpu/pipeline/platform_media_pipeline_create.h"

namespace media {

// static
PlatformMediaPipeline* PlatformMediaPipelineCreate(
    IPCDataSource* data_source,
    const PlatformMediaPipeline::AudioConfigChangedCB& audio_config_changed_cb,
    const PlatformMediaPipeline::VideoConfigChangedCB& video_config_changed_cb,
    PlatformMediaDecodingMode preferred_video_decoding_mode,
    const PlatformMediaPipeline::MakeGLContextCurrentCB& make_gl_context_current_cb) {
  return nullptr;
}

}  // namespace media
