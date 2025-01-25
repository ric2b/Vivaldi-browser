// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import * as Handlers from './handlers/handlers.js';
import * as Insights from './insights/insights.js';
import * as Lantern from './lantern/lantern.js';
import * as LanternComputationData from './LanternComputationData.js';
import type * as Model from './ModelImpl.js';
import * as Types from './types/types.js';

const enum Status {
  IDLE = 'IDLE',
  PARSING = 'PARSING',
  FINISHED_PARSING = 'FINISHED_PARSING',
  ERRORED_WHILE_PARSING = 'ERRORED_WHILE_PARSING',
}

export class TraceParseProgressEvent extends Event {
  static readonly eventName = 'traceparseprogress';
  constructor(public data: Model.TraceParseEventProgressData, init: EventInit = {bubbles: true}) {
    super(TraceParseProgressEvent.eventName, init);
  }
}

/**
 * Parsing a trace can take time. On large traces we see a breakdown of time like so:
 *   - handleEvent() loop:  ~20%
 *   - finalize() loop:     ~60%
 *   - shallowClone calls:  ~20%
 * The numbers below are set so we can report a progress percentage of [0...1]
 */
const enum ProgressPhase {
  HANDLE_EVENT = 0.2,
  FINALIZE = 0.8,
  CLONE = 1.0,
}
function calculateProgress(value: number, phase: ProgressPhase): number {
  // Finalize values should be [0.2...0.8]
  if (phase === ProgressPhase.FINALIZE) {
    return (value * (ProgressPhase.FINALIZE - ProgressPhase.HANDLE_EVENT)) + ProgressPhase.HANDLE_EVENT;
  }
  return value * phase;
}

declare global {
  interface HTMLElementEventMap {
    [TraceParseProgressEvent.eventName]: TraceParseProgressEvent;
  }
}

export class TraceProcessor extends EventTarget {
  // We force the Meta handler to be enabled, so the TraceHandlers type here is
  // the model handlers the user passes in and the Meta handler.
  readonly #traceHandlers: Partial<Handlers.Types.Handlers>;
  #status = Status.IDLE;
  #modelConfiguration = Types.Configuration.defaults();
  #data: Handlers.Types.TraceParseData|null = null;
  #insights: Insights.Types.TraceInsightData|null = null;

  static createWithAllHandlers(): TraceProcessor {
    return new TraceProcessor(Handlers.ModelHandlers, Types.Configuration.defaults());
  }

  static getEnabledInsightRunners(traceParsedData: Handlers.Types.TraceParseData):
      Partial<Insights.Types.InsightRunnersType> {
    const enabledInsights = {} as Insights.Types.InsightRunnersType;
    for (const [name, insight] of Object.entries(Insights.InsightRunners)) {
      const deps = insight.deps();
      if (deps.some(dep => !traceParsedData[dep])) {
        continue;
      }
      Object.assign(enabledInsights, {[name]: insight});
    }
    return enabledInsights;
  }

