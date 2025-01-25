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

import m from 'mithril';

import {AsyncDisposableStack} from '../base/disposable_stack';
import {Engine} from '../trace_processor/engine';
import {NUM, STR, STR_NULL} from '../trace_processor/query_result';
import {createPerfettoTable} from '../trace_processor/sql_utils';
import {
  Flamegraph,
  FlamegraphFilters,
  FlamegraphQueryData,
} from '../widgets/flamegraph';
import {AsyncLimiter} from '../base/async_limiter';
import {assertExists} from '../base/logging';
import {Monitor} from '../base/monitor';
import {featureFlags} from './feature_flags';
import {uuidv4Sql} from '../base/uuid';

export interface QueryFlamegraphColumn {
  // The name of the column in SQL.
  readonly name: string;

  // The human readable name describing the contents of the column.
  readonly displayName: string;
}

export interface QueryFlamegraphMetric {
  // The human readable name of the metric: will be shown to the user to change
  // between metrics.
  readonly name: string;

  // The human readable SI-style unit of `selfValue`. Values will be shown to
  // the user suffixed with this.
  readonly unit: string;

  // SQL statement which need to be run in preparation for being able to execute
  // `statement`.
  readonly dependencySql?: string;

  // A single SQL statement which returns the columns `id`, `parentId`, `name`
  // `selfValue`, all columns specified by `unaggregatableProperties` and
  // `aggregatableProperties`.
  readonly statement: string;

  // Additional contextual columns containing data which should not be merged
  // between sibling nodes, even if they have the same name.
  //
  // Examples include the mapping that a name comes from, the heap graph root
  // type etc.
  //
  // Note: the name is always unaggregatable and should not be specified here.
  readonly unaggregatableProperties?: ReadonlyArray<QueryFlamegraphColumn>;

  // Additional contextual columns containing data which will be displayed to
  // the user if there is no merging. If there is merging, currently the value
  // will not be shown.
  //
  // Examples include the source file and line number.
  //
  // TODO(lalitm): reconsider the decision to show nothing, instead maybe show
  // the top 5 options etc.
  readonly aggregatableProperties?: ReadonlyArray<QueryFlamegraphColumn>;
}

// Given a table and columns on those table (corresponding to metrics),
// returns an array of `QueryFlamegraphMetric` structs which can be passed
// in QueryFlamegraph's attrs.
//
// `tableOrSubquery` should have the columns `id`, `parentId`, `name` and all
// columns specified by `tableMetrics[].name`, `unaggregatableProperties` and
// `aggregatableProperties`.
export function metricsFromTableOrSubquery(
  tableOrSubquery: string,
  tableMetrics: ReadonlyArray<{name: string; unit: string; columnName: string}>,
  dependencySql?: string,
  unaggregatableProperties?: ReadonlyArray<QueryFlamegraphColumn>,
  aggregatableProperties?: ReadonlyArray<QueryFlamegraphColumn>,
): QueryFlamegraphMetric[] {
  const metrics = [];
  for (const {name, unit, columnName} of tableMetrics) {
    metrics.push({
      name,
      unit,
      dependencySql,
      statement: `
        select *, ${columnName} as value
        from ${tableOrSubquery}
      `,
      unaggregatableProperties,
      aggregatableProperties,
    });
  }
  return metrics;
}

export interface QueryFlamegraphAttrs {
  readonly engine: Engine;
  readonly metrics: ReadonlyArray<QueryFlamegraphMetric>;
}

// A Mithril component which wraps the `Flamegraph` widget and fetches the data
// for the widget by querying an `Engine`.
export class QueryFlamegraph implements m.ClassComponent<QueryFlamegraphAttrs> {
  private selectedMetricName;
  private data?: FlamegraphQueryData;
  private filters: FlamegraphFilters = {
    showStack: [],
    hideStack: [],
    showFromFrame: [],
    hideFrame: [],
    pivot: undefined,
  };
  private attrs: QueryFlamegraphAttrs;
  private selMonitor = new Monitor([() => this.attrs.metrics]);
  private queryLimiter = new AsyncLimiter();

