// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.HintlessActivityTabObserver;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.TabSwitcherButtonCoordinator;
import org.chromium.chrome.browser.toolbar.TabSwitcherButtonView;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.vivaldi.browser.toolbar.BackButton;
import org.vivaldi.browser.toolbar.ForwardButton;
import org.vivaldi.browser.toolbar.PanelButton;

/**
 * The coordinator for the browsing mode bottom toolbar. This class has two primary components,
 * an Android view that handles user actions and a composited texture that draws when the controls
 * are being scrolled off-screen. The Android version does not draw unless the controls offset is 0.
 */
public class BrowsingModeBottomToolbarCoordinator {
    /** The mediator that handles events from outside the browsing mode bottom toolbar. */
    private final BrowsingModeBottomToolbarMediator mMediator;

    /** The home button that lives in the bottom toolbar. */
    private final HomeButton mHomeButton;

    /** The share button that lives in the bottom toolbar. */
    private final ShareButton mShareButton;

    /** The search accelerator that lives in the bottom toolbar. */
    private final SearchAccelerator mSearchAccelerator;

    /** Vivaldi buttons in the bottom toolbar. */
    private final BackButton mBackButton;
    private final ForwardButton mForwardButton;
    private final PanelButton mPanelButton;

    /** The tab switcher button component that lives in the bottom toolbar. */
    private final TabSwitcherButtonCoordinator mTabSwitcherButtonCoordinator;

    /** The menu button that lives in the browsing mode bottom toolbar. */
    private final MenuButton mMenuButton;

    /**
     * Build the coordinator that manages the browsing mode bottom toolbar.
     * @param root The root {@link View} for locating the views to inflate.
     * @param tabProvider The {@link ActivityTabProvider} used for making the IPH.
     * @param homeButtonListener The {@link OnClickListener} for the home button.
     * @param searchAcceleratorListener The {@link OnClickListener} for the search accelerator.
     * @param shareButtonListener The {@link OnClickListener} for the share button.
     */
    BrowsingModeBottomToolbarCoordinator(View root, ActivityTabProvider tabProvider,
            OnClickListener homeButtonListener, OnClickListener searchAcceleratorListener,
            OnClickListener shareButtonListener, OnLongClickListener tabSwitcherLongClickListener) {
        BrowsingModeBottomToolbarModel model = new BrowsingModeBottomToolbarModel();

        final ViewGroup toolbarRoot = root.findViewById(R.id.bottom_toolbar_browsing);

        PropertyModelChangeProcessor.create(
                model, toolbarRoot, new BrowsingModeBottomToolbarViewBinder());

        mMediator = new BrowsingModeBottomToolbarMediator(model);
        if (ChromeApplication.isVivaldi()) {
            mHomeButton = toolbarRoot.findViewById(R.id.vivaldi_home_button);
        } else {
        mHomeButton = toolbarRoot.findViewById(R.id.home_button);
        mHomeButton.setWrapperView(toolbarRoot.findViewById(R.id.home_button_wrapper));
        }
        mHomeButton.setOnClickListener(homeButtonListener);
        mHomeButton.setActivityTabProvider(tabProvider);

        mShareButton = toolbarRoot.findViewById(R.id.share_button);
        mShareButton.setWrapperView(toolbarRoot.findViewById(R.id.share_button_wrapper));
        mShareButton.setOnClickListener(shareButtonListener);
        mShareButton.setActivityTabProvider(tabProvider);

        mSearchAccelerator = toolbarRoot.findViewById(R.id.vivaldi_search_accelerator);
        if (!ChromeApplication.isVivaldi())
        mSearchAccelerator.setWrapperView(
                toolbarRoot.findViewById(R.id.search_accelerator_wrapper));
        mSearchAccelerator.setOnClickListener(searchAcceleratorListener);

        mTabSwitcherButtonCoordinator = new TabSwitcherButtonCoordinator(toolbarRoot);
        // TODO(amaralp): Make this adhere to MVC framework.
        TabSwitcherButtonView tabSwitcherButtonView =
                toolbarRoot.findViewById(R.id.tab_switcher_button);
        tabSwitcherButtonView.setWrapperView(
                toolbarRoot.findViewById(R.id.tab_switcher_button_wrapper));

        if (tabSwitcherLongClickListener != null) {
            tabSwitcherButtonView.setOnLongClickListener(
                (view) -> tabSwitcherLongClickListener.onLongClick(tabSwitcherButtonView));
        }
        mMenuButton = toolbarRoot.findViewById(R.id.menu_button_wrapper);
        mMenuButton.setWrapperView(toolbarRoot.findViewById(R.id.labeled_menu_button_wrapper));

        /** Vivaldi */
        ChromeTabbedActivity activity = (ChromeTabbedActivity)root.getContext();
        mBackButton = toolbarRoot.findViewById(R.id.back_button);
        mBackButton.setOnClickListener((View v) -> activity.getToolbarManager().back());
        mForwardButton = toolbarRoot.findViewById(R.id.forward_button);
        mForwardButton.setOnClickListener((View v) -> activity.getToolbarManager().forward());

        mPanelButton = toolbarRoot.findViewById(R.id.panel_button);
        mPanelButton.setOnClickListener(
                (View v) -> BookmarkUtils.showBookmarkManager(activity));

        toolbarRoot.findViewById(R.id.home_button_wrapper).setVisibility(View.GONE);
        toolbarRoot.findViewById(R.id.share_button_wrapper).setVisibility(View.GONE);
        toolbarRoot.findViewById(R.id.search_accelerator_wrapper).setVisibility(View.GONE);
        toolbarRoot.findViewById(R.id.tab_switcher_button_wrapper).setVisibility(View.GONE);
        toolbarRoot.findViewById(R.id.labeled_menu_button_wrapper).setVisibility(View.GONE);
        /** Vivaldi */

        tabProvider.addObserverAndTrigger(new HintlessActivityTabObserver() {
            @Override
            public void onActivityTabChanged(Tab tab) {
                if (tab == null) return;
                final Tracker tracker = TrackerFactory.getTrackerForProfile(tab.getProfile());
                tracker.addOnInitializedCallback((ready) -> mMediator.showIPH(tab.getActivity(),
                        mSearchAccelerator, tracker));
                tabProvider.removeObserver(this);
            }
        });
    }

