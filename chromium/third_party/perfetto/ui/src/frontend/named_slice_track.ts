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

import {getColorForSlice} from '../core/colorizer';
import {Slice} from '../public/track';
import {STR_NULL} from '../trace_processor/query_result';
import {
  BASE_ROW,
  BaseSliceTrack,
  OnSliceClickArgs,
  OnSliceOverArgs,
  SLICE_FLAGS_INCOMPLETE,
  SLICE_FLAGS_INSTANT,
} from './base_slice_track';
import {globals} from './globals';
import {NewTrackArgs} from './track';
import {renderDuration} from './widgets/duration';

export const NAMED_ROW = {
  // Base columns (tsq, ts, dur, id, depth).
  ...BASE_ROW,

  // Impl-specific columns.
  name: STR_NULL,
};
export type NamedRow = typeof NAMED_ROW;

export abstract class NamedSliceTrack<
  SliceType extends Slice = Slice,
  RowType extends NamedRow = NamedRow,
> extends BaseSliceTrack<SliceType, RowType> {
  constructor(args: NewTrackArgs) {
    super(args);
  }

  // Converts a SQL result row to an "Impl" Slice.
  protected rowToSliceBase(row: RowType): Slice {
    const baseSlice = super.rowToSliceBase(row);
    // Ignore PIDs or numeric arguments when hashing.
    const name = row.name ?? '';
    const colorScheme = getColorForSlice(name);
    return {...baseSlice, title: name, colorScheme};
  }

  onSliceOver(args: OnSliceOverArgs<SliceType>) {
    const {title, dur, flags} = args.slice;
    let duration;
    if (flags & SLICE_FLAGS_INCOMPLETE) {
      duration = 'Incomplete';
    } else if (flags & SLICE_FLAGS_INSTANT) {
      duration = 'Instant';
    } else {
      duration = renderDuration(dur);
    }
    args.tooltip = [`${title} - [${duration}]`];
  }

  onSliceClick(args: OnSliceClickArgs<SliceType>) {
    globals.selectionManager.setLegacy({
      kind: 'SLICE',
      id: args.slice.id,
      trackUri: this.uri,
      table: 'slice',
    });
  }
}
