import { LogMessageWithStack } from '../../internal/logging/log_message.js';
import { TransferredTestCaseResult, LiveTestCaseResult } from '../../internal/logging/result.js';
import { TestCaseRecorder } from '../../internal/logging/test_case_recorder.js';
import { TestQueryWithExpectation } from '../../internal/query/query.js';
import { timeout } from '../../util/timeout.js';
import { assert } from '../../util/util.js';

import { CTSOptions, kDefaultCTSOptions } from './options.js';
import { WorkerTestRunRequest } from './utils_worker.js';

/** Query all currently-registered service workers, and unregister them. */
function unregisterAllServiceWorkers() {
  void navigator.serviceWorker.getRegistrations().then(registrations => {
    for (const registration of registrations) {
      void registration.unregister();
    }
  });
}

// NOTE: This code runs on startup for any runtime with worker support. Here, we use that chance to
// delete any leaked service workers, and register to clean up after ourselves at shutdown.
unregisterAllServiceWorkers();
window.addEventListener('beforeunload', () => {
  unregisterAllServiceWorkers();
});

class TestBaseWorker {
  protected readonly ctsOptions: CTSOptions;
  protected readonly resolvers = new Map<string, (result: LiveTestCaseResult) => void>();

  constructor(worker: CTSOptions['worker'], ctsOptions?: CTSOptions) {
    this.ctsOptions = { ...(ctsOptions || kDefaultCTSOptions), ...{ worker } };
  }

  onmessage(ev: MessageEvent) {
    const query: string = ev.data.query;
    const result: TransferredTestCaseResult = ev.data.result;
    if (result.logs) {
      for (const l of result.logs) {
        Object.setPrototypeOf(l, LogMessageWithStack.prototype);
      }
    }
    this.resolvers.get(query)!(result as LiveTestCaseResult);
    this.resolvers.delete(query);

    // MAINTENANCE_TODO(kainino0x): update the Logger with this result (or don't have a logger and
    // update the entire results JSON somehow at some point).
  }

  async makeRequestAndRecordResult(
    target: MessagePort | Worker | ServiceWorker,
    rec: TestCaseRecorder,
    query: string,
    expectations: TestQueryWithExpectation[]
  ) {
    const request: WorkerTestRunRequest = {
      query,
      expectations,
      ctsOptions: this.ctsOptions,
    };
    target.postMessage(request);

    const workerResult = await new Promise<LiveTestCaseResult>(resolve => {
      assert(!this.resolvers.has(query), "can't request same query twice simultaneously");
      this.resolvers.set(query, resolve);
    });
    rec.injectResult(workerResult);
  }
}

export class TestDedicatedWorker extends TestBaseWorker {
  private readonly worker: Worker;

  constructor(ctsOptions?: CTSOptions) {
    super('dedicated', ctsOptions);
    const selfPath = import.meta.url;
    const selfPathDir = selfPath.substring(0, selfPath.lastIndexOf('/'));
    const workerPath = selfPathDir + '/test_worker-worker.js';
    this.worker = new Worker(workerPath, { type: 'module' });
    this.worker.onmessage = ev => this.onmessage(ev);
  }

  async run(
    rec: TestCaseRecorder,
    query: string,
    expectations: TestQueryWithExpectation[] = []
  ): Promise<void> {
    await this.makeRequestAndRecordResult(this.worker, rec, query, expectations);
  }
}

export class TestWorker extends TestDedicatedWorker {}

export class TestSharedWorker extends TestBaseWorker {
  private readonly port: MessagePort;

  constructor(ctsOptions?: CTSOptions) {
    super('shared', ctsOptions);
    const selfPath = import.meta.url;
    const selfPathDir = selfPath.substring(0, selfPath.lastIndexOf('/'));
    const workerPath = selfPathDir + '/test_worker-worker.js';
    const worker = new SharedWorker(workerPath, { type: 'module' });
    this.port = worker.port;
    this.port.start();
    this.port.onmessage = ev => this.onmessage(ev);
  }

  async run(
    rec: TestCaseRecorder,
    query: string,
    expectations: TestQueryWithExpectation[] = []
  ): Promise<void> {
    await this.makeRequestAndRecordResult(this.port, rec, query, expectations);
  }
}

export class TestServiceWorker extends TestBaseWorker {
  constructor(ctsOptions?: CTSOptions) {
    super('service', ctsOptions);
  }

  async run(
    rec: TestCaseRecorder,
    query: string,
    expectations: TestQueryWithExpectation[] = []
  ): Promise<void> {
    const [suite, name] = query.split(':', 2);
    const fileName = name.split(',').join('/');
    const serviceWorkerURL = new URL(
      `/out/${suite}/webworker/${fileName}.worker.js`,
      window.location.href
    ).toString();

    // If a registration already exists for this path, it will be ignored.
    const registration = await navigator.serviceWorker.register(serviceWorkerURL, {
      type: 'module',
    });
    // Make sure the registration we just requested is active. (We don't worry about it being
    // outdated from a previous page load, because we wipe all service workers on shutdown/startup.)
    while (!registration.active || registration.active.scriptURL !== serviceWorkerURL) {
      await new Promise(resolve => timeout(resolve, 0));
    }
    const serviceWorker = registration.active;

    navigator.serviceWorker.onmessage = ev => this.onmessage(ev);
    await this.makeRequestAndRecordResult(serviceWorker, rec, query, expectations);
  }
}
