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
import {
  ThreadStateSqlId,
  Utid,
} from '../../trace_processor/sql_utils/core_types';
import {duration, time} from '../../base/time';
import {Anchor} from '../../widgets/anchor';
import {Icons} from '../../base/semantic_icons';
import {globals} from '../globals';
import {THREAD_STATE_TRACK_KIND} from '../../public/track_kinds';
import {ThreadState} from '../../trace_processor/sql_utils/thread_state';
import {scrollTo} from '../../public/scroll_helper';

interface ThreadStateRefAttrs {
  id: ThreadStateSqlId;
  ts: time;
  dur: duration;
  utid: Utid;
  // If not present, a placeholder name will be used.
  name?: string;

  // Whether clicking on the reference should change the current tab
  // to "current selection" tab in addition to updating the selection
  // and changing the viewport. True by default.
  readonly switchToCurrentSelectionTab?: boolean;
}

export class ThreadStateRef implements m.ClassComponent<ThreadStateRefAttrs> {
  view(vnode: m.Vnode<ThreadStateRefAttrs>) {
    return m(
      Anchor,
      {
        icon: Icons.UpdateSelection,
        onclick: () => {
          const trackDescriptor = globals.trackManager
            .getAllTracks()
            .find(
              (td) =>
                td.tags?.kind === THREAD_STATE_TRACK_KIND &&
                td.tags?.utid === vnode.attrs.utid,
            );

          if (trackDescriptor === undefined) return;

          globals.selectionManager.setLegacy(
            {
              kind: 'THREAD_STATE',
              id: vnode.attrs.id,
              trackUri: trackDescriptor.uri,
            },
            {
              switchToCurrentSelectionTab:
                vnode.attrs.switchToCurrentSelectionTab,
            },
          );
          scrollTo({
            track: {uri: trackDescriptor.uri, expandGroup: true},
            time: {start: vnode.attrs.ts},
          });
        },
      },
      vnode.attrs.name ?? `Thread State ${vnode.attrs.id}`,
    );
  }
}

export function threadStateRef(state: ThreadState): m.Child {
  if (state.thread === undefined) return null;

  return m(ThreadStateRef, {
    id: state.threadStateSqlId,
    ts: state.ts,
    dur: state.dur,
    utid: state.thread?.utid,
  });
}
