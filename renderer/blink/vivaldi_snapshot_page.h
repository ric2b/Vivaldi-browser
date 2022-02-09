// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_BLINK_VIVALDI_SNAPSHOT_PAGE_H_
#define RENDERER_BLINK_VIVALDI_SNAPSHOT_PAGE_H_

#include "third_party/blink/renderer/core/core_export.h"

class SkBitmap;

namespace gfx {
class Rect;
}

namespace blink {
class LocalFrame;
}  // namespace blink

CORE_EXPORT bool VivaldiSnapshotPage(blink::LocalFrame* local_frame,
                                     bool full_page,
                                     const gfx::Rect& rect,
                                     SkBitmap* bitmap);

#endif  // RENDERER_BLINK_VIVALDI_SNAPSHOT_PAGE_H_
