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

import {
  expandProcessName,
  BlockingCallMetricData,
  MetricHandler,
} from './metricUtils';
import {PluginContextTrace} from '../../../public';
import {PLUGIN_ID} from '../pluginId';
import {SimpleSliceTrackConfig} from '../../../frontend/simple_slice_track';
import {addJankCUJDebugTrack} from '../../dev.perfetto.AndroidCujs';
import {
  addAndPinSliceTrack,
  TrackType,
} from '../../dev.perfetto.AndroidCujs/trackUtils';

class BlockingCallMetricHandler implements MetricHandler {
  /**
   * Match metric key & return parsed data if successful.
   *
   * @param {string} metricKey The metric key to match.
   * @returns {BlockingCallMetricData | undefined} Parsed data or undefined if no match.
   */
  public match(metricKey: string): BlockingCallMetricData | undefined {
    const matcher =
      /perfetto_android_blocking_call-cuj-name-(?<process>.*)-name-(?<cujName>.*)-blocking_calls-name-(?<blockingCallName>([^\-]*))-(?<aggregation>.*)/;
    const match = matcher.exec(metricKey);
    if (!match?.groups) {
      return undefined;
    }
    const metricData: BlockingCallMetricData = {
      process: expandProcessName(match.groups.process),
      cujName: match.groups.cujName,
      blockingCallName: match.groups.blockingCallName,
      aggregation: match.groups.aggregation,
    };
    return metricData;
  }

  /**
   * Adds the debug tracks for Blocking Call metrics
   * registerStaticTrack used when plugin adds tracks onTraceload()
   * addDebugSliceTrack used for adding tracks using the command
   *
   * @param {BlockingCallMetricData} metricData Parsed metric data for the cuj scoped jank
   * @param {PluginContextTrace} ctx PluginContextTrace for trace related properties and methods
   * @param {TrackType} type 'static' when called onTraceload and 'debug' when called through command
   * @returns {void} Adds one track for Jank CUJ slice and one for Janky CUJ frames
   */
  public addMetricTrack(
    metricData: BlockingCallMetricData,
    ctx: PluginContextTrace,
    type: TrackType,
  ): void {
    this.pinSingleCuj(ctx, metricData, type);
    const uri = `${PLUGIN_ID}#BlockingCallSlices#${metricData}`;
    // TODO: b/349502258 - Refactor to single API
    const {config: blockingCallMetricConfig, trackName: trackName} =
      this.blockingCallTrackConfig(metricData);
    addAndPinSliceTrack(ctx, blockingCallMetricConfig, trackName, type, uri);
  }

  private pinSingleCuj(
    ctx: PluginContextTrace,
    metricData: BlockingCallMetricData,
    type: TrackType,
  ) {
    const uri = `${PLUGIN_ID}#BlockingCallCUJ#${metricData}`;
    const trackName = `Jank CUJ: ${metricData.cujName}`;
    addJankCUJDebugTrack(ctx, trackName, type, metricData.cujName, uri);
  }

  private blockingCallTrackConfig(metricData: BlockingCallMetricData): {
    config: SimpleSliceTrackConfig;
    trackName: string;
  } {
    const cuj = metricData.cujName;
    const processName = metricData.process;
    const blockingCallName = metricData.blockingCallName;

    // TODO: b/296349525 - Migrate jank tables from run metrics to stdlib
    const blockingCallDuringCujQuery = `
  SELECT name, ts, dur
  FROM main_thread_slices_scoped_to_cujs
  WHERE process_name = "${processName}"
      AND cuj_name = "${cuj}"
      AND name = "${blockingCallName}"
  `;

    const blockingCallMetricConfig: SimpleSliceTrackConfig = {
      data: {
        sqlSource: blockingCallDuringCujQuery,
        columns: ['name', 'ts', 'dur'],
      },
      columns: {ts: 'ts', dur: 'dur', name: 'name'},
      argColumns: ['name', 'ts', 'dur'],
    };

    const trackName = 'Blocking calls in ' + processName;

    return {config: blockingCallMetricConfig, trackName: trackName};
  }
}

export const pinBlockingCallHandlerInstance = new BlockingCallMetricHandler();
