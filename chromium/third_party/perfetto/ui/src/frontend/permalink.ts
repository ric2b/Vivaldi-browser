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
import {assertExists} from '../base/logging';
import {Actions} from '../common/actions';
import {ConversionJobStatus} from '../common/conversion_jobs';
import {
  JsonSerialize,
  parseAppState,
  serializeAppState,
} from '../common/state_serialization';
import {
  BUCKET_NAME,
  MIME_BINARY,
  MIME_JSON,
  GcsUploader,
} from '../common/gcs_uploader';
import {globals} from './globals';
import {
  publishConversionJobStatusUpdate,
  publishPermalinkHash,
} from './publish';
import {Router} from './router';
import {Optional} from '../base/utils';
import {
  SERIALIZED_STATE_VERSION,
  SerializedAppState,
} from '../common/state_serialization_schema';
import {z} from 'zod';
import {showModal} from '../widgets/modal';

// Permalink serialization has two layers:
// 1. Serialization of the app state (state_serialization.ts):
//    This is a JSON object that represents the visual app state (pinned tracks,
//    visible viewport bounds, etc) BUT not the trace source.
// 2. An outer layer that contains the app state AND a link to the trace file.
//    (This file)
//
// In a nutshell:
//   AppState:  {viewport: {...}, pinnedTracks: {...}, notes: {...}}
//   Permalink: {appState: {see above}, traceUrl: 'https://gcs/trace/file'}
//
// This file deals with the outer layer, state_serialization.ts with the inner.

const PERMALINK_SCHEMA = z.object({
  traceUrl: z.string().optional(),

  // We don't want to enforce validation at this level but want to delegate it
  // to parseAppState(), for two reasons:
  // 1. parseAppState() does further semantic checks (e.g. version checking).
  // 2. We want to still load the traceUrl even if the app state is invalid.
  appState: z.any().optional(),

  // This is for the very unusual case of clicking on "Share settings" in the
  // recording page. In this case there is no trace or app state. We just
  // create a permalink with the recording state.
  recordingOpts: z.any().optional(),
});

type PermalinkState = z.infer<typeof PERMALINK_SCHEMA>;

export interface PermalinkOptions {
  mode: 'APP_STATE' | 'RECORDING_OPTS';
}

export async function createPermalink(opts: PermalinkOptions): Promise<void> {
  const jobName = 'create_permalink';
  publishConversionJobStatusUpdate({
    jobName,
    jobStatus: ConversionJobStatus.InProgress,
  });

  try {
    const hash = await createPermalinkInternal(opts);
    publishPermalinkHash(hash);
  } finally {
    publishConversionJobStatusUpdate({
      jobName,
      jobStatus: ConversionJobStatus.NotRunning,
    });
  }
}

// Returns the file name, not the full url (i.e. the name of the GCS object).
async function createPermalinkInternal(
  opts: PermalinkOptions,
): Promise<string> {
  const permalinkData: PermalinkState = {};

  if (opts.mode === 'RECORDING_OPTS') {
    permalinkData.recordingOpts = globals.state.recordConfig;
  } else if (opts.mode === 'APP_STATE') {
    // Check if we need to upload the trace file, before serializing the app
    // state.
    let alreadyUploadedUrl = '';
    const engine = assertExists(globals.getCurrentEngine());
    let dataToUpload: File | ArrayBuffer | undefined = undefined;
    let traceName = `trace ${engine.id}`;
    if (engine.source.type === 'FILE') {
      dataToUpload = engine.source.file;
      traceName = dataToUpload.name;
    } else if (engine.source.type === 'ARRAY_BUFFER') {
      dataToUpload = engine.source.buffer;
    } else if (engine.source.type === 'URL') {
      alreadyUploadedUrl = engine.source.url;
    } else {
      throw new Error(`Cannot share trace ${JSON.stringify(engine.source)}`);
    }

    // Upload the trace file, unless it's already uploaded (type == 'URL').
    // Internally TraceGcsUploader will skip the upload if an object with the
    // same hash exists already.
    if (alreadyUploadedUrl) {
      permalinkData.traceUrl = alreadyUploadedUrl;
    } else if (dataToUpload !== undefined) {
      updateStatus(`Uploading ${traceName}`);
      const uploader: GcsUploader = new GcsUploader(dataToUpload, {
        mimeType: MIME_BINARY,
        onProgress: () => reportUpdateProgress(uploader),
      });
      await uploader.waitForCompletion();
      permalinkData.traceUrl = uploader.uploadedUrl;
    }

    permalinkData.appState = serializeAppState();
  }

  // Serialize the permalink with the app state (or recording state) and upload.
  updateStatus(`Creating permalink...`);
  const permalinkJson = JsonSerialize(permalinkData);
  const uploader: GcsUploader = new GcsUploader(permalinkJson, {
    mimeType: MIME_JSON,
    onProgress: () => reportUpdateProgress(uploader),
  });
  await uploader.waitForCompletion();

  return uploader.uploadedFileName;
}