  constructor({attrs}: m.Vnode<QueryFlamegraphAttrs>) {
    this.attrs = attrs;
    this.selectedMetricName = attrs.metrics[0].name;
  }

  view({attrs}: m.Vnode<QueryFlamegraphAttrs>) {
    this.attrs = attrs;
    if (this.selMonitor.ifStateChanged()) {
      this.selectedMetricName = attrs.metrics[0].name;
      this.data = undefined;
      this.fetchData(attrs);
    }
    return m(Flamegraph, {
      metrics: attrs.metrics,
      selectedMetricName: this.selectedMetricName,
      data: this.data,
      onMetricChange: (name) => {
        this.selectedMetricName = name;
        this.data = undefined;
        this.fetchData(attrs);
      },
      onFiltersChanged: (filters) => {
        this.filters = filters;
        this.data = undefined;
        this.fetchData(attrs);
      },
    });
  }

  private async fetchData(attrs: QueryFlamegraphAttrs) {
    const metric = assertExists(
      attrs.metrics.find((metric) => metric.name === this.selectedMetricName),
    );
    const engine = attrs.engine;
    const filters = this.filters;
    this.queryLimiter.schedule(async () => {
      this.data = await computeFlamegraphTree(engine, metric, filters);
    });
  }
}

async function computeFlamegraphTree(
  engine: Engine,
  {
    dependencySql,
    statement,
    unaggregatableProperties,
    aggregatableProperties,
  }: QueryFlamegraphMetric,
  {showStack, hideStack, showFromFrame, hideFrame, pivot}: FlamegraphFilters,
): Promise<FlamegraphQueryData> {
  // Pivot also essentially acts as a "show stack" filter so treat it like one.
  const showStackAndPivot = [...showStack];
  if (pivot !== undefined) {
    showStackAndPivot.push(pivot);
  }

  const showStackFilter =
    showStackAndPivot.length === 0
      ? '0'
      : showStackAndPivot
          .map((x, i) => `((name like '%${x}%') << ${i})`)
          .join(' | ');
  const showStackBits = (1 << showStackAndPivot.length) - 1;

  const hideStackFilter =
    hideStack.length === 0
      ? 'false'
      : hideStack.map((x) => `name like '%${x}%'`).join(' OR ');

  const showFromFrameFilter =
    showFromFrame.length === 0
      ? '0'
      : showFromFrame
          .map((x, i) => `((name like '%${x}%') << ${i})`)
          .join(' | ');
  const showFromFrameBits = (1 << showFromFrame.length) - 1;

  const hideFrameFilter =
    hideFrame.length === 0
      ? 'false'
      : hideFrame.map((x) => `name like '%${x}%'`).join(' OR ');

  const pivotFilter = pivot === undefined ? '0' : `name like '%${pivot}%'`;

  const unagg = unaggregatableProperties ?? [];
  const unaggCols = unagg.map((x) => x.name);

  const agg = aggregatableProperties ?? [];
  const aggCols = agg.map((x) => x.name);

  const groupingColumns = `(${(unaggCols.length === 0 ? ['groupingColumn'] : unaggCols).join()})`;
  const groupedColumns = `(${(aggCols.length === 0 ? ['groupedColumn'] : aggCols).join()})`;

  if (dependencySql !== undefined) {
    await engine.query(dependencySql);
  }
  await engine.query(`include perfetto module viz.flamegraph;`);

  const uuid = uuidv4Sql();
  await using disposable = new AsyncDisposableStack();
  // TODO(lalitm): this doesn't need to be called unless we have
  // a non-empty set of filters.
  disposable.use(
    await createPerfettoTable(
      engine,
      `_flamegraph_source_${uuid}`,
      `
        select *
        from _viz_flamegraph_prepare_filter!(
          (
            select
              id,
              parentId,
              name,
              value,
              ${(unaggCols.length === 0 ? [`'' as groupingColumn`] : unaggCols).join()},
              ${(aggCols.length === 0 ? [`'' as groupedColumn`] : aggCols).join()}
            FROM (${statement})
          ),
          (${showStackFilter}),
          (${hideStackFilter}),
          (${showFromFrameFilter}),
          (${hideFrameFilter}),
          (${pivotFilter}),
          ${1 << showStackAndPivot.length},
          ${groupingColumns}
        )
      `,
    ),
  );
  // TODO(lalitm): this doesn't need to be called unless we have
  // a non-empty set of filters.
  disposable.use(
    await createPerfettoTable(
      engine,
      `_flamegraph_filtered_${uuid}`,
      `
        select *
        from _viz_flamegraph_filter_frames!(
          _flamegraph_source_${uuid},
          ${showFromFrameBits}
        )
      `,
    ),
  );
  disposable.use(
    await createPerfettoTable(
      engine,
      `_flamegraph_accumulated_${uuid}`,
      `
        select *
        from _viz_flamegraph_accumulate!(
          _flamegraph_filtered_${uuid},
          ${showStackBits}
        )
      `,
    ),
  );
  disposable.use(
    await createPerfettoTable(
      engine,
      `_flamegraph_hash_${uuid}`,
      `
        select *
        from _viz_flamegraph_downwards_hash!(
          _flamegraph_source_${uuid},
          _flamegraph_filtered_${uuid},
          _flamegraph_accumulated_${uuid},
          ${groupingColumns},
          ${groupedColumns}
        )
        union all
        select *
        from _viz_flamegraph_upwards_hash!(
          _flamegraph_source_${uuid},
          _flamegraph_filtered_${uuid},
          _flamegraph_accumulated_${uuid},
          ${groupingColumns},
          ${groupedColumns}
        )
        order by hash
      `,
    ),
  );
  disposable.use(
    await createPerfettoTable(
      engine,
      `_flamegraph_merged_${uuid}`,
      `
        select *
        from _viz_flamegraph_merge_hashes!(
          _flamegraph_hash_${uuid},
          ${groupingColumns},
          ${groupedColumns}
        )
      `,
    ),
  );
  disposable.use(
    await createPerfettoTable(
      engine,
      `_flamegraph_layout_${uuid}`,
      `
        select *
        from _viz_flamegraph_local_layout!(
          _flamegraph_merged_${uuid}
        );
      `,
    ),
  );
  const res = await engine.query(`
    select *
    from _viz_flamegraph_global_layout!(
      _flamegraph_merged_${uuid},
      _flamegraph_layout_${uuid},
      ${groupingColumns},
      ${groupedColumns}
    )
  `);

  const it = res.iter({
    id: NUM,
    parentId: NUM,
    depth: NUM,
    name: STR,
    selfValue: NUM,
    cumulativeValue: NUM,
    xStart: NUM,
    xEnd: NUM,
    ...Object.fromEntries(unaggCols.map((m) => [m, STR_NULL])),
    ...Object.fromEntries(aggCols.map((m) => [m, STR_NULL])),
  });
  let allRootsCumulativeValue = 0;
  let minDepth = 0;
  let maxDepth = 0;
  const nodes = [];
  for (; it.valid(); it.next()) {
    const properties = new Map<string, string>();
    for (const a of [...agg, ...unagg]) {
      const r = it.get(a.name);
      if (r !== null) {
        properties.set(a.displayName, r as string);
      }
    }
    nodes.push({
      id: it.id,
      parentId: it.parentId,
      depth: it.depth,
      name: it.name,
      selfValue: it.selfValue,
      cumulativeValue: it.cumulativeValue,
      xStart: it.xStart,
      xEnd: it.xEnd,
      properties,
    });
    if (it.depth === 1) {
      allRootsCumulativeValue += it.cumulativeValue;
    }
    minDepth = Math.min(minDepth, it.depth);
    maxDepth = Math.max(maxDepth, it.depth);
  }
  return {nodes, allRootsCumulativeValue, minDepth, maxDepth};
}

export const USE_NEW_FLAMEGRAPH_IMPL = featureFlags.register({
  id: 'useNewFlamegraphImpl',
  name: 'Use new flamegraph implementation',
  description: 'Use new flamgraph implementation in details panels.',
  defaultValue: true,
});
