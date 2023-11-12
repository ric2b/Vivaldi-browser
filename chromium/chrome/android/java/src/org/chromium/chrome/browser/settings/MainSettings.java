// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.autofill.settings.SettingsLauncherHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.HashMap;
import java.util.Map;

// Vivaldi
import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.text.TextUtils;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.ui.base.DeviceFormFactor;

import org.vivaldi.browser.common.VivaldiDefaultBrowserUtils;
import org.vivaldi.browser.common.VivaldiRelaunchUtils;
import org.vivaldi.browser.common.VivaldiUtils;
import org.vivaldi.browser.preferences.AdsAndTrackerPreference;
import org.vivaldi.browser.preferences.AutomaticCloseTabsMainPreference;
import org.vivaldi.browser.preferences.NewTabPositionMainPreference;
import org.vivaldi.browser.preferences.ScreenLockSwitch;
import org.vivaldi.browser.preferences.StartPageModePreference;
import org.vivaldi.browser.preferences.StatusBarVisibilityPreference;
import org.vivaldi.browser.preferences.VivaldiPreferences;
import org.vivaldi.browser.preferences.VivaldiPreferencesBridge;
import org.vivaldi.browser.preferences.VivaldiSyncPreference;
import org.vivaldi.browser.prompts.AddWidgetBottomSheet;
import org.vivaldi.browser.prompts.AddWidgetPromptHandler;

/**
 * The main settings screen, shown when the user first opens Settings.
 */
