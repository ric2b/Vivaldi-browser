#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"

namespace blink {

void WebFrameWidgetImpl::LoadImageAt(const gfx::Point& point) {
  View()->LoadImageAt(point);
}

void WebFrameWidgetImpl::SetImagesEnabled(const bool show) {
  View()->SetImagesEnabled(show);
}

void WebFrameWidgetImpl::SetServeResourceFromCacheOnly(const bool only_cache) {
  View()->SetServeResourceFromCacheOnly(only_cache);
}

}  // namespace blink
