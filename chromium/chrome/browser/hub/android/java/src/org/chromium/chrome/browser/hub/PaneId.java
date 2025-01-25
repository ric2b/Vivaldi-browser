// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * ID values for each Pane in the Hub. New Panes must define a new unique PaneId. PaneId order does
 * not affect Pane order in the Hub. Add new entries to the end of the list before {@link
 * PaneId.COUNT}.
 *
 * <p>IMPORTANT: Do not delete or renumber entries. Keep this list in sync with HubPaneId in
 * tools/metrics/histograms/metadata/android/enums.xml.
 */
@IntDef({
    PaneId.TAB_SWITCHER,
    PaneId.INCOGNITO_TAB_SWITCHER,
    PaneId.BOOKMARKS,
    PaneId.TAB_GROUPS,
    PaneId.SYNC_TABS, // Vivaldi
    PaneId.TRASH_TABS, // Vivaldi
    PaneId.COUNT
})
@Retention(RetentionPolicy.SOURCE)
public @interface PaneId {
    int TAB_SWITCHER = 0;
    int INCOGNITO_TAB_SWITCHER = 1;
    int BOOKMARKS = 2;
    int TAB_GROUPS = 3;
    int SYNC_TABS = 4; // Vivaldi
    int TRASH_TABS = 5; // Vivaldi
    /** Must be last. */
    int COUNT = 6;
}
