// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {Time} from '../base/time';
import {createEmptyRecordConfig} from '../controller/record_config_types';
import {featureFlags} from '../core/feature_flags';
import {Aggregation} from '../frontend/pivot_table_types';
import {
  autosaveConfigStore,
  recordTargetStore,
} from '../frontend/record_config';
import {NonSerializableState, State, STATE_VERSION} from './state';

const AUTOLOAD_STARTED_CONFIG_FLAG = featureFlags.register({
  id: 'autoloadStartedConfig',
  name: 'Auto-load last used recording config',
  description:
    'Starting a recording automatically saves its configuration. ' +
    'This flag controls whether this config is automatically loaded.',
  defaultValue: true,
});

export function keyedMap<T>(
  keyFn: (key: T) => string,
  ...values: T[]
): Map<string, T> {
  const result = new Map<string, T>();

  for (const value of values) {
    result.set(keyFn(value), value);
  }

  return result;
}

export const COUNT_AGGREGATION: Aggregation = {
  aggregationFunction: 'COUNT',
  // Exact column is ignored for count aggregation because it does not matter
  // what to count, use empty strings.
  column: {kind: 'regular', table: '', column: ''},
};

export function createEmptyNonSerializableState(): NonSerializableState {
  return {
    pivotTable: {
      queryResult: null,
      selectedPivots: [
        {
          kind: 'regular',
          table: '_slice_with_thread_and_process_info',
          column: 'name',
        },
      ],
      selectedAggregations: [
        {
          aggregationFunction: 'SUM',
          column: {
            kind: 'regular',
            table: '_slice_with_thread_and_process_info',
            column: 'dur',
          },
          sortDirection: 'DESC',
        },
        {
          aggregationFunction: 'SUM',
          column: {
            kind: 'regular',
            table: '_slice_with_thread_and_process_info',
            column: 'thread_dur',
          },
        },
        COUNT_AGGREGATION,
      ],
      constrainToArea: true,
      queryRequested: false,
    },
  };
}

export function createEmptyState(): State {
  return {
    version: STATE_VERSION,
    nextId: '-1',
    newEngineMode: 'USE_HTTP_RPC_IF_AVAILABLE',
    aggregatePreferences: {},
    queries: {},

    recordConfig: AUTOLOAD_STARTED_CONFIG_FLAG.get()
      ? autosaveConfigStore.get()
      : createEmptyRecordConfig(),
    displayConfigAsPbtxt: false,
    lastLoadedConfig: {type: 'NONE'},

    status: {msg: '', timestamp: 0},
    traceConversionInProgress: false,

    perfDebug: false,
    sidebarVisible: true,
    hoveredUtid: -1,
    hoveredPid: -1,
    hoverCursorTimestamp: Time.INVALID,
    hoveredNoteTimestamp: Time.INVALID,
    highlightedSliceId: -1,
    focusedFlowIdLeft: -1,
    focusedFlowIdRight: -1,

    recordingInProgress: false,
    recordingCancelled: false,
    extensionInstalled: false,
    flamegraphModalDismissed: false,
    recordingTarget: recordTargetStore.getValidTarget(),
    availableAdbDevices: [],

    fetchChromeCategories: false,
    chromeCategories: undefined,
    nonSerializableState: createEmptyNonSerializableState(),

    // Somewhere to store plugins' persistent state.
    plugins: {},

    trackFilterTerm: undefined,
    forceRunControllers: 0,
  };
}