/**
 * Loads a permalink from Google Cloud Storage.
 * This is invoked when passing !#?s=fileName to URL.
 * @param gcsFileName the file name of the cloud storage object. This is
 * expected to be a JSON file that respects the schema defined by
 * PERMALINK_SCHEMA.
 */
export async function loadPermalink(gcsFileName: string): Promise<void> {
  // Otherwise, this is a request to load the permalink.
  const url = `https://storage.googleapis.com/${BUCKET_NAME}/${gcsFileName}`;
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Could not fetch permalink.\n URL: ${url}`);
  }
  const text = await response.text();
  const permalinkJson = JSON.parse(text);
  let permalink: PermalinkState;
  let error = '';

  // Try to recover permalinks generated by older versions of the UI before
  // r.android.com/3119920 .
  const convertedLegacyPermalink = tryLoadLegacyPermalink(permalinkJson);
  if (convertedLegacyPermalink !== undefined) {
    permalink = convertedLegacyPermalink;
  } else {
    const res = PERMALINK_SCHEMA.safeParse(permalinkJson);
    if (res.success) {
      permalink = res.data;
    } else {
      error = res.error.toString();
      permalink = {};
    }
  }

  if (permalink.recordingOpts !== undefined) {
    // This permalink state only contains a RecordConfig. Show the
    // recording page with the config, but keep other state as-is.
    globals.dispatch(
      Actions.setRecordConfig({config: permalink.recordingOpts}),
    );
    Router.navigate('#!/record');
    return;
  }
  if (permalink.appState !== undefined) {
    // This is the most common case where the permalink contains the app state
    // (and optionally a traceUrl, below). globals.restoreAppStateAfterTraceLoad
    // will be processed by trace_controller.ts after the trace has loaded.
    const parseRes = parseAppState(permalink.appState);
    if (parseRes.success) {
      globals.restoreAppStateAfterTraceLoad = parseRes.data;
    } else {
      error = parseRes.error;
    }
  }
  if (permalink.traceUrl) {
    globals.dispatch(Actions.openTraceFromUrl({url: permalink.traceUrl}));
  }

  if (error) {
    showModal({
      title: 'Failed to restore the serialized app state',
      content: m(
        'div',
        m(
          'p',
          'Something went wrong when restoring the app state.' +
            'This is due to some backwards-incompatible change ' +
            'when the permalink is generated and then opened using ' +
            'two different UI versions.',
        ),
        m(
          'p',
          "I'm going to try to open the trace file anyways, but " +
            'the zoom level, pinned tracks and other UI ' +
            "state wont't be recovered",
        ),
        m('p', 'Error details:'),
        m('.modal-logs', error),
      ),
      buttons: [
        {
          text: 'Open only the trace file',
          primary: true,
        },
      ],
    });
  }
}

// Tries to recover a previous permalink, before the split in two layers,
// where the permalink JSON contains the app state, which contains inside it
// the trace URL.
// If we suceed, convert it to a new-style JSON object preserving some minimal
// information (really just vieport and pinned tracks).
function tryLoadLegacyPermalink(data: unknown): Optional<PermalinkState> {
  const legacyData = data as {
    version?: number;
    engine?: {source?: {url?: string}};
    pinnedTracks?: string[];
    frontendLocalState?: {
      visibleState?: {start?: {value?: string}; end?: {value?: string}};
    };
  };
  if (legacyData.version === undefined) return undefined;
  const vizState = legacyData.frontendLocalState?.visibleState;
  return {
    traceUrl: legacyData.engine?.source?.url,
    appState: {
      version: SERIALIZED_STATE_VERSION,
      pinnedTracks: legacyData.pinnedTracks ?? [],
      viewport: vizState
        ? {start: vizState.start?.value, end: vizState.end?.value}
        : undefined,
    } as SerializedAppState,
  } as PermalinkState;
}

function reportUpdateProgress(uploader: GcsUploader) {
  switch (uploader.state) {
    case 'UPLOADING':
      const statusTxt = `Uploading ${uploader.getEtaString()}`;
      updateStatus(statusTxt);
      break;
    case 'ERROR':
      updateStatus(`Upload failed ${uploader.error}`);
      break;
    default:
      break;
  } // switch (state)
}

function updateStatus(msg: string): void {
  // TODO(hjd): Unify loading updates.
  globals.dispatch(
    Actions.updateStatus({
      msg,
      timestamp: Date.now() / 1000,
    }),
  );
}
