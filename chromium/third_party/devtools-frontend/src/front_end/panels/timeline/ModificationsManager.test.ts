// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as TraceEngine from '../../models/trace/trace.js';
import {describeWithEnvironment} from '../../testing/EnvironmentHelpers.js';
import {TraceLoader} from '../../testing/TraceLoader.js';

import * as Timeline from './timeline.js';

describeWithEnvironment('ModificationsManager', () => {
  it('applies modifications when present in a trace file', async function() {
    await TraceLoader.traceEngine(null, 'web-dev-modifications.json.gz');
    const modificationsManager = Timeline.ModificationsManager.ModificationsManager.activeManager();
    if (!modificationsManager) {
      throw new Error('Modifications manager does not exist.');
    }
    const entriesFilter = modificationsManager.getEntriesFilter();
    assert.strictEqual(entriesFilter.expandableEntries().length, 1);
    assert.strictEqual(entriesFilter.invisibleEntries().length, 108);
    assert.deepEqual(modificationsManager.getTimelineBreadcrumbs().initialBreadcrumb, {
      window: {min: 1020034823047, max: 1020036087961, range: 1264914},
      child: {window: {min: 1020034823047, max: 1020035228006.5569, range: 404959.5568847656}, child: null},
    } as TraceEngine.Types.File.Breadcrumb);
    // Make sure the saved Label Annotation is applied
    const labelAnnotation = modificationsManager.getAnnotations()[0];
    const label = (labelAnnotation.type === 'ENTRY_LABEL') ? labelAnnotation.label : '';
    assert.deepEqual(labelAnnotation.type, 'ENTRY_LABEL');
    assert.deepEqual(label, 'Initialize App');
    // Make sure the saved Range Annotation is applied
    const timeRangeAnnotation = modificationsManager.getAnnotations()[1];
    const rangeLabel = (timeRangeAnnotation.type === 'TIME_RANGE') ? timeRangeAnnotation.label : '';
    assert.deepEqual(modificationsManager.getAnnotations()[1].type, 'TIME_RANGE');
    assert.deepEqual(rangeLabel, 'Visibility change 1');
  });

  it('generates a serializable modifications json ', async function() {
    await TraceLoader.traceEngine(null, 'web-dev-modifications.json.gz');
    const modificationsManager = Timeline.ModificationsManager.ModificationsManager.activeManager();
    if (!modificationsManager) {
      throw new Error('Modifications manager does not exist.');
    }
    const entriesFilter = modificationsManager.getEntriesFilter();
    const modifications = modificationsManager.toJSON();
    assert.strictEqual(entriesFilter.expandableEntries().length, 1);
    assert.strictEqual(modifications.entriesModifications.expandableEntries.length, 1);
    assert.strictEqual(modifications.entriesModifications.hiddenEntries.length, 108);
    assert.deepEqual(modifications.initialBreadcrumb, {
      window: {min: 1020034823047, max: 1020036087961, range: 1264914},
      child: {window: {min: 1020034823047, max: 1020035228006.5569, range: 404959.5568847656}, child: null},
    } as TraceEngine.Types.File.Breadcrumb);
    assert.deepEqual(modifications.annotations.entryLabels, [
      {entry: 'p-73704-775-2151-457', label: 'Initialize App'},
    ]);
    assert.deepEqual(modifications.annotations.labelledTimeRanges, [
      {
        bounds: {min: 1020034870460.4769, max: 1020034880507.9258, range: 10047.448852539062},
        label: 'Visibility change 1',
      },
    ]);
  });

  it('creates annotations and generates correct json for annotations', async function() {
    const traceParsedData = (await TraceLoader.traceEngine(null, 'web-dev-with-commit.json.gz')).traceData;
    // Get any entres to create a label and a link with.
    const entry = traceParsedData.Renderer.allTraceEntries[0];
    const entry2 = traceParsedData.Renderer.allTraceEntries[1];

    const modificationsManager = Timeline.ModificationsManager.ModificationsManager.activeManager();
    assert.isOk(modificationsManager);

    modificationsManager.createAnnotation({
      type: 'ENTRY_LABEL',
      entry,
      label: 'entry label',
    });

    modificationsManager.createAnnotation({
      type: 'ENTRIES_LINK',
      entryFrom: entry,
      entryTo: entry2,
    });

    modificationsManager.createAnnotation({
      type: 'TIME_RANGE',
      bounds: {
        min: TraceEngine.Types.Timing.MicroSeconds(0),
        max: TraceEngine.Types.Timing.MicroSeconds(10),
        range: TraceEngine.Types.Timing.MicroSeconds(10),
      },
      label: 'range label',
    });

    const modifications = modificationsManager.toJSON().annotations;
    assert.deepEqual(modifications, {
      entryLabels: [{
        entry: 'r-38',
        label: 'entry label',
      }],
      labelledTimeRanges: [{
        bounds: {
          min: TraceEngine.Types.Timing.MicroSeconds(0),
          max: TraceEngine.Types.Timing.MicroSeconds(10),
          range: TraceEngine.Types.Timing.MicroSeconds(10),
        },
        label: 'range label',
      }],
      linksBetweenEntries: [{
        entryFrom: 'r-38',
        entryTo: 'r-39',
      }],
    });
  });

  it('does not add the annotation link between entries into the json saved into metadata if `entryTo` does not exist',
     async function() {
       const traceParsedData = (await TraceLoader.traceEngine(null, 'web-dev-with-commit.json.gz')).traceData;
       // Get any entry to create links with.
       const entry = traceParsedData.Renderer.allTraceEntries[0];
       const entry2 = traceParsedData.Renderer.allTraceEntries[1];

       const modificationsManager = Timeline.ModificationsManager.ModificationsManager.activeManager();
       assert.isOk(modificationsManager);

       modificationsManager.createAnnotation({
         type: 'ENTRIES_LINK',
         entryFrom: entry,
         entryTo: entry2,
       });

       modificationsManager.createAnnotation({
         type: 'ENTRIES_LINK',
         entryFrom: entry2,
       });

       // Make sure only the link with both 'to' and 'from' entries in in the generated JSON
       const modifications = modificationsManager.toJSON().annotations;
       assert.deepEqual(modifications, {
         entryLabels: [],
         labelledTimeRanges: [],
         linksBetweenEntries: [{
           entryFrom: 'r-38',
           entryTo: 'r-39',
         }],
       });
     });
});
