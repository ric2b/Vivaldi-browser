// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Trace from '../../models/trace/trace.js';

import {TimelineUIUtils} from './TimelineUIUtils.js';

export class IsLong extends Trace.Extras.TraceFilter.TraceFilter {
  #minimumRecordDurationMilli = Trace.Types.Timing.MilliSeconds(0);
  constructor() {
    super();
  }

  setMinimumRecordDuration(value: Trace.Types.Timing.MilliSeconds): void {
    this.#minimumRecordDurationMilli = value;
  }

  accept(event: Trace.Types.Events.Event): boolean {
    const {duration} = Trace.Helpers.Timing.eventTimingsMilliSeconds(event);
    return duration >= this.#minimumRecordDurationMilli;
  }
}

export class Category extends Trace.Extras.TraceFilter.TraceFilter {
  constructor() {
    super();
  }

  accept(event: Trace.Types.Events.Event): boolean {
    return !TimelineUIUtils.eventStyle(event).category.hidden;
  }
}

export class TimelineRegExp extends Trace.Extras.TraceFilter.TraceFilter {
  private regExpInternal!: RegExp|null;
  constructor(regExp?: RegExp) {
    super();
    this.setRegExp(regExp || null);
  }

  setRegExp(regExp: RegExp|null): void {
    this.regExpInternal = regExp;
  }

  regExp(): RegExp|null {
    return this.regExpInternal;
  }

  accept(event: Trace.Types.Events.Event, parsedTrace?: Trace.Handlers.Types.ParsedTrace): boolean {
    return !this.regExpInternal || TimelineUIUtils.testContentMatching(event, this.regExpInternal, parsedTrace);
  }
}
