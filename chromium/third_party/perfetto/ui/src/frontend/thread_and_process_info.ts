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

import {copyToClipboard} from '../base/clipboard';
import {Icons} from '../base/semantic_icons';
import {exists} from '../base/utils';
import {Engine} from '../trace_processor/engine';
import {NUM, NUM_NULL, STR_NULL} from '../trace_processor/query_result';
import {fromNumNull} from '../trace_processor/sql_utils';
import {Anchor} from '../widgets/anchor';
import {MenuItem, PopupMenu2} from '../widgets/menu';

import {Upid, Utid} from './sql_types';

// Interface definitions for process and thread-related information
// and functions to extract them from SQL.

// TODO(altimin): Current implementation ends up querying process and thread
// information separately for each thread. Given that there is a limited
// numer of threads and processes, it might be easier to fetch this information
// once when loading the trace and then just look it up synchronously.

export interface ProcessInfo {
  upid: Upid;
  pid?: number;
  name?: string;
  uid?: number;
  packageName?: string;
  versionCode?: number;
}

export async function getProcessInfo(
  engine: Engine,
  upid: Upid,
): Promise<ProcessInfo> {
  const res = await engine.query(`
    include perfetto module android.process_metadata;
    select
      p.upid,
      p.pid,
      p.name,
      p.uid,
      m.package_name as packageName,
      m.version_code as versionCode
    from process p
    left join android_process_metadata m using (upid)
    where upid = ${upid};
  `);
  const row = res.firstRow({
    upid: NUM,
    pid: NUM,
    name: STR_NULL,
    uid: NUM_NULL,
    packageName: STR_NULL,
    versionCode: NUM_NULL,
  });
  return {
    upid,
    pid: row.pid,
    name: row.name ?? undefined,
    uid: fromNumNull(row.uid),
    packageName: row.packageName ?? undefined,
    versionCode: fromNumNull(row.versionCode),
  };
}

function getDisplayName(
  name: string | undefined,
  id: number | undefined,
): string | undefined {
  if (name === undefined) {
    return id === undefined ? undefined : `${id}`;
  }
  return id === undefined ? name : `${name} [${id}]`;
}

export function renderProcessRef(info: ProcessInfo): m.Children {
  const name = info.name;
  return m(
    PopupMenu2,
    {
      trigger: m(Anchor, getProcessName(info)),
    },
    exists(name) &&
      m(MenuItem, {
        icon: Icons.Copy,
        label: 'Copy process name',
        onclick: () => copyToClipboard(name),
      }),
    exists(info.pid) &&
      m(MenuItem, {
        icon: Icons.Copy,
        label: 'Copy pid',
        onclick: () => copyToClipboard(`${info.pid}`),
      }),
    m(MenuItem, {
      icon: Icons.Copy,
      label: 'Copy upid',
      onclick: () => copyToClipboard(`${info.upid}`),
    }),
  );
}

export function getProcessName(info?: ProcessInfo): string | undefined {
  return getDisplayName(info?.name, info?.pid);
}

export interface ThreadInfo {
  utid: Utid;
  tid?: number;
  name?: string;
  process?: ProcessInfo;
}

export async function getThreadInfo(
  engine: Engine,
  utid: Utid,
): Promise<ThreadInfo> {
  const it = (
    await engine.query(`
        SELECT tid, name, upid
        FROM thread
        WHERE utid = ${utid};
    `)
  ).iter({tid: NUM, name: STR_NULL, upid: NUM_NULL});
  if (!it.valid()) {
    return {
      utid,
    };
  }
  const upid = fromNumNull(it.upid) as Upid | undefined;
  return {
    utid,
    tid: it.tid,
    name: it.name ?? undefined,
    process: upid ? await getProcessInfo(engine, upid) : undefined,
  };
}

export function renderThreadRef(info: ThreadInfo): m.Children {
  const name = info.name;
  return m(
    PopupMenu2,
    {
      trigger: m(Anchor, getThreadName(info)),
    },
    exists(name) &&
      m(MenuItem, {
        icon: Icons.Copy,
        label: 'Copy thread name',
        onclick: () => copyToClipboard(name),
      }),
    exists(info.tid) &&
      m(MenuItem, {
        icon: Icons.Copy,
        label: 'Copy tid',
        onclick: () => copyToClipboard(`${info.tid}`),
      }),
    m(MenuItem, {
      icon: Icons.Copy,
      label: 'Copy utid',
      onclick: () => copyToClipboard(`${info.utid}`),
    }),
  );
}

export function getThreadName(info?: ThreadInfo): string | undefined {
  return getDisplayName(info?.name, info?.tid);
}

// Return the full thread name, including the process name.
export function getFullThreadName(info?: ThreadInfo): string | undefined {
  if (info?.process === undefined) {
    return getThreadName(info);
  }
  return `${getThreadName(info)} ${getProcessName(info.process)}`;
}
