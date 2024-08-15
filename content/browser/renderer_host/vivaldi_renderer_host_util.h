// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VIVALDI_RENDERER_HOST_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_VIVALDI_RENDERER_HOST_UTILS_H_

namespace content {
class RenderWidgetHostImpl;
}

namespace gfx {
class Point;
}

namespace vivaldi {

// Returns a point that represents the difference between the origin points
// of the views in the two RenderWidgetHostImpl that are passed in.
extern gfx::Point GetVivaldiUIOffset(
    content::RenderWidgetHostImpl* parent_host,
    content::RenderWidgetHostImpl* child_host,
    float device_scale_factor);

}  // namespace vivaldi

#endif  // CONTENT_BROWSER_RENDERER_HOST_VIVALDI_RENDERER_HOST_UTILS_H_
