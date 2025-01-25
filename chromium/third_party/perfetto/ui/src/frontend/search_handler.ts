// Copyright (C) 2019 The Android Open Source Project
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

import {searchSegment} from '../base/binary_search';
import {assertUnreachable} from '../base/logging';
import {Actions} from '../common/actions';
import {globals} from './globals';
import {verticalScrollToTrack} from './scroll_helper';

function setToPrevious(current: number) {
  let index = current - 1;
  if (index < 0) {
    index = globals.currentSearchResults.totalResults - 1;
  }
  globals.dispatch(Actions.setSearchIndex({index}));
}

function setToNext(current: number) {
  const index = (current + 1) % globals.currentSearchResults.totalResults;
  globals.dispatch(Actions.setSearchIndex({index}));
}

export function executeSearch(reverse = false) {
  const index = globals.state.searchIndex;
  const vizWindow = globals.timeline.visibleWindow.toTimeSpan();
  const startNs = vizWindow.start;
  const endNs = vizWindow.end;
  const currentTs = globals.currentSearchResults.tses[index];

  // If the value of |globals.currentSearchResults.totalResults| is 0,
  // it means that the query is in progress or no results are found.
  if (globals.currentSearchResults.totalResults === 0) {
    return;
  }

  // If this is a new search or the currentTs is not in the viewport,
  // select the first/last item in the viewport.
  if (
    index === -1 ||
    (currentTs !== -1n && (currentTs < startNs || currentTs > endNs))
  ) {
    if (reverse) {
      const [smaller] = searchSegment(globals.currentSearchResults.tses, endNs);
      // If there is no item in the viewport just go to the previous.
      if (smaller === -1) {
        setToPrevious(index);
      } else {
        globals.dispatch(Actions.setSearchIndex({index: smaller}));
      }
    } else {
      const [, larger] = searchSegment(
        globals.currentSearchResults.tses,
        startNs,
      );
      // If there is no item in the viewport just go to the next.
      if (larger === -1) {
        setToNext(index);
      } else {
        globals.dispatch(Actions.setSearchIndex({index: larger}));
      }
    }
  } else {
    // If the currentTs is in the viewport, increment the index.
    if (reverse) {
      setToPrevious(index);
    } else {
      setToNext(index);
    }
  }
  selectCurrentSearchResult();
}

function selectCurrentSearchResult() {
  const searchIndex = globals.state.searchIndex;
  const source = globals.currentSearchResults.sources[searchIndex];
  const currentId = globals.currentSearchResults.eventIds[searchIndex];
  const trackKey = globals.currentSearchResults.trackKeys[searchIndex];

  if (currentId === undefined) return;

  switch (source) {
    case 'track':
      verticalScrollToTrack(trackKey, true);
      break;
    case 'cpu':
      globals.setLegacySelection(
        {
          kind: 'SCHED_SLICE',
          id: currentId,
          trackKey,
        },
        {
          clearSearch: false,
          pendingScrollId: currentId,
          switchToCurrentSelectionTab: true,
        },
      );
      break;
    case 'log':
      globals.setLegacySelection(
        {
          kind: 'LOG',
          id: currentId,
          trackKey,
        },
        {
          clearSearch: false,
          pendingScrollId: currentId,
          switchToCurrentSelectionTab: true,
        },
      );
      break;
    case 'slice':
      // Search results only include slices from the slice table for now.
      // When we include annotations we need to pass the correct table.
      globals.setLegacySelection(
        {
          kind: 'SLICE',
          id: currentId,
          trackKey,
          table: 'slice',
        },
        {
          clearSearch: false,
          pendingScrollId: currentId,
          switchToCurrentSelectionTab: true,
        },
      );
      break;
    default:
      assertUnreachable(source);
  }
}
