// Copyright (C) 2019 The Android Open Source Project
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

import {AsyncLimiter} from '../../base/async_limiter';
import {Monitor} from '../../base/monitor';
import {isString} from '../../base/object_utils';
import {
  AggregateData,
  Column,
  ColumnDef,
  ThreadStateExtra,
} from '../../common/aggregation_data';
import {Sorting} from '../../common/state';
import {Area} from '../../public/selection';
import {globals} from '../../frontend/globals';
import {publishAggregateData} from '../../frontend/publish';
import {Engine} from '../../trace_processor/engine';
import {NUM} from '../../trace_processor/query_result';
import {Controller} from '../controller';

export interface AggregationControllerArgs {
  engine: Engine;
  kind: string;
}

function isStringColumn(column: Column): boolean {
  return column.kind === 'STRING' || column.kind === 'STATE';
}

export abstract class AggregationController extends Controller<'main'> {
  readonly kind: string;
  private readonly monitor: Monitor;
  private readonly limiter = new AsyncLimiter();

  abstract createAggregateView(engine: Engine, area: Area): Promise<boolean>;

  abstract getExtra(
    engine: Engine,
    area: Area,
  ): Promise<ThreadStateExtra | void>;

  abstract getTabName(): string;
  abstract getDefaultSorting(): Sorting;
  abstract getColumnDefinitions(): ColumnDef[];

  constructor(private args: AggregationControllerArgs) {
    super('main');
    this.kind = this.args.kind;
    this.monitor = new Monitor([
      () => globals.selectionManager.selection,
      () => globals.state.aggregatePreferences[this.args.kind],
    ]);
  }

  run() {
    if (this.monitor.ifStateChanged()) {
      const selection = globals.selectionManager.selection;
      if (selection.kind !== 'area') {
        publishAggregateData({
          data: {
            tabName: this.getTabName(),
            columns: [],
            strings: [],
            columnSums: [],
          },
          kind: this.args.kind,
        });
        return;
      } else {
        this.limiter.schedule(async () => {
          const data = await this.getAggregateData(selection, true);
          publishAggregateData({data, kind: this.args.kind});
        });
      }
    }
  }

  async getAggregateData(
    area: Area,
    areaChanged: boolean,
  ): Promise<AggregateData> {
    if (areaChanged) {
      const viewExists = await this.createAggregateView(this.args.engine, area);
      if (!viewExists) {
        return {
          tabName: this.getTabName(),
          columns: [],
          strings: [],
          columnSums: [],
        };
      }
    }

    const defs = this.getColumnDefinitions();
    const colIds = defs.map((col) => col.columnId);
    const pref = globals.state.aggregatePreferences[this.kind];
    let sorting = `${this.getDefaultSorting().column} ${
      this.getDefaultSorting().direction
    }`;
    // eslint-disable-next-line @typescript-eslint/strict-boolean-expressions
    if (pref && pref.sorting) {
      sorting = `${pref.sorting.column} ${pref.sorting.direction}`;
    }
    const query = `select ${colIds} from ${this.kind} order by ${sorting}`;
    const result = await this.args.engine.query(query);

    const numRows = result.numRows();
    const columns = defs.map((def) => this.columnFromColumnDef(def, numRows));
    const columnSums = await Promise.all(defs.map((def) => this.getSum(def)));
    const extraData = await this.getExtra(this.args.engine, area);
    const extra = extraData ? extraData : undefined;
    const data: AggregateData = {
      tabName: this.getTabName(),
      columns,
      columnSums,
      strings: [],
      extra,
    };

    const stringIndexes = new Map<string, number>();
    function internString(str: string) {
      let idx = stringIndexes.get(str);
      if (idx !== undefined) return idx;
      idx = data.strings.length;
      data.strings.push(str);
      stringIndexes.set(str, idx);
      return idx;
    }

    const it = result.iter({});
    for (let i = 0; it.valid(); it.next(), ++i) {
      for (const column of data.columns) {
        const item = it.get(column.columnId);
        if (item === null) {
          column.data[i] = isStringColumn(column) ? internString('NULL') : 0;
        } else if (isString(item)) {
          column.data[i] = internString(item);
        } else if (item instanceof Uint8Array) {
          column.data[i] = internString('<Binary blob>');
        } else if (typeof item === 'bigint') {
          // TODO(stevegolton) It would be nice to keep bigints as bigints for
          // the purposes of aggregation, however the aggregation infrastructure
          // is likely to be significantly reworked when we introduce EventSet,
          // and the complexity of supporting bigints throughout the aggregation
          // panels in its current form is not worth it. Thus, we simply
          // convert bigints to numbers.
          column.data[i] = Number(item);
        } else {
          column.data[i] = item;
        }
      }
    }

    return data;
  }

  async getSum(def: ColumnDef): Promise<string> {
    if (!def.sum) return '';
    const result = await this.args.engine.query(
      `select ifnull(sum(${def.columnId}), 0) as s from ${this.kind}`,
    );
    let sum = result.firstRow({s: NUM}).s;
    if (def.kind === 'TIMESTAMP_NS') {
      sum = sum / 1e6;
    }
    return `${sum}`;
  }

  columnFromColumnDef(def: ColumnDef, numRows: number): Column {
    // TODO(hjd): The Column type should be based on the
    // ColumnDef type or vice versa to avoid this cast.
    return {
      title: def.title,
      kind: def.kind,
      data: new def.columnConstructor(numRows),
      columnId: def.columnId,
    } as Column;
  }
}
