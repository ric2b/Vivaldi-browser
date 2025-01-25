// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../common/common.js';
import * as Platform from '../platform/platform.js';

import {InspectorFrontendHostInstance} from './InspectorFrontendHost.js';
import {type AidaClientResult, type SyncInformation} from './InspectorFrontendHostAPI.js';
import {bindOutputStream} from './ResourceLoader.js';

export enum Entity {
  UNKNOWN = 0,
  USER = 1,
  SYSTEM = 2,
}

export const enum Rating {
  POSITIVE = 'POSITIVE',
  NEGATIVE = 'NEGATIVE',
}

export interface Chunk {
  text: string;
  entity: Entity;
}

export enum FunctionalityType {
  // Unspecified functionality type.
  FUNCTIONALITY_TYPE_UNSPECIFIED = 0,
  // The generic AI chatbot functionality.
  CHAT = 1,
  // The explain error functionality.
  EXPLAIN_ERROR = 2,
}

export enum ClientFeature {
  // Unspecified client feature.
  CLIENT_FEATURE_UNSPECIFIED = 0,
  // Chrome console insights feature.
  CHROME_CONSOLE_INSIGHTS = 1,
  // Chrome freestyler.
  CHROME_FREESTYLER = 2,
}

export interface AidaRequest {
  input: string;
  preamble?: string;
  // eslint-disable-next-line @typescript-eslint/naming-convention
  chat_history?: Chunk[];
  client: string;
  options?: {
    temperature?: Number,
    // eslint-disable-next-line @typescript-eslint/naming-convention
    model_id?: string,
  };
  metadata?: {
    // eslint-disable-next-line @typescript-eslint/naming-convention
    disable_user_content_logging: boolean,
    // eslint-disable-next-line @typescript-eslint/naming-convention
    string_session_id?: string,
  };
  // eslint-disable-next-line @typescript-eslint/naming-convention
  functionality_type?: FunctionalityType;
  // eslint-disable-next-line @typescript-eslint/naming-convention
  client_feature?: ClientFeature;
}

export interface AidaDoConversationClientEvent {
  // eslint-disable-next-line @typescript-eslint/naming-convention
  corresponding_aida_rpc_global_id: number;
  // eslint-disable-next-line @typescript-eslint/naming-convention
  disable_user_content_logging?: boolean;
  // eslint-disable-next-line @typescript-eslint/naming-convention
  do_conversation_client_event: {
    // eslint-disable-next-line @typescript-eslint/naming-convention
    user_feedback: {
      sentiment?: Rating,
      // eslint-disable-next-line @typescript-eslint/naming-convention
      user_input?: {
        comment?: string,
      },
    },
  };
}

export enum RecitationAction {
  ACTION_UNSPECIFIED = 'ACTION_UNSPECIFIED',
  CITE = 'CITE',
  BLOCK = 'BLOCK',
  NO_ACTION = 'NO_ACTION',
  EXEMPT_FOUND_IN_PROMPT = 'EXEMPT_FOUND_IN_PROMPT',
}

export interface Citation {
  startIndex: number;
  endIndex: number;
  url: string;
}

export interface AttributionMetadata {
  attributionAction: RecitationAction;
  citations: Citation[];
}

export interface AidaResponseMetadata {
  rpcGlobalId?: number;
  attributionMetadata?: AttributionMetadata[];
}

export interface AidaResponse {
  explanation: string;
  metadata: AidaResponseMetadata;
}

export enum AidaAvailability {
  AVAILABLE = 'available',
  NO_ACCOUNT_EMAIL = 'no-account-email',
  NO_ACTIVE_SYNC = 'no-active-sync',
  NO_INTERNET = 'no-internet',
}

export const CLIENT_NAME = 'CHROME_DEVTOOLS';

export class AidaClient {
  static buildConsoleInsightsRequest(input: string): AidaRequest {
    const request: AidaRequest = {
      input,
      client: CLIENT_NAME,
      functionality_type: FunctionalityType.EXPLAIN_ERROR,
      client_feature: ClientFeature.CHROME_CONSOLE_INSIGHTS,
    };
    const config = Common.Settings.Settings.instance().getHostConfig();
    let temperature = NaN;
    let modelId = null;
    let disallowLogging = false;
    if (config?.devToolsConsoleInsights.enabled) {
      temperature = config.devToolsConsoleInsights.aidaTemperature;
      modelId = config.devToolsConsoleInsights.aidaModelId;
      disallowLogging = config.devToolsConsoleInsights.disallowLogging;
    }

    if (!isNaN(temperature)) {
      request.options ??= {};
      request.options.temperature = temperature;
    }
    if (modelId) {
      request.options ??= {};
      request.options.model_id = modelId;
    }
    if (disallowLogging) {
      request.metadata = {
        disable_user_content_logging: true,
      };
    }
    return request;
  }

