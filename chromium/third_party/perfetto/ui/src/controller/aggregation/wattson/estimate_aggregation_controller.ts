// Copyright (C) 2024 The Android Open Source Project
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

import {ColumnDef} from '../../../common/aggregation_data';
import {Area, Sorting} from '../../../common/state';
import {globals} from '../../../frontend/globals';
import {Engine} from '../../../trace_processor/engine';
import {CPUSS_ESTIMATE_TRACK_KIND} from '../../../core/track_kinds';
import {AggregationController} from '../aggregation_controller';
import {hasWattsonSupport} from '../../../core/trace_config_utils';
import {exists} from '../../../base/utils';

export class WattsonEstimateAggregationController extends AggregationController {
  async createAggregateView(engine: Engine, area: Area) {
    await engine.query(`drop view if exists ${this.kind};`);

    // Short circuit if Wattson is not supported for this Perfetto trace
    if (!(await hasWattsonSupport(engine))) return false;

    const estimateTracks: string[] = [];
    for (const trackKey of area.tracks) {
      const track = globals.state.tracks[trackKey];
      if (track?.uri) {
        const trackInfo = globals.trackManager.resolveTrackInfo(track.uri);
        if (
          trackInfo?.tags?.kind === CPUSS_ESTIMATE_TRACK_KIND &&
          exists(trackInfo.tags?.wattson)
        ) {
          estimateTracks.push(`${trackInfo.tags.wattson}`);
        }
      }
    }
    if (estimateTracks.length === 0) return false;

    const query = this.getEstimateTracksQuery(area, estimateTracks);
    engine.query(query);

    return true;
  }

  getEstimateTracksQuery(
    area: Area,
    estimateTracks: ReadonlyArray<string>,
  ): string {
    const duration = area.end - area.start;
    let query = `
      DROP TABLE IF EXISTS _ss_converted_to_mw;
      CREATE PERFETTO TABLE _ss_converted_to_mw AS
      SELECT *,
        ((IFNULL(l3_hit_value, 0) + IFNULL(l3_miss_value, 0)) * 1000 / dur)
          + static_curve as dsu_scu_curve
      FROM _system_state_curves;

      DROP TABLE IF EXISTS _ui_selection_window;
      CREATE PERFETTO TABLE _ui_selection_window AS
      SELECT
        ${area.start} as ts,
        ${duration} as dur;

      DROP TABLE IF EXISTS _windowed_cpuss_estimate;
      CREATE VIRTUAL TABLE _windowed_cpuss_estimate
      USING
        SPAN_JOIN(_ui_selection_window, _ss_converted_to_mw);

      CREATE VIEW ${this.kind} AS
    `;

    // Convert average power track to total energy in UI window, then divide by
    // duration of window to get average estimated power of the window
    estimateTracks.forEach((estimateTrack, i) => {
      if (i != 0) {
        query += `UNION ALL `;
      }
      query += `
        SELECT
        '${estimateTrack}' as name,
        ROUND(SUM(${estimateTrack}_curve * dur) / ${duration}, 2) as power,
        ROUND(SUM(${estimateTrack}_curve * dur) / 1000000000, 2) as energy
        FROM _windowed_cpuss_estimate
      `;
    });
    query += `;`;

    return query;
  }

  getColumnDefinitions(): ColumnDef[] {
    return [
      {
        title: 'Name',
        kind: 'STRING',
        columnConstructor: Uint16Array,
        columnId: 'name',
      },
      {
        title: 'Average estimated power (mW)',
        kind: 'NUMBER',
        columnConstructor: Float64Array,
        columnId: 'power',
        sum: true,
      },
      {
        title: 'Total estimated energy (mWs)',
        kind: 'NUMBER',
        columnConstructor: Float64Array,
        columnId: 'energy',
        sum: true,
      },
    ];
  }

  async getExtra() {}

  getTabName() {
    return 'Wattson estimates';
  }

  getDefaultSorting(): Sorting {
    return {column: 'name', direction: 'ASC'};
  }
}
