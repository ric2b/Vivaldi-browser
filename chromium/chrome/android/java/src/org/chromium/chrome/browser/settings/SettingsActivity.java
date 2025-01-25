// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Color;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockLauncher;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.settings.SettingsPage;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.Locale;

// Vivaldi
import android.view.WindowManager;
import androidx.appcompat.widget.SearchView;
import androidx.preference.PreferenceScreen;
import android.provider.Settings;
import android.view.inputmethod.EditorInfo;

import java.util.ArrayList;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.accessibility.settings.ChromeAccessibilitySettingsDelegate;
import org.chromium.chrome.browser.night_mode.NightModeMetrics;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;

import org.vivaldi.browser.common.VivaldiRelaunchUtils;
import org.vivaldi.browser.preferences.PreferenceSearchManager;
import org.vivaldi.browser.preferences.VivaldiPreferences;
import org.vivaldi.browser.preferences.VivaldiPreferencesBridge;

import static org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings.PREF_CAPTIONS;
import static org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings.RESET_UI_SCALE;
import static org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings.RESET_ZOOM;
import static org.chromium.chrome.browser.settings.MainSettings.PREF_ALLOW_BACKGROUND_MEDIA;


/**
 * The Chrome settings activity.
 *
 * <p>This activity displays a single {@link Fragment}, typically a {@link
 * PreferenceFragmentCompat}. There are two types of fragments shown in the activity:
 * <i>embeddable</i> fragments that implement {@link SettingsPage}, and <i>standalone</i> fragments
 * that do not implement it. Embeddable fragments may be embedded into a column in the multi-column
 * settings UI, if it is enabled and the window is large enough. Standalone fragments, in contrast,
 * are always shown as occupying the whole window.
 *
 * <p>Embeddable fragments must not modify the activity UI outside of the fragment, e.g. the
 * activity title and the action bar, because the same activity instance is shared among multiple
 * fragments as the user navigates through the settings. Instead, fragments should implement methods
 * in {@link SettingsPage} to ask the activity to update its UI appropriately.
 *
 * <p>Standalone fragments may modify the activity UI as needed. A standalone fragment is always
 * launched with a fresh settings activity instance that is not shared with other fragments.
 */
