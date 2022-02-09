// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_BLINK_VIVALDI_SNAPSHOT_PAGE_H_
#define RENDERER_BLINK_VIVALDI_SNAPSHOT_PAGE_H_

#include "third_party/blink/renderer/core/core_export.h"

class SkBitmap;

namespace blink {
class IntRect;
class LocalFrame;
}

CORE_EXPORT bool VivaldiSnapshotPage(blink::LocalFrame* local_frame,
                                     bool full_page,
                                     const blink::IntRect& rect,
                                     SkBitmap* bitmap);

#endif  // RENDERER_BLINK_VIVALDI_SNAPSHOT_PAGE_H_
