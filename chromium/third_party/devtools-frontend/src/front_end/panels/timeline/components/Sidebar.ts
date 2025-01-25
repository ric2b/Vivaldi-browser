// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../../core/common/common.js';
import * as Root from '../../../core/root/root.js';
import type * as TraceEngine from '../../../models/trace/trace.js';
import * as UI from '../../../ui/legacy/legacy.js';
import type * as Overlays from '../overlays/overlays.js';

import {SidebarAnnotationsTab} from './SidebarAnnotationsTab.js';
import {SidebarInsightsTab} from './SidebarInsightsTab.js';

export interface ActiveInsight {
  name: string;
  navigationId: string;
  createOverlayFn: (() => Overlays.Overlays.TimelineOverlay[]);
}

export class RemoveAnnotation extends Event {
  static readonly eventName = 'removeannotation';

  constructor(public removedAnnotation: TraceEngine.Types.File.Annotation) {
    super(RemoveAnnotation.eventName, {bubbles: true, composed: true});
  }
}

export class EventReferenceClick extends Event {
  static readonly eventName = 'sidebarmetricclick';

  constructor(public metricEvent: TraceEngine.Types.TraceEvents.TraceEventData) {
    super(EventReferenceClick.eventName, {bubbles: true, composed: true});
  }
}

declare global {
  interface GlobalEventHandlersEventMap {
    [EventReferenceClick.eventName]: EventReferenceClick;
  }
}

export const enum SidebarTabs {
  INSIGHTS = 'insights',
  ANNOTATIONS = 'annotations',
}
export const DEFAULT_SIDEBAR_TAB = SidebarTabs.INSIGHTS;

export const DEFAULT_SIDEBAR_WIDTH_PX = 240;
const MIN_SIDEBAR_WIDTH_PX = 170;

export class SidebarWidget extends UI.Widget.VBox {
  #tabbedPane = new UI.TabbedPane.TabbedPane();

  #insightsView = new InsightsView();
  #annotationsView = new AnnotationsView();

  /**
   * Track if the user has opened the sidebar before. We do this so that the
   * very first time they record/import a trace after the sidebar ships, we can
   * automatically pop it open to aid discovery. But, after that, the sidebar
   * visibility will be persisted based on if the user opens or closes it - the
   * SplitWidget tracks its state in its own setting.
   */
  #userHasOpenedSidebarOnce =
      Common.Settings.Settings.instance().createSetting<boolean>('timeline-user-has-opened-siderbar-once', false);

  userHasOpenedSidebarOnce(): boolean {
    return this.#userHasOpenedSidebarOnce.get();
  }

  constructor() {
    super();
    this.setMinimumSize(MIN_SIDEBAR_WIDTH_PX, 0);
  }

  override wasShown(): void {
    this.#userHasOpenedSidebarOnce.set(true);
    this.#tabbedPane.show(this.element);
    if (!this.#tabbedPane.hasTab(SidebarTabs.INSIGHTS) &&
        Root.Runtime.experiments.isEnabled(Root.Runtime.ExperimentName.TIMELINE_INSIGHTS)) {
      this.#tabbedPane.appendTab(
          SidebarTabs.INSIGHTS, 'Insights', this.#insightsView, undefined, undefined, false, false, 0,
          'timeline.insights-tab');
    }
    if (!this.#tabbedPane.hasTab(SidebarTabs.ANNOTATIONS) &&
        Root.Runtime.experiments.isEnabled(Root.Runtime.ExperimentName.TIMELINE_ANNOTATIONS)) {
      this.#tabbedPane.appendTab(
          'annotations', 'Annotations', this.#annotationsView, undefined, undefined, false, false, 1,
          'timeline.annotations-tab');
    }
    // TODO: automatically select the right tab depending on what content is
    // available to us.
  }

  setAnnotations(
      updatedAnnotations: TraceEngine.Types.File.Annotation[],
      annotationEntryToColorMap: Map<TraceEngine.Types.TraceEvents.TraceEventData, string>): void {
    this.#annotationsView.setAnnotations(updatedAnnotations, annotationEntryToColorMap);
  }

  setTraceParsedData(traceParsedData: TraceEngine.Handlers.Types.TraceParseData|null): void {
    this.#insightsView.setTraceParsedData(traceParsedData);
  }

  setInsights(insights: TraceEngine.Insights.Types.TraceInsightData|null): void {
    this.#insightsView.setInsights(insights);
  }

  setActiveInsight(activeInsight: ActiveInsight|null): void {
    this.#insightsView.setActiveInsight(activeInsight);
  }
}

class InsightsView extends UI.Widget.VBox {
  #component = new SidebarInsightsTab();

  constructor() {
    super();
    this.element.classList.add('sidebar-insights');
    this.element.appendChild(this.#component);
  }

  setTraceParsedData(data: TraceEngine.Handlers.Types.TraceParseData|null): void {
    this.#component.traceParsedData = data;
  }

  setInsights(data: TraceEngine.Insights.Types.TraceInsightData|null): void {
    this.#component.insights = data;
  }

  setActiveInsight(active: ActiveInsight|null): void {
    this.#component.activeInsight = active;
  }
}

class AnnotationsView extends UI.Widget.VBox {
  #component = new SidebarAnnotationsTab();

  constructor() {
    super();
    this.element.classList.add('sidebar-annotations');
    this.element.appendChild(this.#component);
  }

  setAnnotations(
      annotations: TraceEngine.Types.File.Annotation[],
      annotationEntryToColorMap: Map<TraceEngine.Types.TraceEvents.TraceEventData, string>): void {
    this.#component.annotationEntryToColorMap = annotationEntryToColorMap;
    this.#component.annotations = annotations;
  }
}
