// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import * as TraceEngine from '../../../models/trace/trace.js';
import {describeWithEnvironment} from '../../../testing/EnvironmentHelpers.js';
import {
  makeInstantEvent,
  MockFlameChartDelegate,
  setupIgnoreListManagerEnvironment,
} from '../../../testing/TraceHelpers.js';
import {TraceLoader} from '../../../testing/TraceLoader.js';
import * as RenderCoordinator from '../../../ui/components/render_coordinator/render_coordinator.js';
import * as PerfUI from '../../../ui/legacy/components/perf_ui/perf_ui.js';
import * as Timeline from '../timeline.js';

import * as Components from './components/components.js';
import * as Overlays from './overlays.js';

const coordinator = RenderCoordinator.RenderCoordinator.RenderCoordinator.instance();

const FAKE_OVERLAY_ENTRY_QUERIES: Overlays.Overlays.OverlayEntryQueries = {
  isEntryCollapsedByUser() {
    return false;
  },
  firstVisibleParentForEntry() {
    return null;
  },
};

/**
 * The Overlays expects to be provided with both the main and network charts
 * and data providers. This function creates all of those and optionally sets
 * the trace data for the providers if it is provided.
 */
function createCharts(traceData?: TraceEngine.Handlers.Types.TraceParseData): Overlays.Overlays.TimelineCharts {
  const mainProvider = new Timeline.TimelineFlameChartDataProvider.TimelineFlameChartDataProvider();
  const networkProvider = new Timeline.TimelineFlameChartNetworkDataProvider.TimelineFlameChartNetworkDataProvider();

  const delegate = new MockFlameChartDelegate();
  const mainChart = new PerfUI.FlameChart.FlameChart(mainProvider, delegate);
  const networkChart = new PerfUI.FlameChart.FlameChart(networkProvider, delegate);

  if (traceData) {
    mainProvider.setModel(traceData);
    networkProvider.setModel(traceData);

    // Force the charts to render. Normally the TimelineFlameChartView would do
    // this, but we aren't creating one for these tests.
    mainChart.update();
    networkChart.update();
  }

  return {
    mainProvider,
    mainChart,
    networkProvider,
    networkChart,
  };
}

