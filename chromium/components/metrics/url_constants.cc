// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/url_constants.h"

#include "build/build_config.h"

#include "sync/vivaldi_sync_urls.h"

namespace metrics {

#if defined(OS_ANDROID) || defined(OS_IOS)
const char kDefaultMetricsServerUrl[] =
    "https://clientservices.googleapis.com/uma/v2";
#else
const char kDefaultMetricsServerUrl[] = SYNC_URL("/uma/v2");
#endif

const char kDefaultMetricsMimeType[] = "application/vnd.chrome.uma";

} // namespace metrics

