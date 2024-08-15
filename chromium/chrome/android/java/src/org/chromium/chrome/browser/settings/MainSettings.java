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

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.autofill.settings.SettingsLauncherHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.password_manager.settings.PasswordsPreference;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.SigninAndHistoryOptInActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.sync.settings.SyncPromoPreference;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
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
import android.content.SharedPreferences;
import android.text.TextUtils;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.bookmarks.BookmarkFeatures;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.ui.base.DeviceFormFactor;

import org.vivaldi.browser.adblock.AdblockManager;
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

/** The main settings screen, shown when the user first opens Settings. */
public class MainSettings extends ChromeBaseSettingsFragment
        implements TemplateUrlService.LoadListener,
                SyncService.SyncStateChangedListener,
                SigninManager.SignInStateObserver {
    public static final String PREF_SYNC_PROMO = "sync_promo";
    public static final String PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION =
            "account_and_google_services_section";
    public static final String PREF_SIGN_IN = "sign_in";
    public static final String PREF_MANAGE_SYNC = "manage_sync";
    public static final String PREF_GOOGLE_SERVICES = "google_services";
    public static final String PREF_SEARCH_ENGINE = "search_engine";
    public static final String PREF_PASSWORDS = "passwords";
    public static final String PREF_TABS = "tabs";
    public static final String PREF_HOMEPAGE = "homepage";
    public static final String PREF_HOME_MODULES_CONFIG = "home_modules_config";
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
    public static final String PREF_PLUS_ADDRESSES = "plus_addresses";
    public static final String PREF_SAFETY_HUB = "safety_hub";

    public static final String PREF_VIVALDI_SYNC = "vivaldi_sync";
    public static final String PREF_VIVALDI_GAME = "vivaldi_game";
    public static final String PREF_STATUS_BAR_VISIBILITY = "status_bar_visibility";
    public static final String PREF_DOUBLE_TAP_BACK_TO_EXIT = "double_tap_back_to_exit";
    public static final String PREF_ALLOW_BACKGROUND_MEDIA = "allow_background_media";
    public static final String PREF_PWA_DISABLED = "pwa_disabled";
    public static final String PREF_ALWAYS_SHOW_DESKTOP_SITE = "always_show_desktop_site";

    private final Map<String, Preference> mAllPreferences = new HashMap<>();

    private ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private ChromeBasePreference mManageSync;
    private @Nullable PasswordCheck mPasswordCheck;
    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    private VivaldiSyncPreference mVivaldiSyncPreference;
    private SharedPreferences.OnSharedPreferenceChangeListener mPrefsListener;

    public MainSettings() {
        setHasOptionsMenu(true);
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
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(getProfile());
        if (signinManager.isSigninSupported(/* requireUpdatedPlayServices= */ false)) {
            signinManager.addSignInStateObserver(this);
        }
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }
        mVivaldiSyncPreference.registerForUpdates();

        // Vivaldi
        mPrefsListener = (sharedPrefs, key) -> {
            if (TextUtils.equals(key, VivaldiPreferences.ADDRESS_BAR_TO_BOTTOM))
                updateSummary();
        };
        ContextUtils.getAppSharedPreferences().registerOnSharedPreferenceChangeListener(
                mPrefsListener);
    }

    @Override
    public void onStop() {
        super.onStop();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(getProfile());
        if (signinManager.isSigninSupported(/* requireUpdatedPlayServices= */ false)) {
            signinManager.removeSignInStateObserver(this);
        }
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
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

    private void createPreferences() {
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();

        if (ChromeApplicationImpl.isVivaldi())
            SettingsUtils.addPreferencesFromResource(this, R.xml.vivaldi_main_preferences);
        else
        SettingsUtils.addPreferencesFromResource(this, R.xml.main_preferences);

        ProfileDataCache profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(getContext());
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(getProfile());
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(getProfile());

        SyncPromoPreference syncPromoPreference = findPreference(PREF_SYNC_PROMO);
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(R.string.sign_in_to_chrome).build();
        syncPromoPreference.initialize(
                profileDataCache,
                accountManagerFacade,
                signinManager,
                identityManager,
                new SyncPromoController(
                        getProfile(),
                        bottomSheetStrings,
                        SigninAccessPoint.SETTINGS,
                        SyncConsentActivityLauncherImpl.get(),
                        SigninAndHistoryOptInActivityLauncherImpl.get()));

        SignInPreference signInPreference = findPreference(PREF_SIGN_IN);
        signInPreference.initialize(getProfile(), profileDataCache, accountManagerFacade);

        cachePreferences();

        if (ChromeApplicationImpl.isVivaldi()) {
            removePreferenceIfPresent(VivaldiPreferences.SCREEN_LOCK);
            removePreferenceIfPresent(PREF_SYNC_PROMO);
            if (!BookmarkFeatures.isNewStartPageEnabled()) {
                removePreferenceIfPresent(VivaldiPreferences.SHOW_CUSTOMIZE_SPEEDDIAL_PREF);
                removePreferenceIfPresent(VivaldiPreferences.SHOW_SPEEDDIAL_PREF);
            }
            if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
                removePreferenceIfPresent(VivaldiPreferences.APP_MENU_BAR_SETTING);
            }
            if (BuildConfig.IS_OEM_AUTOMOTIVE_BUILD) {
                removePreferenceIfPresent(PREF_DOWNLOADS); // Ref. POLE-20
                removePreferenceIfPresent(PREF_STATUS_BAR_VISIBILITY); // Ref. POLE-26
                removePreferenceIfPresent(PREF_DOUBLE_TAP_BACK_TO_EXIT); // Ref. POLE-30
                // Remove if OEM runs phone UI (Mercedes co driver display).
                removePreferenceIfPresent(VivaldiPreferences.APP_MENU_BAR_SETTING);
                removePreferenceIfPresent(VivaldiPreferences.ADD_VIVALDI_SEARCH_WIDGET);
                if (VivaldiUtils.driverDistractionHandlingEnabled()) {
                    // Incompatible with driver distraction handling, remove.
                    removePreferenceIfPresent(PREF_ALLOW_BACKGROUND_MEDIA);
                }
                if (VivaldiUtils.isVivaldiScreenLockEnabled()) {
                    addPreferenceIfAbsent(VivaldiPreferences.SCREEN_LOCK);
                }
            }
            // Remove for Mercedes. Ref. VAB-7254.
            if (BuildConfig.IS_OEM_MERCEDES_BUILD) {
                removePreferenceIfPresent(PREF_VIVALDI_GAME);
                removePreferenceIfPresent(VivaldiPreferences.SET_AS_DEFAULT_BROWSER);
            }
            // Rate Vivaldi should be available for GAS builds.
            if (BuildConfig.IS_OEM_AUTOMOTIVE_BUILD && !BuildConfig.IS_OEM_GAS_BUILD) {
                removePreferenceIfPresent(VivaldiPreferences.RATE_VIVALDI);
            }
        }

        updateAutofillPreferences();
        updatePlusAddressesPreference();

        // TODO(crbug.com/40242060): Remove the passwords managed subtitle for local and UPM
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
            intent.putExtra(
                    Settings.EXTRA_APP_PACKAGE,
                    ContextUtils.getApplicationContext().getPackageName());
            PackageManager pm = getActivity().getPackageManager();
            if (intent.resolveActivity(pm) != null) {
                Preference notifications = findPreference(PREF_NOTIFICATIONS);
                notifications.setOnPreferenceClickListener(
                        preference -> {
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

        TemplateUrlService templateUrlService =
                TemplateUrlServiceFactory.getForProfile(getProfile());
        if (!templateUrlService.isLoaded()) {
            templateUrlService.registerLoadListener(this);
            templateUrlService.load();
        }

        if (!ChromeApplicationImpl.isVivaldi())
        new AdaptiveToolbarStatePredictor(getProfile(), null)
                .recomputeUiState(
                        uiState -> {
                            // We don't show the toolbar shortcut settings page if disabled from
                            // finch.
                            if (uiState.canShowUi) return;
                            getPreferenceScreen()
                                    .removePreference(findPreference(PREF_TOOLBAR_SHORTCUT));
                        });

        if (!BuildConfig.IS_VIVALDI)
        if (BuildInfo.getInstance().isAutomotive) {
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_CHECK));
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_HUB));
        } else if (!ChromeFeatureList.sSafetyHub.isEnabled()) {
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_HUB));
        } else {
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_CHECK));
        }

        if (ChromeApplicationImpl.isVivaldi()) {
            if (BuildConfig.IS_OEM_AUTOMOTIVE_BUILD) {
                Preference safetyCheck = findPreference(PREF_SAFETY_CHECK);
                if (safetyCheck != null)
                    getPreferenceScreen().removePreference(safetyCheck);
            }
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

            // Handle the PWA Disabled Toggle
            ChromeSwitchPreference pwaDisabledPref = findPreference(PREF_PWA_DISABLED);
            if (pwaDisabledPref != null) {
                if (BuildConfig.IS_OEM_AUTOMOTIVE_BUILD) {
                    getPreferenceScreen().removePreference(pwaDisabledPref);
                } else {
                    pwaDisabledPref.setOnPreferenceChangeListener((preference, newValue) -> {
                        VivaldiPreferencesBridge vivaldiPrefs = new VivaldiPreferencesBridge();
                        vivaldiPrefs.setPWADisabled((boolean) newValue);
                        return true;
                    });
                }
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

            ChromeSwitchPreference showSpeedDialPref =
                    findPreference(VivaldiPreferences.SHOW_SPEEDDIAL_PREF);
            if (showSpeedDialPref != null) {
                boolean showSpeedDial =
                        VivaldiPreferences.getSharedPreferencesManager().readBoolean(
                                VivaldiPreferences.SHOW_SPEEDDIAL_PREF,
                                true);

                showSpeedDialPref.setChecked(showSpeedDial);
            }

            ChromeSwitchPreference showCustomizePref =
                    findPreference(VivaldiPreferences.SHOW_CUSTOMIZE_SPEEDDIAL_PREF);
            if (showCustomizePref != null) {
                boolean showCustomize =
                        VivaldiPreferences.getSharedPreferencesManager().readBoolean(
                                VivaldiPreferences.SHOW_CUSTOMIZE_SPEEDDIAL_PREF,
                                true);

                showCustomizePref.setChecked(showCustomize);
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
        if (IdentityServicesProvider.get()
                .getSigninManager(getProfile())
                .isSigninSupported(/* requireUpdatedPlayServices= */ false)) {
            addPreferenceIfAbsent(PREF_SIGN_IN);
        } else {
            removePreferenceIfPresent(PREF_SIGN_IN);
        }

        updateManageSyncPreference();
        updateSearchEnginePreference();
        updateAutofillPreferences();
        updatePlusAddressesPreference();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID)) {
            addPreferenceIfAbsent(PREF_TABS);
        } else {
            removePreferenceIfPresent(PREF_TABS);
        }

        Preference homepagePref = addPreferenceIfAbsent(PREF_HOMEPAGE);
        setOnOffSummary(homepagePref, HomepageManager.getInstance().isHomepageEnabled());

        if (HomeModulesConfigManager.getInstance().hasModuleShownInSettings()) {
            addPreferenceIfAbsent(PREF_HOME_MODULES_CONFIG);
        } else {
            removePreferenceIfPresent(PREF_HOME_MODULES_CONFIG);
        }

        if (NightModeUtils.isNightModeSupported()) {
            addPreferenceIfAbsent(PREF_UI_THEME)
                    .getExtras()
                    .putInt(
                            ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
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
        if (AdblockManager.getInstance().isLoaded()) {
            pref = getPreferenceScreen().findPreference("ads_and_tracker");
            pref.setSummary(AdsAndTrackerPreference.updateSummary());
        } else {
            removePreferenceIfPresent("ads_and_tracker");
        }

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

        // Vivaldi: Set toggle for PWA installs.
        pref = findPreference(PREF_PWA_DISABLED);
        if (pref != null) {
            VivaldiPreferencesBridge vivaldiPrefs = new VivaldiPreferencesBridge();
            ((ChromeSwitchPreference) pref)
                    .setChecked(vivaldiPrefs.isPWADisabled());
        }

        // Vivaldi
        if (VivaldiUtils.isVivaldiScreenLockEnabled()) {
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
                    .setEnabled(HomepageManager.getInstance().isHomepageEnabled());

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
        // Vivaldi Ref. AUTO-116. NOTE(simonb@vivaldi.com): Update UI if UI has been changed
        if(VivaldiPreferences.getSharedPreferencesManager().
                readInt(VivaldiPreferences.UI_SCALE_VALUE) != getResources().
                getConfiguration().densityDpi && VivaldiPreferences.getSharedPreferencesManager().
                readInt(VivaldiPreferences.UI_SCALE_VALUE) != 0){
            getActivity().recreate();
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
        String primaryAccountName =
                CoreAccountInfo.getEmailFrom(
                        IdentityServicesProvider.get()
                                .getIdentityManager(getProfile())
                                .getPrimaryAccountInfo(ConsentLevel.SIGNIN));

        // TODO(crbug.com/40067770): Remove usage of ConsentLevel.SYNC after kSync users are
        // migrated to kSignin in phase 3. See ConsentLevel::kSync documentation for details.
        boolean isSyncConsentAvailable =
                IdentityServicesProvider.get()
                                .getIdentityManager(getProfile())
                                .getPrimaryAccountInfo(ConsentLevel.SYNC)
                        != null;
        boolean showManageSync =
                primaryAccountName != null
                        && (!ChromeFeatureList.isEnabled(
                                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                                || isSyncConsentAvailable);
        mManageSync.setVisible(showManageSync);
        if (!showManageSync) return;

        mManageSync.setIcon(SyncSettingsUtils.getSyncStatusIcon(getActivity(), getProfile()));
        mManageSync.setSummary(SyncSettingsUtils.getSyncStatusSummary(getActivity(), getProfile()));

        mManageSync.setOnPreferenceClickListener(
                pref -> {
                    Context context = getContext();
                    if (SyncServiceFactory.getForProfile(getProfile())
                            .isSyncDisabledByEnterprisePolicy()) {
                        SyncSettingsUtils.showSyncDisabledByAdministratorToast(context);
                    } else if (isSyncConsentAvailable) {
                        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                        settingsLauncher.launchSettingsActivity(context, ManageSyncSettings.class);
                    } else {
                        // TODO(crbug.com/40067770): Remove after rolling out
                        // REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS.
                        assert !ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
                        SyncConsentActivityLauncherImpl.get()
                                .launchActivityForPromoDefaultFlow(
                                        context,
                                        SigninAccessPoint.SETTINGS_SYNC_OFF_ROW,
                                        primaryAccountName);
                    }
                    return true;
                });
    }

    private void updateSearchEnginePreference() {
        TemplateUrlService templateUrlService =
                TemplateUrlServiceFactory.getForProfile(getProfile());
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
                        AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)) {
            addPreferenceIfAbsent(PREF_AUTOFILL_OPTIONS);
            Preference preference = findPreference(PREF_AUTOFILL_OPTIONS);
            preference.setFragment(null);
            preference.setOnPreferenceClickListener(
                    unused -> {
                        new SettingsLauncherImpl()
                                .launchSettingsActivity(
                                        getContext(),
                                        AutofillOptionsFragment.class,
                                        AutofillOptionsFragment.createRequiredArgs(
                                                AutofillOptionsReferrer.SETTINGS));
                        return true; // Means event is consumed.
                    });
        } else {
            removePreferenceIfPresent(PREF_AUTOFILL_OPTIONS);
        }
        findPreference(PREF_AUTOFILL_PAYMENTS)
                .setOnPreferenceClickListener(
                        preference ->
                                SettingsLauncherHelper.showAutofillCreditCardSettings(
                                        getActivity()));
        findPreference(PREF_AUTOFILL_ADDRESSES)
                .setOnPreferenceClickListener(
                        preference ->
                                SettingsLauncherHelper.showAutofillProfileSettings(getActivity()));
        PasswordsPreference passwordsPreference = findPreference(PREF_PASSWORDS);
        passwordsPreference.setProfile(getProfile());
        passwordsPreference.setOnPreferenceClickListener(
                preference -> {
                    PasswordManagerLauncher.showPasswordSettings(
                            getActivity(),
                            getProfile(),
                            ManagePasswordsReferrer.CHROME_SETTINGS,
                            mModalDialogManagerSupplier,
                            /* managePasskeys= */ false);
                    return true;
                });
    }

    private void updatePlusAddressesPreference() {
        // TODO(crbug.com/40276862): Replace with a static string once name is finalized.
        String title =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.PLUS_ADDRESSES_ENABLED, "settings-label");
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PLUS_ADDRESSES_ENABLED)
                && !title.isEmpty()) {
            addPreferenceIfAbsent(PREF_PLUS_ADDRESSES);
            Preference preference = findPreference(PREF_PLUS_ADDRESSES);
            preference.setTitle(title);
            preference.setOnPreferenceClickListener(
                    unused -> {
                        String url =
                                ChromeFeatureList.getFieldTrialParamByFeature(
                                        ChromeFeatureList.PLUS_ADDRESSES_ENABLED, "manage-url");
                        CustomTabActivity.showInfoPage(getContext(), url);
                        return true;
                    });
        } else {
            removePreferenceIfPresent(PREF_PLUS_ADDRESSES);
        }
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
        TemplateUrlServiceFactory.getForProfile(getProfile()).unregisterLoadListener(this);
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
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.getForProfile(getProfile())
                            .isDefaultSearchManaged();
                }
                if (PREF_PASSWORDS.equals(preference.getKey())) {
                    return UserPrefs.get(getProfile())
                            .isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE);
                }
                return false;
            }

            @Override
            public boolean isPreferenceClickDisabled(Preference preference) {
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.getForProfile(getProfile())
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
