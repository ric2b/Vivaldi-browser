// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"

#include "base/feature_list.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/browser/observers/assert_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/back_forward_cache_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/cross_origin_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/early_hints_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/fenced_frames_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/performance_manager_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/prerender_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/privacy_sandbox_ads_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/same_origin_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/shared_storage_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/uma_file_and_data_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/features.h"

namespace page_load_metrics {

PageLoadMetricsEmbedderBase::PageLoadMetricsEmbedderBase(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

PageLoadMetricsEmbedderBase::~PageLoadMetricsEmbedderBase() = default;

void PageLoadMetricsEmbedderBase::RegisterObservers(PageLoadTracker* tracker) {
  // Register observers used by all embedders
#if DCHECK_IS_ON()
  // Link Preview doesn't emit activation event yet and assertion of event
  // orders fail.
  //
  // TODO(b:302999778): Reenable it.
  if (!tracker->GetWebContents()->IsInPreviewMode()) {
    tracker->AddObserver(std::make_unique<AssertPageLoadMetricsObserver>());
  }
#endif

  if (!IsNoStatePrefetch(web_contents()) && !IsSidePanel(web_contents()) &&
      !IsNonTabWebUI()) {
    tracker->AddObserver(
        std::make_unique<BackForwardCachePageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<UmaPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<UseCounterPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<EarlyHintsPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<FencedFramesPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<PrerenderPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<SameOriginPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<CrossOriginPageLoadMetricsObserver>());
    if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
      tracker->AddObserver(
          std::make_unique<SharedStoragePageLoadMetricsObserver>());
    }
    tracker->AddObserver(
        std::make_unique<PrivacySandboxAdsPageLoadMetricsObserver>());
    tracker->AddObserver(
        std::make_unique<UmaFileAndDataPageLoadMetricsObserver>());
    tracker->AddObserver(std::make_unique<PerformanceManagerMetricsObserver>());
  }
  // Allow the embedder to register any embedder-specific observers
  RegisterEmbedderObservers(tracker);
}

std::unique_ptr<base::OneShotTimer> PageLoadMetricsEmbedderBase::CreateTimer() {
  return std::make_unique<base::OneShotTimer>();
}

}  // namespace page_load_metrics
