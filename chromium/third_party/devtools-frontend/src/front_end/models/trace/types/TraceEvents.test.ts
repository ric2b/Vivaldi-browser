// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {describeWithEnvironment} from '../../../testing/EnvironmentHelpers.js';
import {TraceLoader} from '../../../testing/TraceLoader.js';
import * as TraceEngine from '../trace.js';

describeWithEnvironment('TraceEvent types', function() {
  const {Phase, isNestableAsyncPhase, isAsyncPhase, isFlowPhase} = TraceEngine.Types.TraceEvents;
  it('is able to determine if a phase is a nestable async phase', function() {
    assert.isTrue(isNestableAsyncPhase(Phase.ASYNC_NESTABLE_START));
    assert.isTrue(isNestableAsyncPhase(Phase.ASYNC_NESTABLE_END));
    assert.isTrue(isNestableAsyncPhase(Phase.ASYNC_NESTABLE_INSTANT));
  });

  it('is able to determine if a phase is not a nestable async phase', function() {
    assert.isFalse(isNestableAsyncPhase(Phase.BEGIN));
    assert.isFalse(isNestableAsyncPhase(Phase.END));
    assert.isFalse(isNestableAsyncPhase(Phase.ASYNC_BEGIN));
  });

  it('is able to determine if a phase is an async phase', function() {
    assert.isTrue(isAsyncPhase(Phase.ASYNC_NESTABLE_START));
    assert.isTrue(isAsyncPhase(Phase.ASYNC_NESTABLE_END));
    assert.isTrue(isAsyncPhase(Phase.ASYNC_NESTABLE_INSTANT));
    assert.isTrue(isAsyncPhase(Phase.ASYNC_BEGIN));
    assert.isTrue(isAsyncPhase(Phase.ASYNC_STEP_INTO));
    assert.isTrue(isAsyncPhase(Phase.ASYNC_STEP_PAST));
    assert.isTrue(isAsyncPhase(Phase.ASYNC_END));
  });

  it('is able to determine if a phase is not an async phase', function() {
    assert.isFalse(isAsyncPhase(Phase.BEGIN));
    assert.isFalse(isAsyncPhase(Phase.METADATA));
    assert.isFalse(isAsyncPhase(Phase.OBJECT_CREATED));
  });

  it('is able to determine if a phase is a flow phase', function() {
    assert.isTrue(isFlowPhase(Phase.FLOW_START));
    assert.isTrue(isFlowPhase(Phase.FLOW_STEP));
    assert.isTrue(isFlowPhase(Phase.FLOW_END));
  });

  it('is able to determine if a phase is not a flow phase', function() {
    assert.isFalse(isFlowPhase(Phase.ASYNC_STEP_INTO));
    assert.isFalse(isFlowPhase(Phase.ASYNC_NESTABLE_START));
    assert.isFalse(isFlowPhase(Phase.BEGIN));
  });

  it('is able to determine that an event is a synthetic user timing event', async function() {
    const {traceData} = await TraceLoader.traceEngine(this, 'timings-track.json.gz');
    const timingEvent = traceData.UserTimings.performanceMeasures[0];
    assert.isTrue(TraceEngine.Types.TraceEvents.isSyntheticUserTiming(timingEvent));
    const consoleEvent = traceData.UserTimings.consoleTimings[0];
    assert.isFalse(TraceEngine.Types.TraceEvents.isSyntheticUserTiming(consoleEvent));
  });

  it('is able to determine that an event is a synthetic console event', async function() {
    const {traceData} = await TraceLoader.traceEngine(this, 'timings-track.json.gz');
    const consoleEvent = traceData.UserTimings.consoleTimings[0];
    assert.isTrue(TraceEngine.Types.TraceEvents.isSyntheticConsoleTiming(consoleEvent));
    const timingEvent = traceData.UserTimings.performanceMeasures[0];
    assert.isFalse(TraceEngine.Types.TraceEvents.isSyntheticConsoleTiming(timingEvent));
  });

  it('is able to detemrine that an event is a synthetic network request event', async function() {
    const {traceData} = await TraceLoader.traceEngine(this, 'lcp-images.json.gz');
    const networkEvent = traceData.NetworkRequests.byTime[0];
    assert.isTrue(TraceEngine.Types.TraceEvents.isSyntheticNetworkRequestEvent(networkEvent));
    const otherEvent = traceData.Renderer.allTraceEntries[0];
    assert.isFalse(TraceEngine.Types.TraceEvents.isSyntheticNetworkRequestEvent(otherEvent));
  });

  it('is able to determine that an event is a synthetic layout shift event', async function() {
    const {traceData} = await TraceLoader.traceEngine(this, 'cls-single-frame.json.gz');
    const syntheticLayoutShift = traceData.LayoutShifts.clusters[0].events[0];
    assert.isTrue(TraceEngine.Types.TraceEvents.isSyntheticLayoutShift(syntheticLayoutShift));
  });

  it('is able to identify that an event is a legacy timeline frame', async function() {
    const {traceData} = await TraceLoader.traceEngine(this, 'web-dev-with-commit.json.gz');
    const frame = traceData.Frames.frames.at(0);
    assert.isOk(frame);
    assert.isTrue(TraceEngine.Types.TraceEvents.isLegacyTimelineFrame(frame));

    const networkEvent = traceData.NetworkRequests.byTime.at(0);
    assert.isOk(networkEvent);
    assert.isFalse(TraceEngine.Types.TraceEvents.isLegacyTimelineFrame(networkEvent));
  });
});
