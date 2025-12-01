import { globalTestConfig } from '../../framework/test_config.js';
import { version } from '../version.js';

import { LiveTestCaseResult } from './result.js';
import { TestCaseRecorder } from './test_case_recorder.js';

export type LogResults = Map<string, LiveTestCaseResult>;

export class Logger {
  readonly overriddenDebugMode: boolean | undefined;
  readonly results: LogResults = new Map();
  defaultDeviceDescription: string | undefined;

  constructor({ overrideDebugMode }: { overrideDebugMode?: boolean } = {}) {
    this.overriddenDebugMode = overrideDebugMode;
  }

  record(name: string): [TestCaseRecorder, LiveTestCaseResult] {
    const result: LiveTestCaseResult = { status: 'running', timems: -1 };
    this.results.set(name, result);
    return [
      new TestCaseRecorder(result, this.overriddenDebugMode ?? globalTestConfig.enableDebugLogs),
      result,
    ];
  }

  asJSON(space?: number, predFunc?: (key: string, value: LiveTestCaseResult) => boolean): string {
    return JSON.stringify(
      {
        version,
        defaultDevice: this.defaultDeviceDescription,
        results: Array.from(
          new Map(
            [...this.results].filter(([key, value]) => (predFunc ? predFunc(key, value) : true))
          )
        ),
      },
      undefined,
      space
    );
  }
}
