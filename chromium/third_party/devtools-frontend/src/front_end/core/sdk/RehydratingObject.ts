// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as Protocol from '../../generated/protocol.js';

export interface RehydratingScript {
  scriptId: Protocol.Runtime.ScriptId;
  isolate: string;
  url: string;
  executionContextId: Protocol.Runtime.ExecutionContextId;
  startLine: number;
  startColumn: number;
  endLine: number;
  endColumn: number;
  hash: string;
  isModule?: boolean;
  hasSourceUrl?: boolean;
  sourceMapUrl?: string;
  length?: number;
  sourceText?: string;
  auxData?: RehydratingExecutionContextAuxData;
}

export interface RehydratingExecutionContextAuxData {
  frameId?: Protocol.Page.FrameId;
  isDefault?: boolean;
  type?: string;
}

export interface RehydratingExecutionContext {
  id: Protocol.Runtime.ExecutionContextId;
  origin: string;
  v8Context?: string;
  name?: string;
  auxData?: RehydratingExecutionContextAuxData;
  isolate?: string;
}

export interface RehydratingTarget {
  targetId: Protocol.Page.FrameId;
  type: string;
  url: string;
  pid?: number;
  isolate?: string;
}

export type HydratingDataPerTarget = Map<RehydratingTarget, [RehydratingExecutionContext[], RehydratingScript[]]>;

export interface ProtocolMessage {
  id: number;
  method: string;
  sessionId?: number;
  params?: object;
}

export interface ProtocolEvent {
  method: string;
  params: object;
}

export interface ProtocolResponse {
  id: number;
}

export type ServerMessage = (ProtocolEvent|ProtocolMessage|ProtocolResponse)&{
  [others: string]: unknown,
};

export interface Session {
  target: RehydratingTarget;
  executionContexts: RehydratingExecutionContext[];
  scripts: RehydratingScript[];
}
