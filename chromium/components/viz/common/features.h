// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FEATURES_H_
#define COMPONENTS_VIZ_COMMON_FEATURES_H_

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/viz/common/delegated_ink_prediction_configuration.h"
#include "components/viz/common/viz_common_export.h"

// See the following for guidance on adding new viz feature flags:
// https://cs.chromium.org/chromium/src/components/viz/README.md#runtime-features

namespace features {

#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidBrowserControlsInViz);
#endif  // BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackdropFilterMirrorEdgeMode);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDelegatedCompositing);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseDCompSurfacesForDelegatedInk);
#if BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDelegatedCompositingLimitToUi);
#endif
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kRenderPassDrawnRect);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kRecordSkPicture);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseDrmBlackFullscreenOptimization);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseMultipleOverlays);
VIZ_COMMON_EXPORT extern const char kMaxOverlaysParam[];
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kVideoDetectorIgnoreNonVideos);
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDynamicColorGamut);
#endif
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kVizFrameSubmissionForWebView);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseRealBuffersForPageFlipTest);
#if BUILDFLAG(IS_FUCHSIA)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSkiaOutputDeviceBufferQueue);
#endif
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcLogCapturePipeline);
#if BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSetPresentDuration);
#endif  // BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebViewVulkanIntermediateBuffer);
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSurfaceLayerForVideoDefault);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebViewEnableADPF);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebViewEnableADPFRendererMain);
#endif
#if BUILDFLAG(IS_APPLE)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kCALayerNewLimit);
VIZ_COMMON_EXPORT extern const base::FeatureParam<int> kCALayerNewLimitDefault;
VIZ_COMMON_EXPORT extern const base::FeatureParam<int>
    kCALayerNewLimitManyVideos;
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanSkipRenderPassOverlay);
#endif

#if BUILDFLAG(IS_MAC)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kCVDisplayLinkBeginFrameSource);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kVSyncAlignedPresent);
#endif

VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDrawPredictedInkPoint);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowUndamagedNonrootRenderPassToSkip);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowForceMergeRenderPassWithRequireOverlayQuads);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kRendererAllocatesImages);
#endif
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kOnBeginFrameAcks);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kOnBeginFrameThrottleVideo);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedBitmapToSharedImage);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableADPFScrollBoost);
VIZ_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kADPFBoostTimeout;
VIZ_COMMON_EXPORT extern const base::FeatureParam<double>
    kADPFMidFrameBoostDurationMultiplier;
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableADPFRendererMain);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableADPFGpuCompositorThread);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableADPFAsyncThreadsVerification);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseDisplaySDRMaxLuminanceNits);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kInvalidateLocalSurfaceIdPreCommit);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kHideDelegatedFrameHostMac);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEvictionUnlocksResources);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kSingleVideoFrameRateThrottling);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kBatchMainThreadReleaseCallbacks);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kColorConversionInRenderer);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kLastVSyncArgsKillswitch);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlitRequestsForViewTransition);

VIZ_COMMON_EXPORT extern const char kDraw1Point12Ms[];
VIZ_COMMON_EXPORT extern const char kDraw2Points6Ms[];
VIZ_COMMON_EXPORT extern const char kDraw1Point6Ms[];
VIZ_COMMON_EXPORT extern const char kDraw2Points3Ms[];
VIZ_COMMON_EXPORT extern const char kPredictorKalman[];
VIZ_COMMON_EXPORT extern const char kPredictorLinearResampling[];
VIZ_COMMON_EXPORT extern const char kPredictorLinear1[];
VIZ_COMMON_EXPORT extern const char kPredictorLinear2[];
VIZ_COMMON_EXPORT extern const char kPredictorLsq[];

VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDrawImmediatelyWhenInteractive);

VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kSnapshotEvictedRootSurface);
VIZ_COMMON_EXPORT extern const base::FeatureParam<double>
    kSnapshotEvictedRootSurfaceScale;
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kBatchResourceRelease);

#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT bool IsDynamicColorGamutEnabled();
#endif
VIZ_COMMON_EXPORT bool IsDelegatedCompositingEnabled();
VIZ_COMMON_EXPORT bool IsUsingVizFrameSubmissionForWebView();
VIZ_COMMON_EXPORT bool IsUsingPreferredIntervalForVideo();
VIZ_COMMON_EXPORT bool ShouldWebRtcLogCapturePipeline();
#if BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT bool ShouldUseSetPresentDuration();
#endif  // BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT std::optional<int> ShouldDrawPredictedInkPoints();
VIZ_COMMON_EXPORT std::string InkPredictor();
VIZ_COMMON_EXPORT bool UseWebViewNewInvalidateHeuristic();
VIZ_COMMON_EXPORT bool UseSurfaceLayerForVideo();
VIZ_COMMON_EXPORT int MaxOverlaysConsidered();
VIZ_COMMON_EXPORT bool ShouldOnBeginFrameThrottleVideo();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
VIZ_COMMON_EXPORT bool ShouldRendererAllocateImages();
#endif
VIZ_COMMON_EXPORT bool IsOnBeginFrameAcksEnabled();
VIZ_COMMON_EXPORT bool ShouldDrawImmediatelyWhenInteractive();
VIZ_COMMON_EXPORT std::optional<double> SnapshotEvictedRootSurfaceScale();
VIZ_COMMON_EXPORT bool IsCVDisplayLinkBeginFrameSourceEnabled();
VIZ_COMMON_EXPORT bool IsVSyncAlignedPresentEnabled();
VIZ_COMMON_EXPORT int NumPendingFrameSupported();
VIZ_COMMON_EXPORT bool ShouldLogFrameQuadInfo();

}  // namespace features

#endif  // COMPONENTS_VIZ_COMMON_FEATURES_H_
