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
import {Duration, duration, time} from '../../base/time';
import {raf} from '../../core/raf_scheduler';
import {BottomTab, NewBottomTabArgs} from '../../frontend/bottom_tab';
import {GenericSliceDetailsTabConfig} from '../../frontend/generic_slice_details_tab';
import {hasArgs, renderArguments} from '../../frontend/slice_args';
import {renderDetails} from '../../frontend/slice_details';
import {
  getDescendantSliceTree,
  getSlice,
  getSliceFromConstraints,
  SliceDetails,
  SliceTreeNode,
} from '../../trace_processor/sql_utils/slice';
import {
  asSliceSqlId,
  SliceSqlId,
} from '../../trace_processor/sql_utils/core_types';
import {
  ColumnDescriptor,
  Table,
  TableData,
  widgetColumn,
} from '../../frontend/tables/table';
import {TreeTable, TreeTableAttrs} from '../../frontend/widgets/treetable';
import {NUM, STR} from '../../trace_processor/query_result';
import {DetailsShell} from '../../widgets/details_shell';
import {GridLayout, GridLayoutColumn} from '../../widgets/grid_layout';
import {Section} from '../../widgets/section';
import {MultiParagraphText, TextParagraph} from '../../widgets/text_paragraph';
import {Tree, TreeNode} from '../../widgets/tree';
import {
  EventLatencyCauseThreadTracks,
  EventLatencyStage,
  getCauseLink,
  getEventLatencyCauseTracks,
  getScrollJankCauseStage,
} from './scroll_jank_cause_link_utils';
import {ScrollJankCauseMap} from './scroll_jank_cause_map';
import {
  getScrollJankSlices,
  getSliceForTrack,
  ScrollJankSlice,
} from './scroll_jank_slice';
import {sliceRef} from '../../frontend/widgets/slice';
import {SCROLL_JANK_V3_TRACK_KIND} from '../../public/track_kinds';
import {globals} from '../../frontend/globals';

// Given a node in the slice tree, return a path from root to it.
function getPath(slice: SliceTreeNode): string[] {
  const result: string[] = [];
  let node: SliceTreeNode | undefined = slice;
  while (node.parent !== undefined) {
    result.push(node.name);
    node = node.parent;
  }
  return result.reverse();
}

// Given a slice tree node and a path, find the node following the path from
// the given slice, or `undefined` if not found.
function findSliceInTreeByPath(
  slice: SliceTreeNode | undefined,
  path: string[],
): SliceTreeNode | undefined {
  if (slice === undefined) {
    return undefined;
  }
  let result = slice;
  for (const segment of path) {
    let found = false;
    for (const child of result.children) {
      if (child.name === segment) {
        found = true;
        result = child;
        break;
      }
    }
    if (!found) {
      return undefined;
    }
  }
  return result;
}

function durationDelta(value: duration, base?: duration): string {
  if (base === undefined) {
    return 'NULL';
  }
  const delta = value - base;
  return `${delta > 0 ? '+' : ''}${Duration.humanise(delta)}`;
}

export class EventLatencySliceDetailsPanel extends BottomTab<GenericSliceDetailsTabConfig> {
  static readonly kind = 'dev.perfetto.EventLatencySliceDetailsPanel';

  private loaded = false;
  private name = '';
  private topEventLatencyId: SliceSqlId | undefined = undefined;

  private sliceDetails?: SliceDetails;
  private jankySlice?: ScrollJankSlice;

  // Whether this stage has caused jank. This is also true for top level
  // EventLatency slices where a descendant is a cause of jank.
  private isJankStage = false;

  // For top level EventLatency slices - if any descendant is a cause of jank,
  // this field stores information about that descendant slice. Otherwise, this
  // is stores information about the current stage;
  private relevantThreadStage: EventLatencyStage | undefined;
  private relevantThreadTracks: EventLatencyCauseThreadTracks[] = [];
  // Stages tree for the current EventLatency.
  private eventLatencyBreakdown?: SliceTreeNode;
  // Stages tree for the next EventLatency.
  private nextEventLatencyBreakdown?: SliceTreeNode;
  // Stages tree for the prev EventLatency.
  private prevEventLatencyBreakdown?: SliceTreeNode;

