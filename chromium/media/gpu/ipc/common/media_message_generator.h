// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.

#include "build/build_config.h"
#include "media/gpu/ipc/common/media_messages.h"

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#include "platform_media/common/media_pipeline_messages.h"
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)
