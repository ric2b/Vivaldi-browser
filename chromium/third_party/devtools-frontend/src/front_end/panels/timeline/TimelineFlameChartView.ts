// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import * as Root from '../../core/root/root.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as Bindings from '../../models/bindings/bindings.js';
import * as TraceEngine from '../../models/trace/trace.js';
import * as TraceBounds from '../../services/trace_bounds/trace_bounds.js';
import * as PerfUI from '../../ui/legacy/components/perf_ui/perf_ui.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as VisualLogging from '../../ui/visual_logging/visual_logging.js';

import type * as TimelineComponents from './components/components.js';
import {CountersGraph} from './CountersGraph.js';
import {SHOULD_SHOW_EASTER_EGG} from './EasterEgg.js';
import {ModificationsManager} from './ModificationsManager.js';
import * as Overlays from './overlays/overlays.js';
import {targetForEvent} from './TargetForEvent.js';
import {TimelineDetailsView} from './TimelineDetailsView.js';
import {TimelineRegExp} from './TimelineFilters.js';
import {
  Events as TimelineFlameChartDataProviderEvents,
  TimelineFlameChartDataProvider,
} from './TimelineFlameChartDataProvider.js';
import {TimelineFlameChartNetworkDataProvider} from './TimelineFlameChartNetworkDataProvider.js';
import timelineFlameChartViewStyles from './timelineFlameChartView.css.js';
import {type TimelineModeViewDelegate} from './TimelinePanel.js';
import {TimelineSelection} from './TimelineSelection.js';
import {AggregatedTimelineTreeView} from './TimelineTreeView.js';
import {type TimelineMarkerStyle} from './TimelineUIUtils.js';