  private tracksByTrackId: Map<number, string>;

  static create(
    args: NewBottomTabArgs<GenericSliceDetailsTabConfig>,
  ): EventLatencySliceDetailsPanel {
    return new EventLatencySliceDetailsPanel(args);
  }

  constructor(args: NewBottomTabArgs<GenericSliceDetailsTabConfig>) {
    super(args);

    this.tracksByTrackId = new Map<number, string>();
    globals.trackManager.getAllTracks().forEach((td) => {
      td.tags?.trackIds?.forEach((trackId) => {
        this.tracksByTrackId.set(trackId, td.uri);
      });
    });

    this.loadData();
  }

  async loadData() {
    const queryResult = await this.engine.query(`
      SELECT
        name
      FROM ${this.config.sqlTableName}
      WHERE id = ${this.config.id}
      `);

    const iter = queryResult.firstRow({
      name: STR,
    });

    this.name = iter.name;

    await this.loadSlice();
    await this.loadJankSlice();
    await this.loadRelevantThreads();
    await this.loadEventLatencyBreakdown();

    this.loaded = true;
  }

  async loadSlice() {
    this.sliceDetails = await getSlice(
      this.engine,
      asSliceSqlId(this.config.id),
    );
    raf.scheduleRedraw();
  }

  async loadJankSlice() {
    if (!this.sliceDetails) return;
    // Get the id for the top-level EventLatency slice (this or parent), as
    // this id is used in the ScrollJankV3 track to identify the corresponding
    // janky interval.
    if (this.sliceDetails.name === 'EventLatency') {
      this.topEventLatencyId = this.sliceDetails.id;
    } else {
      this.topEventLatencyId = asSliceSqlId(
        await this.getOldestAncestorSliceId(),
      );
    }

    const possibleSlices = await getScrollJankSlices(
      this.engine,
      this.topEventLatencyId,
    );
    // We may not get any slices if the EventLatency doesn't indicate any
    // jank occurred.
    if (possibleSlices.length > 0) {
      this.jankySlice = possibleSlices[0];
    }
  }

  async loadRelevantThreads() {
    if (!this.sliceDetails) return;
    if (!this.topEventLatencyId) return;

    // Relevant threads should only be available on a "Janky" EventLatency
    // slice to allow the user to jump to the possible cause of jank.
    if (this.sliceDetails.name === 'EventLatency' && !this.jankySlice) return;

    const possibleScrollJankStage = await getScrollJankCauseStage(
      this.engine,
      this.topEventLatencyId,
    );
    if (this.sliceDetails.name === 'EventLatency') {
      this.isJankStage = true;
      this.relevantThreadStage = possibleScrollJankStage;
    } else {
      if (
        possibleScrollJankStage &&
        this.sliceDetails.name === possibleScrollJankStage.name
      ) {
        this.isJankStage = true;
      }
      this.relevantThreadStage = {
        name: this.sliceDetails.name,
        eventLatencyId: this.topEventLatencyId,
        ts: this.sliceDetails.ts,
        dur: this.sliceDetails.dur,
      };
    }

    if (this.relevantThreadStage) {
      this.relevantThreadTracks = await getEventLatencyCauseTracks(
        this.engine,
        this.relevantThreadStage,
      );
    }
  }

