/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import * as Common from '../../core/common/common.js';
import * as i18n from '../../core/i18n/i18n.js';
import type * as Platform from '../../core/platform/platform.js';
import * as Root from '../../core/root/root.js';
import * as Bindings from '../../models/bindings/bindings.js';
import type * as TimelineModel from '../../models/timeline_model/timeline_model.js';
import * as TraceEngine from '../../models/trace/trace.js';
import * as PerfUI from '../../ui/legacy/components/perf_ui/perf_ui.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as ThemeSupport from '../../ui/legacy/theme_support/theme_support.js';

import {CompatibilityTracksAppender, type TrackAppenderName} from './CompatibilityTracksAppender.js';
import * as Components from './components/components.js';
import {ExtensionDataGatherer} from './ExtensionDataGatherer.js';
import {initiatorsDataToDraw} from './Initiators.js';
import {ModificationsManager} from './ModificationsManager.js';
import {ThreadAppender} from './ThreadAppender.js';
import timelineFlamechartPopoverStyles from './timelineFlamechartPopover.css.js';
import {FlameChartStyle, Selection} from './TimelineFlameChartView.js';
import {TimelineSelection} from './TimelineSelection.js';

const UIStrings = {
  /**
   *@description Text for rendering frames
   */
  frames: 'Frames',
  /**
   *@description Text in Timeline Flame Chart Data Provider of the Performance panel
   */
  idleFrame: 'Idle Frame',
  /**
   *@description Text in Timeline Frame Chart Data Provider of the Performance panel
   */
  droppedFrame: 'Dropped Frame',
  /**
   *@description Text in Timeline Frame Chart Data Provider of the Performance panel
   */
  partiallyPresentedFrame: 'Partially Presented Frame',
  /**
   *@description Text for a rendering frame
   */
  frame: 'Frame',
};
const str_ = i18n.i18n.registerUIStrings('panels/timeline/TimelineFlameChartDataProvider.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

// Frames are special cased because we don't render them as a normal track, hence the distinction here.
export type TimelineFlameChartEntry =
    TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame|TraceEngine.Types.TraceEvents.TraceEventData;

export class TimelineFlameChartDataProvider extends Common.ObjectWrapper.ObjectWrapper<EventTypes> implements
    PerfUI.FlameChart.FlameChartDataProvider {
  private droppedFramePatternCanvas: HTMLCanvasElement;
  private partialFramePatternCanvas: HTMLCanvasElement;
  private timelineDataInternal: PerfUI.FlameChart.FlameChartTimelineData|null;
  private currentLevel: number;

  // The Performance and the Timeline models are expected to be
  // deprecated in favor of using traceEngineData (new RPP engine) only
  // as part of the work in crbug.com/1386091. For this reason they
  // have the "legacy" prefix on their name.
  private compatibilityTracksAppender: CompatibilityTracksAppender|null;
  private traceEngineData: TraceEngine.Handlers.Types.TraceParseData|null;
  private isCpuProfile = false;

  private minimumBoundaryInternal: number;
  private timeSpan: number;
  private readonly headerLevel1: PerfUI.FlameChart.GroupStyle;
  private readonly headerLevel2: PerfUI.FlameChart.GroupStyle;
  private readonly staticHeader: PerfUI.FlameChart.GroupStyle;
  private framesHeader: PerfUI.FlameChart.GroupStyle;
  private readonly screenshotsHeader: PerfUI.FlameChart.GroupStyle;
  private entryData!: TimelineFlameChartEntry[];
  private entryTypeByLevel!: EntryType[];
  private screenshotImageCache!: Map<TraceEngine.Types.TraceEvents.SyntheticScreenshot, HTMLImageElement|null>;
  private entryIndexToTitle!: string[];
  private lastInitiatorEntry!: number;
  private lastInitiatorsData: PerfUI.FlameChart.FlameChartInitiatorData[] = [];
  private lastSelection?: Selection;
  #font: string;
  #eventIndexByEvent: WeakMap<
      TraceEngine.Types.TraceEvents.TraceEventData|TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame,
      number|null> = new WeakMap();

  constructor() {
    super();
    this.reset();
    this.#font = `${PerfUI.Font.DEFAULT_FONT_SIZE} ${PerfUI.Font.getFontFamilyForCanvas()}`;
    this.droppedFramePatternCanvas = document.createElement('canvas');
    this.partialFramePatternCanvas = document.createElement('canvas');
    this.preparePatternCanvas();
    this.timelineDataInternal = null;
    this.currentLevel = 0;
    this.compatibilityTracksAppender = null;
    this.traceEngineData = null;
    this.minimumBoundaryInternal = 0;
    this.timeSpan = 0;

    this.headerLevel1 = this.buildGroupStyle({shareHeaderLine: false});
    this.headerLevel2 = this.buildGroupStyle({padding: 2, nestingLevel: 1, collapsible: false});
    this.staticHeader = this.buildGroupStyle({collapsible: false});
    this.framesHeader = this.buildGroupStyle({useFirstLineForOverview: true});
    this.screenshotsHeader =
        this.buildGroupStyle({useFirstLineForOverview: true, nestingLevel: 1, collapsible: false, itemsHeight: 150});

    ThemeSupport.ThemeSupport.instance().addEventListener(ThemeSupport.ThemeChangeEvent.eventName, () => {
      const headers = [
        this.headerLevel1,
        this.headerLevel2,
        this.staticHeader,
        this.framesHeader,
        this.screenshotsHeader,
      ];
      for (const header of headers) {
        header.color = ThemeSupport.ThemeSupport.instance().getComputedValue('--sys-color-on-surface');
        header.backgroundColor =
            ThemeSupport.ThemeSupport.instance().getComputedValue('--sys-color-cdt-base-container');
      }
    });
  }

  hasTrackConfigurationMode(): boolean {
    return true;
  }

  modifyTree(node: number, action: TraceEngine.EntriesFilter.FilterAction): void {
    const entry = this.entryData[node] as TraceEngine.Types.TraceEvents.SyntheticTraceEntry;

    ModificationsManager.activeManager()?.getEntriesFilter().applyFilterAction({type: action, entry});
  }

  findPossibleContextMenuActions(node: number): TraceEngine.EntriesFilter.PossibleFilterActions|void {
    const entry = this.entryData[node] as TraceEngine.Types.TraceEvents.SyntheticTraceEntry;
    return ModificationsManager.activeManager()?.getEntriesFilter().findPossibleActions(entry);
  }

  private buildGroupStyle(extra: Object): PerfUI.FlameChart.GroupStyle {
    const defaultGroupStyle = {
      padding: 4,
      height: 17,
      collapsible: true,
      color: ThemeSupport.ThemeSupport.instance().getComputedValue('--sys-color-on-surface'),
      backgroundColor: ThemeSupport.ThemeSupport.instance().getComputedValue('--sys-color-cdt-base-container'),
      nestingLevel: 0,
      shareHeaderLine: true,
    };
    return Object.assign(defaultGroupStyle, extra);
  }

  setModel(traceEngineData: TraceEngine.Handlers.Types.TraceParseData|null, isCpuProfile = false): void {
    this.reset();
    this.traceEngineData = traceEngineData;
    ExtensionDataGatherer.instance().modelChanged(traceEngineData);

    this.isCpuProfile = isCpuProfile;
    if (traceEngineData) {
      const {traceBounds} = traceEngineData.Meta;
      const minTime = TraceEngine.Helpers.Timing.microSecondsToMilliseconds(traceBounds.min);
      const maxTime = TraceEngine.Helpers.Timing.microSecondsToMilliseconds(traceBounds.max);
      this.minimumBoundaryInternal = minTime;
      this.timeSpan = minTime === maxTime ? 1000 : maxTime - this.minimumBoundaryInternal;
    }
  }

  /**
   * Instances and caches a CompatibilityTracksAppender using the
   * internal flame chart data and the trace parsed data coming from the
   * trace engine.
   * The model data must have been set to the data provider instance before
   * attempting to instance the CompatibilityTracksAppender.
   */
  compatibilityTracksAppenderInstance(forceNew = false): CompatibilityTracksAppender {
    if (!this.compatibilityTracksAppender || forceNew) {
      if (!this.traceEngineData) {
        throw new Error(
            'Attempted to instantiate a CompatibilityTracksAppender without having set the trace parse data first.');
      }
      this.timelineDataInternal = this.#instantiateTimelineData();
      this.compatibilityTracksAppender = new CompatibilityTracksAppender(
          this.timelineDataInternal, this.traceEngineData, this.entryData, this.entryTypeByLevel);
    }
    return this.compatibilityTracksAppender;
  }

  /**
   * Returns the instance of the timeline flame chart data, without
   * adding data to it. In case the timeline data hasn't been instanced
   * creates a new instance and returns it.
   */
  #instantiateTimelineData(): PerfUI.FlameChart.FlameChartTimelineData {
    if (!this.timelineDataInternal) {
      this.timelineDataInternal = PerfUI.FlameChart.FlameChartTimelineData.createEmpty();
    }
    return this.timelineDataInternal;
  }

  /**
   * Builds the flame chart data using the track appenders
   */
  buildFromTrackAppenders(options?: {filterThreadsByName?: string, expandedTracks?: Set<TrackAppenderName>}): void {
    if (!this.compatibilityTracksAppender) {
      return;
    }
    const appenders = this.compatibilityTracksAppender.allVisibleTrackAppenders();
    for (const appender of appenders) {
      const skipThreadAppenderByName =
          appender instanceof ThreadAppender && !appender.trackName().includes(options?.filterThreadsByName || '');
      if (skipThreadAppenderByName) {
        continue;
      }
      const expanded = Boolean(options?.expandedTracks?.has(appender.appenderName));
      this.currentLevel = appender.appendTrackAtLevel(this.currentLevel, expanded);
    }
  }

  groupTreeEvents(group: PerfUI.FlameChart.Group): TraceEngine.Types.TraceEvents.TraceEventData[]|null {
    return this.compatibilityTracksAppender?.groupEventsForTreeView(group) ?? null;
  }

  mainFrameNavigationStartEvents(): readonly TraceEngine.Types.TraceEvents.TraceEventNavigationStart[] {
    if (!this.traceEngineData) {
      return [];
    }
    return this.traceEngineData.Meta.mainFrameNavigations;
  }

  entryTitle(entryIndex: number): string|null {
    const entryType = this.#entryTypeForIndex(entryIndex);
    if (entryType === EntryType.Screenshot) {
      return '';
    }
    if (entryType === EntryType.TrackAppender) {
      const timelineData = (this.timelineDataInternal as PerfUI.FlameChart.FlameChartTimelineData);
      const eventLevel = timelineData.entryLevels[entryIndex];
      const event = (this.entryData[entryIndex] as TraceEngine.Types.TraceEvents.TraceEventData);
      return this.compatibilityTracksAppender?.titleForEvent(event, eventLevel) || null;
    }
    let title: Common.UIString.LocalizedString|string = this.entryIndexToTitle[entryIndex];
    if (!title) {
      title = `Unexpected entryIndex ${entryIndex}`;
      console.error(title);
    }
    return title;
  }

  textColor(index: number): string {
    const event = this.entryData[index];
    if (!TimelineFlameChartDataProvider.timelineEntryIsTraceEvent(event)) {
      return FlameChartStyle.textColor;
    }
    return this.isIgnoreListedEvent(event) ? '#888' : FlameChartStyle.textColor;
  }

  entryFont(_index: number): string|null {
    return this.#font;
  }

  // resetCompatibilityTracksAppender boolean set to false does not recreate the thread appenders
  reset(resetCompatibilityTracksAppender: boolean = true): void {
    this.currentLevel = 0;
    this.entryData = [];
    this.entryTypeByLevel = [];
    this.entryIndexToTitle = [];
    this.screenshotImageCache = new Map();
    this.#eventIndexByEvent = new Map();
    if (resetCompatibilityTracksAppender) {
      this.compatibilityTracksAppender = null;
      this.timelineDataInternal = null;
    } else if (!resetCompatibilityTracksAppender && this.timelineDataInternal) {
      this.compatibilityTracksAppender?.setFlameChartDataAndEntryData(
          this.timelineDataInternal, this.entryData, this.entryTypeByLevel);
      this.compatibilityTracksAppender?.threadAppenders().forEach(
          threadAppender => threadAppender.setHeaderAppended(false));
    }
  }

  maxStackDepth(): number {
    return this.currentLevel;
  }

  /**
   * Builds the flame chart data using the tracks appender (which use
   * the new trace engine) and the legacy code paths present in this
   * file. The result built data is cached and returned.
   */
  timelineData(rebuild: boolean = false): PerfUI.FlameChart.FlameChartTimelineData {
    if (this.timelineDataInternal && this.timelineDataInternal.entryLevels.length !== 0 && !rebuild) {
      // The flame chart data is built already, so return the cached
      // data.
      return this.timelineDataInternal;
    }

    this.timelineDataInternal = PerfUI.FlameChart.FlameChartTimelineData.createEmpty();

    if (rebuild) {
      this.reset(/* resetCompatibilityTracksAppender= */ false);
    }

    this.currentLevel = 0;

    if (this.traceEngineData) {
      this.compatibilityTracksAppender = this.compatibilityTracksAppenderInstance();
      if (this.traceEngineData.Meta.traceIsGeneric) {
        this.#processGenericTrace();
      } else {
        this.#processInspectorTrace();
      }
    }
    return this.timelineDataInternal;
  }

  /**
   * Register the groups (aka tracks) with the VisualElements framework so
   * later on we can log when an entry inside this group is selected.
   */
  #processGenericTrace(): void {
    if (!this.compatibilityTracksAppender) {
      return;
    }

    const appendersByProcess = this.compatibilityTracksAppender.allThreadAppendersByProcess();

    for (const [pid, threadAppenders] of appendersByProcess) {
      const processGroupStyle = this.buildGroupStyle({shareHeaderLine: false});
      const processName = this.traceEngineData?.Meta.processNames.get(pid)?.args.name || 'Process';
      this.appendHeader(`${processName} (${pid})`, processGroupStyle, true, false);
      for (const appender of threadAppenders) {
        appender.setHeaderNestingLevel(1);
        this.currentLevel = appender.appendTrackAtLevel(this.currentLevel);
      }
    }
  }

  #processInspectorTrace(): void {
    if (!this.isCpuProfile) {
      // CPU Profiles do not have frames and screenshots.
      this.#appendFramesAndScreenshotsTrack();
    }

    const weight = (track: {type?: string, forMainFrame?: boolean, appenderName?: TrackAppenderName}): number => {
      switch (track.appenderName) {
        case 'Animations':
          return 0;
        case 'Timings':
          return 1;
        case 'Interactions':
          return 2;
        case 'LayoutShifts':
          return 3;
        case 'Extension':
          return 4;
        case 'Thread':
          return 5;
        case 'GPU':
          return 6;
        case 'Thread_AuctionWorklet':
          return 7;
        default:
          return 8;
      }
    };

    const allTrackAppenders =
        this.compatibilityTracksAppender ? this.compatibilityTracksAppender.allVisibleTrackAppenders() : [];

    allTrackAppenders.sort((a, b) => weight(a) - weight(b));

    for (const appender of allTrackAppenders) {
      if (!this.traceEngineData) {
        continue;
      }

      this.currentLevel = appender.appendTrackAtLevel(this.currentLevel);

      // If there is not a selected group, we want to default to selecting the
      // main thread track. Therefore in this check we look to see if the
      // current appender is a ThreadAppender and represnets the Main Thread.
      // If it is, we mark the group as selected.
      if (this.timelineDataInternal && !this.timelineDataInternal.selectedGroup) {
        if (appender instanceof ThreadAppender &&
            (appender.threadType === TraceEngine.Handlers.Threads.ThreadType.MAIN_THREAD ||
             appender.threadType === TraceEngine.Handlers.Threads.ThreadType.CPU_PROFILE)) {
          const group = this.compatibilityTracksAppender?.groupForAppender(appender);
          if (group) {
            this.timelineDataInternal.selectedGroup = group;
          }
        }
      }
    }
    if (this.timelineDataInternal && this.timelineDataInternal.selectedGroup) {
      this.timelineDataInternal.selectedGroup.expanded = true;
    }
  }

  minimumBoundary(): number {
    return this.minimumBoundaryInternal;
  }

  totalTime(): number {
    return this.timeSpan;
  }

  static timelineEntryIsTraceEvent(entry: TimelineFlameChartEntry):
      entry is TraceEngine.Types.TraceEvents.TraceEventData {
    return entry instanceof TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame === false;
  }

  search(
      startTime: TraceEngine.Types.Timing.MilliSeconds, endTime: TraceEngine.Types.Timing.MilliSeconds,
      filter: TimelineModel.TimelineModelFilter.TimelineModelFilter): number[] {
    const result = [];
    this.timelineData();
    for (let i = 0; i < this.entryData.length; ++i) {
      const entry = this.entryData[i];
      if (!entry) {
        continue;
      }

      if (!TimelineFlameChartDataProvider.timelineEntryIsTraceEvent(entry)) {
        // We only search for events, not for frames, hence this early exit.
        continue;
      }

      if (TraceEngine.Types.TraceEvents.isTraceEventScreenshot(entry)) {
        // Screenshots are represented as trace events, but you can't search for them, so skip.
        continue;
      }

      const entryStartTime = TraceEngine.Helpers.Timing.eventTimingsMilliSeconds(entry).startTime;
      const entryEndTime = TraceEngine.Helpers.Timing.eventTimingsMilliSeconds(entry).endTime;

      if (entryStartTime > endTime) {
        continue;
      }
      if ((entryEndTime || entryStartTime) < startTime) {
        continue;
      }
      if (filter.accept(entry, this.traceEngineData || undefined)) {
        result.push(i);
      }
    }
    result.sort((a, b) => {
      const firstEvent = this.entryData.at(a);
      if (!firstEvent) {
        return 0;
      }
      const secondEvent = this.entryData.at(b);
      if (!secondEvent) {
        return 0;
      }

      if (!TimelineFlameChartDataProvider.timelineEntryIsTraceEvent(firstEvent) ||
          !TimelineFlameChartDataProvider.timelineEntryIsTraceEvent(secondEvent)) {
        return 0;
      }

      return TraceEngine.Helpers.Trace.eventTimeComparator(firstEvent, secondEvent);
    });
    return result;
  }

  isIgnoreListedEvent(event: TraceEngine.Types.TraceEvents.TraceEventData): boolean {
    if (TraceEngine.Types.TraceEvents.isProfileCall(event)) {
      return this.isIgnoreListedURL(event.callFrame.url as Platform.DevToolsPath.UrlString);
    }
    return false;
  }

  private isIgnoreListedURL(url: Platform.DevToolsPath.UrlString): boolean {
    return Bindings.IgnoreListManager.IgnoreListManager.instance().isUserIgnoreListedURL(url);
  }

  getEntryTypeForLevel(level: number): EntryType {
    return this.entryTypeByLevel[level];
  }

  /**
   * The frames and screenshots track is special cased because it is rendered
   * differently to the rest of the tracks and not as a series of events. This
   * is why it is not done via the appender system; we track frames &
   * screenshots as a different EntryType to the TrackAppender entries,
   * because then when it comes to drawing we can decorate them differently.
   **/
  #appendFramesAndScreenshotsTrack(): void {
    if (!this.traceEngineData) {
      return;
    }
    const filmStrip = TraceEngine.Extras.FilmStrip.fromTraceData(this.traceEngineData);
    const hasScreenshots = filmStrip.frames.length > 0;

    this.framesHeader.collapsible = hasScreenshots;
    const expanded = Root.Runtime.Runtime.queryParam('flamechart-force-expand') === 'frames';

    this.appendHeader(i18nString(UIStrings.frames), this.framesHeader, false /* selectable */, expanded);

    this.entryTypeByLevel[this.currentLevel] = EntryType.Frame;
    for (const frame of this.traceEngineData.Frames.frames) {
      this.#appendNewEngineFrame(frame);
    }
    ++this.currentLevel;

    if (!hasScreenshots) {
      return;
    }
    this.#appendScreenshots(filmStrip);
  }

  #appendScreenshots(filmStrip: TraceEngine.Extras.FilmStrip.Data): void {
    if (!this.timelineDataInternal || !this.traceEngineData) {
      return;
    }
    this.appendHeader('', this.screenshotsHeader, false /* selectable */);
    this.entryTypeByLevel[this.currentLevel] = EntryType.Screenshot;
    let prevTimestamp: TraceEngine.Types.Timing.MilliSeconds|undefined = undefined;

    for (const filmStripFrame of filmStrip.frames) {
      const screenshotTimeInMilliSeconds =
          TraceEngine.Helpers.Timing.microSecondsToMilliseconds(filmStripFrame.screenshotEvent.ts);
      this.entryData.push(filmStripFrame.screenshotEvent);
      (this.timelineDataInternal.entryLevels as number[]).push(this.currentLevel);
      (this.timelineDataInternal.entryStartTimes as number[]).push(screenshotTimeInMilliSeconds);
      if (prevTimestamp) {
        (this.timelineDataInternal.entryTotalTimes as number[]).push(screenshotTimeInMilliSeconds - prevTimestamp);
      }
      prevTimestamp = screenshotTimeInMilliSeconds;
    }
    if (filmStrip.frames.length && prevTimestamp !== undefined) {
      const maxRecordTimeMillis =
          TraceEngine.Helpers.Timing.traceWindowMilliSeconds(this.traceEngineData.Meta.traceBounds).max;

      // Set the total time of the final screenshot so it takes up the remainder of the trace.
      (this.timelineDataInternal.entryTotalTimes as number[]).push(maxRecordTimeMillis - prevTimestamp);
    }
    ++this.currentLevel;
  }

  #entryTypeForIndex(entryIndex: number): EntryType {
    const level = this.timelineData().entryLevels[entryIndex];
    return this.entryTypeByLevel[level];
  }
  prepareHighlightedEntryInfo(entryIndex: number): Element|null {
    let time = '';
    let title;
    let warningElements: Element[] = [];
    let nameSpanTimelineInfoTime = 'timeline-info-time';

    const additionalContent: HTMLElement[] = [];

    const entryType = this.#entryTypeForIndex(entryIndex);
    if (entryType === EntryType.TrackAppender) {
      if (!this.compatibilityTracksAppender) {
        return null;
      }
      const event = (this.entryData[entryIndex] as TraceEngine.Types.TraceEvents.TraceEventData);
      const timelineData = (this.timelineDataInternal as PerfUI.FlameChart.FlameChartTimelineData);
      const eventLevel = timelineData.entryLevels[entryIndex];
      const highlightedEntryInfo = this.compatibilityTracksAppender.highlightedEntryInfo(event, eventLevel);
      title = highlightedEntryInfo.title;
      time = highlightedEntryInfo.formattedTime;
      warningElements = highlightedEntryInfo.warningElements || warningElements;
      if (TraceEngine.Types.TraceEvents.isSyntheticInteractionEvent(event)) {
        const breakdown = new Components.InteractionBreakdown.InteractionBreakdown();
        breakdown.entry = event;
        additionalContent.push(breakdown);
      }
    } else if (entryType === EntryType.Frame) {
      const frame = (this.entryData[entryIndex] as TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);
      time = i18n.TimeUtilities.preciseMillisToString(
          TraceEngine.Helpers.Timing.microSecondsToMilliseconds(frame.duration), 1);

      if (frame.idle) {
        title = i18nString(UIStrings.idleFrame);
      } else if (frame.dropped) {
        if (frame.isPartial) {
          title = i18nString(UIStrings.partiallyPresentedFrame);
        } else {
          title = i18nString(UIStrings.droppedFrame);
        }
        nameSpanTimelineInfoTime = 'timeline-info-warning';
      } else {
        title = i18nString(UIStrings.frame);
      }
    } else {
      return null;
    }

    const element = document.createElement('div');
    const root = UI.UIUtils.createShadowRootWithCoreStyles(element, {
      cssFile: [timelineFlamechartPopoverStyles],
      delegatesFocus: undefined,
    });
    const contents = root.createChild('div', 'timeline-flamechart-popover');
    contents.createChild('span', nameSpanTimelineInfoTime).textContent = time;
    contents.createChild('span', 'timeline-info-title').textContent = title;
    if (warningElements) {
      for (const warningElement of warningElements) {
        warningElement.classList.add('timeline-info-warning');
        contents.appendChild(warningElement);
      }
    }
    for (const elem of additionalContent) {
      contents.appendChild(elem);
    }
    return element;
  }

  prepareHighlightedHiddenEntriesArrowInfo(entryIndex: number): Element|null {
    const element = document.createElement('div');
    const root = UI.UIUtils.createShadowRootWithCoreStyles(element, {
      cssFile: [timelineFlamechartPopoverStyles],
      delegatesFocus: undefined,
    });

    const entry = this.entryData[entryIndex] as TraceEngine.Types.TraceEvents.SyntheticTraceEntry;
    const hiddenEntriesAmount =
        ModificationsManager.activeManager()?.getEntriesFilter().findHiddenDescendantsAmount(entry);

    if (!hiddenEntriesAmount) {
      return null;
    }
    const contents = root.createChild('div', 'timeline-flamechart-popover');
    contents.createChild('span', 'timeline-info-title').textContent = hiddenEntriesAmount + ' hidden';

    return element;
  }

  entryColor(entryIndex: number): string {
    const entryType = this.#entryTypeForIndex(entryIndex);
    if (entryType === EntryType.Frame) {
      return 'white';
    }
    if (entryType === EntryType.TrackAppender) {
      const timelineData = (this.timelineDataInternal as PerfUI.FlameChart.FlameChartTimelineData);
      const eventLevel = timelineData.entryLevels[entryIndex];
      const event = (this.entryData[entryIndex] as TraceEngine.Types.TraceEvents.TraceEventData);
      return this.compatibilityTracksAppender?.colorForEvent(event, eventLevel) || '';
    }
    return '';
  }

  private preparePatternCanvas(): void {
    // Set the candy stripe pattern to 17px so it repeats well.
    const size = 17;
    this.droppedFramePatternCanvas.width = size;
    this.droppedFramePatternCanvas.height = size;

    this.partialFramePatternCanvas.width = size;
    this.partialFramePatternCanvas.height = size;

    const ctx = this.droppedFramePatternCanvas.getContext('2d');
    if (ctx) {
      // Make a dense solid-line pattern.
      ctx.translate(size * 0.5, size * 0.5);
      ctx.rotate(Math.PI * 0.25);
      ctx.translate(-size * 0.5, -size * 0.5);

      ctx.fillStyle = 'rgb(255, 255, 255)';
      for (let x = -size; x < size * 2; x += 3) {
        ctx.fillRect(x, -size, 1, size * 3);
      }
    }

    const ctx2 = this.partialFramePatternCanvas.getContext('2d');
    if (ctx2) {
      // Make a sparse dashed-line pattern.
      ctx2.strokeStyle = 'rgb(255, 255, 255)';
      ctx2.lineWidth = 2;
      ctx2.beginPath();
      ctx2.moveTo(17, 0);
      ctx2.lineTo(10, 7);
      ctx2.moveTo(8, 9);
      ctx2.lineTo(2, 15);
      ctx2.stroke();
    }
  }

  private drawFrame(
      entryIndex: number, context: CanvasRenderingContext2D, text: string|null, barX: number, barY: number,
      barWidth: number, barHeight: number): void {
    const hPadding = 1;
    const frame = (this.entryData[entryIndex] as TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);
    barX += hPadding;
    barWidth -= 2 * hPadding;
    if (frame.idle) {
      context.fillStyle = 'white';
    } else if (frame.dropped) {
      if (frame.isPartial) {
        // For partially presented frame boxes, paint a yellow background with
        // a sparse white dashed-line pattern overlay.
        context.fillStyle = '#f0e442';
        context.fillRect(barX, barY, barWidth, barHeight);

        const overlay = context.createPattern(this.partialFramePatternCanvas, 'repeat');
        context.fillStyle = overlay || context.fillStyle;
      } else {
        // For dropped frame boxes, paint a red background with a dense white
        // solid-line pattern overlay.
        context.fillStyle = '#f08080';
        context.fillRect(barX, barY, barWidth, barHeight);

        const overlay = context.createPattern(this.droppedFramePatternCanvas, 'repeat');
        context.fillStyle = overlay || context.fillStyle;
      }
    } else {
      context.fillStyle = '#d7f0d1';
    }
    context.fillRect(barX, barY, barWidth, barHeight);

    const frameDurationText = i18n.TimeUtilities.preciseMillisToString(
        TraceEngine.Helpers.Timing.microSecondsToMilliseconds(frame.duration), 1);
    const textWidth = context.measureText(frameDurationText).width;
    if (textWidth <= barWidth) {
      context.fillStyle = this.textColor(entryIndex);
      context.fillText(frameDurationText, barX + (barWidth - textWidth) / 2, barY + barHeight - 4);
    }
  }

  private async drawScreenshot(
      entryIndex: number, context: CanvasRenderingContext2D, barX: number, barY: number, barWidth: number,
      barHeight: number): Promise<void> {
    const screenshot = (this.entryData[entryIndex] as TraceEngine.Types.TraceEvents.SyntheticScreenshot);
    if (!this.screenshotImageCache.has(screenshot)) {
      this.screenshotImageCache.set(screenshot, null);
      const data = screenshot.args.dataUri;
      const image = await UI.UIUtils.loadImage(data);
      this.screenshotImageCache.set(screenshot, image);
      this.dispatchEventToListeners(Events.DataChanged);
      return;
    }

    const image = this.screenshotImageCache.get(screenshot);
    if (!image) {
      return;
    }
    const imageX = barX + 1;
    const imageY = barY + 1;
    const imageHeight = barHeight - 2;
    const scale = imageHeight / image.naturalHeight;
    const imageWidth = Math.floor(image.naturalWidth * scale);
    context.save();
    context.beginPath();
    context.rect(barX, barY, barWidth, barHeight);
    context.clip();
    context.drawImage(image, imageX, imageY, imageWidth, imageHeight);
    context.strokeStyle = '#ccc';
    context.strokeRect(imageX - 0.5, imageY - 0.5, Math.min(barWidth - 1, imageWidth + 1), imageHeight);
    context.restore();
  }

  decorateEntry(
      entryIndex: number, context: CanvasRenderingContext2D, text: string|null, barX: number, barY: number,
      barWidth: number, barHeight: number, unclippedBarX: number, timeToPixelRatio: number): boolean {
    const entryType = this.#entryTypeForIndex(entryIndex);

    if (entryType === EntryType.Frame) {
      this.drawFrame(entryIndex, context, text, barX, barY, barWidth, barHeight);
      return true;
    }

    if (entryType === EntryType.Screenshot) {
      void this.drawScreenshot(entryIndex, context, barX, barY, barWidth, barHeight);
      return true;
    }

    if (entryType === EntryType.TrackAppender) {
      const entry = this.entryData[entryIndex] as TraceEngine.Types.TraceEvents.TraceEventData;
      if (TraceEngine.Types.TraceEvents.isSyntheticInteractionEvent(entry)) {
        this.#drawInteractionEventWithWhiskers(
            context, entryIndex, text, entry, barX, barY, unclippedBarX, barWidth, barHeight, timeToPixelRatio);
        return true;
      }
    }

    return false;
  }

  /**
   * Draws the left and right whiskers around an interaction in the timeline.
   * @param context - the canvas that will be drawn onto
   * @param entryIndex
   * @param entryTitle - the title of the entry
   * @param entry - the entry itself
   * @param barX - the starting X pixel position of the bar representing this event. This is clipped: if the bar is off the left side of the screen, this value will be 0
   * @param barY - the starting Y pixel position of the bar representing this event.
   * @param unclippedBarXStartPixel - the starting X pixel position of the bar representing this event, not clipped. This means if the bar is off the left of the screen this will be a negative number.
   * @param barWidth - the width of the full bar in pixels
   * @param barHeight - the height of the full bar in pixels
   * @param timeToPixelRatio - the ratio required to convert a millisecond time to a pixel value.
   **/
  #drawInteractionEventWithWhiskers(
      context: CanvasRenderingContext2D, entryIndex: number, entryTitle: string|null,
      entry: TraceEngine.Types.TraceEvents.SyntheticInteractionPair, barX: number, barY: number,
      unclippedBarXStartPixel: number, barWidth: number, barHeight: number, timeToPixelRatio: number): void {
    /**
     * An interaction is drawn with whiskers as so:
     * |----------[=======]-------------|
     * => The left whisker is the event's start time (event.ts)
     * => The box start is the event's processingStart time
     * => The box end is the event's processingEnd time
     * => The right whisker is the event's end time (event.ts + event.dur)
     *
     * When we draw the event in the InteractionsAppender, we draw a huge box
     * that spans the entire of the above. So here we need to draw over the
     * rectangle that is outside of {processingStart, processingEnd} and
     * replace it with the whiskers.
     * TODO(crbug.com/1495248): rework how we draw whiskers to avoid this inefficiency
     */

    const beginTime = TraceEngine.Helpers.Timing.microSecondsToMilliseconds(entry.ts);
    const entireBarEndXPixel = barX + barWidth;

    function timeToPixel(time: TraceEngine.Types.Timing.MicroSeconds): number {
      const timeMilli = TraceEngine.Helpers.Timing.microSecondsToMilliseconds(time);
      return Math.floor(unclippedBarXStartPixel + (timeMilli - beginTime) * timeToPixelRatio);
    }

    context.save();

    // Clear portions of initial rect to prepare for the ticks.
    context.fillStyle = ThemeSupport.ThemeSupport.instance().getComputedValue('--sys-color-cdt-base-container');
    let desiredBoxStartX = timeToPixel(entry.processingStart);
    const desiredBoxEndX = timeToPixel(entry.processingEnd);

    // If the entry has no processing duration, ensure the box is 1px wide so at least it is visible.
    if (entry.processingEnd - entry.processingStart === 0) {
      desiredBoxStartX -= 1;
    }

    context.fillRect(barX, barY - 0.5, desiredBoxStartX - barX, barHeight);
    context.fillRect(desiredBoxEndX, barY - 0.5, entireBarEndXPixel - desiredBoxEndX, barHeight);

    // Draws left and right whiskers
    function drawTick(begin: number, end: number, y: number): void {
      const tickHeightPx = 6;
      context.moveTo(begin, y - tickHeightPx / 2);
      context.lineTo(begin, y + tickHeightPx / 2);
      context.moveTo(begin, y);
      context.lineTo(end, y);
    }

    // The left whisker starts at the enty timestamp, and continues until the start of the box (processingStart).
    const leftWhiskerX = timeToPixel(entry.ts);
    // The right whisker ends at (entry.ts + entry.dur). We draw the line from the end of the box (processingEnd).
    const rightWhiskerX = timeToPixel(TraceEngine.Types.Timing.MicroSeconds(entry.ts + entry.dur));
    context.beginPath();
    context.lineWidth = 1;
    context.strokeStyle = '#ccc';
    const lineY = Math.floor(barY + barHeight / 2) + 0.5;
    const leftTick = leftWhiskerX + 0.5;
    const rightTick = rightWhiskerX - 0.5;
    drawTick(leftTick, desiredBoxStartX, lineY);
    drawTick(rightTick, desiredBoxEndX, lineY);
    context.stroke();

    if (entryTitle) {
      // BarX will be set to 0 if the start of the box if off the screen to the
      // left. If this happens, the desiredBoxStartX will be negative. In that
      // case, we fallback to the BarX. This ensures that even if the box
      // starts off-screen, we draw the text at the first visible on screen
      // pixels, so the user can still see the event's title.
      const textStartX = desiredBoxStartX > 0 ? desiredBoxStartX : barX;
      context.font = this.#font;
      const textWidth = UI.UIUtils.measureTextWidth(context, entryTitle);

      // These numbers are duplicated from FlameChart.ts.
      const textPadding = 5;
      const textBaseline = 5;

      // Only draw the text if it can fit in the amount of box that is visible.
      if (textWidth <= desiredBoxEndX - textStartX + textPadding) {
        context.fillStyle = this.textColor(entryIndex);
        context.fillText(entryTitle, textStartX + textPadding, barY + barHeight - textBaseline);
      }
    }
    context.restore();
  }

  forceDecoration(entryIndex: number): boolean {
    const entryType = this.#entryTypeForIndex(entryIndex);
    if (entryType === EntryType.Frame) {
      return true;
    }
    if (entryType === EntryType.Screenshot) {
      return true;
    }
    const event = (this.entryData[entryIndex] as TraceEngine.Types.TraceEvents.TraceEventData);

    if (TraceEngine.Types.TraceEvents.isSyntheticInteractionEvent(event)) {
      // We draw interactions with whiskers, which are done via the
      // decorateEntry() method, hence we always want to force these to be
      // decorated.
      return true;
    }
    return Boolean(this.traceEngineData?.Warnings.perEvent.get(event));
  }

  private appendHeader(title: string, style: PerfUI.FlameChart.GroupStyle, selectable: boolean, expanded?: boolean):
      PerfUI.FlameChart.Group {
    const group =
        ({startLevel: this.currentLevel, name: title, style: style, selectable: selectable, expanded} as
         PerfUI.FlameChart.Group);
    (this.timelineDataInternal as PerfUI.FlameChart.FlameChartTimelineData).groups.push(group);
    return group;
  }

  #appendNewEngineFrame(frame: TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame): void {
    const index = this.entryData.length;
    this.entryData.push(frame);
    const durationMilliseconds = TraceEngine.Helpers.Timing.microSecondsToMilliseconds(frame.duration);
    this.entryIndexToTitle[index] = i18n.TimeUtilities.millisToString(durationMilliseconds, true);
    if (!this.timelineDataInternal) {
      return;
    }
    this.timelineDataInternal.entryLevels[index] = this.currentLevel;
    this.timelineDataInternal.entryTotalTimes[index] = durationMilliseconds;
    this.timelineDataInternal.entryStartTimes[index] =
        TraceEngine.Helpers.Timing.microSecondsToMilliseconds(frame.startTime);
  }

  createSelection(entryIndex: number): TimelineSelection|null {
    const entryType = this.#entryTypeForIndex(entryIndex);
    let timelineSelection: TimelineSelection|null = null;
    const entry = this.entryData[entryIndex];
    if (entry && TimelineFlameChartDataProvider.timelineEntryIsTraceEvent(entry)) {
      timelineSelection = TimelineSelection.fromTraceEvent(entry);
    } else if (entryType === EntryType.Frame) {
      timelineSelection = TimelineSelection.fromFrame(
          (this.entryData[entryIndex] as TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame));
    }
    if (timelineSelection) {
      this.lastSelection = new Selection(timelineSelection, entryIndex);
    }
    return timelineSelection;
  }

  formatValue(value: number, precision?: number): string {
    return i18n.TimeUtilities.preciseMillisToString(value, precision);
  }

  canJumpToEntry(_entryIndex: number): boolean {
    return false;
  }

  entryIndexForSelection(selection: TimelineSelection|null): number {
    if (!selection || TimelineSelection.isRangeSelection(selection.object) ||
        TimelineSelection.isSyntheticNetworkRequestDetailsEventSelection(selection.object)) {
      return -1;
    }

    if (this.lastSelection && this.lastSelection.timelineSelection.object === selection.object) {
      return this.lastSelection.entryIndex;
    }

    // If the index is -1 and the selection is a TraceEvent, it might be
    // the case that this Entry is hidden by the Context Menu action.
    // Try revealing the entry and getting the index again.
    if (this.entryData.indexOf(selection.object) === -1 && TimelineSelection.isTraceEventSelection(selection.object)) {
      if (this.timelineDataInternal?.selectedGroup) {
        ModificationsManager.activeManager()?.getEntriesFilter().revealEntry(
            selection.object as TraceEngine.Types.TraceEvents.SyntheticTraceEntry);
        this.timelineData(true);
      }
    }

    const index = this.entryData.indexOf(selection.object);
    if (index !== -1) {
      this.lastSelection = new Selection(selection, index);
    }
    return index;
  }

  indexForEvent(targetEvent: TraceEngine.Types.TraceEvents.TraceEventData|
                TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame): number|null {
    // Gets the index for the given event by walking through the array of entryData.
    // This may seem inefficient - but we have seen that by building up large
    // maps keyed by trace events that this has a significant impact on the
    // performance of the panel.
    // Therefore, we strike a middle ground: look up the event the first time,
    // but then cache the result.
    const fromCache = this.#eventIndexByEvent.get(targetEvent);
    if (fromCache) {
      return fromCache;
    }
    const index = this.entryData.indexOf(targetEvent);
    const result = index > -1 ? index : null;
    this.#eventIndexByEvent.set(targetEvent, result);
    return result;
  }

  /**
   * Build the data for initiators and initiated entries.
   * @param entryIndex
   * @returns if we should re-render the flame chart (canvas)
   */
  buildFlowForInitiator(entryIndex: number): boolean {
    if (!this.traceEngineData) {
      return false;
    }
    if (!this.timelineDataInternal) {
      return false;
    }
    if (this.lastInitiatorEntry === entryIndex) {
      if (this.lastInitiatorsData) {
        this.timelineDataInternal.initiatorsData = this.lastInitiatorsData;
      }
      return false;
    }
    if (!this.compatibilityTracksAppender) {
      return false;
    }

    // Remove all previously assigned decorations indicating that the flow event entries are hidden
    const previousInitiatorsDataLength = this.timelineDataInternal.initiatorsData.length;
    // |entryIndex| equals -1 means there is no entry selected, just clear the
    // initiator cache if there is any previous arrow and return true to
    // re-render.
    if (entryIndex === -1) {
      this.lastInitiatorEntry = entryIndex;
      if (previousInitiatorsDataLength === 0) {
        // This means there is no arrow before, so we don't need to re-render.
        return false;
      }
      // Reset to clear any previous arrows from the last event.
      this.timelineDataInternal.resetFlowData();
      return true;
    }

    const entryType = this.#entryTypeForIndex(entryIndex);
    if (entryType !== EntryType.TrackAppender) {
      return false;
    }
    const event = this.entryData[entryIndex] as TraceEngine.Types.TraceEvents.TraceEventData;
    // Reset to clear any previous arrows from the last event.
    this.timelineDataInternal.resetFlowData();
    this.lastInitiatorEntry = entryIndex;

    const hiddenEvents: TraceEngine.Types.TraceEvents.TraceEventData[] =
        ModificationsManager.activeManager()?.getEntriesFilter().invisibleEntries() ?? [];
    const expandableEntries: TraceEngine.Types.TraceEvents.TraceEventData[] =
        ModificationsManager.activeManager()?.getEntriesFilter().expandableEntries() ?? [];

    const initiatorsData = initiatorsDataToDraw(
        this.traceEngineData,
        event,
        hiddenEvents,
        expandableEntries,
    );
    // This means there is no change for arrows.
    if (previousInitiatorsDataLength === 0 && initiatorsData.length === 0) {
      return false;
    }
    for (const intiatorData of initiatorsData) {
      const eventIndex = this.indexForEvent(intiatorData.event);
      const initiatorIndex = this.indexForEvent(intiatorData.initiator);
      if (eventIndex === null || initiatorIndex === null) {
        continue;
      }
      this.timelineDataInternal.initiatorsData.push({
        initiatorIndex,
        eventIndex,
        isInitiatorHidden: intiatorData.isInitiatorHidden,
        isEntryHidden: intiatorData.isEntryHidden,
      });
    }
    this.lastInitiatorsData = this.timelineDataInternal.initiatorsData;
    return true;
  }

  eventByIndex(entryIndex: number): TraceEngine.Types.TraceEvents.TraceEventData
      |TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame|null {
    if (entryIndex < 0) {
      return null;
    }
    const entryType = this.#entryTypeForIndex(entryIndex);
    if (entryType === EntryType.TrackAppender) {
      return this.entryData[entryIndex] as TraceEngine.Types.TraceEvents.TraceEventData;
    }
    if (entryType === EntryType.Frame) {
      return this.entryData[entryIndex] as TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame;
    }
    return null;
  }
}

export const InstantEventVisibleDurationMs = TraceEngine.Types.Timing.MilliSeconds(0.001);

export const enum Events {
  DataChanged = 'DataChanged',
}

export type EventTypes = {
  [Events.DataChanged]: void,
};

// an entry is a trace event, they are classified into "entry types"
// because some events are rendered differently. For example, screenshot
// events are rendered as images. Checks for entry types allow to have
// different styles, names, etc. for events that look differently.
// In the future we won't have this checks: instead we will forward
// the event to the corresponding "track appender" and it will determine
// how the event shall be rendered.
export const enum EntryType {
  Frame = 'Frame',
  TrackAppender = 'TrackAppender',
  Screenshot = 'Screenshot',
}
