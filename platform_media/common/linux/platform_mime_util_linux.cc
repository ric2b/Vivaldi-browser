#include "platform_media/common/platform_mime_util.h"

namespace media {

bool IsPlatformMediaPipelineAvailable(PlatformMediaCheckType) {
  return false;
}

bool IsPlatformAudioDecoderAvailable(AudioCodec) {
  return false;
}

bool IsPlatformVideoDecoderAvailable() {
  return false;
}

}  // namespace media
