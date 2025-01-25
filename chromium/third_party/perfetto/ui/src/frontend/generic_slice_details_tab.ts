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

import m from 'mithril';
import {GenericSliceDetailsTabConfig} from '../public/details_panel';
import {raf} from '../core/raf_scheduler';
import {ColumnType} from '../trace_processor/query_result';
import {sqlValueToReadableString} from '../trace_processor/sql_utils';
import {DetailsShell} from '../widgets/details_shell';
import {GridLayout} from '../widgets/grid_layout';
import {Section} from '../widgets/section';
import {SqlRef} from '../widgets/sql_ref';
import {dictToTree, Tree, TreeNode} from '../widgets/tree';
import {BottomTab, NewBottomTabArgs} from './bottom_tab';

export {
  ColumnConfig,
  Columns,
  GenericSliceDetailsTabConfig,
  GenericSliceDetailsTabConfigBase,
} from '../public/details_panel';

// A details tab, which fetches slice-like object from a given SQL table by id
// and renders it according to the provided config, specifying which columns
// need to be rendered and how.
export class GenericSliceDetailsTab extends BottomTab<GenericSliceDetailsTabConfig> {
  static readonly kind = 'dev.perfetto.GenericSliceDetailsTab';

  data: {[key: string]: ColumnType} | undefined;

  static create(
    args: NewBottomTabArgs<GenericSliceDetailsTabConfig>,
  ): GenericSliceDetailsTab {
    return new GenericSliceDetailsTab(args);
  }

  constructor(args: NewBottomTabArgs<GenericSliceDetailsTabConfig>) {
    super(args);

    this.engine
      .query(
        `select * from ${this.config.sqlTableName} where id = ${this.config.id}`,
      )
      .then((queryResult) => {
        this.data = queryResult.firstRow({});
        raf.scheduleFullRedraw();
      });
  }

  viewTab() {
    if (this.data === undefined) {
      return m('h2', 'Loading');
    }

    const args: {[key: string]: m.Child} = {};
    if (this.config.columns !== undefined) {
      for (const key of Object.keys(this.config.columns)) {
        let argKey = key;
        if (this.config.columns[key].displayName !== undefined) {
          argKey = this.config.columns[key].displayName!;
        }
        args[argKey] = sqlValueToReadableString(this.data[key]);
      }
    } else {
      for (const key of Object.keys(this.data)) {
        args[key] = sqlValueToReadableString(this.data[key]);
      }
    }

    const details = dictToTree(args);

    return m(
      DetailsShell,
      {
        title: this.config.title,
      },
      m(
        GridLayout,
        m(Section, {title: 'Details'}, m(Tree, details)),
        m(
          Section,
          {title: 'Metadata'},
          m(Tree, [
            m(TreeNode, {
              left: 'SQL ID',
              right: m(SqlRef, {
                table: this.config.sqlTableName,
                id: this.config.id,
              }),
            }),
          ]),
        ),
      ),
    );
  }

  getTitle(): string {
    return this.config.title;
  }

  isLoading() {
    return this.data === undefined;
  }
}
