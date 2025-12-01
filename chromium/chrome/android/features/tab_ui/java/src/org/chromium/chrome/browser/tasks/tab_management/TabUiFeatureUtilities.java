// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Build;

import org.chromium.build.annotations.NullMarked;

import java.util.Locale;
import java.util.Set;

// Vivaldi
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.vivaldi.browser.preferences.VivaldiPreferences;

/** A class to handle the state of flags for tab_management. */
@NullMarked
public class TabUiFeatureUtilities {
    private static final Set<String> TAB_TEARING_OEM_ALLOWLIST = Set.of("samsung");

    /** Returns whether device OEM is allow-listed for tab tearing */
    public static boolean doesOemSupportDragToCreateInstance() {
        return TAB_TEARING_OEM_ALLOWLIST.contains(Build.MANUFACTURER.toLowerCase(Locale.US));
    }

    /** Vivaldi
     * @return Whether the tab group feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidEnabled() {
            return VivaldiPreferences.getSharedPreferencesManager().readBoolean(
                    VivaldiPreferences.ENABLE_TAB_STACK, true);
    }
    // End Vivaldi
}
