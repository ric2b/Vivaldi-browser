// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.os.Bundle;
import android.text.SpannableString;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsSettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SecuritySettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.browser.usage_stats.UsageStatsConsentDialog;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

// Vivaldi
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.vivaldi.browser.preferences.VivaldiPreferences;

/**
 * Fragment to keep track of the all the privacy related preferences.
 */
public class PrivacySettings
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    private static final String PREF_CAN_MAKE_PAYMENT = "can_make_payment";
    private static final String PREF_NETWORK_PREDICTIONS = "preload_pages";
    private static final String PREF_SECURE_DNS = "secure_dns";
    private static final String PREF_USAGE_STATS = "usage_stats_reporting";
    private static final String PREF_DO_NOT_TRACK = "do_not_track";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_SYNC_AND_SERVICES_LINK = "sync_and_services_link";
    private static final String PREF_CLEAR_BROWSING_DATA = "clear_browsing_data";
    // Vivaldi
    private static final String PREF_CLEAR_SESSION_BROWSING_DATA = "clear_session_browsing_data";
    private static final String PREF_CONTEXTUAL_SEARCH = "contextual_search";
    private static final String PREF_WEBRTC_BROADCAST_IP = "webrtc_broadcast_ip";

    private static final String[] NEW_PRIVACY_PREFERENCE_ORDER = {PREF_CLEAR_BROWSING_DATA,
            // Vivaldi
            PREF_CLEAR_SESSION_BROWSING_DATA, PREF_CONTEXTUAL_SEARCH, PREF_WEBRTC_BROADCAST_IP,
            PREF_SAFE_BROWSING, PREF_CAN_MAKE_PAYMENT, PREF_NETWORK_PREDICTIONS, PREF_USAGE_STATS,
            PREF_SECURE_DNS, PREF_DO_NOT_TRACK, PREF_SYNC_AND_SERVICES_LINK};

    private ManagedPreferenceDelegate mManagedPreferenceDelegate;

    /**
     * Vivaldi
     * Strings here must match what is defined in
     * third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.cc
     */
    private static final String WEBRTC_IP_HANDLING_POLICY_DEFAULT =
            "default";
    private static final String WEBRTC_IP_HANDLING_POLICY_DISABLE_NON_PROXIED_UDP =
            "disable_non_proxied_udp";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        PrivacyPreferencesManager privacyPrefManager = PrivacyPreferencesManager.getInstance();
        privacyPrefManager.migrateNetworkPredictionPreferences();
        SettingsUtils.addPreferencesFromResource(this, R.xml.privacy_preferences);
        assert NEW_PRIVACY_PREFERENCE_ORDER.length
                == getPreferenceScreen().getPreferenceCount()
            : "All preferences in the screen should be added in the new order list. "
                        + "If you add a new preference, please also update "
                        + "NEW_PRIVACY_PREFERENCE_ORDER.";

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_REORDERED_ANDROID)) {
            for (int i = 0; i < NEW_PRIVACY_PREFERENCE_ORDER.length; i++) {
                findPreference(NEW_PRIVACY_PREFERENCE_ORDER[i]).setOrder(i);
            }
        }

        // If the flag for adding a "Security" section UI is enabled, a "Security" section will be
        // added under this section and this section will be renamed to "Privacy and security".
        // See (go/esb-clank-dd) for more context.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SAFE_BROWSING_SECURITY_SECTION_UI)) {
            getActivity().setTitle(R.string.prefs_privacy_security);
            Preference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
            safeBrowsingPreference.setSummary(
                    SecuritySettingsFragment.getSafeBrowsingSummaryString(getContext()));
            safeBrowsingPreference.setOnPreferenceClickListener((preference) -> {
                preference.getExtras().putInt(
                        SecuritySettingsFragment.ACCESS_POINT, SettingsAccessPoint.PARENT_SETTINGS);
                return false;
            });
        } else {
            getActivity().setTitle(R.string.prefs_privacy);
            getPreferenceScreen().removePreference(findPreference(PREF_SAFE_BROWSING));
        }
        setHasOptionsMenu(true);

        mManagedPreferenceDelegate = createManagedPreferenceDelegate();

        ChromeSwitchPreference canMakePaymentPref =
                (ChromeSwitchPreference) findPreference(PREF_CAN_MAKE_PAYMENT);
        canMakePaymentPref.setOnPreferenceChangeListener(this);

        ChromeSwitchPreference networkPredictionPref =
                (ChromeSwitchPreference) findPreference(PREF_NETWORK_PREDICTIONS);
        networkPredictionPref.setChecked(
                PrivacyPreferencesManager.getInstance().getNetworkPredictionEnabled());
        networkPredictionPref.setOnPreferenceChangeListener(this);
        networkPredictionPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        Preference secureDnsPref = findPreference(PREF_SECURE_DNS);
        if (ChromeApplication.isVivaldi())
            secureDnsPref.setVisible(true);
        else
        secureDnsPref.setVisible(SecureDnsSettings.isUiEnabled());

        if (ChromeApplication.isVivaldi())
            getPreferenceScreen().removePreference(findPreference(PREF_SYNC_AND_SERVICES_LINK));
        else {
        Preference syncAndServicesLink = findPreference(PREF_SYNC_AND_SERVICES_LINK);
        syncAndServicesLink.setSummary(buildSyncAndServicesLink());
        }

        // Vivaldi
        ChromeSwitchPreference webRtcBroadcastIpPref =
                (ChromeSwitchPreference) findPreference(PREF_WEBRTC_BROADCAST_IP);
        if (webRtcBroadcastIpPref != null) {
            webRtcBroadcastIpPref.setOnPreferenceChangeListener(this);
            String policy = UserPrefs.get(Profile.getLastUsedRegularProfile()).getString(
                    Pref.WEB_RTCIP_HANDLING_POLICY);
            webRtcBroadcastIpPref.setChecked(policy.equals(WEBRTC_IP_HANDLING_POLICY_DEFAULT));
        }

        updateSummaries();
    }

    private SpannableString buildSyncAndServicesLink() {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            NoUnderlineClickableSpan syncAndServicesLink =
                    new NoUnderlineClickableSpan(getResources(), v -> {
                        settingsLauncher.launchSettingsActivity(getActivity(),
                                SyncAndServicesSettings.class,
                                SyncAndServicesSettings.createArguments(false));
                    });
            return SpanApplier.applySpans(getString(R.string.privacy_sync_and_services_link_legacy),
                    new SpanApplier.SpanInfo("<link>", "</link>", syncAndServicesLink));
        }

        NoUnderlineClickableSpan servicesLink = new NoUnderlineClickableSpan(getResources(), v -> {
            settingsLauncher.launchSettingsActivity(getActivity(), GoogleServicesSettings.class);
        });
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SYNC)
                == null) {
            // Sync is off, show the string with one link to "Google Services".
            return SpanApplier.applySpans(
                    getString(R.string.privacy_sync_and_services_link_sync_off),
                    new SpanApplier.SpanInfo("<link>", "</link>", servicesLink));
        }
        // Otherwise, show the string with both links to "Sync" and "Google Services".
        NoUnderlineClickableSpan syncLink = new NoUnderlineClickableSpan(getResources(), v -> {
            settingsLauncher.launchSettingsActivity(getActivity(), ManageSyncSettings.class,
                    ManageSyncSettings.createArguments(false));
        });
        return SpanApplier.applySpans(getString(R.string.privacy_sync_and_services_link_sync_on),
                new SpanApplier.SpanInfo("<link1>", "</link1>", syncLink),
                new SpanApplier.SpanInfo("<link2>", "</link2>", servicesLink));
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_CAN_MAKE_PAYMENT.equals(key)) {
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED, (boolean) newValue);
        } else if (PREF_NETWORK_PREDICTIONS.equals(key)) {
            PrivacyPreferencesManager.getInstance().setNetworkPredictionEnabled((boolean) newValue);
        }
        // Vivaldi
        else if (PREF_WEBRTC_BROADCAST_IP.equals(key)) {
            UserPrefs.get(Profile.getLastUsedRegularProfile()).setString(
                    Pref.WEB_RTCIP_HANDLING_POLICY, ((boolean) newValue)
                        ? WEBRTC_IP_HANDLING_POLICY_DEFAULT
                        : WEBRTC_IP_HANDLING_POLICY_DISABLE_NON_PROXIED_UDP);
        }
        return true;
    }

    @Override
    public void onResume() {
        super.onResume();
        updateSummaries();
    }

    /**
     * Updates the summaries for several preferences.
     */
    public void updateSummaries() {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());

        ChromeSwitchPreference canMakePaymentPref =
                (ChromeSwitchPreference) findPreference(PREF_CAN_MAKE_PAYMENT);
        if (canMakePaymentPref != null) {
            canMakePaymentPref.setChecked(prefService.getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED));
        }

        Preference doNotTrackPref = findPreference(PREF_DO_NOT_TRACK);
        if (doNotTrackPref != null) {
            doNotTrackPref.setSummary(prefService.getBoolean(Pref.ENABLE_DO_NOT_TRACK)
                            ? R.string.text_on
                            : R.string.text_off);
        }

        Preference secureDnsPref = findPreference(PREF_SECURE_DNS);
        if (secureDnsPref != null && secureDnsPref.isVisible()) {
            secureDnsPref.setSummary(SecureDnsSettings.getSummary(getContext()));
        }

        Preference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
        if (safeBrowsingPreference != null && safeBrowsingPreference.isVisible()) {
            safeBrowsingPreference.setSummary(
                    SecuritySettingsFragment.getSafeBrowsingSummaryString(getContext()));
        }

        Preference usageStatsPref = findPreference(PREF_USAGE_STATS);
        if (usageStatsPref != null) {
            if (BuildInfo.isAtLeastQ() && prefService.getBoolean(Pref.USAGE_STATS_ENABLED)) {
                usageStatsPref.setOnPreferenceClickListener(preference -> {
                    UsageStatsConsentDialog
                            .create(getActivity(), true,
                                    (didConfirm) -> {
                                        if (didConfirm) {
                                            updateSummaries();
                                        }
                                    })
                            .show();
                    return true;
                });
            } else {
                getPreferenceScreen().removePreference(usageStatsPref);
            }
        }
        // Vivaldi
        Preference contextualPref = findPreference(PREF_CONTEXTUAL_SEARCH);
        if (contextualPref != null)
            contextualPref.setSummary(
                    ContextualSearchManager.isContextualSearchDisabled() ?
                            R.string.text_off : R.string.text_on);

        Preference clearSessionBrowsingDataPref = findPreference(PREF_CLEAR_SESSION_BROWSING_DATA);
        if (clearSessionBrowsingDataPref != null)
            // check if the option is enabled or not
            clearSessionBrowsingDataPref.setSummary(
                    VivaldiPreferences.getSharedPreferencesManager().readBoolean(
                            VivaldiPreferences.CLEAR_SESSION_BROWSING_DATA, false) ?
                            R.string.text_on : R.string.text_off);
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            if (PREF_NETWORK_PREDICTIONS.equals(key)) {
                return PrivacyPreferencesManager.getInstance().isNetworkPredictionManaged();
            }
            return false;
        };
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedbackLauncherImpl.getInstance().show(getActivity(),
                    getString(R.string.help_context_privacy), Profile.getLastUsedRegularProfile(),
                    null);
            return true;
        }
        return false;
    }
}
