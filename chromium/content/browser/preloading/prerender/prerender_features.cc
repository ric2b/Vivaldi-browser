// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_features.h"

#include "third_party/blink/public/common/features.h"

namespace features {

// Kill-switch controlled by the field trial. When this feature is enabled,
// PrerenderHostRegistry doesn't query about the current memory footprint and
// bypasses the memory limit check, while it still checks the limit on the
// number of ongoing prerendering requests and memory pressure events to prevent
// excessive memory usage. See https://crbug.com/1382697 for details.
BASE_FEATURE(kPrerender2BypassMemoryLimitCheck,
             "Prerender2BypassMemoryLimitCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a new limit and scheduler for prerender triggers.
// See crbug.com/1464021 for more details.
BASE_FEATURE(kPrerender2NewLimitAndScheduler,
             "Prerender2NewLimitAndScheduler",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows activation in background tab. For now, this is used only on web
// platform tests on macOS to run activation with target hint tests that have
// race conditions between visibility change and activation start on a prerender
// WebContents. Note that this issue does not happen on browser_tests, so this
// could be specific to WPT setup.
// TODO(crbug.com/40249964): Allow activation in background by default.
BASE_FEATURE(kPrerender2AllowActivationInBackground,
             "Prerender2AllowActivationInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Prerender2 Embedders trigger based on rules decided by the browser. Prevent
// the browser from triggering on the hosts listed.
// Blocked hosts are expected to be passed as a comma separated string.
// e.g. example1.test,example2.test
BASE_FEATURE(kPrerender2EmbedderBlockedHosts,
             "Prerender2EmbedderBlockedHosts",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kPrerender2EmbedderBlockedHostsParam{
    &kPrerender2EmbedderBlockedHosts, "embedder_blocked_hosts", ""};

// Enables fallback from prerender to prefetch for Speculation Rules.
// See https://crbug.com/342089123 for more details.
BASE_FEATURE(kPrerender2FallbackPrefetchSpecRules,
             "Prerender2FallbackPrefetchSpecRules",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutEagerPrerender{
        &blink::features::kPrerender2NoVarySearch,
        "wait_for_headers_timeout_eager_prerender", 1000};

const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutModeratePrerender{
        &blink::features::kPrerender2NoVarySearch,
        "wait_for_headers_timeout_moderate_prerender", 0};

const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutConservativePrerender{
        &blink::features::kPrerender2NoVarySearch,
        "wait_for_headers_timeout_conservative_prerender", 0};

const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutForEmbedders{
        &blink::features::kPrerender2NoVarySearch,
        "wait_for_headers_timeout_embedders", 1000};

}  // namespace features
