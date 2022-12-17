// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/url_constants.h"

#include "app/vivaldi_constants.h"

namespace metrics {

const char kNewMetricsServerUrl[] =
    KNOWN_404("/uma/v2");

const char kNewMetricsServerUrlInsecure[] =
    KNOWN_404("/uma/v2");

const char kOldMetricsServerUrl[] = KNOWN_404("/uma/v2");

const char kDefaultMetricsMimeType[] = "application/vnd.chrome.uma";

} // namespace metrics
