// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_PROCESS_LAUNCH_CAUSES_H_
#define CONTENT_COMMON_GPU_PROCESS_LAUNCH_CAUSES_H_

namespace content {

enum CauseForGpuLaunch {
  CAUSE_FOR_GPU_LAUNCH_PLATFORM_MEDIA_PIPELINE,
  CAUSE_FOR_GPU_LAUNCH_OTHER,

  // All new values should be inserted above this point so that
  // existing values continue to match up with those in histograms.xml.
  CAUSE_FOR_GPU_LAUNCH_MAX_ENUM
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_PROCESS_LAUNCH_CAUSES_H_