describeWithEnvironment('Overlays', () => {
  beforeEach(() => {
    setupIgnoreListManagerEnvironment();
  });

  it('can calculate the x position of an event based on the dimensions and its timestamp', async () => {
    const flameChartsContainer = document.createElement('div');
    const mainFlameChartsContainer = flameChartsContainer.createChild('div');
    const networkFlameChartsContainer = flameChartsContainer.createChild('div');
    const container = flameChartsContainer.createChild('div');

    const overlays = new Overlays.Overlays.Overlays({
      container,
      flameChartsContainers: {
        main: mainFlameChartsContainer,
        network: networkFlameChartsContainer,
      },
      charts: createCharts(),
      entryQueries: FAKE_OVERLAY_ENTRY_QUERIES,
    });

    // Set up the dimensions so it is 100px wide
    overlays.updateChartDimensions('main', {
      widthPixels: 100,
      heightPixels: 50,
      scrollOffsetPixels: 0,
      allGroupsCollapsed: false,
    });
    overlays.updateChartDimensions('network', {
      widthPixels: 100,
      heightPixels: 50,
      scrollOffsetPixels: 0,
      allGroupsCollapsed: false,
    });

    const windowMin = TraceEngine.Types.Timing.MicroSeconds(0);
    const windowMax = TraceEngine.Types.Timing.MicroSeconds(100);
    // Set the visible window to be 0-100 microseconds
    overlays.updateVisibleWindow(TraceEngine.Helpers.Timing.traceWindowFromMicroSeconds(windowMin, windowMax));

    // Now set an event to be at 50 microseconds.
    const event = makeInstantEvent('test-event', 50);

    const xPosition = overlays.xPixelForEventStartOnChart(event);
    assert.strictEqual(xPosition, 50);
  });

  it('can calculate the y position of a main chart event', async function() {
    const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
    const charts = createCharts(traceData);

    const flameChartsContainer = document.createElement('div');
    const mainFlameChartsContainer = flameChartsContainer.createChild('div');
    const networkFlameChartsContainer = flameChartsContainer.createChild('div');
    const container = flameChartsContainer.createChild('div');

    const overlays = new Overlays.Overlays.Overlays({
      container,
      flameChartsContainers: {
        main: mainFlameChartsContainer,
        network: networkFlameChartsContainer,
      },
      charts,
      entryQueries: FAKE_OVERLAY_ENTRY_QUERIES,
    });

    overlays.updateChartDimensions('main', {
      widthPixels: 1000,
      heightPixels: 500,
      scrollOffsetPixels: 0,
      allGroupsCollapsed: false,
    });
    overlays.updateChartDimensions('network', {
      widthPixels: 1000,
      heightPixels: 200,
      scrollOffsetPixels: 0,
      allGroupsCollapsed: false,
    });

    // Set the visible window to be the entire trace.
    overlays.updateVisibleWindow(traceData.Meta.traceBounds);

    // Find an event on the main chart that is not a frame (you cannot add overlays to frames)
    const event = charts.mainProvider.eventByIndex?.(50);
    assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);
    assert.isOk(event);
    const yPixel = overlays.yPixelForEventOnChart(event);
    // The Y offset for the main chart is 233px, but we add 208px on (200px for the
    // network chart, and 8px for the re-size handle) giving us the expected
    // 441px.
    assert.strictEqual(yPixel, 441);
  });

  it('can adjust the y position of a main chart event when the network track is collapsed', async function() {
    const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
    const charts = createCharts(traceData);

    const flameChartsContainer = document.createElement('div');
    const mainFlameChartsContainer = flameChartsContainer.createChild('div');
    const networkFlameChartsContainer = flameChartsContainer.createChild('div');
    const container = flameChartsContainer.createChild('div');

    const overlays = new Overlays.Overlays.Overlays({
      container,
      flameChartsContainers: {
        main: mainFlameChartsContainer,
        network: networkFlameChartsContainer,
      },
      charts,
      entryQueries: FAKE_OVERLAY_ENTRY_QUERIES,
    });

    overlays.updateChartDimensions('main', {
      widthPixels: 1000,
      heightPixels: 500,
      scrollOffsetPixels: 0,
      allGroupsCollapsed: false,
    });
    overlays.updateChartDimensions('network', {
      widthPixels: 1000,
      heightPixels: 34,
      scrollOffsetPixels: 0,
      // Make the network track collapsed
      allGroupsCollapsed: true,
    });

    // Set the visible window to be the entire trace.
    overlays.updateVisibleWindow(traceData.Meta.traceBounds);

    // Find an event on the main chart that is not a frame (you cannot add overlays to frames)
    const event = charts.mainProvider.eventByIndex?.(50);
    assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);
    assert.isOk(event);
    const yPixel = overlays.yPixelForEventOnChart(event);
    // The Y offset for the main chart is 233px, but we add 34px on (the height
    // of the collapsed network chart, with no resizer bar as it is hidden when
    // the network track is collapsed). This gives us 233+34 = 267.
    assert.strictEqual(yPixel, 267);
  });

  it('can calculate the y position of a network chart event', async function() {
    const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
    const charts = createCharts(traceData);

    const flameChartsContainer = document.createElement('div');
    const mainFlameChartsContainer = flameChartsContainer.createChild('div');
    const networkFlameChartsContainer = flameChartsContainer.createChild('div');
    const container = flameChartsContainer.createChild('div');

    const overlays = new Overlays.Overlays.Overlays({
      container,
      flameChartsContainers: {
        main: mainFlameChartsContainer,
        network: networkFlameChartsContainer,
      },
      charts,
      entryQueries: FAKE_OVERLAY_ENTRY_QUERIES,
    });

    overlays.updateChartDimensions('main', {
      widthPixels: 1000,
      heightPixels: 500,
      scrollOffsetPixels: 0,
      allGroupsCollapsed: false,
    });
    overlays.updateChartDimensions('network', {
      widthPixels: 1000,
      heightPixels: 200,
      scrollOffsetPixels: 0,
      allGroupsCollapsed: false,
    });

    // Set the visible window to be the entire trace.
    overlays.updateVisibleWindow(traceData.Meta.traceBounds);

    // Fake the level being visible: because we don't fully render the chart we
    // need to fake this for this test.
    sinon.stub(charts.networkChart, 'levelIsVisible').callsFake(() => true);

    // Find an event on the network chart
    const event = charts.networkProvider.eventByIndex?.(0);
    assert.isOk(event);
    const yPixel = overlays.yPixelForEventOnChart(event);
    // This event is in the first level, but the first level has some offset
    // above it to allow for the header row and the row with the timestamps on
    // it, hence why this value is not 0px.
    assert.strictEqual(yPixel, 34);
  });

  describe('rendering overlays', () => {
    function setupChartWithDimensionsAndAnnotationOverlayListeners(
        traceData: TraceEngine.Handlers.Types.TraceParseData): {
      container: HTMLElement,
      overlays: Overlays.Overlays.Overlays,
      charts: Overlays.Overlays.TimelineCharts,
    } {
      const charts = createCharts(traceData);

      const flameChartsContainer = document.createElement('div');
      const mainFlameChartsContainer = flameChartsContainer.createChild('div');
      const networkFlameChartsContainer = flameChartsContainer.createChild('div');
      const container = flameChartsContainer.createChild('div');

      const overlays = new Overlays.Overlays.Overlays({
        container,
        flameChartsContainers: {
          main: mainFlameChartsContainer,
          network: networkFlameChartsContainer,
        },
        charts,
        entryQueries: FAKE_OVERLAY_ENTRY_QUERIES,
      });
      const currManager = Timeline.ModificationsManager.ModificationsManager.activeManager();
      // The Annotations Overlays are added through the ModificationsManager listener
      currManager?.addEventListener(Timeline.ModificationsManager.AnnotationModifiedEvent.eventName, event => {
        const {overlay, action} = (event as Timeline.ModificationsManager.AnnotationModifiedEvent);
        if (action === 'Add') {
          overlays.add(overlay);
        }
        overlays.update();
      });

      // When an annotation overlay is remomved, this event is dispatched to the Modifications Manager.
      overlays.addEventListener(Overlays.Overlays.AnnotationOverlayActionEvent.eventName, event => {
        const {overlay, action} = (event as Overlays.Overlays.AnnotationOverlayActionEvent);
        if (action === 'Remove') {
          overlays.remove(overlay);
        }
        overlays.update();
      });

      overlays.updateChartDimensions('main', {
        widthPixels: 1000,
        heightPixels: 500,
        scrollOffsetPixels: 0,
        allGroupsCollapsed: false,
      });
      overlays.updateChartDimensions('network', {
        widthPixels: 1000,
        heightPixels: 200,
        scrollOffsetPixels: 0,
        allGroupsCollapsed: false,
      });

      // Set the visible window to be the entire trace.
      overlays.updateVisibleWindow(traceData.Meta.traceBounds);
      return {overlays, container, charts};
    }

    it('can render an entry selected overlay', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);
      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

      overlays.add({
        type: 'ENTRY_SELECTED',
        entry: event,
      });
      overlays.update();

      // Ensure that the overlay was created.
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_SELECTED');
      assert.isOk(overlayDOM);
    });

    it('does not render an ENTRY_OUTLINE if the entry is also the ENTRY_SELECTED entry', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);
      overlays.add({
        type: 'ENTRY_OUTLINE',
        entry: event,
        outlineReason: 'ERROR',
      });
      overlays.update();

      const outlineVisible =
          container.querySelector<HTMLElement>('.overlay-type-ENTRY_OUTLINE')?.style.display === 'block';
      assert.isTrue(outlineVisible, 'The ENTRY_OUTLINE should be visible');

      // Now make a selected entry too
      overlays.add({
        type: 'ENTRY_SELECTED',
        entry: event,
      });
      overlays.update();
      const outlineNowHidden =
          container.querySelector<HTMLElement>('.overlay-type-ENTRY_OUTLINE')?.style.display === 'none';
      assert.isTrue(outlineNowHidden, 'The ENTRY_OUTLINE should be hidden');
    });

    it('only ever renders a single selected overlay', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event1 = charts.mainProvider.eventByIndex?.(50);
      const event2 = charts.mainProvider.eventByIndex?.(51);
      assert.isOk(event1);
      assert.isOk(event2);

      overlays.add({
        type: 'ENTRY_SELECTED',
        entry: event1,
      });
      overlays.update();
      overlays.add({
        type: 'ENTRY_SELECTED',
        entry: event2,
      });
      overlays.update();

      // There should only be one of these
      const entrySelectedOverlays = container.querySelectorAll<HTMLElement>('.overlay-type-ENTRY_SELECTED');
      assert.lengthOf(entrySelectedOverlays, 1);
    });

    it('can render entry label overlay', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);
      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

      overlays.add({
        type: 'ENTRY_LABEL',
        entry: event,
        label: 'entry label',
      });
      overlays.update();

      // Ensure that the overlay was created.
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_LABEL');
      assert.isOk(overlayDOM);
    });

    it('only renders one CURSOR_TIMESTAMP_MARKER as it is a singleton', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      overlays.add({
        type: 'CURSOR_TIMESTAMP_MARKER',
        timestamp: traceData.Meta.traceBounds.min,
      });
      overlays.add({
        type: 'CURSOR_TIMESTAMP_MARKER',
        timestamp: traceData.Meta.traceBounds.max,
      });
      overlays.update();
      assert.lengthOf(container.children, 1);
    });

    it('can render the label for entry label overlay', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);
      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

      overlays.add({
        type: 'ENTRY_LABEL',
        entry: event,
        label: 'entry label',
      });
      overlays.update();

      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_LABEL');
      assert.isOk(overlayDOM);
      const component = overlayDOM?.querySelector('devtools-entry-label-overlay');
      assert.isOk(component?.shadowRoot);

      const elementsWrapper = component.shadowRoot.querySelector<HTMLElement>('.label-parts-wrapper');
      assert.isOk(elementsWrapper);

      const labelBox = elementsWrapper.querySelector<HTMLElement>('.label-box');
      assert.isOk(labelBox);

      const inputField = labelBox.querySelector<HTMLElement>('.input-field');
      assert.isOk(inputField);

      assert.strictEqual(inputField?.innerText, 'entry label');
    });

    it('Inputting `Enter`into label overlay makes it non-editable', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);
      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

      // Create an entry label overlay
      overlays.add({
        type: 'ENTRY_LABEL',
        entry: event,
        label: 'label',
      });
      overlays.update();

      // Ensure that the overlay was created.
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_LABEL');
      assert.isOk(overlayDOM);

      const component = overlayDOM?.querySelector('devtools-entry-label-overlay');
      assert.isOk(component?.shadowRoot);
      component.connectedCallback();
      const elementsWrapper = component.shadowRoot.querySelector<HTMLElement>('.label-parts-wrapper');
      assert.isOk(elementsWrapper);

      const label = elementsWrapper.querySelector<HTMLElement>('.label-box');
      assert.isOk(label);
      const inputField = label.querySelector<HTMLElement>('.input-field');
      assert.isOk(inputField);

      // Double click on the label box to make it editable and focus on it
      inputField.dispatchEvent(new FocusEvent('dblclick', {bubbles: true}));

      // Ensure the label content is editable
      assert.isTrue(inputField.isContentEditable);

      // Press `Enter` to make the lable not editable
      inputField.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', cancelable: true, bubbles: true}));

      // Ensure the label content is not editable
      assert.isFalse(inputField.isContentEditable);
    });

    it('Inputting `Enter` into time range label field when the label is empty removes the overlay', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);
      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

      // Create a time range overlay with an empty label
      overlays.add({
        type: 'TIME_RANGE',
        label: '',
        showDuration: true,
        // Make this overlay the entire span of the trace
        bounds: traceData.Meta.traceBounds,
      });
      overlays.update();

      // Ensure that the overlay was created.
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-TIME_RANGE');
      assert.isOk(overlayDOM);

      const component = overlayDOM?.querySelector('devtools-time-range-overlay');
      assert.isOk(component?.shadowRoot);
      component.connectedCallback();
      const rangeContainer = component.shadowRoot.querySelector<HTMLElement>('.range-container');
      assert.isOk(rangeContainer);

      const labelBox = rangeContainer.querySelector<HTMLElement>('.label-text');
      assert.isOk(labelBox);

      // Double click on the label box to make it editable and focus on it
      labelBox.dispatchEvent(new FocusEvent('dblclick', {bubbles: true}));

      // Press `Enter` on the label field
      labelBox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', cancelable: true, bubbles: true}));

      // Ensure that the entry overlay has been removed because it was saved empty
      assert.strictEqual(overlays.overlaysOfType('TIME_RANGE').length, 0);
    });

    it('Inputting `Enter` into time range label field when the label is not empty does not remove the overlay',
       async function() {
         const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
         const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
         const event = charts.mainProvider.eventByIndex?.(50);
         assert.isOk(event);
         assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

         // Create a time range overlay with a label
         overlays.add({
           type: 'TIME_RANGE',
           label: 'label',
           showDuration: true,
           // Make this overlay the entire span of the trace
           bounds: traceData.Meta.traceBounds,
         });
         overlays.update();

         // Ensure that the overlay was created.
         const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-TIME_RANGE');
         assert.isOk(overlayDOM);

         const component = overlayDOM?.querySelector('devtools-time-range-overlay');
         assert.isOk(component?.shadowRoot);
         component.connectedCallback();
         const rangeContainer = component.shadowRoot.querySelector<HTMLElement>('.range-container');
         assert.isOk(rangeContainer);

         const labelBox = rangeContainer.querySelector<HTMLElement>('.label-text');
         assert.isOk(labelBox);

         // Double click on the label box to make it editable and focus on it
         labelBox.dispatchEvent(new FocusEvent('dblclick', {bubbles: true}));

         // Press `Enter` on the label field
         labelBox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', cancelable: true, bubbles: true}));

         // Ensure that the entry overlay has not been because it was has a non-empty label
         assert.strictEqual(overlays.overlaysOfType('TIME_RANGE').length, 1);
       });

    it('Can create multiple Time Range Overlays for Time Range annotations', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);
      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

      Timeline.ModificationsManager.ModificationsManager.activeManager()?.createAnnotation({
        type: 'TIME_RANGE',
        label: 'label',
        bounds: traceData.Meta.traceBounds,
      });

      Timeline.ModificationsManager.ModificationsManager.activeManager()?.createAnnotation({
        type: 'TIME_RANGE',
        label: 'label2',
        bounds: traceData.Meta.traceBounds,
      });
      overlays.update();

      assert.strictEqual(overlays.overlaysOfType('TIME_RANGE').length, 2);
    });

    it('Removes empty label if it is empty when navigated away from (removed focused from)', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);

      // Create an entry label overlay
      Timeline.ModificationsManager.ModificationsManager.activeManager()?.createAnnotation({
        type: 'ENTRY_LABEL',
        entry: event as TraceEngine.Types.TraceEvents.TraceEventData,
        label: '',
      });
      overlays.update();

      // Ensure that the overlay was created.
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_LABEL');
      assert.isOk(overlayDOM);
      const component = overlayDOM?.querySelector('devtools-entry-label-overlay');
      assert.isOk(component?.shadowRoot);

      component.connectedCallback();
      const elementsWrapper = component.shadowRoot.querySelector<HTMLElement>('.label-parts-wrapper');
      assert.isOk(elementsWrapper);

      const label = elementsWrapper.querySelector<HTMLElement>('.label-box');
      assert.isOk(label);

      const inputField = elementsWrapper.querySelector<HTMLElement>('.input-field');
      assert.isOk(inputField);

      // Double click on the label box to make it editable and focus on it
      inputField.dispatchEvent(new FocusEvent('dblclick', {bubbles: true}));

      // Ensure that the entry has 1 overlay
      assert.strictEqual(overlays.overlaysForEntry(event).length, 1);

      // Change the content to not editable by changing the element blur like when clicking outside of it.
      // The label is empty since no initial value was passed into it and no characters were entered.
      inputField.dispatchEvent(new FocusEvent('blur', {bubbles: true}));

      // Ensure that the entry overlay has been removed because it was saved empty
      assert.strictEqual(overlays.overlaysForEntry(event).length, 0);
    });

    it('Update label overlay when the label changes', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);

      // Create an entry label overlay
      Timeline.ModificationsManager.ModificationsManager.activeManager()?.createAnnotation({
        type: 'ENTRY_LABEL',
        entry: event as TraceEngine.Types.TraceEvents.TraceEventData,
        label: '',
      });
      overlays.update();

      // Ensure that the overlay was created.
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_LABEL');
      assert.isOk(overlayDOM);
      const component = overlayDOM?.querySelector('devtools-entry-label-overlay');
      assert.isOk(component?.shadowRoot);

      component.connectedCallback();
      component.dispatchEvent(new Components.EntryLabelOverlay.EntryLabelChangeEvent('new label'));

      const updatedOverlay = overlays.overlaysForEntry(event)[0] as Overlays.Overlays.EntryLabel;
      assert.isOk(updatedOverlay);
      // Make sure the label was updated in the Overlay Object
      assert.strictEqual(updatedOverlay.label, 'new label');
    });

    it('creates an overlay for a time range when an time range annotation is created', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);

      // Since TIME_RANGE is AnnotationOverlay, create it through ModificationsManager
      Timeline.ModificationsManager.ModificationsManager.activeManager()?.createAnnotation({
        type: 'TIME_RANGE',
        label: '',
        // Make this overlay the entire span of the trace
        bounds: traceData.Meta.traceBounds,
      });
      overlays.update();
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-TIME_RANGE');
      assert.isOk(overlayDOM);
    });

    it('can render an overlay for a time range', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      overlays.add({
        type: 'TIME_RANGE',
        label: '',
        showDuration: true,
        // Make this overlay the entire span of the trace
        bounds: traceData.Meta.traceBounds,
      });
      overlays.update();
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-TIME_RANGE');
      assert.isOk(overlayDOM);
    });

    it('can update a time range overlay with new bounds', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const rangeOverlay = overlays.add({
        type: 'TIME_RANGE',
        label: '',
        showDuration: true,
        // Make this overlay the entire span of the trace
        bounds: traceData.Meta.traceBounds,
      });
      overlays.update();
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-TIME_RANGE');
      assert.isOk(overlayDOM);
      const firstWidth = window.parseInt(overlayDOM.style.width);

      // change the bounds so the new min is +1second of time.
      const newBounds = TraceEngine.Helpers.Timing.traceWindowFromMicroSeconds(
          TraceEngine.Types.Timing.MicroSeconds(rangeOverlay.bounds.min + (1_000 * 1_000)),
          rangeOverlay.bounds.max,
      );
      overlays.updateExisting(rangeOverlay, {bounds: newBounds});
      overlays.update();
      const secondWidth = window.parseInt(overlayDOM.style.width);
      // The new time range is smaller so the DOM element should have less width
      assert.isTrue(secondWidth < firstWidth);
    });

    it('renders the overlay for a selected layout shift entry correctly', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'cls-single-frame.json.gz');
      const {overlays, container} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const layoutShiftEvent = traceData.LayoutShifts.clusters.at(0)?.events.at(0);
      if (!layoutShiftEvent) {
        throw new Error('layoutShiftEvent was unexpectedly undefined');
      }
      overlays.add({
        type: 'ENTRY_SELECTED',
        entry: layoutShiftEvent,
      });
      const boundsRange = TraceEngine.Types.Timing.MicroSeconds(20_000);
      const boundsMax = TraceEngine.Types.Timing.MicroSeconds(layoutShiftEvent.ts + boundsRange);
      overlays.updateVisibleWindow({min: layoutShiftEvent.ts, max: boundsMax, range: boundsRange});
      overlays.update();
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_SELECTED');
      assert.isOk(overlayDOM);
      assert.strictEqual(window.parseInt(overlayDOM.style.width), 250);
    });

    it('renders the duration and label for a time range overlay', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      overlays.add({
        type: 'TIME_RANGE',
        label: '',
        showDuration: true,
        // Make this overlay the entire span of the trace
        bounds: traceData.Meta.traceBounds,
      });
      overlays.update();
      await coordinator.done();
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-TIME_RANGE');
      const component = overlayDOM?.querySelector('devtools-time-range-overlay');
      assert.isOk(component?.shadowRoot);
      const rangeContainer = component.shadowRoot.querySelector<HTMLElement>('.range-container');
      assert.isOk(rangeContainer);
      const duration = rangeContainer.querySelector<HTMLElement>('.duration');
      assert.isOk(duration);
      assert.strictEqual(duration?.innerText, '1.26\xA0s');
    });

    it('can remove an overlay', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);
      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

      const selectedOverlay = overlays.add({
        type: 'ENTRY_SELECTED',
        entry: event,
      });
      overlays.update();
      assert.lengthOf(container.children, 1);

      overlays.remove(selectedOverlay);
      overlays.update();
      assert.lengthOf(container.children, 0);
    });

    it('can render an entry selected overlay for a frame', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev-with-commit.json.gz');
      const {overlays, container, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const timelineFrame = charts.mainProvider.eventByIndex?.(5);
      assert.isOk(timelineFrame);
      assert.instanceOf(timelineFrame, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

      overlays.add({
        type: 'ENTRY_SELECTED',
        entry: timelineFrame,
      });
      overlays.update();

      // Ensure that the overlay was created.
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_SELECTED');
      assert.isOk(overlayDOM);
    });

    it('can return a list of overlays for an entry', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);
      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);

      overlays.add({
        type: 'ENTRY_SELECTED',
        entry: event,
      });

      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);
      const existingOverlays = overlays.overlaysForEntry(event);
      assert.deepEqual(existingOverlays, [{
                         type: 'ENTRY_SELECTED',
                         entry: event,
                       }]);
    });

    it('can delete overlays and remove them from the DOM', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {container, overlays, charts} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);

      assert.notInstanceOf(event, TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame);
      overlays.add({
        type: 'ENTRY_SELECTED',
        entry: event,
      });
      overlays.update();

      assert.lengthOf(container.children, 1);
      const removedCount = overlays.removeOverlaysOfType('ENTRY_SELECTED');
      assert.strictEqual(removedCount, 1);
      assert.lengthOf(container.children, 0);
    });

    it('the label entry field is editable when created', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const charts = createCharts(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);

      // Since ENTRY_LABEL is AnnotationOverlay, create it through ModificationsManager
      Timeline.ModificationsManager.ModificationsManager.activeManager()?.createAnnotation({
        type: 'ENTRY_LABEL',
        label: '',
        entry: event as TraceEngine.Types.TraceEvents.TraceEventData,
      });

      overlays.update();
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_LABEL');
      assert.isOk(overlayDOM);
      const component = overlayDOM?.querySelector('devtools-entry-label-overlay');
      assert.isOk(component?.shadowRoot);

      const elementsWrapper = component.shadowRoot.querySelector<HTMLElement>('.label-parts-wrapper');
      const labelBox = elementsWrapper?.querySelector<HTMLElement>('.label-box') as HTMLSpanElement;

      const inputField = labelBox.querySelector<HTMLElement>('.input-field');
      assert.isOk(inputField);
      // The label input box should be editable after it is created and before anything else happened
      assert.isTrue(inputField.isContentEditable);
    });

    it('the label entry field is in focus after being double clicked on', async function() {
      const {traceData} = await TraceLoader.traceEngine(this, 'web-dev.json.gz');
      const {overlays, container} = setupChartWithDimensionsAndAnnotationOverlayListeners(traceData);
      const charts = createCharts(traceData);
      const event = charts.mainProvider.eventByIndex?.(50);
      assert.isOk(event);

      // Since ENTRY_LABEL is AnnotationOverlay, create it through ModificationsManager
      Timeline.ModificationsManager.ModificationsManager.activeManager()?.createAnnotation({
        type: 'ENTRY_LABEL',
        label: '',
        entry: event as TraceEngine.Types.TraceEvents.TraceEventData,
      });

      overlays.update();
      const overlayDOM = container.querySelector<HTMLElement>('.overlay-type-ENTRY_LABEL');
      assert.isOk(overlayDOM);
      const component = overlayDOM?.querySelector('devtools-entry-label-overlay');
      assert.isOk(component?.shadowRoot);

      const elementsWrapper = component.shadowRoot.querySelector<HTMLElement>('.label-parts-wrapper');
      assert.isOk(elementsWrapper);
      const labelBox = elementsWrapper.querySelector<HTMLElement>('.label-box') as HTMLSpanElement;

      const inputField = labelBox.querySelector<HTMLElement>('.input-field');
      assert.isOk(inputField);

      // The label input box should be editable after it is created and before anything else happened
      assert.isTrue(inputField.isContentEditable);

      // Make the content to editable by changing the element blur like when clicking outside of it.
      // When that happens, the content should be set to not editable.
      inputField.dispatchEvent(new FocusEvent('blur', {bubbles: true}));
      assert.isFalse(inputField.isContentEditable);

      // Double click on the label to make it editable again
      inputField.dispatchEvent(new FocusEvent('dblclick', {bubbles: true}));
      assert.isTrue(inputField.isContentEditable);
    });
  });

  describe('traceWindowContainingOverlays', () => {
    it('calculates the smallest window that fits the overlay inside', () => {
      const FAKE_EVENT_1 = {
        ts: 0,
        dur: 10,
      } as TraceEngine.Types.TraceEvents.TraceEventData;
      const FAKE_EVENT_2 = {
        ts: 5,
        dur: 100,
      } as TraceEngine.Types.TraceEvents.TraceEventData;

      const overlay1: Overlays.Overlays.EntryOutline = {
        entry: FAKE_EVENT_1,
        type: 'ENTRY_OUTLINE',
        outlineReason: 'INFO',
      };
      const overlay2: Overlays.Overlays.EntryOutline = {
        entry: FAKE_EVENT_2,
        type: 'ENTRY_OUTLINE',
        outlineReason: 'INFO',
      };
      const traceWindow = Overlays.Overlays.traceWindowContainingOverlays([overlay1, overlay2]);
      assert.strictEqual(traceWindow.min, 0);
      assert.strictEqual(traceWindow.max, 105);
    });
  });
});
