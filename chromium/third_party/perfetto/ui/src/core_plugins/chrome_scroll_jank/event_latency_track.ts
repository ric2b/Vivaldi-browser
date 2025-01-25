// Copyright (C) 2023 The Android Open Source Project
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

import {globals} from '../../frontend/globals';
import {NamedRow} from '../../frontend/named_slice_track';
import {NewTrackArgs} from '../../frontend/track';
import {CHROME_EVENT_LATENCY_TRACK_KIND} from '../../public/track_kinds';
import {Slice} from '../../public/track';
import {
  CustomSqlDetailsPanelConfig,
  CustomSqlTableDefConfig,
  CustomSqlTableSliceTrack,
} from '../../frontend/tracks/custom_sql_table_slice_track';
import {EventLatencySliceDetailsPanel} from './event_latency_details_panel';
import {JANK_COLOR} from './jank_colors';
import {ScrollJankPluginState} from './common';

export const JANKY_LATENCY_NAME = 'Janky EventLatency';

export class EventLatencyTrack extends CustomSqlTableSliceTrack {
  constructor(
    args: NewTrackArgs,
    private baseTable: string,
  ) {
    super(args);
    ScrollJankPluginState.getInstance().registerTrack({
      kind: CHROME_EVENT_LATENCY_TRACK_KIND,
      trackUri: this.uri,
      tableName: this.tableName,
      detailsPanelConfig: this.getDetailsPanel(),
    });
  }

  async onDestroy(): Promise<void> {
    await super.onDestroy();
    ScrollJankPluginState.getInstance().unregisterTrack(
      CHROME_EVENT_LATENCY_TRACK_KIND,
    );
  }

  getSqlSource(): string {
    return `SELECT * FROM ${this.baseTable}`;
  }

  getDetailsPanel(): CustomSqlDetailsPanelConfig {
    return {
      kind: EventLatencySliceDetailsPanel.kind,
      config: {title: '', sqlTableName: this.tableName},
    };
  }

  getSqlDataSource(): CustomSqlTableDefConfig {
    return {
      sqlTableName: this.baseTable,
    };
  }

  rowToSlice(row: NamedRow): Slice {
    const baseSlice = super.rowToSlice(row);
    if (baseSlice.title === JANKY_LATENCY_NAME) {
      return {...baseSlice, colorScheme: JANK_COLOR};
    } else {
      return baseSlice;
    }
  }

  onUpdatedSlices(slices: Slice[]) {
    for (const slice of slices) {
      const currentSelection = globals.selectionManager.legacySelection;
      const isSelected =
        currentSelection &&
        currentSelection.kind === 'GENERIC_SLICE' &&
        currentSelection.id !== undefined &&
        currentSelection.id === slice.id;

      const highlighted = globals.state.highlightedSliceId === slice.id;
      const hasFocus = highlighted || isSelected;
      slice.isHighlighted = !!hasFocus;
    }
    super.onUpdatedSlices(slices);
  }

  // At the moment we will just display the slice details. However, on select,
  // this behavior should be customized to show jank-related data.
}
