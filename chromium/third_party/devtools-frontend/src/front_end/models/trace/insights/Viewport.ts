// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Helpers from '../helpers/helpers.js';
import type * as Types from '../types/types.js';

import {type InsightResult, InsightWarning, type NavigationInsightContext, type RequiredData} from './types.js';

export function deps(): ['Meta', 'UserInteractions'] {
  return ['Meta', 'UserInteractions'];
}

export type ViewportInsightResult = InsightResult<{
  mobileOptimized: boolean | null,
  viewportEvent?: Types.TraceEvents.TraceEventParseMetaViewport,
}>;

export function generateInsight(
    traceParsedData: RequiredData<typeof deps>, context: NavigationInsightContext): ViewportInsightResult {
  const compositorEvents = traceParsedData.UserInteractions.beginCommitCompositorFrameEvents.filter(event => {
    if (event.args.frame !== context.frameId) {
      return false;
    }

    const navigation =
        Helpers.Trace.getNavigationForTraceEvent(event, context.frameId, traceParsedData.Meta.navigationsByFrameId);
    return navigation === context.navigation;
  });

  if (!compositorEvents.length) {
    // Trace doesn't have the data we need.
    return {
      mobileOptimized: null,
      warnings: [InsightWarning.NO_LAYOUT],
    };
  }

  const viewportEvent = traceParsedData.UserInteractions.parseMetaViewportEvents.find(event => {
    if (event.args.data.frame !== context.frameId) {
      return false;
    }

    const navigation =
        Helpers.Trace.getNavigationForTraceEvent(event, context.frameId, traceParsedData.Meta.navigationsByFrameId);
    return navigation === context.navigation;
  });

  // Returns true only if all events are mobile optimized.
  for (const event of compositorEvents) {
    if (!event.args.is_mobile_optimized) {
      return {
        mobileOptimized: false,
        viewportEvent,
      };
    }
  }

  return {
    mobileOptimized: true,
    viewportEvent,
  };
}
