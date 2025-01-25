// Copyright (C) 2020 The Android Open Source Project
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

import {ColumnDef} from '../../common/aggregation_data';
import {Area, Sorting} from '../../common/state';
import {globals} from '../../frontend/globals';
import {Engine} from '../../trace_processor/engine';

import {AggregationController} from './aggregation_controller';
import {
  ASYNC_SLICE_TRACK_KIND,
  THREAD_SLICE_TRACK_KIND,
} from '../../core/track_kinds';

export function getSelectedTrackKeys(area: Area): number[] {
  const selectedTrackKeys: number[] = [];
  for (const trackKey of area.tracks) {
    const track = globals.state.tracks[trackKey];
    // Track will be undefined for track groups.
    if (track?.uri !== undefined) {
      const trackInfo = globals.trackManager.resolveTrackInfo(track.uri);
      if (trackInfo?.tags?.kind === THREAD_SLICE_TRACK_KIND) {
        trackInfo.tags.trackIds &&
          selectedTrackKeys.push(...trackInfo.tags.trackIds);
      }
      if (trackInfo?.tags?.kind === ASYNC_SLICE_TRACK_KIND) {
        trackInfo.tags.trackIds &&
          selectedTrackKeys.push(...trackInfo.tags.trackIds);
      }
    }
  }
  return selectedTrackKeys;
}

export class SliceAggregationController extends AggregationController {
  async createAggregateView(engine: Engine, area: Area) {
    await engine.query(`drop view if exists ${this.kind};`);

    const selectedTrackKeys = getSelectedTrackKeys(area);

    if (selectedTrackKeys.length === 0) return false;

    const query = `create view ${this.kind} as
        SELECT
        name,
        sum(dur) AS total_dur,
        sum(dur)/count(1) as avg_dur,
        count(1) as occurrences
        FROM slices
        WHERE track_id IN (${selectedTrackKeys}) AND
        ts + dur > ${area.start} AND
        ts < ${area.end} group by name`;

    await engine.query(query);
    return true;
  }

  getTabName() {
    return 'Slices';
  }

  async getExtra() {}

  getDefaultSorting(): Sorting {
    return {column: 'total_dur', direction: 'DESC'};
  }

  getColumnDefinitions(): ColumnDef[] {
    return [
      {
        title: 'Name',
        kind: 'STRING',
        columnConstructor: Uint32Array,
        columnId: 'name',
      },
      {
        title: 'Wall duration (ms)',
        kind: 'TIMESTAMP_NS',
        columnConstructor: Float64Array,
        columnId: 'total_dur',
        sum: true,
      },
      {
        title: 'Avg Wall duration (ms)',
        kind: 'TIMESTAMP_NS',
        columnConstructor: Float64Array,
        columnId: 'avg_dur',
      },
      {
        title: 'Occurrences',
        kind: 'NUMBER',
        columnConstructor: Uint32Array,
        columnId: 'occurrences',
        sum: true,
      },
    ];
  }
}