  constructor(traceHandlers: Partial<Handlers.Types.Handlers>, modelConfiguration?: Types.Configuration.Configuration) {
    super();

    this.#verifyHandlers(traceHandlers);
    this.#traceHandlers = {
      Meta: Handlers.ModelHandlers.Meta,
      ...traceHandlers,
    };
    if (modelConfiguration) {
      this.#modelConfiguration = modelConfiguration;
    }
    this.#passConfigToHandlers();
  }

  #passConfigToHandlers(): void {
    for (const handler of Object.values(this.#traceHandlers)) {
      // Bit of an odd double check, but without this TypeScript refuses to let
      // you call the function as it thinks it might be undefined.
      if ('handleUserConfig' in handler && handler.handleUserConfig) {
        handler.handleUserConfig(this.#modelConfiguration);
      }
    }
  }

  /**
   * When the user passes in a set of handlers, we want to ensure that we have all
   * the required handlers. Handlers can depend on other handlers, so if the user
   * passes in FooHandler which depends on BarHandler, they must also pass in
   * BarHandler too. This method verifies that all dependencies are met, and
   * throws if not.
   **/
  #verifyHandlers(providedHandlers: Partial<Handlers.Types.Handlers>): void {
    // Tiny optimisation: if the amount of provided handlers matches the amount
    // of handlers in the Handlers.ModelHandlers object, that means that the
    // user has passed in every handler we have. So therefore they cannot have
    // missed any, and there is no need to iterate through the handlers and
    // check the dependencies.
    if (Object.keys(providedHandlers).length === Object.keys(Handlers.ModelHandlers).length) {
      return;
    }
    const requiredHandlerKeys: Set<Handlers.Types.TraceEventHandlerName> = new Set();
    for (const [handlerName, handler] of Object.entries(providedHandlers)) {
      requiredHandlerKeys.add(handlerName as Handlers.Types.TraceEventHandlerName);
      const deps = 'deps' in handler ? handler.deps() : [];
      for (const depName of deps) {
        requiredHandlerKeys.add(depName);
      }
    }

    const providedHandlerKeys = new Set(Object.keys(providedHandlers));
    // We always force the Meta handler to be enabled when creating the
    // Processor, so if it is missing from the set the user gave us that is OK,
    // as we will have enabled it anyway.
    requiredHandlerKeys.delete('Meta');

    for (const requiredKey of requiredHandlerKeys) {
      if (!providedHandlerKeys.has(requiredKey)) {
        throw new Error(`Required handler ${requiredKey} not provided.`);
      }
    }
  }

  reset(): void {
    if (this.#status === Status.PARSING) {
      throw new Error('Trace processor can\'t reset while parsing.');
    }

    const handlers = Object.values(this.#traceHandlers);
    for (const handler of handlers) {
      handler.reset();
    }

    this.#data = null;
    this.#insights = null;
    this.#status = Status.IDLE;
  }

  async parse(traceEvents: readonly Types.TraceEvents.TraceEventData[], freshRecording = false): Promise<void> {
    if (this.#status !== Status.IDLE) {
      throw new Error(`Trace processor can't start parsing when not idle. Current state: ${this.#status}`);
    }
    try {
      this.#status = Status.PARSING;
      await this.#computeTraceParsedData(traceEvents, freshRecording);
      if (this.#data) {
        this.#computeInsights(this.#data, traceEvents);
      }
      this.#status = Status.FINISHED_PARSING;
    } catch (e) {
      this.#status = Status.ERRORED_WHILE_PARSING;
      throw e;
    }
  }

  /**
   * Run all the handlers and set the result to `#data`.
   */
  async #computeTraceParsedData(traceEvents: readonly Types.TraceEvents.TraceEventData[], freshRecording: boolean):
      Promise<void> {
    /**
     * We want to yield regularly to maintain responsiveness. If we yield too often, we're wasting idle time.
     * We could do this by checking `performance.now()` regularly, but it's an expensive call in such a hot loop.
     * `eventsPerChunk` is an approximated proxy metric.
     * But how big a chunk? We're aiming for long tasks that are no smaller than 100ms and not bigger than 200ms.
     * It's CPU dependent, so it should be calibrated on oldish hardware.
     * Illustration of a previous change to `eventsPerChunk`: https://imgur.com/wzp8BnR
     */
    const eventsPerChunk = 50_000;
    // Convert to array so that we are able to iterate all handlers multiple times.
    const sortedHandlers = [...sortHandlers(this.#traceHandlers).values()];

    // Reset.
    for (const handler of sortedHandlers) {
      handler.reset();
    }

    // Initialize.
    for (const handler of sortedHandlers) {
      handler.initialize?.(freshRecording);
    }

    // Handle each event.
    for (let i = 0; i < traceEvents.length; ++i) {
      // Every so often we take a break just to render.
      if (i % eventsPerChunk === 0 && i) {
        // Take the opportunity to provide status update events.
        const percent = calculateProgress(i / traceEvents.length, ProgressPhase.HANDLE_EVENT);
        this.dispatchEvent(new TraceParseProgressEvent({percent}));
        // TODO(paulirish): consider using `scheduler.yield()` or `scheduler.postTask(() => {}, {priority: 'user-blocking'})`
        await new Promise(resolve => setTimeout(resolve, 0));
      }
      const event = traceEvents[i];
      for (let j = 0; j < sortedHandlers.length; ++j) {
        sortedHandlers[j].handleEvent(event);
      }
    }

    // Finalize.
    for (const [i, handler] of sortedHandlers.entries()) {
      if (handler.finalize) {
        // Yield to the UI because finalize() calls can be expensive
        // TODO(jacktfranklin): consider using `scheduler.yield()` or `scheduler.postTask(() => {}, {priority: 'user-blocking'})`
        await new Promise(resolve => setTimeout(resolve, 0));
        await handler.finalize();
      }
      const percent = calculateProgress(i / sortedHandlers.length, ProgressPhase.FINALIZE);
      this.dispatchEvent(new TraceParseProgressEvent({percent}));
    }

    // Handlers that depend on other handlers do so via .data(), which used to always
    // return a shallow clone of its internal data structures. However, that pattern
    // easily results in egregious amounts of allocation. Now .data() does not do any
    // cloning, and it happens here instead so that users of the trace processor may
    // still assume that the parsed data is theirs.
    // See: crbug/41484172
    const shallowClone = (value: unknown, recurse = true): unknown => {
      if (value instanceof Map) {
        return new Map(value);
      }
      if (value instanceof Set) {
        return new Set(value);
      }
      if (Array.isArray(value)) {
        return [...value];
      }
      if (typeof value === 'object' && value && recurse) {
        const obj: Record<string, unknown> = {};
        for (const [key, v] of Object.entries(value)) {
          obj[key] = shallowClone(v, false);
        }
        return obj;
      }
      return value;
    };

    const traceParsedData = {};
    for (const [name, handler] of Object.entries(this.#traceHandlers)) {
      const data = shallowClone(handler.data());
      Object.assign(traceParsedData, {[name]: data});
    }
    this.dispatchEvent(new TraceParseProgressEvent({percent: ProgressPhase.CLONE}));

    this.#data = traceParsedData as Handlers.Types.TraceParseData;
  }

  get traceParsedData(): Handlers.Types.TraceParseData|null {
    if (this.#status !== Status.FINISHED_PARSING) {
      return null;
    }

    return this.#data;
  }

  get insights(): Insights.Types.TraceInsightData|null {
    if (this.#status !== Status.FINISHED_PARSING) {
      return null;
    }

    return this.#insights;
  }

  #createLanternContext(
      traceParsedData: Handlers.Types.TraceParseData, traceEvents: readonly Types.TraceEvents.TraceEventData[],
      frameId: string, navigationId: string): Insights.Types.LanternContext|undefined {
    // Check for required handlers.
    if (!traceParsedData.NetworkRequests || !traceParsedData.Workers || !traceParsedData.PageLoadMetrics) {
      return;
    }
    if (!traceParsedData.NetworkRequests.byTime.length) {
      throw new Lantern.Core.LanternError('No network requests found in trace');
    }

    const navStarts = traceParsedData.Meta.navigationsByFrameId.get(frameId);
    const navStartIndex = navStarts?.findIndex(n => n.args.data?.navigationId === navigationId);
    if (!navStarts || navStartIndex === undefined || navStartIndex === -1) {
      throw new Lantern.Core.LanternError('Could not find navigation start');
    }

    const startTime = navStarts[navStartIndex].ts;
    const endTime = navStartIndex + 1 < navStarts.length ? navStarts[navStartIndex + 1].ts : Number.POSITIVE_INFINITY;
    const boundedTraceEvents = traceEvents.filter(e => e.ts >= startTime && e.ts < endTime);

    // Lantern.Types.TraceEvent and Types.TraceEvents.TraceEventData represent the same
    // object - a trace event - but one is more flexible than the other. It should be safe to cast between them.
    const trace: Lantern.Types.Trace = {
      traceEvents: boundedTraceEvents as unknown as Lantern.Types.TraceEvent[],
    };

    const requests = LanternComputationData.createNetworkRequests(trace, traceParsedData, startTime, endTime);
    const graph = LanternComputationData.createGraph(requests, trace, traceParsedData);
    const processedNavigation =
        LanternComputationData.createProcessedNavigation(traceParsedData, frameId, navigationId);

    const networkAnalysis = Lantern.Core.NetworkAnalyzer.analyze(requests);
    const simulator: Lantern.Simulation.Simulator<Types.TraceEvents.SyntheticNetworkRequest> =
        Lantern.Simulation.Simulator.createSimulator({
          networkAnalysis,
          throttlingMethod: 'simulate',
        });

    const computeData = {graph, simulator, processedNavigation};
    const fcpResult = Lantern.Metrics.FirstContentfulPaint.compute(computeData);
    const lcpResult = Lantern.Metrics.LargestContentfulPaint.compute(computeData, {fcpResult});
    const interactiveResult = Lantern.Metrics.Interactive.compute(computeData, {lcpResult});
    const tbtResult = Lantern.Metrics.TotalBlockingTime.compute(computeData, {fcpResult, interactiveResult});
    const metrics = {
      firstContentfulPaint: fcpResult,
      interactive: interactiveResult,
      largestContentfulPaint: lcpResult,
      totalBlockingTime: tbtResult,
    };

    return {graph, simulator, metrics};
  }

  /**
   * Run all the insights and set the result to `#insights`.
   */
  #computeInsights(
      traceParsedData: Handlers.Types.TraceParseData, traceEvents: readonly Types.TraceEvents.TraceEventData[]): void {
    this.#insights = new Map();

    const enabledInsightRunners = TraceProcessor.getEnabledInsightRunners(traceParsedData);

    for (const navigation of traceParsedData.Meta.mainFrameNavigations) {
      const frameId = navigation.args.frame;
      const navigationId = navigation.args.data?.navigationId;
      if (!frameId || !navigationId) {
        continue;
      }

      // The lantern sub-context is optional on NavigationInsightContext, so not setting it is OK.
      // This is also a hedge against an error inside Lantern resulting in breaking the entire performance panel.
      // Additionally, many trace fixtures are too old to be processed by Lantern.
      let lantern;
      try {
        lantern = this.#createLanternContext(traceParsedData, traceEvents, frameId, navigationId);
      } catch (e) {
        // Don't allow an error in constructing the Lantern graphs to break the rest of the trace processor.
        // Log unexpected errors, but suppress anything that occurs from a trace being too old.
        // Otherwise tests using old fixtures become way too noisy.
        const expectedErrors = [
          'mainDocumentRequest not found',
          'missing metric scores for main frame',
          'missing metric: FCP',
          'missing metric: LCP',
          'No network requests found in trace',
          'Trace is too old',
        ];
        if (!(e instanceof Lantern.Core.LanternError)) {
          // If this wasn't a managed LanternError, the stack trace is likely needed for debugging.
          console.error(e);
        } else if (!expectedErrors.some(err => e.message === err)) {
          // To reduce noise from tests, only print errors that are not expected to occur because a trace is
          // too old (for which there is no single check).
          console.error(e.message);
        }
      }

      const context: Insights.Types.NavigationInsightContext = {
        frameId,
        navigation,
        navigationId,
        lantern,
      };

      const navInsightData = {} as Insights.Types.NavigationInsightData;
      for (const [name, insight] of Object.entries(enabledInsightRunners)) {
        let insightResult;
        try {
          insightResult = insight.generateInsight(traceParsedData, context);
        } catch (err) {
          insightResult = err;
        }
        Object.assign(navInsightData, {[name]: insightResult});
      }

      this.#insights.set(context.navigationId, navInsightData);
    }
  }
}

/**
 * Some Handlers need data provided by others. Dependencies of a handler handler are
 * declared in the `deps` field.
 * @returns A map from trace event handler name to trace event hander whose entries
 * iterate in such a way that each handler is visited after its dependencies.
 */
export function sortHandlers(
    traceHandlers: Partial<{[key in Handlers.Types.TraceEventHandlerName]: Handlers.Types.TraceEventHandler}>):
    Map<Handlers.Types.TraceEventHandlerName, Handlers.Types.TraceEventHandler> {
  const sortedMap = new Map<Handlers.Types.TraceEventHandlerName, Handlers.Types.TraceEventHandler>();
  const visited = new Set<Handlers.Types.TraceEventHandlerName>();
  const visitHandler = (handlerName: Handlers.Types.TraceEventHandlerName): void => {
    if (sortedMap.has(handlerName)) {
      return;
    }
    if (visited.has(handlerName)) {
      let stackPath = '';
      for (const handler of visited) {
        if (stackPath || handler === handlerName) {
          stackPath += `${handler}->`;
        }
      }
      stackPath += handlerName;
      throw new Error(`Found dependency cycle in trace event handlers: ${stackPath}`);
    }
    visited.add(handlerName);
    const handler = traceHandlers[handlerName];
    if (!handler) {
      return;
    }
    const deps = handler.deps?.();
    if (deps) {
      deps.forEach(visitHandler);
    }
    sortedMap.set(handlerName, handler);
  };

  for (const handlerName of Object.keys(traceHandlers)) {
    visitHandler(handlerName as Handlers.Types.TraceEventHandlerName);
  }
  return sortedMap;
}