  async loadEventLatencyBreakdown() {
    if (this.topEventLatencyId === undefined) {
      return;
    }
    this.eventLatencyBreakdown = await getDescendantSliceTree(
      this.engine,
      this.topEventLatencyId,
    );

    // TODO(altimin): this should be based on an stdlib table and consider only
    // EventLatencies within the same scroll.
    // This is a copy of the statement in event_latency_track. It should move to
    // stdlib instead of living in the UI code.
    const whereClause = `
    EXTRACT_ARG(arg_set_id, 'event_latency.event_type') IN (
      'FIRST_GESTURE_SCROLL_UPDATE',
      'GESTURE_SCROLL_UPDATE',
      'INERTIAL_GESTURE_SCROLL_UPDATE')
    AND HAS_DESCENDANT_SLICE_WITH_NAME(
      id,
      'SubmitCompositorFrameToPresentationCompositorFrame')`;
    const prevEventLatency = await getSliceFromConstraints(this.engine, {
      filters: [
        `name = 'EventLatency'`,
        `id < ${this.topEventLatencyId}`,
        whereClause,
      ],
      orderBy: [{fieldName: 'id', direction: 'DESC'}],
      limit: 1,
    });
    if (prevEventLatency.length > 0) {
      this.prevEventLatencyBreakdown = await getDescendantSliceTree(
        this.engine,
        prevEventLatency[0].id,
      );
    }

    const nextEventLatency = await getSliceFromConstraints(this.engine, {
      filters: [
        `name = 'EventLatency'`,
        `id > ${this.topEventLatencyId}`,
        whereClause,
      ],
      orderBy: ['id'],
      limit: 1,
    });
    if (nextEventLatency.length > 0) {
      this.nextEventLatencyBreakdown = await getDescendantSliceTree(
        this.engine,
        nextEventLatency[0].id,
      );
    }
  }

  private getRelevantLinks(): m.Child {
    if (!this.sliceDetails) return undefined;

    // Relevant threads should only be available on a "Janky" EventLatency
    // slice to allow the user to jump to the possible cause of jank.
    if (
      this.sliceDetails.name === 'EventLatency' &&
      !this.relevantThreadStage
    ) {
      return undefined;
    }

    const name = this.relevantThreadStage
      ? this.relevantThreadStage.name
      : this.sliceDetails.name;
    const ts = this.relevantThreadStage
      ? this.relevantThreadStage.ts
      : this.sliceDetails.ts;
    const dur = this.relevantThreadStage
      ? this.relevantThreadStage.dur
      : this.sliceDetails.dur;
    const stageDetails = ScrollJankCauseMap.getEventLatencyDetails(name);
    if (stageDetails === undefined) return undefined;

    const childWidgets: m.Child[] = [];
    childWidgets.push(m(TextParagraph, {text: stageDetails.description}));

    interface RelevantThreadRow {
      description: string;
      tracks: EventLatencyCauseThreadTracks;
      ts: time;
      dur: duration;
    }

    const columns: ColumnDescriptor<RelevantThreadRow>[] = [
      widgetColumn<RelevantThreadRow>('Relevant Thread', (x) =>
        getCauseLink(x.tracks, this.tracksByTrackId, x.ts, x.dur),
      ),
      widgetColumn<RelevantThreadRow>('Description', (x) => {
        if (x.description === '') {
          return x.description;
        } else {
          return m(TextParagraph, {text: x.description});
        }
      }),
    ];

    const trackLinks: RelevantThreadRow[] = [];

    for (let i = 0; i < this.relevantThreadTracks.length; i++) {
      const track = this.relevantThreadTracks[i];
      let description = '';
      if (i == 0 || track.thread != this.relevantThreadTracks[i - 1].thread) {
        description = track.causeDescription;
      }
      trackLinks.push({
        description: description,
        tracks: this.relevantThreadTracks[i],
        ts: ts,
        dur: dur,
      });
    }

    const tableData = new TableData(trackLinks);

    if (trackLinks.length > 0) {
      childWidgets.push(
        m(Table, {
          data: tableData,
          columns: columns,
        }),
      );
    }

    return m(
      Section,
      {title: this.isJankStage ? `Jank Cause: ${name}` : name},
      childWidgets,
    );
  }

  private async getOldestAncestorSliceId(): Promise<number> {
    let eventLatencyId = -1;
    if (!this.sliceDetails) return eventLatencyId;
    const queryResult = await this.engine.query(`
      SELECT
        id
      FROM ancestor_slice(${this.sliceDetails.id})
      WHERE name = 'EventLatency'
    `);

    const it = queryResult.iter({
      id: NUM,
    });

    for (; it.valid(); it.next()) {
      eventLatencyId = it.id;
      break;
    }

    return eventLatencyId;
  }