const UIStrings = {
  /**
   *@description Text in Timeline Flame Chart View of the Performance panel
   *@example {Frame} PH1
   *@example {10ms} PH2
   */
  sAtS: '{PH1} at {PH2}',
};
const str_ = i18n.i18n.registerUIStrings('panels/timeline/TimelineFlameChartView.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

const MAX_HIGHLIGHTED_SEARCH_ELEMENTS: number = 200;

export class TimelineFlameChartView extends UI.Widget.VBox implements PerfUI.FlameChart.FlameChartDelegate,
                                                                      UI.SearchableView.Searchable {
  private readonly delegate: TimelineModeViewDelegate;
  /**
   * Tracks the indexes of matched entries when the user searches the panel.
   * Defaults to undefined which indicates the user has not searched.
   */
  private searchResults: PerfUI.FlameChart.DataProviderSearchResult[]|undefined = undefined;
  private eventListeners: Common.EventTarget.EventDescriptor[];
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  private readonly networkSplitWidget: UI.SplitWidget.SplitWidget;
  private mainDataProvider: TimelineFlameChartDataProvider;
  private readonly mainFlameChart: PerfUI.FlameChart.FlameChart;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private readonly networkFlameChartGroupExpansionSetting: Common.Settings.Setting<any>;
  private networkDataProvider: TimelineFlameChartNetworkDataProvider;
  private readonly networkFlameChart: PerfUI.FlameChart.FlameChart;
  private readonly networkPane: UI.Widget.VBox;
  private readonly splitResizer: HTMLElement;
  private readonly chartSplitWidget: UI.SplitWidget.SplitWidget;
  private brickGame?: PerfUI.BrickBreaker.BrickBreaker;
  private readonly countersView: CountersGraph;
  private readonly detailsSplitWidget: UI.SplitWidget.SplitWidget;
  private readonly detailsView: TimelineDetailsView;
  private readonly onMainAddEntryLabelAnnotation: (event: Common.EventTarget.EventTargetEvent<number>) => void;
  private readonly onNetworkAddEntryLabelAnnotation: (event: Common.EventTarget.EventTargetEvent<number>) => void;
  private readonly onMainEntriesLinkAnnotationCreated:
      (event: Common.EventTarget.EventTargetEvent<{entryFromIndex: number}>) => void;
  private readonly onNetworkEntriesLinkAnnotationCreated:
      (event: Common.EventTarget.EventTargetEvent<{entryFromIndex: number}>) => void;
  private readonly onMainEntrySelected: (event: Common.EventTarget.EventTargetEvent<number>) => void;
  private readonly onNetworkEntrySelected: (event: Common.EventTarget.EventTargetEvent<number>) => void;
  readonly #boundRefreshAfterIgnoreList: () => void;
  #selectedEvents: TraceEngine.Types.TraceEvents.TraceEventData[]|null;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private readonly groupBySetting: Common.Settings.Setting<any>;
  private searchableView!: UI.SearchableView.SearchableView;
  private needsResizeToPreferredHeights?: boolean;
  private selectedSearchResult?: PerfUI.FlameChart.DataProviderSearchResult;
  private searchRegex?: RegExp;
  #traceEngineData: TraceEngine.Handlers.Types.TraceParseData|null;
  #traceInsightsData: TraceEngine.Insights.Types.TraceInsightData|null = null;
  #selectedGroupName: string|null = null;
  #onTraceBoundsChangeBound = this.#onTraceBoundsChange.bind(this);
  #gameKeyMatches = 0;
  #gameTimeout = setTimeout(() => ({}), 0);

  #overlaysContainer: HTMLElement = document.createElement('div');
  #overlays: Overlays.Overlays.Overlays;

  #timeRangeSelectionAnnotation: TraceEngine.Types.File.TimeRangeAnnotation|null = null;
  // Keep track of the link annotation that hasn't been fully selected yet.
  // We only store it here when only 'entryFrom' has been selected and
  // 'EntryTo' selection still needs to be updated.
  #linkSelectionAnnotation: TraceEngine.Types.File.EntriesLinkAnnotation|null = null;

  #currentInsightOverlays: Array<Overlays.Overlays.TimelineOverlay> = [];
  #activeInsight: TimelineComponents.Sidebar.ActiveInsight|null = null;

  #tooltipElement = document.createElement('div');

  // We use an symbol as the loggable for each group. This is because
  // groups can get re-built at times and we need a common reference to act as
  // the reference for each group that we log. By storing these symbols in
  // a map keyed off the context of the group, we ensure we persist the
  // loggable even if the group gets rebuilt at some point in time.
  #loggableForGroupByLogContext: Map<string, Symbol> = new Map();

  constructor(delegate: TimelineModeViewDelegate) {
    super();
    this.element.classList.add('timeline-flamechart');

    this.delegate = delegate;
    this.eventListeners = [];
    this.#traceEngineData = null;

    const flameChartsContainer = new UI.Widget.VBox();
    flameChartsContainer.element.classList.add('flame-charts-container');

    // Create main and network flamecharts.
    this.networkSplitWidget = new UI.SplitWidget.SplitWidget(false, false, 'timeline-flamechart-main-view', 150);
    this.networkSplitWidget.show(flameChartsContainer.element);

    this.#overlaysContainer.classList.add('timeline-overlays-container');
    flameChartsContainer.element.appendChild(this.#overlaysContainer);

    this.#tooltipElement.classList.add('timeline-entry-tooltip-element');
    flameChartsContainer.element.appendChild(this.#tooltipElement);

    // Ensure that the network panel & resizer appears above the main thread.
    this.networkSplitWidget.sidebarElement().style.zIndex = '120';

    const mainViewGroupExpansionSetting =
        Common.Settings.Settings.instance().createSetting('timeline-flamechart-main-view-group-expansion', {});
    this.mainDataProvider = new TimelineFlameChartDataProvider();
    this.mainDataProvider.addEventListener(
        TimelineFlameChartDataProviderEvents.DATA_CHANGED, () => this.mainFlameChart.scheduleUpdate());
    this.mainFlameChart = new PerfUI.FlameChart.FlameChart(this.mainDataProvider, this, {
      groupExpansionSetting: mainViewGroupExpansionSetting,
      // The TimelineOverlays are used for selected elements
      selectedElementOutline: false,
      tooltipElement: this.#tooltipElement,
      useOverlaysForCursorRuler: true,
    });
    this.mainFlameChart.alwaysShowVerticalScroll();
    this.mainFlameChart.enableRuler(false);

    this.mainFlameChart.addEventListener(PerfUI.FlameChart.Events.LATEST_DRAW_DIMENSIONS, dimensions => {
      this.#overlays.updateChartDimensions('main', dimensions.data.chart);
      this.#overlays.updateVisibleWindow(dimensions.data.traceWindow);
      this.#overlays.update();
    });

    this.networkFlameChartGroupExpansionSetting =
        Common.Settings.Settings.instance().createSetting('timeline-flamechart-network-view-group-expansion', {});
    this.networkDataProvider = new TimelineFlameChartNetworkDataProvider();
    this.networkFlameChart = new PerfUI.FlameChart.FlameChart(this.networkDataProvider, this, {
      groupExpansionSetting: this.networkFlameChartGroupExpansionSetting,
      // The TimelineOverlays are used for selected elements
      selectedElementOutline: false,
      tooltipElement: this.#tooltipElement,
      useOverlaysForCursorRuler: true,
    });
    this.networkFlameChart.alwaysShowVerticalScroll();
    this.networkFlameChart.addEventListener(PerfUI.FlameChart.Events.LATEST_DRAW_DIMENSIONS, dimensions => {
      this.#overlays.updateChartDimensions('network', dimensions.data.chart);
      this.#overlays.updateVisibleWindow(dimensions.data.traceWindow);
      this.#overlays.update();

      // If the height of the network chart has changed, we need to tell the
      // main flame chart because its tooltips are positioned based in part on
      // the height of the network chart.
      this.mainFlameChart.setTooltipYPixelAdjustment(this.#overlays.networkChartOffsetHeight());
    });

    this.mainFlameChart.addEventListener(PerfUI.FlameChart.Events.MOUSE_MOVE, event => {
      this.#processFlameChartMouseMoveEvent(event.data);
    });

    this.networkFlameChart.addEventListener(PerfUI.FlameChart.Events.MOUSE_MOVE, event => {
      this.#processFlameChartMouseMoveEvent(event.data);
    });

    this.#overlays = new Overlays.Overlays.Overlays({
      container: this.#overlaysContainer,
      flameChartsContainers: {
        main: this.mainFlameChart.element,
        network: this.networkFlameChart.element,
      },
      charts: {
        mainChart: this.mainFlameChart,
        mainProvider: this.mainDataProvider,
        networkChart: this.networkFlameChart,
        networkProvider: this.networkDataProvider,
      },
      entryQueries: {
        isEntryCollapsedByUser: (entry: TraceEngine.Types.TraceEvents.TraceEventData): boolean => {
          return ModificationsManager.activeManager()?.getEntriesFilter().entryIsInvisible(entry) ?? false;
        },
        firstVisibleParentForEntry(entry) {
          return ModificationsManager.activeManager()?.getEntriesFilter().firstVisibleParentEntryForEntry(entry) ??
              null;
        },
      },
    });

    this.#overlays.addEventListener(Overlays.Overlays.AnnotationOverlayActionEvent.eventName, event => {
      const {overlay, action} = (event as Overlays.Overlays.AnnotationOverlayActionEvent);
      if (action === 'Remove') {
        // If the overlay removed is the current time range, set it to null so that
        // we would create a new time range overlay and annotation on the next time range selection instead
        // of trying to update the current overlay that does not exist.
        if (ModificationsManager.activeManager()?.getAnnotationByOverlay(overlay) ===
            this.#timeRangeSelectionAnnotation) {
          this.#timeRangeSelectionAnnotation = null;
        }
        ModificationsManager.activeManager()?.removeAnnotationOverlay(overlay);
      } else if (action === 'Update') {
        ModificationsManager.activeManager()?.updateAnnotationOverlay(overlay);
      }
    });

    this.networkPane = new UI.Widget.VBox();
    this.networkPane.setMinimumSize(23, 23);
    this.networkFlameChart.show(this.networkPane.element);
    this.splitResizer = this.networkPane.element.createChild('div', 'timeline-flamechart-resizer');
    this.networkSplitWidget.hideDefaultResizer(true);
    this.networkSplitWidget.installResizer(this.splitResizer);
    this.networkSplitWidget.setMainWidget(this.mainFlameChart);
    this.networkSplitWidget.setSidebarWidget(this.networkPane);

    // Create counters chart splitter.
    this.chartSplitWidget = new UI.SplitWidget.SplitWidget(false, true, 'timeline-counters-split-view-state');
    this.countersView = new CountersGraph(this.delegate);
    this.chartSplitWidget.setMainWidget(flameChartsContainer);
    this.chartSplitWidget.setSidebarWidget(this.countersView);
    this.chartSplitWidget.hideDefaultResizer();
    this.chartSplitWidget.installResizer((this.countersView.resizerElement() as Element));

    // Create top level properties splitter.
    this.detailsSplitWidget = new UI.SplitWidget.SplitWidget(false, true, 'timeline-panel-details-split-view-state');
    this.detailsSplitWidget.element.classList.add('timeline-details-split');
    this.detailsView = new TimelineDetailsView(delegate);
    this.detailsSplitWidget.installResizer(this.detailsView.headerElement());
    this.detailsSplitWidget.setMainWidget(this.chartSplitWidget);
    this.detailsSplitWidget.setSidebarWidget(this.detailsView);
    this.detailsSplitWidget.show(this.element);

    this.onMainAddEntryLabelAnnotation = this.onAddEntryLabelAnnotation.bind(this, this.mainDataProvider);
    this.onNetworkAddEntryLabelAnnotation = this.onAddEntryLabelAnnotation.bind(this, this.networkDataProvider);
    this.onMainEntriesLinkAnnotationCreated = event => {
      this.onEntriesLinkAnnotationCreate(this.mainDataProvider, event.data.entryFromIndex);
    }, this;
    this.onNetworkEntriesLinkAnnotationCreated = event => {
      this.onEntriesLinkAnnotationCreate(this.networkDataProvider, event.data.entryFromIndex);
    }, this;
    if (Root.Runtime.experiments.isEnabled(Root.Runtime.ExperimentName.TIMELINE_ANNOTATIONS)) {
      this.mainFlameChart.addEventListener(
          PerfUI.FlameChart.Events.ENTRY_LABEL_ANNOTATION_ADDED, this.onMainAddEntryLabelAnnotation, this);
      this.networkFlameChart.addEventListener(
          PerfUI.FlameChart.Events.ENTRY_LABEL_ANNOTATION_ADDED, this.onNetworkAddEntryLabelAnnotation, this);

      this.mainFlameChart.addEventListener(
          PerfUI.FlameChart.Events.ENTRIES_LINK_ANNOTATION_CREATED, this.onMainEntriesLinkAnnotationCreated, this);
      this.networkFlameChart.addEventListener(
          PerfUI.FlameChart.Events.ENTRIES_LINK_ANNOTATION_CREATED, this.onNetworkEntriesLinkAnnotationCreated, this);
    }
    this.onMainEntrySelected = this.onEntrySelected.bind(this, this.mainDataProvider);
    this.onNetworkEntrySelected = this.onEntrySelected.bind(this, this.networkDataProvider);
    this.mainFlameChart.addEventListener(PerfUI.FlameChart.Events.ENTRY_SELECTED, this.onMainEntrySelected, this);
    this.mainFlameChart.addEventListener(PerfUI.FlameChart.Events.ENTRY_INVOKED, this.onMainEntrySelected, this);
    this.networkFlameChart.addEventListener(PerfUI.FlameChart.Events.ENTRY_SELECTED, this.onNetworkEntrySelected, this);
    this.networkFlameChart.addEventListener(PerfUI.FlameChart.Events.ENTRY_INVOKED, this.onNetworkEntrySelected, this);
    this.mainFlameChart.addEventListener(PerfUI.FlameChart.Events.ENTRY_HOVERED, event => {
      this.onEntryHovered(event);
      this.updateLinkSelectionAnnotation(this.mainDataProvider, event.data);
    }, this);
    this.networkFlameChart.addEventListener(PerfUI.FlameChart.Events.ENTRY_HOVERED, event => {
      this.updateLinkSelectionAnnotation(this.networkDataProvider, event.data);
    }, this);

    this.element.addEventListener('keydown', this.#keydownHandler.bind(this));
    this.#boundRefreshAfterIgnoreList = this.#refreshAfterIgnoreList.bind(this);
    this.#selectedEvents = null;

    this.groupBySetting = Common.Settings.Settings.instance().createSetting(
        'timeline-tree-group-by', AggregatedTimelineTreeView.GroupBy.None);
    this.groupBySetting.addChangeListener(this.refreshMainFlameChart, this);
    this.refreshMainFlameChart();

    TraceBounds.TraceBounds.onChange(this.#onTraceBoundsChangeBound);
  }

  setActiveInsight(insight: TimelineComponents.Sidebar.ActiveInsight|null): void {
    this.#activeInsight = insight;
    const traceBounds = TraceBounds.TraceBounds.BoundsManager.instance().state()?.micro.entireTraceBounds;

    for (const overlay of this.#currentInsightOverlays) {
      this.removeOverlay(overlay);
    }

    if (!this.#activeInsight || !traceBounds) {
      return;
    }

    if (insight) {
      const newInsightOverlays = insight.createOverlayFn();
      this.#currentInsightOverlays = newInsightOverlays;
      if (this.#currentInsightOverlays.length === 0) {
        return;
      }

      const entries: TraceEngine.Types.TraceEvents.TraceEventData[] = [];

      for (const overlay of this.#currentInsightOverlays) {
        this.addOverlay(overlay);

        entries.push(...Overlays.Overlays.entriesForOverlay(overlay));
      }

      for (const entry of entries) {
        // Ensure that the track for the entries are open.
        this.#expandEntryTrack(entry);
      }

      const overlaysBounds = Overlays.Overlays.traceWindowContainingOverlays(this.#currentInsightOverlays);
      // Trace window covering all overlays expanded by 100% so that the overlays cover 50% of the visible window.
      const expandedBounds =
          TraceEngine.Helpers.Timing.expandWindowByPercentOrToOneMillisecond(overlaysBounds, traceBounds, 100);

      // Set the timeline visible window and ignore the minimap bounds. This
      // allows us to pick a visible window even if the overlays are outside of
      // the current breadcrumb. If this happens, the event listener for
      // BoundsManager changes in TimelineMiniMap will detect it and activate
      // the correct breadcrumb for us.
      TraceBounds.TraceBounds.BoundsManager.instance().setTimelineVisibleWindow(
          expandedBounds, {ignoreMiniMapBounds: true, shouldAnimate: true});

      // Reveal entry if we have one.
      if (entries.length !== 0) {
        const earliestEntry =
            entries.reduce((earliest, current) => (earliest.ts < current.ts ? earliest : current), entries[0]);
        this.revealEventVertically(earliestEntry);
      }
    }
  }

  /**
   * Expands the track / group that the given entry is in.
   */
  #expandEntryTrack(entry: TraceEngine.Types.TraceEvents.TraceEventData): void {
    const chartName = Overlays.Overlays.chartForEntry(entry);
    const provider = chartName === 'main' ? this.mainDataProvider : this.networkDataProvider;
    const entryChart = chartName === 'main' ? this.mainFlameChart : this.networkFlameChart;

    const entryIndex = provider.indexForEvent?.(entry) ?? null;
    if (entryIndex === null) {
      return;
    }

    const group = provider.groupForEvent?.(entryIndex) ?? null;
    if (!group) {
      return;
    }
    const groupIndex = provider.timelineData().groups.indexOf(group);

    if (!group.expanded && groupIndex > -1) {
      entryChart.toggleGroupExpand(groupIndex);
    }
  }

  #processFlameChartMouseMoveEvent(data: PerfUI.FlameChart.EventTypes['MouseMove']): void {
    const {mouseEvent, timeInMicroSeconds} = data;

    // If the user is no longer holding shift, remove any existing marker.
    if (!mouseEvent.shiftKey) {
      const removedCount = this.#overlays.removeOverlaysOfType('CURSOR_TIMESTAMP_MARKER');
      if (removedCount > 0) {
        // Don't trigger lots of updates on a mouse move if we didn't actually
        // remove any overlays.
        this.#overlays.update();
      }
    }

    if (!mouseEvent.metaKey && mouseEvent.shiftKey) {
      // CURSOR_TIMESTAMP_MARKER is a singleton; if one already exists it will
      // be updated rather than create an entirely new one.
      this.addOverlay({
        type: 'CURSOR_TIMESTAMP_MARKER',
        timestamp: timeInMicroSeconds,
      });
    }
  }

  #keydownHandler(event: KeyboardEvent): void {
    const keyCombo = 'fixme';
    if (event.key === keyCombo[this.#gameKeyMatches]) {
      this.#gameKeyMatches++;
      clearTimeout(this.#gameTimeout);
      this.#gameTimeout = setTimeout(() => {
        this.#gameKeyMatches = 0;
      }, 2000);
    } else {
      this.#gameKeyMatches = 0;
      clearTimeout(this.#gameTimeout);
    }
    if (this.#gameKeyMatches !== keyCombo.length) {
      return;
    }
    this.runBrickBreakerGame();
  }

  runBrickBreakerGame(): void {
    if (!SHOULD_SHOW_EASTER_EGG) {
      return;
    }
    if ([...this.element.childNodes].find(child => child instanceof PerfUI.BrickBreaker.BrickBreaker)) {
      return;
    }
    this.brickGame = new PerfUI.BrickBreaker.BrickBreaker(this.mainFlameChart);
    this.brickGame.classList.add('brick-game');
    this.element.append(this.brickGame);
  }

  #onTraceBoundsChange(event: TraceBounds.TraceBounds.StateChangedEvent): void {
    if (event.updateType === 'MINIMAP_BOUNDS') {
      // If the update type was a changing of the minimap bounds, we do not
      // need to redraw the timeline.
      return;
    }

    const visibleWindow = event.state.milli.timelineTraceWindow;
    const shouldAnimate = Boolean(event.options.shouldAnimate);
    this.mainFlameChart.setWindowTimes(visibleWindow.min, visibleWindow.max, shouldAnimate);
    this.networkDataProvider.setWindowTimes(visibleWindow.min, visibleWindow.max);
    this.networkFlameChart.setWindowTimes(visibleWindow.min, visibleWindow.max, shouldAnimate);
    // Updating search results can be very expensive. Debounce to avoid over-calling it.
    const debouncedUpdate = Common.Debouncer.debounce(() => {
      this.updateSearchResults(false, false);
    }, 100);
    debouncedUpdate();
  }

  isNetworkTrackShownForTests(): boolean {
    return this.networkSplitWidget.showMode() !== UI.SplitWidget.ShowMode.ONLY_MAIN;
  }

  getLinkSelectionAnnotation(): TraceEngine.Types.File.EntriesLinkAnnotation|null {
    return this.#linkSelectionAnnotation;
  }

  getMainDataProvider(): TimelineFlameChartDataProvider {
    return this.mainDataProvider;
  }

  getNetworkDataProvider(): TimelineFlameChartNetworkDataProvider {
    return this.networkDataProvider;
  }

  refreshMainFlameChart(): void {
    this.mainFlameChart.update();
  }

  extensionDataVisibilityChanged(): void {
    this.#reset();
    this.mainDataProvider.reset(true);
    this.mainDataProvider.timelineData(true);
    this.refreshMainFlameChart();
  }

  windowChanged(
      windowStartTime: TraceEngine.Types.Timing.MilliSeconds, windowEndTime: TraceEngine.Types.Timing.MilliSeconds,
      animate: boolean): void {
    TraceBounds.TraceBounds.BoundsManager.instance().setTimelineVisibleWindow(
        TraceEngine.Helpers.Timing.traceWindowFromMilliSeconds(
            TraceEngine.Types.Timing.MilliSeconds(windowStartTime),
            TraceEngine.Types.Timing.MilliSeconds(windowEndTime),
            ),
        {shouldAnimate: animate},
    );
  }

  /**
   * @param startTime - the start time of the selection in MilliSeconds
   * @param endTime - the end time of the selection in MilliSeconds
   * TODO(crbug.com/346312365): update the type definitions in ChartViewport.ts
   */
  updateRangeSelection(startTime: number, endTime: number): void {
    this.delegate.select(TimelineSelection.fromRange(startTime, endTime));
    if (Root.Runtime.experiments.isEnabled(Root.Runtime.ExperimentName.TIMELINE_ANNOTATIONS)) {
      const bounds = TraceEngine.Helpers.Timing.traceWindowFromMilliSeconds(
          TraceEngine.Types.Timing.MilliSeconds(startTime),
          TraceEngine.Types.Timing.MilliSeconds(endTime),
      );

      // If the current time range annotation has a label, the range selection
      // for it is finished and we need to create a new time range annotations.
      if (this.#timeRangeSelectionAnnotation && !this.#timeRangeSelectionAnnotation?.label) {
        this.#timeRangeSelectionAnnotation.bounds = bounds;
        ModificationsManager.activeManager()?.updateAnnotation(this.#timeRangeSelectionAnnotation);
      } else {
        this.#timeRangeSelectionAnnotation = {
          type: 'TIME_RANGE',
          label: '',
          bounds,
        };
        ModificationsManager.activeManager()?.createAnnotation(this.#timeRangeSelectionAnnotation);
      }
    }
  }

  getMainFlameChart(): PerfUI.FlameChart.FlameChart {
    return this.mainFlameChart;
  }

  // This function is public for test purpose.
  getNetworkFlameChart(): PerfUI.FlameChart.FlameChart {
    return this.networkFlameChart;
  }

  updateSelectedGroup(flameChart: PerfUI.FlameChart.FlameChart, group: PerfUI.FlameChart.Group|null): void {
    if (flameChart !== this.mainFlameChart || this.#selectedGroupName === group?.name) {
      return;
    }
    this.#selectedGroupName = group?.name || null;
    this.#selectedEvents = group ? this.mainDataProvider.groupTreeEvents(group) : null;
    this.#updateDetailViews();
  }

  setModel(newTraceEngineData: TraceEngine.Handlers.Types.TraceParseData|null, isCpuProfile = false): void {
    if (newTraceEngineData === this.#traceEngineData) {
      return;
    }
    this.#selectedGroupName = null;
    this.#traceEngineData = newTraceEngineData;
    Common.EventTarget.removeEventListeners(this.eventListeners);
    this.#selectedEvents = null;
    this.mainDataProvider.setModel(newTraceEngineData, isCpuProfile);
    this.networkDataProvider.setModel(newTraceEngineData);
    this.#reset();
    this.updateSearchResults(false, false);
    this.refreshMainFlameChart();
    this.#updateFlameCharts();
  }

  setInsights(insights: TraceEngine.Insights.Types.TraceInsightData|null): void {
    if (this.#traceInsightsData !== insights) {
      this.#traceInsightsData = insights;
    }
  }

  #reset(): void {
    if (this.networkDataProvider.isEmpty()) {
      this.mainFlameChart.enableRuler(true);
      this.networkSplitWidget.hideSidebar();
    } else {
      this.mainFlameChart.enableRuler(false);
      this.networkSplitWidget.showBoth();
      this.resizeToPreferredHeights();
    }
    this.#overlays.reset();
    this.mainFlameChart.reset();
    this.networkFlameChart.reset();
    this.updateSearchResults(false, false);

    const traceBoundsState = TraceBounds.TraceBounds.BoundsManager.instance().state();
    if (!traceBoundsState) {
      throw new Error('TimelineFlameChartView could not set the window bounds.');
    }
    const visibleWindow = traceBoundsState.milli.timelineTraceWindow;
    this.mainFlameChart.setWindowTimes(visibleWindow.min, visibleWindow.max);
    this.networkDataProvider.setWindowTimes(visibleWindow.min, visibleWindow.max);
    this.networkFlameChart.setWindowTimes(visibleWindow.min, visibleWindow.max);
  }

  #refreshAfterIgnoreList(): void {
    // The ignore list will only affect Thread tracks, which will only be in main flame chart.
    // So just force recalculate and redraw the main flame chart here.
    this.mainDataProvider.timelineData(true);
    this.mainFlameChart.scheduleUpdate();
  }

  #updateDetailViews(): void {
    this.countersView.setModel(this.#traceEngineData, this.#selectedEvents);
    void this.detailsView.setModel(this.#traceEngineData, this.#selectedEvents, this.#traceInsightsData);
  }

  #updateFlameCharts(): void {
    this.mainFlameChart.scheduleUpdate();
    this.networkFlameChart.scheduleUpdate();

    this.#registerLoggableGroups();
  }

  #registerLoggableGroups(): void {
    const groups = [
      ...this.mainFlameChart.timelineData()?.groups ?? [],
      ...this.networkFlameChart.timelineData()?.groups ?? [],
    ];
    for (const group of groups) {
      if (!group.jslogContext) {
        continue;
      }
      const loggable = this.#loggableForGroupByLogContext.get(group.jslogContext) ?? Symbol(group.jslogContext);

      if (!this.#loggableForGroupByLogContext.has(group.jslogContext)) {
        // This is the first time this group has been created, so register its loggable.
        this.#loggableForGroupByLogContext.set(group.jslogContext, loggable);
        VisualLogging.registerLoggable(
            loggable, `${VisualLogging.section().context(`timeline.${group.jslogContext}`)}`, this.delegate.element);
      }
    }
  }

  // If an entry is hovered over and a creation of link annotation is in progress, update that annotation with a hovered entry.
  updateLinkSelectionAnnotation(
      dataProvider: TimelineFlameChartDataProvider|TimelineFlameChartNetworkDataProvider, entryIndex: number): void {
    if (!this.#linkSelectionAnnotation) {
      return;
    }
    const toSelectionObject = this.#selectionIfTraceEvent(entryIndex, dataProvider);

    if (toSelectionObject) {
      this.#linkSelectionAnnotation.entryTo = toSelectionObject;
    } else {
      delete this.#linkSelectionAnnotation['entryTo'];
    }
    ModificationsManager.activeManager()?.updateAnnotation(this.#linkSelectionAnnotation);
  }

  private onEntryHovered(commonEvent: Common.EventTarget.EventTargetEvent<number>): void {
    SDK.OverlayModel.OverlayModel.hideDOMNodeHighlight();
    const entryIndex = commonEvent.data;
    const event = this.mainDataProvider.eventByIndex(entryIndex);
    if (!event || !this.#traceEngineData) {
      return;
    }
    if (event instanceof TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame) {
      return;
    }

    const target = targetForEvent(this.#traceEngineData, event);
    if (!target) {
      return;
    }

    const nodeIds = TraceEngine.Extras.FetchNodes.nodeIdsForEvent(this.#traceEngineData, event);
    for (const nodeId of nodeIds) {
      new SDK.DOMModel.DeferredDOMNode(target, nodeId).highlight();
    }
  }

  highlightEvent(event: TraceEngine.Types.TraceEvents.TraceEventData|null): void {
    const entryIndex =
        event ? this.mainDataProvider.entryIndexForSelection(TimelineSelection.fromTraceEvent(event)) : -1;
    if (entryIndex >= 0) {
      this.mainFlameChart.highlightEntry(entryIndex);
    } else {
      this.mainFlameChart.hideHighlight();
    }
  }

  override willHide(): void {
    this.networkFlameChartGroupExpansionSetting.removeChangeListener(this.resizeToPreferredHeights, this);
    Bindings.IgnoreListManager.IgnoreListManager.instance().removeChangeListener(this.#boundRefreshAfterIgnoreList);
  }

  override wasShown(): void {
    this.registerCSSFiles([timelineFlameChartViewStyles]);
    this.networkFlameChartGroupExpansionSetting.addChangeListener(this.resizeToPreferredHeights, this);
    Bindings.IgnoreListManager.IgnoreListManager.instance().addChangeListener(this.#boundRefreshAfterIgnoreList);
    if (this.needsResizeToPreferredHeights) {
      this.resizeToPreferredHeights();
    }
    this.#updateFlameCharts();
  }

  updateCountersGraphToggle(showMemoryGraph: boolean): void {
    if (showMemoryGraph) {
      this.chartSplitWidget.showBoth();
    } else {
      this.chartSplitWidget.hideSidebar();
    }
  }

  revealEvent(event: TraceEngine.Types.TraceEvents.TraceEventData): void {
    const mainIndex = this.mainDataProvider.indexForEvent(event);
    const networkIndex = this.networkDataProvider.indexForEvent(event);
    if (mainIndex) {
      this.mainFlameChart.revealEntry(mainIndex);
    } else if (networkIndex) {
      this.networkFlameChart.revealEntry(networkIndex);
    }
  }

  // Given an event, it reveals its position vertically
  revealEventVertically(event: TraceEngine.Types.TraceEvents.TraceEventData): void {
    const mainIndex = this.mainDataProvider.indexForEvent(event);
    const networkIndex = this.networkDataProvider.indexForEvent(event);
    if (mainIndex) {
      this.mainFlameChart.revealEntryVertically(mainIndex);
    } else if (networkIndex) {
      this.networkFlameChart.revealEntryVertically(networkIndex);
    }
  }

  setSelectionAndReveal(selection: TimelineSelection|null): void {
    const mainIndex = this.mainDataProvider.entryIndexForSelection(selection);
    const networkIndex = this.networkDataProvider.entryIndexForSelection(selection);
    this.mainFlameChart.setSelectedEntry(mainIndex);
    this.networkFlameChart.setSelectedEntry(networkIndex);

    // Clear any existing entry selection.
    this.#overlays.removeOverlaysOfType('ENTRY_SELECTED');
    // If:
    // 1. There is no selection, or the selection is not a range selection
    // AND 2. we have an active time range selection overlay
    // AND 3. The label of the selection is not empty
    // then we need to remove it.
    if ((selection === null || !TimelineSelection.isRangeSelection(selection.object)) &&
        this.#timeRangeSelectionAnnotation && !this.#timeRangeSelectionAnnotation.label) {
      ModificationsManager.activeManager()?.removeAnnotation(this.#timeRangeSelectionAnnotation);
      this.#timeRangeSelectionAnnotation = null;
    }

    let index = this.mainDataProvider.entryIndexForSelection(selection);
    this.mainFlameChart.setSelectedEntry(index);
    index = this.networkDataProvider.entryIndexForSelection(selection);
    this.networkFlameChart.setSelectedEntry(index);
    if (this.detailsView) {
      // TODO(crbug.com/1459265):  Change to await after migration work.
      void this.detailsView.setSelection(selection);
    }

    // Create the entry selected overlay if the selection represents a frame or trace event (either network, or anything else)
    if (selection &&
        (TimelineSelection.isTraceEventSelection(selection.object) ||
         TimelineSelection.isSyntheticNetworkRequestDetailsEventSelection(selection.object) ||
         TimelineSelection.isLegacyTimelineFrame(selection.object))) {
      this.addOverlay({
        type: 'ENTRY_SELECTED',
        entry: selection.object,
      });
    }
  }

  /**
   * Used to create multiple overlays at once without triggering a redraw for each one.
   */
  bulkAddOverlays(overlays: Overlays.Overlays.TimelineOverlay[]): void {
    for (const overlay of overlays) {
      this.#overlays.add(overlay);
    }
    this.#overlays.update();
  }
  addOverlay<T extends Overlays.Overlays.TimelineOverlay>(newOverlay: T): T {
    const overlay = this.#overlays.add(newOverlay);
    this.#overlays.update();
    return overlay;
  }

  removeOverlay(removedOverlay: Overlays.Overlays.TimelineOverlay): void {
    this.#overlays.remove(removedOverlay);
    this.#overlays.update();
  }

  updateExistingOverlay<T extends Overlays.Overlays.TimelineOverlay>(existingOverlay: T, newData: Partial<T>): void {
    this.#overlays.updateExisting(existingOverlay, newData);
    this.#overlays.update();
  }

  enterLabelEditMode(overlay: Overlays.Overlays.EntryLabel): void {
    this.#overlays.enterLabelEditMode(overlay);
  }

  private onAddEntryLabelAnnotation(
      dataProvider: TimelineFlameChartDataProvider|TimelineFlameChartNetworkDataProvider,
      event: Common.EventTarget.EventTargetEvent<number>): void {
    const selection = dataProvider.createSelection(event.data);
    if (selection &&
        (TimelineSelection.isTraceEventSelection(selection.object) ||
         TimelineSelection.isSyntheticNetworkRequestDetailsEventSelection(selection.object) ||
         TimelineSelection.isLegacyTimelineFrame(selection.object))) {
      this.setSelectionAndReveal(selection);
      ModificationsManager.activeManager()?.createAnnotation({
        type: 'ENTRY_LABEL',
        entry: selection.object,
        label: '',
      });
    }
  }

  onEntriesLinkAnnotationCreate(
      dataProvider: TimelineFlameChartDataProvider|TimelineFlameChartNetworkDataProvider,
      entryFromIndex: number): void {
    const fromSelectionObject = (entryFromIndex) ? this.#selectionIfTraceEvent(entryFromIndex, dataProvider) : null;

    if (fromSelectionObject) {
      this.#linkSelectionAnnotation = {
        type: 'ENTRIES_LINK',
        entryFrom: fromSelectionObject,
      };
      ModificationsManager.activeManager()?.createAnnotation(this.#linkSelectionAnnotation);
    }
  }

  #selectionIfTraceEvent(
      index: number, dataProvider: TimelineFlameChartDataProvider|TimelineFlameChartNetworkDataProvider):
      TraceEngine.Types.TraceEvents.TraceEventData|TraceEngine.Types.TraceEvents.SyntheticNetworkRequest|null {
    const selection = dataProvider.createSelection(index);
    if (!selection) {
      return null;
    }

    if (TimelineSelection.isTraceEventSelection(selection.object) ||
        TimelineSelection.isSyntheticNetworkRequestDetailsEventSelection(selection.object)) {
      return selection.object;
    }

    if (TimelineSelection.isLegacyTimelineFrame(selection.object)) {
      return selection.object as TraceEngine.Types.TraceEvents.LegacyTimelineFrame;
    }

    return null;
  }

  private onEntrySelected(
      dataProvider: TimelineFlameChartDataProvider|TimelineFlameChartNetworkDataProvider,
      event: Common.EventTarget.EventTargetEvent<number>): void {
    const data = dataProvider.timelineData();
    if (!data) {
      return;
    }
    const entryIndex = event.data;

    const entryLevel = data.entryLevels[entryIndex];

    // Find the group that contains this level and log a click for it.
    const group = groupForLevel(data.groups, entryLevel);
    if (group && group.jslogContext) {
      const loggable = this.#loggableForGroupByLogContext.get(group.jslogContext) ?? null;
      if (loggable) {
        VisualLogging.logClick(loggable, new MouseEvent('click'));
      }
    }

    dataProvider.buildFlowForInitiator(entryIndex);
    this.delegate.select(dataProvider.createSelection(entryIndex));

    if (this.#linkSelectionAnnotation) {
      this.handleToEntryOfLinkBetweenEntriesSelection(entryIndex);
    }
  }

  handleToEntryOfLinkBetweenEntriesSelection(toIndex: number): void {
    // If there is a link annotation in the process of being created when an empty
    // space in the Flamechart is clicked, delete the link being created.
    //
    // If an entry is clicked when a link between entries in created and the entry that an arrow
    // is pointing to is earlier than the one it starts from, switch 'to' and 'from' entries to
    // reverse the arrow.
    if (this.#linkSelectionAnnotation && toIndex === -1) {
      ModificationsManager.activeManager()?.removeAnnotation(this.#linkSelectionAnnotation);
    } else if (
        this.#linkSelectionAnnotation && this.#linkSelectionAnnotation?.entryTo &&
        (this.#linkSelectionAnnotation?.entryFrom.ts > this.#linkSelectionAnnotation?.entryTo.ts)) {
      const entryFrom = this.#linkSelectionAnnotation.entryFrom;
      const entryTo = this.#linkSelectionAnnotation.entryTo;
      this.#linkSelectionAnnotation.entryTo = entryFrom;
      this.#linkSelectionAnnotation.entryFrom = entryTo;
      ModificationsManager.activeManager()?.updateAnnotation(this.#linkSelectionAnnotation);
    }
    // Regardless of if the link in progress was deleted or the clicked entry is the final selection,
    // set the link selection in progress to null so a new one is created if the an event to create
    // of update the current link is dispatched.
    this.#linkSelectionAnnotation = null;
  }

  resizeToPreferredHeights(): void {
    if (!this.isShowing()) {
      this.needsResizeToPreferredHeights = true;
      return;
    }
    this.needsResizeToPreferredHeights = false;
    this.networkPane.element.classList.toggle(
        'timeline-network-resizer-disabled', !this.networkDataProvider.isExpanded());
    this.networkSplitWidget.setSidebarSize(
        this.networkDataProvider.preferredHeight() + this.splitResizer.clientHeight + PerfUI.FlameChart.RulerHeight +
        2);
  }

  setSearchableView(searchableView: UI.SearchableView.SearchableView): void {
    this.searchableView = searchableView;
  }

  // UI.SearchableView.Searchable implementation

  searchResultIndexForEntryIndex(index: number): number {
    if (!this.searchResults) {
      return -1;
    }
    return this.searchResults.findIndex(result => result.index === index);
  }

  jumpToNextSearchResult(): void {
    if (!this.searchResults || !this.searchResults.length) {
      return;
    }
    const index =
        typeof this.selectedSearchResult !== 'undefined' ? this.searchResults.indexOf(this.selectedSearchResult) : -1;
    this.#selectSearchResult(Platform.NumberUtilities.mod(index + 1, this.searchResults.length));
  }

  jumpToPreviousSearchResult(): void {
    if (!this.searchResults || !this.searchResults.length) {
      return;
    }
    const index =
        typeof this.selectedSearchResult !== 'undefined' ? this.searchResults.indexOf(this.selectedSearchResult) : 0;
    this.#selectSearchResult(Platform.NumberUtilities.mod(index - 1, this.searchResults.length));
  }

  supportsCaseSensitiveSearch(): boolean {
    return true;
  }

  supportsRegexSearch(): boolean {
    return true;
  }

  #selectSearchResult(searchResultIndex: number): void {
    this.searchableView.updateCurrentMatchIndex(searchResultIndex);
    const matchedResult = this.searchResults?.at(searchResultIndex) ?? null;
    if (!matchedResult) {
      return;
    }

    switch (matchedResult.provider) {
      case 'main': {
        this.delegate.select(this.mainDataProvider.createSelection(matchedResult.index));
        this.mainFlameChart.showPopoverForSearchResult(matchedResult.index);
        break;
      }
      case 'network': {
        this.delegate.select(this.networkDataProvider.createSelection(matchedResult.index));
        this.networkFlameChart.showPopoverForSearchResult(matchedResult.index);
        break;
      }
      case 'other':
        // TimelineFlameChartView only has main/network so we can ignore.
        break;
      default:
        Platform.assertNever(matchedResult.provider, `Unknown SearchResult[provider]: ${matchedResult.provider}`);
    }
    this.selectedSearchResult = matchedResult;
  }

  private updateSearchResults(shouldJump: boolean, jumpBackwards?: boolean): void {
    const traceBoundsState = TraceBounds.TraceBounds.BoundsManager.instance().state();
    if (!traceBoundsState) {
      return;
    }

    const oldSelectedSearchResult = this.selectedSearchResult;
    delete this.selectedSearchResult;
    this.searchResults = [];
    this.mainFlameChart.removeSearchResultHighlights();
    this.networkFlameChart.removeSearchResultHighlights();
    if (!this.searchRegex) {
      return;
    }
    const regExpFilter = new TimelineRegExp(this.searchRegex);
    const visibleWindow = traceBoundsState.micro.timelineTraceWindow;

    /**
     * Get the matches for the user's search result. We search both providers
     * but before storing the results we need to "tag" the results with the
     * provider they came from. We do this so that when the user highlights a
     * search result we know which flame chart to talk to to highlight it.
     */
    const mainMatches = this.mainDataProvider.search(visibleWindow, regExpFilter);
    const networkMatches = this.networkDataProvider.search(visibleWindow, regExpFilter);

    // Merge both result sets into one, sorted by start time. This means as the
    // user navigates back/forwards they will do so in time order and not do
    // all the main results before the network results, or some other
    // unexpected ordering.
    this.searchResults = mainMatches.concat(networkMatches).sort((m1, m2) => {
      return m1.startTimeMilli - m2.startTimeMilli;
    });

    this.searchableView.updateSearchMatchesCount(this.searchResults.length);

    // To avoid too many highlights when the search regex matches too many entries,
    // for example, when user only types in "e" as the search query,
    // We only highlight the search results when the number of matches is less than or equal to 200.
    if (this.searchResults.length <= MAX_HIGHLIGHTED_SEARCH_ELEMENTS) {
      this.mainFlameChart.highlightAllEntries(mainMatches.map(m => m.index));
      this.networkFlameChart.highlightAllEntries(networkMatches.map(m => m.index));
    }
    if (!shouldJump || !this.searchResults.length) {
      return;
    }
    let selectedIndex = this.#indexOfSearchResult(oldSelectedSearchResult);
    if (selectedIndex === -1) {
      selectedIndex = jumpBackwards ? this.searchResults.length - 1 : 0;
    }
    this.#selectSearchResult(selectedIndex);
  }

  #indexOfSearchResult(target?: PerfUI.FlameChart.DataProviderSearchResult): number {
    if (!target) {
      return -1;
    }

    return this.searchResults?.findIndex(result => {
      return result.provider === target.provider && result.index === target.index;
    }) ??
        -1;
  }

  /**
   * Returns the indexes of the elements that matched the most recent
   * query. Elements are indexed by the data provider and correspond
   * to their position in the data provider entry data array.
   * Public only for tests.
   */
  getSearchResults(): PerfUI.FlameChart.DataProviderSearchResult[]|undefined {
    return this.searchResults;
  }

  onSearchCanceled(): void {
    if (typeof this.selectedSearchResult !== 'undefined') {
      this.delegate.select(null);
    }
    delete this.searchResults;
    delete this.selectedSearchResult;
    delete this.searchRegex;
    this.mainFlameChart.showPopoverForSearchResult(-1);
    this.mainFlameChart.removeSearchResultHighlights();
    this.networkFlameChart.showPopoverForSearchResult(-1);
    this.networkFlameChart.removeSearchResultHighlights();
  }

  performSearch(searchConfig: UI.SearchableView.SearchConfig, shouldJump: boolean, jumpBackwards?: boolean): void {
    this.searchRegex = searchConfig.toSearchRegex().regex;
    this.updateSearchResults(shouldJump, jumpBackwards);
  }
}

export class Selection {
  timelineSelection: TimelineSelection;
  entryIndex: number;
  constructor(selection: TimelineSelection, entryIndex: number) {
    this.timelineSelection = selection;
    this.entryIndex = entryIndex;
  }
}

export const FlameChartStyle = {
  textColor: '#333',
};

export class TimelineFlameChartMarker implements PerfUI.FlameChart.FlameChartMarker {
  private readonly startTimeInternal: number;
  private readonly startOffset: number;
  private style: TimelineMarkerStyle;
  constructor(startTime: number, startOffset: number, style: TimelineMarkerStyle) {
    this.startTimeInternal = startTime;
    this.startOffset = startOffset;
    this.style = style;
  }

  startTime(): number {
    return this.startTimeInternal;
  }

  color(): string {
    return this.style.color;
  }

  title(): string|null {
    if (this.style.lowPriority) {
      return null;
    }
    const startTime = i18n.TimeUtilities.millisToString(this.startOffset);
    return i18nString(UIStrings.sAtS, {PH1: this.style.title, PH2: startTime});
  }

  draw(context: CanvasRenderingContext2D, x: number, height: number, pixelsPerMillisecond: number): void {
    const lowPriorityVisibilityThresholdInPixelsPerMs = 4;

    if (this.style.lowPriority && pixelsPerMillisecond < lowPriorityVisibilityThresholdInPixelsPerMs) {
      return;
    }

    context.save();
    if (this.style.tall) {
      context.strokeStyle = this.style.color;
      context.lineWidth = this.style.lineWidth;
      context.translate(this.style.lineWidth < 1 || (this.style.lineWidth & 1) ? 0.5 : 0, 0.5);
      context.beginPath();
      context.moveTo(x, 0);
      context.setLineDash(this.style.dashStyle);
      context.lineTo(x, context.canvas.height);
      context.stroke();
    }
    context.restore();
  }
}

export const enum ColorBy {
  URL = 'URL',
}

/**
 * Find the Group that contains the provided level, or `null` if no group is
 * found.
 */
export function groupForLevel(groups: PerfUI.FlameChart.Group[], level: number): PerfUI.FlameChart.Group|null {
  const groupForLevel = groups.find((group, groupIndex) => {
    const nextGroup = groups.at(groupIndex + 1);
    const groupEndLevel = nextGroup ? nextGroup.startLevel - 1 : Infinity;

    return group.startLevel <= level && groupEndLevel >= level;
  });
  return groupForLevel ?? null;
}