    /**
     * Initialize the bottom toolbar with the components that had native initialization
     * dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     * @param tabSwitcherListener An {@link OnClickListener} that is triggered when the
     *                            tab switcher button is clicked.
     * @param menuButtonHelper An {@link AppMenuButtonHelper} that is triggered when the
     *                         menu button is clicked.
     * @param overviewModeBehavior The overview mode manager.
     * @param tabCountProvider Updates the tab count number in the tab switcher button.
     * @param themeColorProvider Notifies components when theme color changes.
     * @param incognitoStateProvider Notifies components when incognito state changes.
     */
    void initializeWithNative(OnClickListener tabSwitcherListener,
            AppMenuButtonHelper menuButtonHelper, OverviewModeBehavior overviewModeBehavior,
            TabCountProvider tabCountProvider, ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider) {
        mMediator.setOverviewModeBehavior(overviewModeBehavior);
        mMediator.setThemeColorProvider(themeColorProvider);

        mHomeButton.setThemeColorProvider(themeColorProvider);
        mShareButton.setThemeColorProvider(themeColorProvider);
        mSearchAccelerator.setThemeColorProvider(themeColorProvider);
        mSearchAccelerator.setIncognitoStateProvider(incognitoStateProvider);

        /** Vivaldi */
        mBackButton.setThemeColorProvider(themeColorProvider);
        mForwardButton.setThemeColorProvider(themeColorProvider);
        mPanelButton.setThemeColorProvider(themeColorProvider);
        /** Vivaldi */

        mTabSwitcherButtonCoordinator.setTabSwitcherListener(tabSwitcherListener);
        mTabSwitcherButtonCoordinator.setThemeColorProvider(themeColorProvider);
        mTabSwitcherButtonCoordinator.setTabCountProvider(tabCountProvider);

        assert menuButtonHelper != null;
        mMenuButton.setAppMenuButtonHelper(menuButtonHelper);
        mMenuButton.setThemeColorProvider(themeColorProvider);
    }

    /**
     * Show the update badge over the bottom toolbar's app menu.
     */
    void showAppMenuUpdateBadge() {
        mMenuButton.showAppMenuUpdateBadgeIfAvailable(true);
    }

    /**
     * Remove the update badge.
     */
    void removeAppMenuUpdateBadge() {
        mMenuButton.removeAppMenuUpdateBadge(true);
    }

    /**
     * @return Whether the update badge is showing.
     */
    boolean isShowingAppMenuUpdateBadge() {
        return mMenuButton.isShowingAppMenuUpdateBadge();
    }

    /**
     * @return The browsing mode bottom toolbar's menu button.
     */
    MenuButton getMenuButton() {
        return mMenuButton;
    }

    /** Vivaldi. Update back button according to available back history. */
    void updateBackButtonVisibility(boolean canGoBack) {
        mBackButton.setEnabled(canGoBack);
    }

    /** Vivaldi. Update forward button according to available forward history. */
    void updateForwardButtonVisibility(boolean canGoForward) {
        mForwardButton.setEnabled(canGoForward);
    }

    /** Vivaldi. Update center button depending on showing home page or not **/
    void updateCenterButton(boolean showSearch) {
        mSearchAccelerator.setVisibility(showSearch ? View.VISIBLE : View.GONE);
        mHomeButton.setVisibility(showSearch ? View.GONE : View.VISIBLE);
    }

    /**
     * Clean up any state when the browsing mode bottom toolbar is destroyed.
     */
    public void destroy() {
        mMediator.destroy();
        mHomeButton.destroy();
        mShareButton.destroy();
        mSearchAccelerator.destroy();
        mTabSwitcherButtonCoordinator.destroy();
        mMenuButton.destroy();

        /** Vivaldi */
        mBackButton.destroy();
        mForwardButton.destroy();
        mPanelButton.destroy();
        /** Vivaldi */
    }
}
