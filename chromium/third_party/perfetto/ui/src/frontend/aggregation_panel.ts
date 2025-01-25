// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
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
import {Actions} from '../common/actions';
import {
  AggregateData,
  Column,
  ThreadStateExtra,
  isEmptyData,
} from '../common/aggregation_data';
import {colorForState} from '../core/colorizer';
import {globals} from './globals';
import {DurationWidget} from './widgets/duration';
import {EmptyState} from '../widgets/empty_state';
import {Anchor} from '../widgets/anchor';
import {Icons} from '../base/semantic_icons';
import {translateState} from '../trace_processor/sql_utils/thread_state';

export interface AggregationPanelAttrs {
  data?: AggregateData;
  kind: string;
}

export class AggregationPanel
  implements m.ClassComponent<AggregationPanelAttrs>
{
  view({attrs}: m.CVnode<AggregationPanelAttrs>) {
    if (!attrs.data || isEmptyData(attrs.data)) {
      return m(
        EmptyState,
        {
          className: 'pf-noselection',
          title: 'No relevant tracks in selection',
        },
        m(
          Anchor,
          {
            icon: Icons.ChangeTab,
            onclick: () => {
              globals.tabManager.showCurrentSelectionTab();
            },
          },
          'Go to current selection tab',
        ),
      );
    }

    return m(
      '.details-panel',
      m(
        '.details-panel-heading.aggregation',
        attrs.data.extra !== undefined &&
          attrs.data.extra.kind === 'THREAD_STATE'
          ? this.showStateSummary(attrs.data.extra)
          : null,
        this.showTimeRange(),
        m(
          'table',
          m(
            'tr',
            attrs.data.columns.map((col) =>
              this.formatColumnHeading(col, attrs.kind),
            ),
          ),
          m(
            'tr.sum',
            attrs.data.columnSums.map((sum) => {
              const sumClass = sum === '' ? 'td' : 'td.sum-data';
              return m(sumClass, sum);
            }),
          ),
        ),
      ),
      m('.details-table.aggregation', m('table', this.getRows(attrs.data))),
    );
  }

  formatColumnHeading(col: Column, id: string) {
    const pref = globals.state.aggregatePreferences[id];
    let sortIcon = '';
    // eslint-disable-next-line @typescript-eslint/strict-boolean-expressions
    if (pref && pref.sorting && pref.sorting.column === col.columnId) {
      sortIcon =
        pref.sorting.direction === 'DESC' ? 'arrow_drop_down' : 'arrow_drop_up';
    }
    return m(
      'th',
      {
        onclick: () => {
          globals.dispatch(
            Actions.updateAggregateSorting({id, column: col.columnId}),
          );
        },
      },
      col.title,
      m('i.material-icons', sortIcon),
    );
  }

  getRows(data: AggregateData) {
    if (data.columns.length === 0) return;
    const rows = [];
    for (let i = 0; i < data.columns[0].data.length; i++) {
      const row = [];
      for (let j = 0; j < data.columns.length; j++) {
        row.push(m('td', this.getFormattedData(data, i, j)));
      }
      rows.push(m('tr', row));
    }
    return rows;
  }

  getFormattedData(data: AggregateData, rowIndex: number, columnIndex: number) {
    switch (data.columns[columnIndex].kind) {
      case 'STRING':
        return data.strings[data.columns[columnIndex].data[rowIndex]];
      case 'TIMESTAMP_NS':
        return `${data.columns[columnIndex].data[rowIndex] / 1000000}`;
      case 'STATE': {
        const concatState =
          data.strings[data.columns[columnIndex].data[rowIndex]];
        const split = concatState.split(',');
        const ioWait =
          split[1] === 'NULL' ? undefined : !!Number.parseInt(split[1], 10);
        return translateState(split[0], ioWait);
      }
      case 'NUMBER':
      default:
        return data.columns[columnIndex].data[rowIndex];
    }
  }

  showTimeRange() {
    const selection = globals.selectionManager.selection;
    if (selection.kind !== 'area') return undefined;
    const duration = selection.end - selection.start;
    return m(
      '.time-range',
      'Selected range: ',
      m(DurationWidget, {dur: duration}),
    );
  }

  // Thread state aggregation panel only
  showStateSummary(data: ThreadStateExtra) {
    if (data === undefined) return undefined;
    const states = [];
    for (let i = 0; i < data.states.length; i++) {
      const colorScheme = colorForState(data.states[i]);
      const width = (data.values[i] / data.totalMs) * 100;
      states.push(
        m(
          '.state',
          {
            style: {
              background: colorScheme.base.cssString,
              color: colorScheme.textBase.cssString,
              width: `${width}%`,
            },
          },
          `${data.states[i]}: ${data.values[i]} ms`,
        ),
      );
    }
    return m('.states', states);
  }
}