public class MainSettings extends PreferenceFragmentCompat
        implements TemplateUrlService.LoadListener, SyncService.SyncStateChangedListener,
                   SigninManager.SignInStateObserver, ProfileDependentSetting {
    public static final String PREF_SYNC_PROMO = "sync_promo";
    public static final String PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION =
            "account_and_google_services_section";
    public static final String PREF_SIGN_IN = "sign_in";
    public static final String PREF_MANAGE_SYNC = "manage_sync";
    public static final String PREF_GOOGLE_SERVICES = "google_services";
    public static final String PREF_SEARCH_ENGINE = "search_engine";
    public static final String PREF_PASSWORDS = "passwords";
    public static final String PREF_HOMEPAGE = "homepage";
    public static final String PREF_TOOLBAR_SHORTCUT = "toolbar_shortcut";
    public static final String PREF_UI_THEME = "ui_theme";
    public static final String PREF_PRIVACY = "privacy";
    public static final String PREF_SAFETY_CHECK = "safety_check";
    public static final String PREF_NOTIFICATIONS = "notifications";
    public static final String PREF_DOWNLOADS = "downloads";
    public static final String PREF_DEVELOPER = "developer";
    public static final String PREF_AUTOFILL_OPTIONS = "autofill_options";
    public static final String PREF_AUTOFILL_ADDRESSES = "autofill_addresses";
    public static final String PREF_AUTOFILL_PAYMENTS = "autofill_payment_methods";

    public static final String PREF_VIVALDI_SYNC = "vivaldi_sync";
    public static final String PREF_VIVALDI_GAME = "vivaldi_game";
    public static final String PREF_STATUS_BAR_VISIBILITY = "status_bar_visibility";
    public static final String PREF_DOUBLE_TAP_BACK_TO_EXIT = "double_tap_back_to_exit";
    public static final String PREF_ALLOW_BACKGROUND_MEDIA = "allow_background_media";
    public static final String PREF_ALWAYS_SHOW_DESKTOP_SITE = "always_show_desktop_site";

    private final ManagedPreferenceDelegate mManagedPreferenceDelegate;
    protected final Map<String, Preference> mAllPreferences = new HashMap<>();
    private ChromeBasePreference mManageSync;
    private @Nullable PasswordCheck mPasswordCheck;
    private Profile mProfile;
    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    private VivaldiSyncPreference mVivaldiSyncPreference;

    public MainSettings() {
        setHasOptionsMenu(true);
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        createPreferences();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.settings);
        if (!ChromeApplicationImpl.isVivaldi())
        mPasswordCheck = PasswordCheckFactory.getOrCreate(new SettingsLauncherImpl());
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // The component should only be destroyed when the activity has been closed by the user
        // (e.g. by pressing on the back button) and not when the activity is temporarily destroyed
        // by the system.
        if (getActivity().isFinishing() && mPasswordCheck != null) PasswordCheckFactory.destroy();
    }

    @Override
    public void onStart() {
        super.onStart();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        if (signinManager.isSigninSupported(/*requireUpdatedPlayServices=*/false)) {
            signinManager.addSignInStateObserver(this);
        }
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }
        mVivaldiSyncPreference.registerForUpdates();
    }

    @Override
    public void onStop() {
        super.onStop();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        if (signinManager.isSigninSupported(/*requireUpdatedPlayServices=*/false)) {
            signinManager.removeSignInStateObserver(this);
        }
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }
        mVivaldiSyncPreference.unregisterForUpdates();
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    private void createPreferences() {
        if (ChromeApplicationImpl.isVivaldi())
            SettingsUtils.addPreferencesFromResource(this, R.xml.vivaldi_main_preferences);
        else
        SettingsUtils.addPreferencesFromResource(this, R.xml.main_preferences);

        cachePreferences();

        if (ChromeApplicationImpl.isVivaldi()) {
            removePreferenceIfPresent(VivaldiPreferences.SCREEN_LOCK);
            if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
                removePreferenceIfPresent(VivaldiPreferences.SHOW_TAB_STRIP);
                removePreferenceIfPresent(VivaldiPreferences.APP_MENU_BAR_SETTING);
            }
            if (BuildConfig.IS_OEM_AUTOMOTIVE_BUILD) {
                removePreferenceIfPresent(VivaldiPreferences.ALWAYS_SHOW_CONTROLS);
                removePreferenceIfPresent(PREF_DOWNLOADS); // Ref. POLE-20
                removePreferenceIfPresent(PREF_STATUS_BAR_VISIBILITY); // Ref. POLE-26
                removePreferenceIfPresent(PREF_DOUBLE_TAP_BACK_TO_EXIT); // Ref. POLE-30
                // Remove if OEM runs phone UI (Mercedes co driver display).
                removePreferenceIfPresent(VivaldiPreferences.APP_MENU_BAR_SETTING);
                removePreferenceIfPresent(VivaldiPreferences.RATE_VIVALDI);
                removePreferenceIfPresent(VivaldiPreferences.ADD_VIVALDI_SEARCH_WIDGET);
                if (VivaldiUtils.driverDistractionHandlingEnabled()) {
                    // Incompatible with driver distraction handling, remove.
                    removePreferenceIfPresent(PREF_ALLOW_BACKGROUND_MEDIA);
                }
                if (BuildConfig.IS_OEM_GAS_BUILD) {
                    // Only for generic AAOS builds for now.
                    addPreferenceIfAbsent(VivaldiPreferences.SCREEN_LOCK);
                }
            }
            // Remove for Mercedes. Ref. VAB-7254.
            if (BuildConfig.IS_OEM_MERCEDES_BUILD) {
                removePreferenceIfPresent(PREF_VIVALDI_GAME);
                removePreferenceIfPresent(VivaldiPreferences.SET_AS_DEFAULT_BROWSER);
            }
        }

        updateAutofillPreferences();

        // TODO(crbug.com/1373451): Remove the passwords managed subtitle for local and UPM
        // unenrolled users who can see it directly in the context of the setting.
        setManagedPreferenceDelegateForPreference(PREF_PASSWORDS);
        setManagedPreferenceDelegateForPreference(PREF_SEARCH_ENGINE);

        // Vivaldi: Ref. VAB-7740, remove notifications settings for Mercedes.
        if (BuildConfig.IS_OEM_MERCEDES_BUILD)
            removePreferenceIfPresent(PREF_NOTIFICATIONS);
        else
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // If we are on Android O+ the Notifications preference should lead to the Android
            // Settings notifications page.
            Intent intent = new Intent();
            intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE,
                    ContextUtils.getApplicationContext().getPackageName());
            PackageManager pm = getActivity().getPackageManager();
            if (intent.resolveActivity(pm) != null) {
                Preference notifications = findPreference(PREF_NOTIFICATIONS);
                notifications.setOnPreferenceClickListener(preference -> {
                    startActivity(intent);
                    // We handle the click so the default action isn't triggered.
                    return true;
                });
            } else {
                removePreferenceIfPresent(PREF_NOTIFICATIONS);
            }
        } else {
            // The per-website notification settings page can be accessed from Site
            // Settings, so we don't need to show this here.
            getPreferenceScreen().removePreference(findPreference(PREF_NOTIFICATIONS));
        }

        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.getForProfile(mProfile);
        if (!templateUrlService.isLoaded()) {
            templateUrlService.registerLoadListener(this);
            templateUrlService.load();
        }

        if (!ChromeApplicationImpl.isVivaldi())
        new AdaptiveToolbarStatePredictor(null).recomputeUiState(uiState -> {
            // We don't show the toolbar shortcut settings page if disabled from finch.
            if (uiState.canShowUi) return;
            getPreferenceScreen().removePreference(findPreference(PREF_TOOLBAR_SHORTCUT));
        });

        if (ChromeApplicationImpl.isVivaldi()) {
            // Handle changes to reverse search suggestion order preference
            Preference reverseSearchSuggestion =
                    findPreference(VivaldiPreferences.REVERSE_SEARCH_SUGGESTION);
            if (reverseSearchSuggestion != null)
                reverseSearchSuggestion.setOnPreferenceChangeListener((preference, o) -> {
                    VivaldiRelaunchUtils.showRelaunchDialog(getContext(), null);
                    return true;
                });

            // Handle set as default browser setting
            ChromeSwitchPreference setDefaultBrowserPref =
                    findPreference(VivaldiPreferences.SET_AS_DEFAULT_BROWSER);
            if (setDefaultBrowserPref != null) {
                setDefaultBrowserPref.setOnPreferenceClickListener(preference -> {
                    if (setDefaultBrowserPref.isChecked()) {
                        setDefaultBrowserPref.setChecked(false);
                        VivaldiDefaultBrowserUtils.setAsDefaultBrowser(getActivity());
                    } else {
                        setDefaultBrowserPref.setChecked(true);
                        VivaldiDefaultBrowserUtils.openDefaultAppsSettings(getActivity());
                    }
                    return true;
                });
            }

            // Handle background media playback toggle change
            ChromeSwitchPreference allowBackgroundMediaPref =
                    findPreference(PREF_ALLOW_BACKGROUND_MEDIA);
            if (allowBackgroundMediaPref != null) {
                allowBackgroundMediaPref.setOnPreferenceChangeListener((preference, newValue) -> {
                    VivaldiPreferencesBridge vivaldiPrefs = new VivaldiPreferencesBridge();
                    vivaldiPrefs.setBackgroundMediaPlaybackAllowed((boolean) newValue);
                    VivaldiRelaunchUtils.showRelaunchDialog(getContext(), null);
                    return true;
                });
            }

            ChromeSwitchPreference alwaysShowDesktopSitePref =
                    findPreference(PREF_ALWAYS_SHOW_DESKTOP_SITE);
            if (alwaysShowDesktopSitePref != null) {
                boolean desktopModeEnabled =
                        VivaldiPreferences.getSharedPreferencesManager().readBoolean(
                                VivaldiPreferences.ALWAYS_SHOW_DESKTOP,
                                BuildConfig.IS_OEM_MERCEDES_BUILD);

                alwaysShowDesktopSitePref.setChecked(desktopModeEnabled);
            }

            ChromeSwitchPreference stayInBrowserPref =
                    findPreference(VivaldiPreferences.STAY_IN_BROWSER);
            if (stayInBrowserPref != null) {
                boolean stayInBrowser =
                        VivaldiPreferences.getSharedPreferencesManager().readBoolean(
                                VivaldiPreferences.STAY_IN_BROWSER,
                                BuildConfig.IS_OEM_MERCEDES_BUILD);

                stayInBrowserPref.setChecked(stayInBrowser);
            }
        }
    }

    /**
     * Stores all preferences in memory so that, if they needed to be added/removed from the
     * PreferenceScreen, there would be no need to reload them from 'main_preferences.xml'.
     */
    private void cachePreferences() {
        int preferenceCount = getPreferenceScreen().getPreferenceCount();
        for (int index = 0; index < preferenceCount; index++) {
            Preference preference = getPreferenceScreen().getPreference(index);
            mAllPreferences.put(preference.getKey(), preference);
        }
        mManageSync = (ChromeBasePreference) findPreference(PREF_MANAGE_SYNC);

        mVivaldiSyncPreference = (VivaldiSyncPreference) mAllPreferences.get(PREF_VIVALDI_SYNC);
    }

    private void setManagedPreferenceDelegateForPreference(String key) {
        ChromeBasePreference chromeBasePreference = (ChromeBasePreference) mAllPreferences.get(key);
        if (chromeBasePreference != null)
        chromeBasePreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
    }

    private void updatePreferences() {
        if (IdentityServicesProvider.get().getSigninManager(mProfile).isSigninSupported(
                    /*requireUpdatedPlayServices=*/false)) {
            addPreferenceIfAbsent(PREF_SIGN_IN);
        } else {
            removePreferenceIfPresent(PREF_SIGN_IN);
        }

        updateManageSyncPreference();
        updateSearchEnginePreference();
        updateAutofillPreferences();

        Preference homepagePref = addPreferenceIfAbsent(PREF_HOMEPAGE);
        setOnOffSummary(homepagePref, HomepageManager.isHomepageEnabled());

        if (NightModeUtils.isNightModeSupported()) {
            addPreferenceIfAbsent(PREF_UI_THEME)
                    .getExtras()
                    .putInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                            ThemeSettingsEntry.SETTINGS);
        } else {
            removePreferenceIfPresent(PREF_UI_THEME);
        }

        if (!ChromeApplicationImpl.isVivaldi() &&
                DeveloperSettings.shouldShowDeveloperSettings()) {
            addPreferenceIfAbsent(PREF_DEVELOPER);
        } else {
            removePreferenceIfPresent(PREF_DEVELOPER);
        }

        // Vivaldi: Update summaries.
        Preference pref = getPreferenceScreen().findPreference("new_tab_position");
        pref.setSummary(NewTabPositionMainPreference.updateSummary());
        pref = getPreferenceScreen().findPreference(PREF_STATUS_BAR_VISIBILITY);
        if (pref != null)
            pref.setSummary(StatusBarVisibilityPreference.updateSummary());
        pref = getPreferenceScreen().findPreference("start_page");
        pref.setSummary(StartPageModePreference.updateSummary());
        pref = getPreferenceScreen().findPreference("ads_and_tracker");
        pref.setSummary(AdsAndTrackerPreference.updateSummary());
        SharedPreferencesManager.getInstance().addObserver(key -> {
            if (TextUtils.equals(key, VivaldiPreferences.ADDRESS_BAR_TO_BOTTOM))
                updateSummary();
        });
        pref = getPreferenceScreen().findPreference("automatic_close_tabs");
        pref.setSummary(AutomaticCloseTabsMainPreference.updateSummary());
        updateSummary();
        pref = findPreference(VivaldiPreferences.SET_AS_DEFAULT_BROWSER);
        if (pref != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ((ChromeSwitchPreference) pref).setChecked(
                    VivaldiDefaultBrowserUtils.checkIfVivaldiDefaultBrowser(getActivity()));
        }

        // Vivaldi: Set toggle for background media.
        pref = findPreference(PREF_ALLOW_BACKGROUND_MEDIA);
        if (pref != null) {
            VivaldiPreferencesBridge vivaldiPrefs = new VivaldiPreferencesBridge();
            ((ChromeSwitchPreference) pref)
                    .setChecked(vivaldiPrefs.isBackgroundMediaPlaybackAllowed());
        }

        // Vivaldi
        if (BuildConfig.IS_OEM_GAS_BUILD) {
            ScreenLockSwitch screenLockSwitch = findPreference(VivaldiPreferences.SCREEN_LOCK);
            if (screenLockSwitch != null) {
                screenLockSwitch.updateSwitch();
            }
        }
        // Handling the home button here.
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext());
        String homeButton = "show_start_page_icon";
        if (isTablet)
            removePreferenceIfPresent(homeButton);
        else
            getPreferenceScreen()
                    .findPreference(homeButton)
                    .setEnabled(HomepageManager.isHomepageEnabled());

        // Handling adding Vivaldi search widget to home screen
        pref = findPreference(VivaldiPreferences.ADD_VIVALDI_SEARCH_WIDGET);
        if (pref != null) {
            // Don't show setting if below Android O OS or if the widget is already added
            if (BuildConfig.IS_OEM_AUTOMOTIVE_BUILD
                    || Build.VERSION.SDK_INT < Build.VERSION_CODES.O
                    || AddWidgetPromptHandler.hasVivaldiSearchWidget(getContext()))
                removePreferenceIfPresent(VivaldiPreferences.ADD_VIVALDI_SEARCH_WIDGET);
            else {
                pref.setOnPreferenceClickListener(preference -> {
                    AppWidgetManager appWidgetManager =
                            getContext().getSystemService(AppWidgetManager.class);
                    ComponentName appWidgetProvider =
                            new ComponentName(getContext(), SearchWidgetProvider.class);
                    if (appWidgetManager != null
                            && appWidgetManager.isRequestPinAppWidgetSupported()) {
                        Bundle bundle = new Bundle();
                        bundle.putBoolean(AddWidgetBottomSheet.SHOW_REPLY, true);
                        Intent pinnedWidgetCallbackIntent =
                                new Intent(getContext(), SearchWidgetProvider.class);
                        pinnedWidgetCallbackIntent.putExtras(bundle);
                        PendingIntent successCallback = PendingIntent.getBroadcast(getContext(), 0,
                                pinnedWidgetCallbackIntent,
                                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

                        appWidgetManager.requestPinAppWidget(
                                appWidgetProvider, null, successCallback);
                    }
                    return true;
                });
            }
        }

        // Handling set as default browser promo card view
        pref = findPreference("default_browser_promo");
        if (pref != null) {
            if (VivaldiDefaultBrowserUtils.checkIfVivaldiDefaultBrowser(getContext())
                    || BuildConfig.IS_OEM_AUTOMOTIVE_BUILD
                    || VivaldiPreferences.getSharedPreferencesManager().readBoolean(
                    VivaldiPreferences.DEFAULT_BROWSER_PROMO_CANCELLED, false))
                removePreferenceIfPresent("default_browser_promo");
        }
    }

    private Preference addPreferenceIfAbsent(String key) {
        Preference preference = getPreferenceScreen().findPreference(key);
        if (preference == null) getPreferenceScreen().addPreference(mAllPreferences.get(key));
        return mAllPreferences.get(key);
    }

    private void removePreferenceIfPresent(String key) {
        Preference preference = getPreferenceScreen().findPreference(key);
        if (preference != null) getPreferenceScreen().removePreference(preference);
    }

    private void updateManageSyncPreference() {
        String primaryAccountName = CoreAccountInfo.getEmailFrom(
                IdentityServicesProvider.get().getIdentityManager(mProfile).getPrimaryAccountInfo(
                        ConsentLevel.SIGNIN));
        boolean showManageSync = primaryAccountName != null;
        mManageSync.setVisible(showManageSync);
        if (!showManageSync) return;

        boolean isSyncConsentAvailable =
                IdentityServicesProvider.get().getIdentityManager(mProfile).getPrimaryAccountInfo(
                        ConsentLevel.SYNC)
                != null;
        mManageSync.setIcon(SyncSettingsUtils.getSyncStatusIcon(getActivity()));
        mManageSync.setSummary(SyncSettingsUtils.getSyncStatusSummary(getActivity()));
        mManageSync.setOnPreferenceClickListener(pref -> {
            Context context = getContext();
            if (SyncServiceFactory.getForProfile(mProfile).isSyncDisabledByEnterprisePolicy()) {
                SyncSettingsUtils.showSyncDisabledByAdministratorToast(context);
            } else if (isSyncConsentAvailable) {
                SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                settingsLauncher.launchSettingsActivity(context, ManageSyncSettings.class);
            } else {
                SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                        context, SigninAccessPoint.SETTINGS_SYNC_OFF_ROW, primaryAccountName);
            }
            return true;
        });
    }

    private void updateSearchEnginePreference() {
        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.getForProfile(mProfile);
        if (!templateUrlService.isLoaded()) {
            ChromeBasePreference searchEnginePref =
                    (ChromeBasePreference) findPreference(PREF_SEARCH_ENGINE);
            searchEnginePref.setEnabled(false);
            return;
        }

        String defaultSearchEngineName = null;
        TemplateUrl dseTemplateUrl = templateUrlService.getDefaultSearchEngineTemplateUrl();
        if (dseTemplateUrl != null) defaultSearchEngineName = dseTemplateUrl.getShortName();

        Preference searchEnginePreference = findPreference(PREF_SEARCH_ENGINE);
        searchEnginePreference.setEnabled(true);
        searchEnginePreference.setSummary(defaultSearchEngineName);
    }

    private void updateAutofillPreferences() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)) {
            addPreferenceIfAbsent(PREF_AUTOFILL_OPTIONS);
            Preference preference = findPreference(PREF_AUTOFILL_OPTIONS);
            preference.setFragment(null);
            preference.setOnPreferenceClickListener(unused -> {
                new SettingsLauncherImpl().launchSettingsActivity(getContext(),
                        AutofillOptionsFragment.class,
                        AutofillOptionsFragment.createRequiredArgs(
                                AutofillOptionsReferrer.SETTINGS));
                return true; // Means event is consumed.
            });
        } else {
            removePreferenceIfPresent(PREF_AUTOFILL_OPTIONS);
        }
        findPreference(PREF_AUTOFILL_PAYMENTS)
                .setOnPreferenceClickListener(preference
                        -> SettingsLauncherHelper.showAutofillCreditCardSettings(getActivity()));
        findPreference(PREF_AUTOFILL_ADDRESSES)
                .setOnPreferenceClickListener(preference
                        -> SettingsLauncherHelper.showAutofillProfileSettings(getActivity()));
        findPreference(PREF_PASSWORDS).setOnPreferenceClickListener(preference -> {
            PasswordManagerLauncher.showPasswordSettings(getActivity(),
                    ManagePasswordsReferrer.CHROME_SETTINGS, mModalDialogManagerSupplier,
                    /*managePasskeys=*/false);
            return true;
        });
    }

    private void setOnOffSummary(Preference pref, boolean isOn) {
        pref.setSummary(isOn ? R.string.text_on : R.string.text_off);
    }

    // SigninManager.SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        // After signing in or out of a managed account, preferences may change or become enabled
        // or disabled.
        new Handler().post(() -> updatePreferences());
    }

    @Override
    public void onSignedOut() {
        updatePreferences();
    }

    // TemplateUrlService.LoadListener implementation.
    @Override
    public void onTemplateUrlServiceLoaded() {
        TemplateUrlServiceFactory.getForProfile(mProfile).unregisterLoadListener(this);
        updateSearchEnginePreference();
    }

    @Override
    public void syncStateChanged() {
        updateManageSyncPreference();
        updateAutofillPreferences();
    }

    public ManagedPreferenceDelegate getManagedPreferenceDelegateForTest() {
        return mManagedPreferenceDelegate;
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.getForProfile(mProfile)
                            .isDefaultSearchManaged();
                }
                if (PREF_PASSWORDS.equals(preference.getKey())) {
                    return UserPrefs.get(mProfile).isManagedPreference(
                            Pref.CREDENTIALS_ENABLE_SERVICE);
                }
                return false;
            }

            @Override
            public boolean isPreferenceClickDisabled(Preference preference) {
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.getForProfile(mProfile)
                            .isDefaultSearchManaged();
                }
                if (PREF_PASSWORDS.equals(preference.getKey())) {
                    return false;
                }
                return isPreferenceControlledByPolicy(preference)
                        || isPreferenceControlledByCustodian(preference);
            }
        };
    }

    public void setModalDialogManagerSupplier(
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    // Vivaldi
    private void updateSummary() {
        // Update Address bar gesture summary based on its position
        getPreferenceScreen().findPreference(
                VivaldiPreferences.ENABLE_ADDRESS_BAR_SWIPE_GESTURE).setSummary(
                        VivaldiUtils.isTopToolbarOn()
                        ? R.string.address_bar_swipe_gesture_down_summary
                        : R.string.address_bar_swipe_gesture_up_summary);
    }
}
