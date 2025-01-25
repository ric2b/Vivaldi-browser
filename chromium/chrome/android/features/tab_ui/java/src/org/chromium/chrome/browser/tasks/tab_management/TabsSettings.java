// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Fragment for tab related configurations to Chrome. */
public class TabsSettings extends ChromeBaseSettingsFragment {
    // Must match key in tabs_settings.xml
    @VisibleForTesting
    static final String PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH =
            "auto_open_synced_tab_groups_switch";

    @VisibleForTesting
    static final String PREF_TAB_ARCHIVE_SETTINGS = "archive_settings_entrypoint";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.tabs_settings);
        getActivity().setTitle(R.string.tabs_settings_title);

        configureAutoOpenSyncedTabGroupsSwitch();
    }

    @Override
    public void onStart() {
        super.onStart();
        configureTabArchiveSettings();
    }

    private void configureAutoOpenSyncedTabGroupsSwitch() {
        ChromeSwitchPreference autoOpenSyncedTabGroupsSwitch =
                (ChromeSwitchPreference) findPreference(PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH);
        boolean isTabGroupSyncAutoOpenConfigurable =
                ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID)
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH);
        if (!isTabGroupSyncAutoOpenConfigurable) {
            autoOpenSyncedTabGroupsSwitch.setVisible(false);
            return;
        }

        PrefService prefService = UserPrefs.get(getProfile());
        boolean isEnabled = prefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS);
        autoOpenSyncedTabGroupsSwitch.setChecked(isEnabled);
        autoOpenSyncedTabGroupsSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean enabled = (boolean) newValue;
                    prefService.setBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS, enabled);
                    RecordHistogram.recordBooleanHistogram(
                            "Tabs.AutoOpenSyncedTabGroupsSwitch.ToggledToState", enabled);
                    return true;
                });
    }

    private void configureTabArchiveSettings() {
        Preference tabArchiveSettingsPref = findPreference(PREF_TAB_ARCHIVE_SETTINGS);
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_TAB_DECLUTTER)) {
            tabArchiveSettingsPref.setVisible(false);
            return;
        }

        TabArchiveSettings archiveSettings =
                new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        if (archiveSettings.getArchiveEnabled()) {
            int tabArchiveTimeDeltaDays = archiveSettings.getArchiveTimeDeltaDays();
            tabArchiveSettingsPref.setSummary(
                    getResources()
                            .getQuantityString(
                                    R.plurals.archive_settings_summary,
                                    tabArchiveTimeDeltaDays,
                                    tabArchiveTimeDeltaDays));
        } else {
            tabArchiveSettingsPref.setSummary(
                    getResources().getString(R.string.archive_settings_time_delta_never));
        }
        archiveSettings.destroy();
    }
}
