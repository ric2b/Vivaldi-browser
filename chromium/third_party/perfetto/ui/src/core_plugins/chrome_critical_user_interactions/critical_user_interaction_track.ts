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

import {OnSliceClickArgs} from '../../frontend/base_slice_track';
import {GenericSliceDetailsTab} from '../../frontend/generic_slice_details_tab';
import {globals} from '../../frontend/globals';
import {NAMED_ROW} from '../../frontend/named_slice_track';
import {NUM, STR} from '../../trace_processor/query_result';
import {Slice} from '../../public/track';
import {
  CustomSqlDetailsPanelConfig,
  CustomSqlImportConfig,
  CustomSqlTableDefConfig,
  CustomSqlTableSliceTrack,
} from '../../frontend/tracks/custom_sql_table_slice_track';
import {PageLoadDetailsPanel} from './page_load_details_panel';
import {StartupDetailsPanel} from './startup_details_panel';
import {WebContentInteractionPanel} from './web_content_interaction_details_panel';

export const CRITICAL_USER_INTERACTIONS_KIND =
  'org.chromium.CriticalUserInteraction.track';

export const CRITICAL_USER_INTERACTIONS_ROW = {
  ...NAMED_ROW,
  scopedId: NUM,
  type: STR,
};
export type CriticalUserInteractionRow = typeof CRITICAL_USER_INTERACTIONS_ROW;

export interface CriticalUserInteractionSlice extends Slice {
  scopedId: number;
  type: string;
}

enum CriticalUserInteractionType {
  UNKNOWN = 'Unknown',
  PAGE_LOAD = 'chrome_page_loads',
  STARTUP = 'chrome_startups',
  WEB_CONTENT_INTERACTION = 'chrome_web_content_interactions',
}

function convertToCriticalUserInteractionType(
  cujType: string,
): CriticalUserInteractionType {
  switch (cujType) {
    case CriticalUserInteractionType.PAGE_LOAD:
      return CriticalUserInteractionType.PAGE_LOAD;
    case CriticalUserInteractionType.STARTUP:
      return CriticalUserInteractionType.STARTUP;
    case CriticalUserInteractionType.WEB_CONTENT_INTERACTION:
      return CriticalUserInteractionType.WEB_CONTENT_INTERACTION;
    default:
      return CriticalUserInteractionType.UNKNOWN;
  }
}

export class CriticalUserInteractionTrack extends CustomSqlTableSliceTrack {
  static readonly kind = `/critical_user_interactions`;

  getSqlDataSource(): CustomSqlTableDefConfig {
    return {
      columns: [
        // The scoped_id is not a unique identifier within the table; generate
        // a unique id from type and scoped_id on the fly to use for slice
        // selection.
        'hash(type, scoped_id) AS id',
        'scoped_id AS scopedId',
        'name',
        'ts',
        'dur',
        'type',
      ],
      sqlTableName: 'chrome_interactions',
    };
  }

  getDetailsPanel(
    args: OnSliceClickArgs<CriticalUserInteractionSlice>,
  ): CustomSqlDetailsPanelConfig {
    let detailsPanel = {
      kind: GenericSliceDetailsTab.kind,
      config: {
        sqlTableName: this.tableName,
        title: 'Chrome Interaction',
      },
    };

    switch (convertToCriticalUserInteractionType(args.slice.type)) {
      case CriticalUserInteractionType.PAGE_LOAD:
        detailsPanel = {
          kind: PageLoadDetailsPanel.kind,
          config: {
            sqlTableName: this.tableName,
            title: 'Chrome Page Load',
          },
        };
        break;
      case CriticalUserInteractionType.STARTUP:
        detailsPanel = {
          kind: StartupDetailsPanel.kind,
          config: {
            sqlTableName: this.tableName,
            title: 'Chrome Startup',
          },
        };
        break;
      case CriticalUserInteractionType.WEB_CONTENT_INTERACTION:
        detailsPanel = {
          kind: WebContentInteractionPanel.kind,
          config: {
            sqlTableName: this.tableName,
            title: 'Chrome Web Content Interaction',
          },
        };
        break;
      default:
        break;
    }
    return detailsPanel;
  }

  onSliceClick(args: OnSliceClickArgs<CriticalUserInteractionSlice>) {
    const detailsPanelConfig = this.getDetailsPanel(args);
    globals.selectionManager.setGenericSlice({
      id: args.slice.scopedId,
      sqlTableName: this.tableName,
      start: args.slice.ts,
      duration: args.slice.dur,
      trackUri: this.uri,
      detailsPanelConfig: {
        kind: detailsPanelConfig.kind,
        config: detailsPanelConfig.config,
      },
    });
  }

  getSqlImports(): CustomSqlImportConfig {
    return {
      modules: ['chrome.interactions'],
    };
  }

  getRowSpec(): CriticalUserInteractionRow {
    return CRITICAL_USER_INTERACTIONS_ROW;
  }

  rowToSlice(row: CriticalUserInteractionRow): CriticalUserInteractionSlice {
    const baseSlice = super.rowToSlice(row);
    const scopedId = row.scopedId;
    const type = row.type;
    return {...baseSlice, scopedId, type};
  }
}