  private getLinksSection(): m.Child {
    return m(
      Section,
      {title: 'Quick links'},
      m(
        Tree,
        m(TreeNode, {
          left: this.sliceDetails
            ? sliceRef(
                this.sliceDetails,
                'EventLatency in context of other Input events',
              )
            : 'EventLatency in context of other Input events',
          right: this.sliceDetails ? '' : 'N/A',
        }),
        m(TreeNode, {
          left: this.jankySlice
            ? getSliceForTrack(
                this.jankySlice,
                SCROLL_JANK_V3_TRACK_KIND,
                'Jank Interval',
              )
            : 'Jank Interval',
          right: this.jankySlice ? '' : 'N/A',
        }),
      ),
    );
  }

  private getBreakdownSection(): m.Child {
    if (this.eventLatencyBreakdown === undefined) {
      return undefined;
    }

    const attrs: TreeTableAttrs<SliceTreeNode> = {
      rows: [this.eventLatencyBreakdown],
      getChildren: (slice) => slice.children,
      columns: [
        {name: 'Name', getData: (slice) => slice.name},
        {name: 'Duration', getData: (slice) => Duration.humanise(slice.dur)},
        {
          name: 'vs prev',
          getData: (slice) =>
            durationDelta(
              slice.dur,
              findSliceInTreeByPath(
                this.prevEventLatencyBreakdown,
                getPath(slice),
              )?.dur,
            ),
        },
        {
          name: 'vs next',
          getData: (slice) =>
            durationDelta(
              slice.dur,
              findSliceInTreeByPath(
                this.nextEventLatencyBreakdown,
                getPath(slice),
              )?.dur,
            ),
        },
      ],
    };

    return m(
      Section,
      {
        title: 'EventLatency Stage Breakdown',
      },
      m(TreeTable<SliceTreeNode>, attrs),
    );
  }

  private getDescriptionText(): m.Child {
    return m(
      MultiParagraphText,
      m(TextParagraph, {
        text: `EventLatency tracks the latency of handling a given input event
                 (Scrolls, Touches, Taps, etc). Ideally from when the input was
                 read by the hardware to when it was reflected on the screen.`,
      }),
      m(TextParagraph, {
        text: `Note however the concept of coalescing or terminating early. This
               occurs when we receive multiple events or handle them quickly by
               converting them into a different event. Such as a TOUCH_MOVE
               being converted into a GESTURE_SCROLL_UPDATE type, or a multiple
               GESTURE_SCROLL_UPDATE events being formed into a single frame at
               the end of the RendererCompositorQueuingDelay.`,
      }),
      m(TextParagraph, {
        text: `*Important:* On some platforms (MacOS) we do not get feedback on
               when something is presented on the screen so the timings are only
               accurate for what we know on a given platform.`,
      }),
    );
  }

  viewTab() {
    if (this.sliceDetails) {
      const slice = this.sliceDetails;

      const rightSideWidgets: m.Child[] = [];
      rightSideWidgets.push(
        m(
          Section,
          {title: 'Description'},
          m('.div', this.getDescriptionText()),
        ),
      );

      const stageWidget = this.getRelevantLinks();
      // eslint-disable-next-line @typescript-eslint/strict-boolean-expressions
      if (stageWidget) {
        rightSideWidgets.push(stageWidget);
      }
      rightSideWidgets.push(this.getLinksSection());
      rightSideWidgets.push(this.getBreakdownSection());

      return m(
        DetailsShell,
        {
          title: 'Slice',
          description: this.name,
        },
        m(
          GridLayout,
          m(
            GridLayoutColumn,
            renderDetails(slice),
            hasArgs(slice.args) &&
              m(
                Section,
                {title: 'Arguments'},
                m(Tree, renderArguments(this.engine, slice.args)),
              ),
          ),
          m(GridLayoutColumn, rightSideWidgets),
        ),
      );
    } else {
      return m(DetailsShell, {title: 'Slice', description: 'Loading...'});
    }
  }

  isLoading() {
    return !this.loaded;
  }

  getTitle(): string {
    return `Current Selection`;
  }
}
