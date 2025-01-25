// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../../core/common/common.js';
import * as i18n from '../../../core/i18n/i18n.js';
import * as Platform from '../../../core/platform/platform.js';
import * as TraceEngine from '../../../models/trace/trace.js';

import {getEventStyle} from './EntryStyles.js';

const UIStrings = {
  /**
   *@description Text shown for an entry in the flame chart that has no explict name.
   */
  anonymous: '(anonymous)',
  /**
   *@description Text used to show an EventDispatch event which has a type associated with it
   *@example {click} PH1
   */
  eventDispatchS: 'Event: {PH1}',
  /**
   *@description Text shown for an entry in the flame chart that represents a frame.
   */
  frame: 'Frame',
  /**
   *@description Text in Timeline Flame Chart Data Provider of the Performance panel
   */
  wsConnectionOpened: 'WebSocket opened',
  /**
   *@description Text in Timeline Flame Chart Data Provider of the Performance panel
   *@example {ws://example.com} PH1
   */
  wsConnectionOpenedWithUrl: 'WebSocket opened: {PH1}',
  /**
   *@description Text in Timeline Flame Chart Data Provider of the Performance panel
   */
  wsConnectionClosed: 'WebSocket closed',
};

const str_ = i18n.i18n.registerUIStrings('panels/timeline/components/EntryName.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

/**
 * Calculates the display name for a given entry.
 * @param traceParsedData - If the trace data is provided
 * as the second argument it can be used to find source map resolved names for
 * profile calls.
 * Use this function to customise the user visible name for an entry. If no
 * custom name is found, we will fallback to the `name` property in the trace
 * entry.
 */
export function nameForEntry(
    entry: TraceEngine.Types.TraceEvents.TraceEventData,
    traceParsedData?: TraceEngine.Handlers.Types.TraceParseData,
    ): string {
  if (TraceEngine.Types.TraceEvents.isProfileCall(entry)) {
    if (traceParsedData) {
      const potentialCallName =
          TraceEngine.Handlers.ModelHandlers.Samples.getProfileCallFunctionName(traceParsedData.Samples, entry);
      // We need this extra check because the call name could be the empty
      // string. If it is, we want to fallback.
      if (potentialCallName) {
        return potentialCallName;
      }
    }
    return entry.callFrame.functionName || i18nString(UIStrings.anonymous);
  }

  if (TraceEngine.Types.TraceEvents.isLegacyTimelineFrame(entry)) {
    return i18n.i18n.lockedString(UIStrings.frame);
  }

  if (TraceEngine.Types.TraceEvents.isTraceEventDispatch(entry)) {
    // EventDispatch represent user actions such as clicks, so in this case
    // rather than show the event title (which is always just "Event"), we
    // add the type ("click") to help the user understand the event.
    return i18nString(UIStrings.eventDispatchS, {PH1: entry.args.data.type});
  }
  if (TraceEngine.Types.TraceEvents.isSyntheticNetworkRequestEvent(entry)) {
    const parsedURL = new Common.ParsedURL.ParsedURL(entry.args.data.url);
    const text =
        parsedURL.isValid ? `${parsedURL.displayName} (${parsedURL.host})` : entry.args.data.url || 'Network request';
    return Platform.StringUtilities.trimEndWithMaxLength(text, 40);
  }

  if (TraceEngine.Types.TraceEvents.isTraceEventWebSocketCreate(entry)) {
    if (entry.args.data.url) {
      return i18nString(UIStrings.wsConnectionOpenedWithUrl, {PH1: entry.args.data.url});
    }

    return i18nString(UIStrings.wsConnectionOpened);
  }

  if (TraceEngine.Types.TraceEvents.isTraceEventWebSocketDestroy(entry)) {
    return i18nString(UIStrings.wsConnectionClosed);
  }

  if (TraceEngine.Types.TraceEvents.isSyntheticInteractionEvent(entry)) {
    return nameForInteractionEvent(entry);
  }

  const eventStyleCustomName = getEventStyle(entry.name as TraceEngine.Types.TraceEvents.KnownEventName)?.title;

  return eventStyleCustomName || entry.name;
}

function nameForInteractionEvent(event: TraceEngine.Types.TraceEvents.SyntheticInteractionPair): string {
  const category = TraceEngine.Handlers.ModelHandlers.UserInteractions.categoryOfInteraction(event);
  // Because we hide nested interactions, we do not want to show the
  // specific type of the interaction that was not hidden, so instead we
  // show just the category of that interaction.
  if (category === 'OTHER') {
    return 'Other';
  }
  if (category === 'KEYBOARD') {
    return 'Keyboard';
  }
  if (category === 'POINTER') {
    return 'Pointer';
  }
  return event.type;
}
