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

import {addDebugSliceTrack} from '../../public/debug_tracks';
import {Trace} from '../../public/trace';
import {PerfettoPlugin, PluginDescriptor} from '../../public/plugin';

class AndroidPerf implements PerfettoPlugin {
  async addAppProcessStartsDebugTrack(
    ctx: Trace,
    reason: string,
    sliceName: string,
  ): Promise<void> {
    const sliceColumns = [
      'id',
      'ts',
      'dur',
      'reason',
      'process_name',
      'intent',
      'table_name',
    ];
    await addDebugSliceTrack(
      ctx,
      {
        sqlSource: `
                    SELECT
                      start_id AS id,
                      proc_start_ts AS ts,
                      total_dur AS dur,
                      reason,
                      process_name,
                      intent,
                      'slice' AS table_name
                    FROM android_app_process_starts
                    WHERE reason = '${reason}'
                 `,
        columns: sliceColumns,
      },
      'app_' + sliceName + '_start reason: ' + reason,
      {ts: 'ts', dur: 'dur', name: sliceName},
      sliceColumns,
    );
  }

  async onTraceLoad(ctx: Trace): Promise<void> {
    ctx.commands.registerCommand({
      id: 'dev.perfetto.AndroidPerf#BinderSystemServerIncoming',
      name: 'Run query: system_server incoming binder graph',
      callback: () =>
        ctx.addQueryResultsTab(
          `INCLUDE PERFETTO MODULE android.binder;
           SELECT * FROM android_binder_incoming_graph((SELECT upid FROM process WHERE name = 'system_server'))`,
          'system_server incoming binder graph',
        ),
    });

    ctx.commands.registerCommand({
      id: 'dev.perfetto.AndroidPerf#BinderSystemServerOutgoing',
      name: 'Run query: system_server outgoing binder graph',
      callback: () =>
        ctx.addQueryResultsTab(
          `INCLUDE PERFETTO MODULE android.binder;
           SELECT * FROM android_binder_outgoing_graph((SELECT upid FROM process WHERE name = 'system_server'))`,
          'system_server outgoing binder graph',
        ),
    });

    ctx.commands.registerCommand({
      id: 'dev.perfetto.AndroidPerf#MonitorContentionSystemServer',
      name: 'Run query: system_server monitor_contention graph',
      callback: () =>
        ctx.addQueryResultsTab(
          `INCLUDE PERFETTO MODULE android.monitor_contention;
           SELECT * FROM android_monitor_contention_graph((SELECT upid FROM process WHERE name = 'system_server'))`,
          'system_server monitor_contention graph',
        ),
    });

    ctx.commands.registerCommand({
      id: 'dev.perfetto.AndroidPerf#BinderAll',
      name: 'Run query: all process binder graph',
      callback: () =>
        ctx.addQueryResultsTab(
          `INCLUDE PERFETTO MODULE android.binder;
           SELECT * FROM android_binder_graph(-1000, 1000, -1000, 1000)`,
          'all process binder graph',
        ),
    });

    ctx.commands.registerCommand({
      id: 'dev.perfetto.AndroidPerf#ThreadClusterDistribution',
      name: 'Run query: runtime cluster distribution for a thread',
      callback: async (tid) => {
        if (tid === undefined) {
          tid = prompt('Enter a thread tid', '');
          if (tid === null) return;
        }
        ctx.addQueryResultsTab(
          `
          INCLUDE PERFETTO MODULE android.cpu.cluster_type;
          WITH
            total_runtime AS (
              SELECT sum(dur) AS total_runtime
              FROM sched s
              LEFT JOIN thread t
                USING (utid)
              WHERE t.tid = ${tid}
            )
            SELECT
              c.cluster_type AS cluster,
              sum(dur)/1e6 AS total_dur_ms,
              sum(dur) * 1.0 / (SELECT * FROM total_runtime) AS percentage
            FROM sched s
            LEFT JOIN thread t
              USING (utid)
            LEFT JOIN android_cpu_cluster_mapping c
              USING (cpu)
            WHERE t.tid = ${tid}
            GROUP BY 1`,
          `runtime cluster distrubtion for tid ${tid}`,
        );
      },
    });

    ctx.commands.registerCommand({
      id: 'dev.perfetto.AndroidPerf#SchedLatency',
      name: 'Run query: top 50 sched latency for a thread',
      callback: async (tid) => {
        if (tid === undefined) {
          tid = prompt('Enter a thread tid', '');
          if (tid === null) return;
        }
        ctx.addQueryResultsTab(
          `
          SELECT ts.*, t.tid, t.name, tt.id AS track_id
          FROM thread_state ts
          LEFT JOIN thread_track tt
           USING (utid)
          LEFT JOIN thread t
           USING (utid)
          WHERE ts.state IN ('R', 'R+') AND tid = ${tid}
           ORDER BY dur DESC
          LIMIT 50`,
          `top 50 sched latency slice for tid ${tid}`,
        );
      },
    });

    ctx.commands.registerCommand({
      id: 'dev.perfetto.AndroidPerf#AppProcessStarts',
      name: 'Add tracks: app process starts',
      callback: async () => {
        await ctx.engine.query(
          `INCLUDE PERFETTO MODULE android.app_process_starts;`,
        );

        const startReason = ['activity', 'service', 'broadcast', 'provider'];
        for (const reason of startReason) {
          await this.addAppProcessStartsDebugTrack(ctx, reason, 'process_name');
        }
      },
    });

    ctx.commands.registerCommand({
      id: 'dev.perfetto.AndroidPerf#AppIntentStarts',
      name: 'Add tracks: app intent starts',
      callback: async () => {
        await ctx.engine.query(
          `INCLUDE PERFETTO MODULE android.app_process_starts;`,
        );

        const startReason = ['activity', 'service', 'broadcast'];
        for (const reason of startReason) {
          await this.addAppProcessStartsDebugTrack(ctx, reason, 'intent');
        }
      },
    });
  }
}

export const plugin: PluginDescriptor = {
  pluginId: 'dev.perfetto.AndroidPerf',
  plugin: AndroidPerf,
};
