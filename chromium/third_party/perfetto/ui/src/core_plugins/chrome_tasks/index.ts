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

import {uuidv4} from '../../base/uuid';
import {GenericSliceDetailsTabConfig} from '../../frontend/generic_slice_details_tab';
import {addSqlTableTab} from '../../frontend/sql_table_tab_command';
import {asUtid} from '../../trace_processor/sql_utils/core_types';
import {BottomTabToSCSAdapter} from '../../public/utils';
import {NUM, NUM_NULL, STR_NULL} from '../../trace_processor/query_result';
import {Trace} from '../../public/trace';
import {PerfettoPlugin, PluginDescriptor} from '../../public/plugin';
import {ChromeTasksDetailsTab} from './details';
import {chromeTasksTable} from './table';
import {ChromeTasksThreadTrack} from './track';
import {GroupNode, TrackNode} from '../../public/workspace';

class ChromeTasksPlugin implements PerfettoPlugin {
  onActivate() {}

  async onTraceLoad(ctx: Trace) {
    await this.createTracks(ctx);

    ctx.commands.registerCommand({
      id: 'org.chromium.ChromeTasks.ShowChromeTasksTable',
      name: 'Show chrome_tasks table',
      callback: () =>
        addSqlTableTab({
          table: chromeTasksTable,
        }),
    });
  }

  async createTracks(ctx: Trace) {
    const it = (
      await ctx.engine.query(`
      INCLUDE PERFETTO MODULE chrome.tasks;

      with relevant_threads as (
        select distinct utid from chrome_tasks
      )
      select
        (CASE process.name
          WHEN 'Browser' THEN 1
          WHEN 'Gpu' THEN 2
          WHEN 'Renderer' THEN 4
          ELSE 3
        END) as processRank,
        process.name as processName,
        process.pid,
        process.upid,
        (CASE thread.name
          WHEN 'CrBrowserMain' THEN 1
          WHEN 'CrRendererMain' THEN 1
          WHEN 'CrGpuMain' THEN 1
          WHEN 'Chrome_IOThread' THEN 2
          WHEN 'Chrome_ChildIOThread' THEN 2
          WHEN 'VizCompositorThread' THEN 3
          WHEN 'NetworkService' THEN 3
          WHEN 'Compositor' THEN 3
          WHEN 'CompositorGpuThread' THEN 4
          WHEN 'CompositorTileWorker&' THEN 5
          WHEN 'ThreadPoolService' THEN 6
          WHEN 'ThreadPoolSingleThreadForegroundBlocking&' THEN 6
          WHEN 'ThreadPoolForegroundWorker' THEN 6
          ELSE 7
         END) as threadRank,
         thread.name as threadName,
         thread.tid,
         thread.utid
      from relevant_threads
      join thread using (utid)
      join process using (upid)
      order by processRank, upid, threadRank, utid
    `)
    ).iter({
      processRank: NUM,
      processName: STR_NULL,
      pid: NUM_NULL,
      upid: NUM,
      threadRank: NUM,
      threadName: STR_NULL,
      tid: NUM_NULL,
      utid: NUM,
    });

    const group = new GroupNode('Chrome Tasks');
    for (; it.valid(); it.next()) {
      const utid = it.utid;
      const uri = `org.chromium.ChromeTasks#thread.${utid}`;
      const title = `${it.threadName} ${it.tid}`;
      ctx.tracks.registerTrack({
        uri,
        track: new ChromeTasksThreadTrack(ctx.engine, uri, asUtid(utid)),
        title,
      });
      group.insertChildInOrder(new TrackNode(uri, title));
      ctx.workspace.insertChildInOrder(group);
    }

    ctx.registerDetailsPanel(
      new BottomTabToSCSAdapter({
        tabFactory: (selection) => {
          if (
            selection.kind === 'GENERIC_SLICE' &&
            selection.detailsPanelConfig.kind === ChromeTasksDetailsTab.kind
          ) {
            const config = selection.detailsPanelConfig.config;
            return new ChromeTasksDetailsTab({
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

export const plugin: PluginDescriptor = {
  pluginId: 'org.chromium.ChromeTasks',
  plugin: ChromeTasksPlugin,
};