  static async getAidaClientAvailability(): Promise<AidaAvailability> {
    if (!navigator.onLine) {
      return AidaAvailability.NO_INTERNET;
    }

    const syncInfo = await new Promise<SyncInformation>(
        resolve => InspectorFrontendHostInstance.getSyncInformation(syncInfo => resolve(syncInfo)));
    if (!syncInfo.accountEmail) {
      return AidaAvailability.NO_ACCOUNT_EMAIL;
    }

    if (!syncInfo.isSyncActive) {
      return AidaAvailability.NO_ACTIVE_SYNC;
    }

    return AidaAvailability.AVAILABLE;
  }

  async * fetch(request: AidaRequest): AsyncGenerator<AidaResponse, void, void> {
    if (!InspectorFrontendHostInstance.doAidaConversation) {
      throw new Error('doAidaConversation is not available');
    }
    const stream = (() => {
      let {promise, resolve, reject} = Platform.PromiseUtilities.promiseWithResolvers<string|null>();
      return {
        write: async(data: string): Promise<void> => {
          resolve(data);
          ({promise, resolve, reject} = Platform.PromiseUtilities.promiseWithResolvers<string|null>());
        },
        close: async(): Promise<void> => {
          resolve(null);
        },
        read: (): Promise<string|null> => {
          return promise;
        },
        fail: (e: Error) => reject(e),
      };
    })();
    const streamId = bindOutputStream(stream);
    InspectorFrontendHostInstance.doAidaConversation(JSON.stringify(request), streamId, result => {
      if (result.statusCode === 403) {
        stream.fail(new Error('Server responded: permission denied'));
      } else if (result.error) {
        stream.fail(new Error(`Cannot send request: ${result.error} ${result.detail || ''}`));
      } else if (result.statusCode !== 200) {
        stream.fail(new Error(`Request failed: ${JSON.stringify(result)}`));
      } else {
        void stream.close();
      }
    });
    let chunk;
    const text = [];
    let inCodeChunk = false;
    const metadata: AidaResponseMetadata = {rpcGlobalId: 0};
    while ((chunk = await stream.read())) {
      let textUpdated = false;
      // The AIDA response is a JSON array of objects, split at the object
      // boundary. Therefore each chunk may start with `[` or `,` and possibly
      // followed by `]`. Each chunk may include one or more objects, so we
      // make sure that each chunk becomes a well-formed JSON array when we
      // parse it by adding `[` and `]` and removing `,` where appropriate.
      if (!chunk.length) {
        continue;
      }
      if (chunk.startsWith(',')) {
        chunk = chunk.slice(1);
      }
      if (!chunk.startsWith('[')) {
        chunk = '[' + chunk;
      }
      if (!chunk.endsWith(']')) {
        chunk = chunk + ']';
      }
      let results;
      try {
        results = JSON.parse(chunk);
      } catch (error) {
        throw new Error('Cannot parse chunk: ' + chunk, {cause: error});
      }
      const CODE_CHUNK_SEPARATOR = '\n`````\n';
      for (const result of results) {
        if ('metadata' in result) {
          metadata.rpcGlobalId = result.metadata.rpcGlobalId;
          if ('attributionMetadata' in result.metadata) {
            if (!metadata.attributionMetadata) {
              metadata.attributionMetadata = [];
            }
            metadata.attributionMetadata.push(result.metadata.attributionMetadata);
          }
        }
        if ('textChunk' in result) {
          if (inCodeChunk) {
            text.push(CODE_CHUNK_SEPARATOR);
            inCodeChunk = false;
          }
          text.push(result.textChunk.text);
          textUpdated = true;
        } else if ('codeChunk' in result) {
          if (!inCodeChunk) {
            text.push(CODE_CHUNK_SEPARATOR);
            inCodeChunk = true;
          }
          text.push(result.codeChunk.code);
          textUpdated = true;
        } else if ('error' in result) {
          throw new Error(`Server responded: ${JSON.stringify(result)}`);
        } else {
          throw new Error('Unknown chunk result');
        }
      }
      if (textUpdated) {
        yield {
          explanation: text.join('') + (inCodeChunk ? CODE_CHUNK_SEPARATOR : ''),
          metadata,
        };
      }
    }
  }

  registerClientEvent(clientEvent: AidaDoConversationClientEvent): Promise<AidaClientResult> {
    const {promise, resolve} = Platform.PromiseUtilities.promiseWithResolvers<AidaClientResult>();
    InspectorFrontendHostInstance.registerAidaClientEvent(
        JSON.stringify({
          client: CLIENT_NAME,
          event_time: new Date().toISOString(),
          ...clientEvent,
        }),
        resolve,
    );

    return promise;
  }
}