public class SettingsActivity extends ChromeBaseAppCompatActivity
        implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback, SnackbarManageable {
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final String EXTRA_SHOW_FRAGMENT = "show_fragment";

    static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";
    static final String EXTRA_SHOW_FRAGMENT_STANDALONE = "show_fragment_standalone";

    /** The current instance of SettingsActivity in the resumed state, if any. */
    private static SettingsActivity sResumedInstance;

    /** Whether this activity has been created for the first time but not yet resumed. */
    private boolean mIsNewlyCreated;

    private static boolean sActivityNotExportedChecked;

    private boolean mStandalone;
    private Profile mProfile;
    private ScrimCoordinator mScrim;
    private ManagedBottomSheetController mManagedBottomSheetController;
    private final OneshotSupplierImpl<BottomSheetController> mBottomSheetControllerSupplier =
            new OneshotSupplierImpl<>();

    private final OneshotSupplierImpl<SnackbarManager> mSnackbarManagerSupplier =
            new OneshotSupplierImpl<>();

    // This is only used on automotive.
    private @Nullable MissingDeviceLockLauncher mMissingDeviceLockLauncher;

    private static final String MAIN_FRAGMENT_TAG = "settings_main";

    // Vivaldi - make possible to scroll down search results (ref. VAB-8621)
    private final int MINIMUM_SEARCH_LENGTH = 2;
    private final ArrayList<Preference> mCurrentPrefs = new ArrayList<>();
    private PreferenceSearchManager mPreferenceSearchManager;
    private SearchView mSearchView;

    @SuppressLint("InlinedApi")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        mStandalone = getIntent().getBooleanExtra(EXTRA_SHOW_FRAGMENT_STANDALONE, false);

        setTitle(R.string.settings);
        ensureActivityNotExported();

        // The browser process must be started here because this Activity may be started explicitly
        // from Android notifications, when Android is restoring Settings after Chrome was
        // killed, or for tests. This should happen before super.onCreate() because it might
        // recreate a fragment, and a fragment might depend on the native library.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        mProfile = ProfileManager.getLastUsedRegularProfile();

        // Register fragment lifecycle callbacks before calling super.onCreate() because it may
        // create fragments if there is a saved instance state.
        FragmentManager fragmentManager = getSupportFragmentManager();
        fragmentManager.registerFragmentLifecycleCallbacks(
                new FragmentDependencyProvider(
                        this,
                        mProfile,
                        mSnackbarManagerSupplier,
                        mBottomSheetControllerSupplier,
                        getModalDialogManagerSupplier()),
                true /* recursive */);
        fragmentManager.registerFragmentLifecycleCallbacks(
                new WideDisplayPaddingApplier(), false /* recursive */);
        fragmentManager.registerFragmentLifecycleCallbacks(
                new SettingsMetricsReporter(), false /* recursive */);
        if (!mStandalone) {
            fragmentManager.registerFragmentLifecycleCallbacks(
                    new TitleUpdater(), false /* recursive */);
        }

        super.onCreate(savedInstanceState);

        setContentView(R.layout.settings_activity);

        Toolbar actionBar = findViewById(R.id.action_bar);
        setSupportActionBar(actionBar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        mIsNewlyCreated = savedInstanceState == null;

        // If savedInstanceState is non-null, then the activity is being
        // recreated and super.onCreate() has already recreated the fragment.
        if (savedInstanceState == null) {
            Fragment fragment = instantiateMainFragment(getIntent());
            fragmentManager
                    .beginTransaction()
                    .replace(R.id.content, fragment, MAIN_FRAGMENT_TAG)
                    .commit();
        }

        setStatusBarColor();
        initBottomSheet();

        mSnackbarManagerSupplier.set(
                new SnackbarManager(this, findViewById(android.R.id.content), null));

        // Vivaldi (VAB-8621)
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        // This callback is called only when the settings UI is operating in the single activity
        // mode.
        assert ChromeFeatureList.sSettingsSingleActivity.isEnabled();

        if (mStandalone) {
            // A standalone activity attempted to launch a non-standalone activity, but the intent
            // was delivered to the standalone activity itself because of FLAG_ACTIVITY_SINGLE_TOP.
            // Resend the intent without the flag to start a new activity. Bouncing activities has
            // some cost in terms of time to launch the final activity, but this is fairly a rare
            // flow anyway.
            intent.removeFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
            startActivity(intent);
            return;
        }

        Fragment fragment = instantiateMainFragment(intent);
        // TODO(b/356743945): Enable transition.
        getSupportFragmentManager()
                .beginTransaction()
                .setReorderingAllowed(true)
                .replace(R.id.content, fragment, MAIN_FRAGMENT_TAG)
                .addToBackStack(null)
                .commit();
    }

    private Fragment instantiateMainFragment(Intent intent) {
        String fragmentName = intent.getStringExtra(EXTRA_SHOW_FRAGMENT);
        if (fragmentName == null) {
            fragmentName = MainSettings.class.getName();
        }
        Bundle arguments = intent.getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);

        return Fragment.instantiate(this, fragmentName, arguments);
    }

    /** Set up the bottom sheet for this activity. */
    private void initBottomSheet() {
        ViewGroup sheetContainer = findViewById(R.id.sheet_container);
        mScrim =
                new ScrimCoordinator(
                        this,
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {
                                // TODO: Implement if status bar needs to change color with the
                                // scrim.
                            }

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {
                                // TODO: Implement if navigation bar needs to change color with the
                                // scrim.
                            }
                        },
                        (ViewGroup) sheetContainer.getParent(),
                        getColor(R.color.default_scrim_color));

        mManagedBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> mScrim,
                        (sheet) -> {},
                        getWindow(),
                        KeyboardVisibilityDelegate.getInstance(),
                        () -> sheetContainer,
                        () -> 0);
        mBottomSheetControllerSupplier.set(mManagedBottomSheetController);
    }

    // OnPreferenceStartFragmentCallback:

    @Override
    public boolean onPreferenceStartFragment(
            PreferenceFragmentCompat caller, Preference preference) {
        startFragment(preference.getFragment(), preference.getExtras());
        return true;
    }

    /**
     * Starts a new Settings activity showing the desired fragment.
     *
     * @param fragmentClass The Class of the fragment to show.
     * @param args Arguments to pass to Fragment.instantiate(), or null.
     */
    public void startFragment(@Nullable String fragmentClass, @Nullable Bundle args) {
        Intent intent = SettingsIntentUtil.createIntent(this, fragmentClass, args);
        startActivity(intent);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        // Vivaldi (VAB-8621)
        if (getMainFragment() instanceof PreferenceFragmentCompat)
            mPreferenceSearchManager = new PreferenceSearchManager(this);
        initBackPressHandler();
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Prevent the user from interacting with multiple instances of SettingsActivity at the same
        // time (e.g. in multi-instance mode on a Samsung device), which would cause many fun bugs.
        if (sResumedInstance != null
                && sResumedInstance.getTaskId() != getTaskId()
                && !mIsNewlyCreated) {
            // This activity was unpaused or recreated while another instance of SettingsActivity
            // was already showing. The existing instance takes precedence.
            finish();
        } else {
            // This activity was newly created and takes precedence over sResumedInstance.
            if (sResumedInstance != null && sResumedInstance.getTaskId() != getTaskId()) {
                sResumedInstance.finish();
            }

            sResumedInstance = this;
            mIsNewlyCreated = false;
        }

        checkForMissingDeviceLockOnAutomotive();
    }

    private void checkForMissingDeviceLockOnAutomotive() {
        if (BuildInfo.getInstance().isAutomotive) {
            if (mMissingDeviceLockLauncher == null) {
                mMissingDeviceLockLauncher =
                        new MissingDeviceLockLauncher(
                                this, mProfile, getModalDialogManagerSupplier().get());
            }
            mMissingDeviceLockLauncher.checkPrivateDataIsProtectedByDeviceLock();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        ProfileManagerUtils.flushPersistentDataForAllProfiles();
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (sResumedInstance == this) sResumedInstance = null;
    }

    @Override
    protected void onDestroy() {
        mScrim.destroy();
        super.onDestroy();
    }

    /**
     * Returns the fragment showing as this activity's main content, typically a {@link
     * PreferenceFragmentCompat}. This does not include dialogs or other {@link Fragment}s shown on
     * top of the main content.
     */
    @VisibleForTesting
    public Fragment getMainFragment() {
        return getSupportFragmentManager().findFragmentById(R.id.content);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // By default, every screen in Settings shows a "Help & feedback" menu item.
        MenuItem help =
                menu.add(
                        Menu.NONE,
                        R.id.menu_id_general_help,
                        Menu.CATEGORY_SECONDARY,
                        R.string.menu_help);
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        getResources(), R.drawable.ic_help_and_feedback, getTheme()));
        //Vivaldi
        if (BuildConfig.IS_VIVALDI && getMainFragment() instanceof PreferenceFragmentCompat) {
            menu.clear();
            MenuItem search =
                    menu.add(Menu.NONE,
                            R.id.settings_id_search,
                            Menu.CATEGORY_SECONDARY,
                            R.string.menu_help);
            search.setIcon(
                    TraceEventVectorDrawableCompat.create(
                            getResources(), R.drawable.settings_ic_search, getTheme()));
            search.setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
            mSearchView = new SearchView(this);
            search.setActionView(mSearchView);
            mSearchView.setOnSearchClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    mCurrentPrefs.clear();
                    PreferenceScreen searchResultScreen =
                            ((PreferenceFragmentCompat)getMainFragment())
                                    .getPreferenceManager()
                                    .getPreferenceScreen();
                    for (int i=0; i<searchResultScreen.getPreferenceCount(); i++) {
                        mCurrentPrefs.add(searchResultScreen.getPreference(i));
                    }
                }
            });
            mSearchView.setMaxWidth(Integer.MAX_VALUE);
            mSearchView.setImeOptions(mSearchView.getImeOptions()| EditorInfo.IME_FLAG_NO_EXTRACT_UI);
            mSearchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
                @Override
                public boolean onQueryTextSubmit(String s) {
                    ArrayList<Preference> searchResult =
                            mPreferenceSearchManager.searchForPreference(s);

                    PreferenceScreen searchResultScreen =
                            ((PreferenceFragmentCompat)getMainFragment())
                                    .getPreferenceManager()
                                    .getPreferenceScreen();
                    searchResultScreen.removeAll();
                    for (Preference preference : searchResult) {
                        searchResultScreen.addPreference(preference);
                    }
                    return false;
                }

                @Override
                public boolean onQueryTextChange(String s) {
                    if (s.length() >= MINIMUM_SEARCH_LENGTH) {
                        displayPreferences(mPreferenceSearchManager.searchForPreference(s));
                    } else if (s.length() == 0) {
                        rebuildCurrentPrefs(mCurrentPrefs);
                    }
                    return false;
                }
            });

            mSearchView.setOnCloseListener(new SearchView.OnCloseListener() {
                @Override
                public boolean onClose() {
                    rebuildCurrentPrefs(mCurrentPrefs);
                    return false;
                }
            });
        }
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        if (menu.size() == 1) {
            MenuItem item = menu.getItem(0);
            if (item.getIcon() != null) item.setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        }
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Fragment mainFragment = getMainFragment();
        if (mainFragment != null && mainFragment.onOptionsItemSelected(item)) {
            return true;
        }

        if (item.getItemId() == android.R.id.home) {
            if (ChromeFeatureList.sSettingsSingleActivity.isEnabled()) {
                FragmentManager fragmentManager = getSupportFragmentManager();
                if (fragmentManager.getBackStackEntryCount() == 0) {
                    finish();
                } else {
                    fragmentManager.popBackStack();
                }
            } else {
                finish();
            }
            return true;
        } else if (item.getItemId() == R.id.menu_id_general_help) {
            HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                    .show(this, getString(R.string.help_context_settings), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void initBackPressHandler() {
        // Handlers registered last will be called first.
        registerMainFragmentBackPressHandler();
        registerBottomSheetBackPressHandler();
    }

    private void registerMainFragmentBackPressHandler() {
        Fragment activeFragment = getMainFragment();
        if (activeFragment instanceof BackPressHandler) {
            BackPressHelper.create(
                    activeFragment.getViewLifecycleOwner(),
                    getOnBackPressedDispatcher(),
                    (BackPressHandler) activeFragment,
                    SecondaryActivity.SETTINGS);
        }
    }

    private void registerBottomSheetBackPressHandler() {
        BackPressHelper.create(
                this,
                getOnBackPressedDispatcher(),
                mBottomSheetControllerSupplier.get().getBottomSheetBackPressHandler(),
                SecondaryActivity.SETTINGS);
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManagerSupplier.get();
    }

    private void ensureActivityNotExported() {
        if (sActivityNotExportedChecked) return;
        sActivityNotExportedChecked = true;
        try {
            ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), 0);
            // If SettingsActivity is exported, then it's vulnerable to a fragment injection
            // exploit:
            // http://securityintelligence.com/new-vulnerability-android-framework-fragment-injection
            if (activityInfo.exported) {
                throw new IllegalStateException("SettingsActivity must not be exported.");
            }
        } catch (NameNotFoundException ex) {
            // Something terribly wrong has happened.
            throw new RuntimeException(ex);
        }
    }

    /** Set device status bar to match the activity background color, if supported. */
    private void setStatusBarColor() {
        // On P+, the status bar color is set via the XML theme.
        if (VERSION.SDK_INT >= Build.VERSION_CODES.P
                && !BuildInfo.getInstance().isAutomotive
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(this)) {
            return;
        }

        if (UiUtils.isSystemUiThemingDisabled()) return;

        // Use transparent color, so the AppBarLayout can color the status bar on scroll.
        UiUtils.setStatusBarColor(getWindow(), Color.TRANSPARENT);

        // Set status bar icon color according to background color.
        UiUtils.setStatusBarIconColor(
                getWindow().getDecorView().getRootView(),
                getResources().getBoolean(R.bool.window_light_status_bar));
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    private class TitleUpdater extends FragmentManager.FragmentLifecycleCallbacks {
        private final Callback<String> mSetTitleCallback =
                (title) -> {
                    if (title == null) {
                        title = "";
                    }
                    setTitle(title);
                };

        private ObservableSupplier<String> mCurrentPageTitle;

        @Override
        public void onFragmentResumed(
                @NonNull FragmentManager fragmentManager, @NonNull Fragment fragment) {
            if (!MAIN_FRAGMENT_TAG.equals(fragment.getTag())) {
                return;
            }

            // TitleUpdater is enabled only when the fragment implements SettingsPage.
            SettingsPage settingsFragment = (SettingsPage) fragment;

            if (mCurrentPageTitle != null) {
                mCurrentPageTitle.removeObserver(mSetTitleCallback);
            }
            mCurrentPageTitle = settingsFragment.getPageTitle();
            mCurrentPageTitle.addObserver(mSetTitleCallback);
        }
    }

    private class WideDisplayPaddingApplier extends FragmentManager.FragmentLifecycleCallbacks {
        @Override
        public void onFragmentViewCreated(
                @NonNull FragmentManager fragmentManager,
                @NonNull Fragment fragment,
                @NonNull View view,
                @Nullable Bundle savedInstanceState) {
            if (MAIN_FRAGMENT_TAG.equals(fragment.getTag())) {
                // Apply the wide display style after the main fragment is committed since its views
                // (particularly a recycler view) are not accessible before the transaction
                // completes.
                WideDisplayPadding.apply(fragment, SettingsActivity.this);
            }
        }
    }

    private static class SettingsMetricsReporter
            extends FragmentManager.FragmentLifecycleCallbacks {
        @Override
        public void onFragmentAttached(
                @NonNull FragmentManager fragmentManager,
                @NonNull Fragment fragment,
                @NonNull Context context) {
            if (!MAIN_FRAGMENT_TAG.equals(fragment.getTag())) {
                return;
            }

            String className = fragment.getClass().getSimpleName();
            RecordHistogram.recordSparseHistogram(
                    "Settings.FragmentAttached", className.hashCode());
            // Log hashCode to easily add new class names to enums.xml.
            Log.d(
                    "SettingsActivity",
                    String.format(
                            Locale.ENGLISH,
                            "Settings.FragmentAttached: <int value=\"%d\" label=\"%s\"/>",
                            className.hashCode(),
                            className));
        }
    }

    // Vivaldi (VAB-8621)
    @SuppressLint("RestrictedApi")
    private void rebuildCurrentPrefs(ArrayList<Preference> prefs) {
        displayPreferences(prefs);
    }
    // Vivaldi (gabriel@vivaldi.com) use for display preferences.
    private void displayPreferences(ArrayList<Preference> preferences) {
        PreferenceScreen searchResultScreen =
                ((PreferenceFragmentCompat)getMainFragment())
                        .getPreferenceManager()
                        .getPreferenceScreen();
        searchResultScreen.removeAll();
        for (Preference preference : preferences) {
            if (preference.getParent() != null)
                preference.getParent().removePreference(preference);
            if (preference instanceof ProfileDependentSetting) {
                ((ProfileDependentSetting) preference).setProfile(mProfile);
            }
            String prefStr = preference.getKey();
            if (prefStr != null) {
                switch (prefStr) {
                    case "passwords":
                        preference.setOnPreferenceClickListener(pref -> {
                            PasswordManagerLauncher.showPasswordSettings(
                                    mSearchView.getContext(),
                                    mProfile,
                                    ManagePasswordsReferrer.CHROME_SETTINGS,
                                    getModalDialogManagerSupplier(),
                                    /* managePasskeys= */ false);
                            return true;
                        });
                        break;
                    case "ui_theme":
                        preference.getExtras()
                                .putInt(
                                        ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                                        NightModeMetrics.ThemeSettingsEntry.SETTINGS);
                        break;
                    case PREF_CAPTIONS:
                        preference.setOnPreferenceClickListener(
                                pref -> {
                                    Intent intent = new Intent(Settings.ACTION_CAPTIONING_SETTINGS);

                                    // Open the activity in a new task because the back button on the caption
                                    // settings page navigates to the previous settings page instead of Chrome.
                                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                                    startActivity(intent);

                                    return true;
                                });
                        break;
                    case RESET_UI_SCALE:
                        preference.setOnPreferenceClickListener(pref -> {

                            int ui_dpi = VivaldiPreferences.getSharedPreferencesManager().readInt(
                                    VivaldiPreferences.UI_SCALE_DEFAULT_VALUE, 440);
                            VivaldiPreferences.getSharedPreferencesManager().writeInt(
                                    VivaldiPreferences.UI_SCALE_VALUE, ui_dpi);
                            // Vivaldi Ref. AUTO-116.
                            // NOTE(simonb@vivaldi.com): Update UI if UI has been changed
                            recreate();
                            return true;
                        });
                        break;
                    case RESET_ZOOM:
                        preference.setOnPreferenceClickListener(pref -> {
                            PageZoomUtils.setDefaultZoomBySeekBarValue(
                                    new ChromeAccessibilitySettingsDelegate(mProfile)
                                            .getBrowserContextHandle(),
                                    PageZoomUtils.PAGE_ZOOM_DEFAULT_SEEK_VALUE);
                            return true;
                        });
                        break;
                    case PREF_ALLOW_BACKGROUND_MEDIA:
                        preference.setOnPreferenceChangeListener((pref, newValue) -> {
                            VivaldiPreferencesBridge vivaldiPrefs = new VivaldiPreferencesBridge();
                            vivaldiPrefs.setBackgroundMediaPlaybackAllowed((boolean) newValue);
                            VivaldiRelaunchUtils.showRelaunchDialog(mSearchView.getContext(), null);
                            return true;
                        });
                        break;
                }
            }
            searchResultScreen.addPreference(preference);
        }
    }
}
