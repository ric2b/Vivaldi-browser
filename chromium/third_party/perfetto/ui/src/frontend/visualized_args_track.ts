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
import {Button} from '../widgets/button';
import {Icons} from '../base/semantic_icons';
import {ThreadSliceTrack} from './thread_slice_track';
import {uuidv4Sql} from '../base/uuid';
import {Engine} from '../trace_processor/engine';
import {createView} from '../trace_processor/sql_utils';
import {globals} from './globals';

export interface VisualizedArgsTrackAttrs {
  readonly uri: string;
  readonly engine: Engine;
  readonly trackId: number;
  readonly maxDepth: number;
  readonly argName: string;
}

export class VisualisedArgsTrack extends ThreadSliceTrack {
  private readonly viewName: string;
  private readonly argName: string;

  constructor({
    uri,
    engine,
    trackId,
    maxDepth,
    argName,
  }: VisualizedArgsTrackAttrs) {
    const uuid = uuidv4Sql();
    const escapedArgName = argName.replace(/[^a-zA-Z]/g, '_');
    const viewName = `__arg_visualisation_helper_${escapedArgName}_${uuid}_slice`;

    super({engine, uri}, trackId, maxDepth, viewName);
    this.viewName = viewName;
    this.argName = argName;
  }

  async onInit() {
    return await createView(
      this.engine,
      this.viewName,
      `
        with slice_with_arg as (
          select
            slice.id,
            slice.track_id,
            slice.ts,
            slice.dur,
            slice.thread_dur,
            NULL as cat,
            args.display_value as name
          from slice
          join args using (arg_set_id)
          where args.key='${this.argName}'
        )
        select
          *,
          (select count()
          from ancestor_slice(s1.id) s2
          join slice_with_arg s3 on s2.id=s3.id
          ) as depth
        from slice_with_arg s1
        order by id
      `,
    );
  }

  getTrackShellButtons(): m.Children {
    return m(Button, {
      onclick: () => {
        globals.workspace.getTrackByUri(this.uri)?.remove();
      },
      icon: Icons.Close,
      title: 'Close',
      compact: true,
    });
  }
}
