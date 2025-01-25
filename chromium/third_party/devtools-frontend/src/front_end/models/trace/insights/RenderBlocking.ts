// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Protocol from '../../../generated/protocol.js';
import * as Handlers from '../handlers/handlers.js';
import * as Helpers from '../helpers/helpers.js';
import type * as Lantern from '../lantern/lantern.js';
import type * as Types from '../types/types.js';

import {
  type InsightResult,
  InsightWarning,
  type LanternContext,
  type NavigationInsightContext,
  type RequiredData,
} from './types.js';

export type RenderBlockingInsightResult = InsightResult<{
  renderBlockingRequests: Types.TraceEvents.SyntheticNetworkRequest[],
  requestIdToWastedMs?: Map<string, number>,
}>;

// Because of the way we detect blocking stylesheets, asynchronously loaded
// CSS with link[rel=preload] and an onload handler (see https://github.com/filamentgroup/loadCSS)
// can be falsely flagged as blocking. Therefore, ignore stylesheets that loaded fast enough
// to possibly be non-blocking (and they have minimal impact anyway).
const MINIMUM_WASTED_MS = 50;

export function deps(): ['NetworkRequests', 'PageLoadMetrics'] {
  return ['NetworkRequests', 'PageLoadMetrics'];
}

/**
 * Given a simulation's nodeTimings, return an object with the nodes/timing keyed by network URL
 */
function getNodesAndTimingByRequestId(nodeTimings: Lantern.Simulation.Result['nodeTimings']):
    Map<string, {node: Lantern.Graph.Node, nodeTiming: Lantern.Types.Simulation.NodeTiming}> {
  const requestIdToNode =
      new Map<string, {node: Lantern.Graph.Node, nodeTiming: Lantern.Types.Simulation.NodeTiming}>();

  for (const [node, nodeTiming] of nodeTimings) {
    if (node.type !== 'network') {
      continue;
    }

    requestIdToNode.set(node.request.requestId, {node, nodeTiming});
  }

  return requestIdToNode;
}

function estimateSavingsWithGraphs(deferredIds: Set<string>, lanternContext: LanternContext): number {
  const simulator = lanternContext.simulator;
  const fcpGraph = lanternContext.metrics.firstContentfulPaint.optimisticGraph;
  const {nodeTimings} = lanternContext.simulator.simulate(fcpGraph);
  const adjustedNodeTimings = new Map(nodeTimings);

  const totalChildNetworkBytes = 0;
  const minimalFCPGraph = fcpGraph.cloneWithRelationships(node => {
    // If a node can be deferred, exclude it from the new FCP graph
    const canDeferRequest = deferredIds.has(node.id);
    return !canDeferRequest;
  });

  if (minimalFCPGraph.type !== 'network') {
    throw new Error('minimalFCPGraph not a NetworkNode');
  }

  // Recalculate the "before" time based on our adjusted node timings.
  const estimateBeforeInline = Math.max(...Array.from(
      Array.from(adjustedNodeTimings).map(timing => timing[1].endTime),
      ));

  // Add the inlined bytes to the HTML response
  const originalTransferSize = minimalFCPGraph.request.transferSize;
  const safeTransferSize = originalTransferSize || 0;
  minimalFCPGraph.request.transferSize = safeTransferSize + totalChildNetworkBytes;
  const estimateAfterInline = simulator.simulate(minimalFCPGraph).timeInMs;
  minimalFCPGraph.request.transferSize = originalTransferSize;
  return Math.round(Math.max(estimateBeforeInline - estimateAfterInline, 0));
}

function computeSavings(
    renderBlockingRequests: Types.TraceEvents.SyntheticNetworkRequest[],
    lanternContext: LanternContext): Pick<RenderBlockingInsightResult, 'metricSavings'|'requestIdToWastedMs'> {
  const nodesAndTimingsByRequestId =
      getNodesAndTimingByRequestId(lanternContext.metrics.firstContentfulPaint.optimisticEstimate.nodeTimings);

  const metricSavings = {FCP: 0, LCP: 0};
  const requestIdToWastedMs = new Map<string, number>();
  const deferredNodeIds = new Set<string>();
  for (const resource of renderBlockingRequests) {
    const nodeAndTiming = nodesAndTimingsByRequestId.get(resource.args.data.requestId);
    if (!nodeAndTiming) {
      continue;
    }

    const {node, nodeTiming} = nodeAndTiming;

    // Mark this node and all its dependents as deferrable
    node.traverse(node => deferredNodeIds.add(node.id));

    // "wastedMs" is the download time of the network request, responseReceived - requestSent
    const wastedMs = Math.round(nodeTiming.duration);
    if (wastedMs < MINIMUM_WASTED_MS) {
      continue;
    }

    requestIdToWastedMs.set(node.id, wastedMs);
  }

  if (requestIdToWastedMs.size) {
    metricSavings.FCP = estimateSavingsWithGraphs(deferredNodeIds, lanternContext);

    // In most cases, render blocking resources only affect LCP if LCP isn't an image.
    // TODO(crbug.com/313905799): https://chromium-review.googlesource.com/c/devtools/devtools-frontend/+/5684935/comment/c85b06da_90b0c274/
    // const lcpIsImage = ???
    // if (!lcpIsImage) {
    //   metricSavings.LCP = metricSavings.FCP;
    // }
  }

  return {metricSavings, requestIdToWastedMs};
}

export function generateInsight(
    traceParsedData: RequiredData<typeof deps>, context: NavigationInsightContext): RenderBlockingInsightResult {
  const firstPaintTs = traceParsedData.PageLoadMetrics.metricScoresByFrameId.get(context.frameId)
                           ?.get(context.navigationId)
                           ?.get(Handlers.ModelHandlers.PageLoadMetrics.MetricName.FP)
                           ?.event?.ts;
  if (!firstPaintTs) {
    return {
      renderBlockingRequests: [],
      warnings: [InsightWarning.NO_FP],
    };
  }

  const renderBlockingRequests = [];
  for (const req of traceParsedData.NetworkRequests.byTime) {
    if (req.args.data.frame !== context.frameId) {
      continue;
    }

    if (req.args.data.renderBlocking !== 'blocking' && req.args.data.renderBlocking !== 'in_body_parser_blocking') {
      continue;
    }

    if (req.args.data.syntheticData.finishTime > firstPaintTs) {
      continue;
    }

    // If a resource is marked `in_body_parser_blocking` it should only be considered render blocking if it is a
    // high enough priority. Some resources (e.g. scripts) are not marked as high priority if they are fetched
    // after a non-preloaded image. (See "early" definition in https://web.dev/articles/fetch-priority)
    //
    // There are edge cases and exceptions (e.g. priority hints) but this gives us the best approximation
    // of render blocking resources in the document body.
    if (req.args.data.renderBlocking === 'in_body_parser_blocking') {
      const priority = req.args.data.priority;
      const isScript = req.args.data.resourceType === Protocol.Network.ResourceType.Script;
      const isBlockingScript = isScript && priority === Protocol.Network.ResourcePriority.High;
      if (priority !== Protocol.Network.ResourcePriority.VeryHigh && !isBlockingScript) {
        continue;
      }
    }

    const navigation =
        Helpers.Trace.getNavigationForTraceEvent(req, context.frameId, traceParsedData.Meta.navigationsByFrameId);
    if (navigation?.args.data?.navigationId !== context.navigationId) {
      continue;
    }

    renderBlockingRequests.push(req);
  }

  const savings = context.lantern && computeSavings(renderBlockingRequests, context.lantern);

  return {
    renderBlockingRequests,
    ...savings,
  };
}
