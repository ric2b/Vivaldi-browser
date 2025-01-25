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

import {uuidv4} from '../../base/uuid';
import {GenericSliceDetailsTabConfig} from '../../frontend/generic_slice_details_tab';
import {TrackNode} from '../../public/workspace';
import {BottomTabToSCSAdapter} from '../../public/utils';
import {NUM} from '../../trace_processor/query_result';
import {Trace} from '../../public/trace';
import {PerfettoPlugin, PluginDescriptor} from '../../public/plugin';
import {ScreenshotTab} from './screenshot_panel';
import {ScreenshotsTrack} from './screenshots_track';

class ScreenshotsPlugin implements PerfettoPlugin {
  async onTraceLoad(ctx: Trace): Promise<void> {
    const res = await ctx.engine.query(`
      INCLUDE PERFETTO MODULE android.screenshots;
      select
        count() as count
      from android_screenshots
    `);
    const {count} = res.firstRow({count: NUM});

    if (count > 0) {
      const displayName = 'Screenshots';
      const uri = '/screenshots';
      ctx.tracks.registerTrack({
        uri,
        title: displayName,
        track: new ScreenshotsTrack({
          engine: ctx.engine,
          uri,
        }),
        tags: {
          kind: ScreenshotsTrack.kind,
        },
      });
      const trackNode = new TrackNode(uri, displayName);
      trackNode.sortOrder = -60;
      ctx.workspace.insertChildInOrder(trackNode);

      ctx.registerDetailsPanel(
        new BottomTabToSCSAdapter({
          tabFactory: (selection) => {
            if (
              selection.kind === 'GENERIC_SLICE' &&
              selection.detailsPanelConfig.kind === ScreenshotTab.kind
            ) {
              const config = selection.detailsPanelConfig.config;
              return new ScreenshotTab({
                config: config as GenericSliceDetailsTabConfig,
                engine: ctx.engine,
                uuid: uuidv4(),
              });
            }
            return undefined;
          },
        }),
      );
    }
  }
}

export const plugin: PluginDescriptor = {
  pluginId: 'perfetto.Screenshots',
  plugin: ScreenshotsPlugin,
};
