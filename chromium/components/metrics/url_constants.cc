// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/url_constants.h"

#include "sync/vivaldi_sync_urls.h"

namespace metrics {

const char kNewMetricsServerUrl[] =
    SYNC_URL("/uma/v2");

const char kNewMetricsServerUrlInsecure[] =
    SYNC_URL("/uma/v2");

const char kOldMetricsServerUrl[] = SYNC_URL("/uma/v2");

const char kDefaultMetricsMimeType[] = "application/vnd.chrome.uma";

} // namespace metrics
