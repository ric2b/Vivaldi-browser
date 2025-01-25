// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Bundle;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Fragment for tab archive configurations to Chrome. */
public class TabArchiveSettingsFragment extends ChromeBaseSettingsFragment {
    // Must match key in tab_archive_settings.xml
    static final String PREF_TAB_ARCHIVE_ALLOW_AUTODELETE = "tab_archive_allow_autodelete";
    static final String INACTIVE_TIMEDELTA_PREF = "tab_archive_time_delta";

    private TabArchiveSettings mArchiveSettings;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mArchiveSettings = new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        SettingsUtils.addPreferencesFromResource(this, R.xml.tab_archive_settings);
        getActivity().setTitle(R.string.archive_settings_title);

        configureSettings();
    }

    private void configureSettings() {
        // Archive time delta radio button.
        TabArchiveTimeDeltaPreference archiveTimeDeltaPreference =
                (TabArchiveTimeDeltaPreference) findPreference(INACTIVE_TIMEDELTA_PREF);
        archiveTimeDeltaPreference.initialize(mArchiveSettings);

        // Auto delete switch.
        ChromeSwitchPreference enableAutoDeleteSwitch =
                (ChromeSwitchPreference) findPreference(PREF_TAB_ARCHIVE_ALLOW_AUTODELETE);
        int autoDeleteTimeDeltaDays = mArchiveSettings.getAutoDeleteTimeDeltaDays();
        enableAutoDeleteSwitch.setTitle(
                getResources()
                        .getQuantityString(
                                R.plurals.archive_settings_allow_autodelete_title,
                                autoDeleteTimeDeltaDays,
                                autoDeleteTimeDeltaDays));
        enableAutoDeleteSwitch.setChecked(mArchiveSettings.isAutoDeleteEnabled());
        enableAutoDeleteSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean enabled = (boolean) newValue;
                    mArchiveSettings.setAutoDeleteEnabled(enabled);
                    RecordHistogram.recordBooleanHistogram(
                            "Tabs.ArchiveSettings.AutoDeleteEnabled", enabled);
                    return true;
                });
    }
}
