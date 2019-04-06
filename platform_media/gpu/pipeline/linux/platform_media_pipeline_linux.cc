#include "platform_media/gpu/pipeline/platform_media_pipeline_create.h"

#include "platform_media/gpu/pipeline/linux/gstreamer_media_pipeline.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"

namespace media {

// static
PlatformMediaPipeline* PlatformMediaPipelineCreate(
    IPCDataSource* data_source,
    const PlatformMediaPipeline::AudioConfigChangedCB& audio_config_changed_cb,
    const PlatformMediaPipeline::VideoConfigChangedCB& video_config_changed_cb) {
  return new GStreamerMediaPipeline(data_source, audio_config_changed_cb, video_config_changed_cb);
}

}  // namespace media
