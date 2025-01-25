// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as i18n from '../../../../core/i18n/i18n.js';
import * as TraceEngine from '../../../../models/trace/trace.js';
import * as LitHtml from '../../../../ui/lit-html/lit-html.js';
import type * as Overlays from '../../overlays/overlays.js';

import {BaseInsight, shouldRenderForCategory} from './Helpers.js';
import * as SidebarInsight from './SidebarInsight.js';
import {Table, type TableData} from './Table.js';
import {InsightsCategories} from './types.js';

const UIStrings = {
  /**
   *@description Time to first byte title for the Largest Contentful Paint's phases timespan breakdown.
   */
  timeToFirstByte: 'Time to first byte',
  /**
   *@description Resource load delay title for the Largest Contentful Paint phases timespan breakdown.
   */
  resourceLoadDelay: 'Resource load delay',
  /**
   *@description Resource load duration title for the Largest Contentful Paint phases timespan breakdown.
   */
  resourceLoadDuration: 'Resource load duration',
  /**
   *@description Element render delay title for the Largest Contentful Paint phases timespan breakdown.
   */
  elementRenderDelay: 'Element render delay',
};
const str_ = i18n.i18n.registerUIStrings('panels/timeline/components/insights/LCPPhases.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

interface PhaseData {
  phase: string;
  timing: number|TraceEngine.Types.Timing.MilliSeconds;
  percent: string;
}

export class LCPPhases extends BaseInsight {
  static readonly litTagName = LitHtml.literal`devtools-performance-lcp-by-phases`;
  override insightCategory: InsightsCategories = InsightsCategories.LCP;
  override internalName: string = 'lcp-by-phase';
  override userVisibleTitle: string = 'LCP by phase';

  #getPhaseData(insights: TraceEngine.Insights.Types.TraceInsightData|null, navigationId: string|null): PhaseData[] {
    if (!insights || !navigationId) {
      return [];
    }
    const insightsByNavigation = insights.get(navigationId);
    if (!insightsByNavigation) {
      return [];
    }
    const lcpInsight = insightsByNavigation.LargestContentfulPaint;
    if (lcpInsight instanceof Error) {
      return [];
    }

    const timing = lcpInsight.lcpMs;
    const phases = lcpInsight.phases;

    if (!timing || !phases) {
      return [];
    }

    const {ttfb, loadDelay, loadTime, renderDelay} = phases;

    if (loadDelay && loadTime) {
      const phaseData = [
        {phase: i18nString(UIStrings.timeToFirstByte), timing: ttfb, percent: `${(100 * ttfb / timing).toFixed(0)}%`},
        {
          phase: i18nString(UIStrings.resourceLoadDelay),
          timing: loadDelay,
          percent: `${(100 * loadDelay / timing).toFixed(0)}%`,
        },
        {
          phase: i18nString(UIStrings.resourceLoadDuration),
          timing: loadTime,
          percent: `${(100 * loadTime / timing).toFixed(0)}%`,
        },
        {
          phase: i18nString(UIStrings.elementRenderDelay),
          timing: renderDelay,
          percent: `${(100 * renderDelay / timing).toFixed(0)}%`,
        },
      ];
      return phaseData;
    }

    // If the lcp is text, we only have ttfb and render delay.
    const phaseData = [
      {phase: i18nString(UIStrings.timeToFirstByte), timing: ttfb, percent: `${(100 * ttfb / timing).toFixed(0)}%`},
      {
        phase: i18nString(UIStrings.elementRenderDelay),
        timing: renderDelay,
        percent: `${(100 * renderDelay / timing).toFixed(0)}%`,
      },
    ];
    return phaseData;
  }

  override createOverlays(): Overlays.Overlays.TimelineOverlay[] {
    if (!this.data.insights || !this.data.navigationId) {
      return [];
    }
    const {navigationId, insights} = this.data;

    const insightsByNavigation = insights.get(navigationId);
    if (!insightsByNavigation) {
      return [];
    }

    const lcpInsight = insightsByNavigation.LargestContentfulPaint;
    if (lcpInsight instanceof Error) {
      return [];
    }

    const phases = lcpInsight.phases;
    const lcpTs = lcpInsight.lcpTs;
    if (!phases || !lcpTs) {
      return [];
    }
    const lcpMicroseconds =
        TraceEngine.Types.Timing.MicroSeconds(TraceEngine.Helpers.Timing.millisecondsToMicroseconds(lcpTs));

    const sections = [];
    // For text LCP, we should only have ttfb and renderDelay sections.
    if (!phases?.loadDelay && !phases?.loadTime) {
      const renderBegin: TraceEngine.Types.Timing.MicroSeconds = TraceEngine.Types.Timing.MicroSeconds(
          lcpMicroseconds - TraceEngine.Helpers.Timing.millisecondsToMicroseconds(phases.renderDelay));
      const renderDelay = TraceEngine.Helpers.Timing.traceWindowFromMicroSeconds(
          renderBegin,
          lcpMicroseconds,
      );

      const mainReqStart = TraceEngine.Types.Timing.MicroSeconds(
          renderBegin - TraceEngine.Helpers.Timing.millisecondsToMicroseconds(phases.ttfb));
      const ttfb = TraceEngine.Helpers.Timing.traceWindowFromMicroSeconds(
          mainReqStart,
          renderBegin,
      );
      sections.push(
          {bounds: ttfb, label: i18nString(UIStrings.timeToFirstByte), showDuration: true},
          {bounds: renderDelay, label: i18nString(UIStrings.elementRenderDelay), showDuration: true});
    } else if (phases?.loadDelay && phases?.loadTime) {
      const renderBegin: TraceEngine.Types.Timing.MicroSeconds = TraceEngine.Types.Timing.MicroSeconds(
          lcpMicroseconds - TraceEngine.Helpers.Timing.millisecondsToMicroseconds(phases.renderDelay));
      const renderDelay = TraceEngine.Helpers.Timing.traceWindowFromMicroSeconds(
          renderBegin,
          lcpMicroseconds,
      );

      const loadBegin = TraceEngine.Types.Timing.MicroSeconds(
          renderBegin - TraceEngine.Helpers.Timing.millisecondsToMicroseconds(phases.loadTime));
      const loadTime = TraceEngine.Helpers.Timing.traceWindowFromMicroSeconds(
          loadBegin,
          renderBegin,
      );

      const loadDelayStart = TraceEngine.Types.Timing.MicroSeconds(
          loadBegin - TraceEngine.Helpers.Timing.millisecondsToMicroseconds(phases.loadDelay));
      const loadDelay = TraceEngine.Helpers.Timing.traceWindowFromMicroSeconds(
          loadDelayStart,
          loadBegin,
      );

      const mainReqStart = TraceEngine.Types.Timing.MicroSeconds(
          loadDelayStart - TraceEngine.Helpers.Timing.millisecondsToMicroseconds(phases.ttfb));
      const ttfb = TraceEngine.Helpers.Timing.traceWindowFromMicroSeconds(
          mainReqStart,
          loadDelayStart,
      );

      sections.push(
          {bounds: ttfb, label: i18nString(UIStrings.timeToFirstByte), showDuration: true},
          {bounds: loadDelay, label: i18nString(UIStrings.resourceLoadDelay), showDuration: true},
          {bounds: loadTime, label: i18nString(UIStrings.resourceLoadDuration), showDuration: true},
          {bounds: renderDelay, label: i18nString(UIStrings.elementRenderDelay), showDuration: true},
      );
    }
    return [{
      type: 'TIMESPAN_BREAKDOWN',
      sections,
    }];
  }

  #renderLCPPhases(phaseData: PhaseData[]): LitHtml.LitTemplate {
    const rows = phaseData.map(({phase, percent}) => [phase, percent]);

    // clang-format off
    return LitHtml.html`
    <div class="insights">
      <${SidebarInsight.SidebarInsight.litTagName} .data=${{
            title: this.userVisibleTitle,
            expanded: this.isActive(),
        } as SidebarInsight.InsightDetails}
        @insighttoggleclick=${this.onSidebarClick}
      >
        <div slot="insight-description" class="insight-description">
          Each
          <x-link class="link" href="https://web.dev/articles/optimize-lcp#lcp-breakdown">phase has specific recommendations to improve.</x-link>
          In an ideal load, the two delay phases should be quite short.
        </div>
        <div slot="insight-content">
          ${LitHtml.html`<${Table.litTagName}
            .data=${{
              headers: ['Phase', '% of LCP'],
              rows,
            } as TableData}>
          </${Table.litTagName}>`}
        </div>
      </${SidebarInsight}>
    </div>`;
    // clang-format on
  }

  #hasDataToRender(phaseData: PhaseData[]): boolean {
    return phaseData ? phaseData.length > 0 : false;
  }

  override render(): void {
    const phaseData = this.#getPhaseData(this.data.insights, this.data.navigationId);
    const matchesCategory = shouldRenderForCategory({
      activeCategory: this.data.activeCategory,
      insightCategory: this.insightCategory,
    });
    const shouldRender = matchesCategory && this.#hasDataToRender(phaseData);
    const output = shouldRender ? this.#renderLCPPhases(phaseData) : LitHtml.nothing;
    LitHtml.render(output, this.shadow, {host: this});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'devtools-performance-lcp-by-phases': LCPPhases;
  }
}

customElements.define('devtools-performance-lcp-by-phases', LCPPhases);
