// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.webfeed.WebFeedFaviconFetcher;
import org.chromium.chrome.browser.feed.webfeed.WebFeedMainMenuItem;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

import org.chromium.chrome.browser.ChromeApplicationImpl;

import org.vivaldi.browser.appmenu.AppMenuIconRowFooter;
import org.vivaldi.browser.common.VivaldiUtils;

/**
 * An {@link AppMenuPropertiesDelegateImpl} for ChromeTabbedActivity.
 */
public class TabbedAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {
    AppMenuDelegate mAppMenuDelegate;
    WebFeedSnackbarController.FeedLauncher mFeedLauncher;
    ModalDialogManager mModalDialogManager;
    SnackbarManager mSnackbarManager;

    // Vivaldi
    private final ObservableSupplier<BookmarkBridge> mBookmarkBridgeSupplier;

    public TabbedAppMenuPropertiesDelegate(Context context, ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector, ToolbarManager toolbarManager, View decorView,
            AppMenuDelegate appMenuDelegate,
            OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            OneshotSupplier<StartSurface> startSurfaceSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier,
            WebFeedSnackbarController.FeedLauncher feedLauncher,
            ModalDialogManager modalDialogManager, SnackbarManager snackbarManager) {
        super(context, activityTabProvider, multiWindowModeStateDispatcher, tabModelSelector,
                toolbarManager, decorView, overviewModeBehaviorSupplier, startSurfaceSupplier,
                bookmarkBridgeSupplier);
        mAppMenuDelegate = appMenuDelegate;
        mFeedLauncher = feedLauncher;
        mModalDialogManager = modalDialogManager;
        mSnackbarManager = snackbarManager;

        // Vivaldi
        mBookmarkBridgeSupplier = bookmarkBridgeSupplier;
    }

    private boolean shouldShowWebFeedMenuItem() {
        if (!FeedFeatures.isWebFeedUIEnabled()) {
            return false;
        }
        Tab tab = mActivityTabProvider.get();
        if (tab == null || tab.isIncognito() || OfflinePageUtils.isOfflinePage(tab)) {
            return false;
        }
        String url = tab.getOriginalUrl().getSpec();
        return url.startsWith(UrlConstants.HTTP_URL_PREFIX)
                || url.startsWith(UrlConstants.HTTPS_URL_PREFIX);
    }

    @Override
    public int getFooterResourceId() {
        // Vivaldi
        if (!mIsTablet && !VivaldiUtils.isTopToolbarOn())
            return R.layout.icon_row_menu_footer;

        if (shouldShowWebFeedMenuItem()) {
            return R.layout.web_feed_main_menu_item;
        }
        return 0;
    }

    @Override
    public void onFooterViewInflated(AppMenuHandler appMenuHandler, View view) {
        if (view instanceof WebFeedMainMenuItem) {
            ((WebFeedMainMenuItem) view)
                    .initialize(mActivityTabProvider.get(), appMenuHandler,
                            WebFeedFaviconFetcher.createDefault(), mFeedLauncher,
                            mModalDialogManager, mSnackbarManager);
        }

        if (ChromeApplicationImpl.isVivaldi() && view instanceof AppMenuIconRowFooter) {
                ((AppMenuIconRowFooter) view).initialize(appMenuHandler,
                                                         mBookmarkBridgeSupplier,
                                                         mActivityTabProvider.get(),
                                                         mAppMenuDelegate);
        }
    }

    @Override
    public int getHeaderResourceId() {
        return 0;
    }

    @Override
    public void onHeaderViewInflated(AppMenuHandler appMenuHandler, View view) {}

    @Override
    public boolean shouldShowFooter(int maxMenuHeight) {
        if (ChromeApplicationImpl.isVivaldi()) {
            boolean showFooter = !VivaldiUtils.isTopToolbarOn();
            // Check if we have a current tab; if not we are in the tab switcher and should not
            // show a footer (icon row menu).
            showFooter &= (mActivityTabProvider != null && mActivityTabProvider.get() != null);
            return showFooter;
        }

        if (shouldShowWebFeedMenuItem()) {
            return true;
        }
        return super.shouldShowFooter(maxMenuHeight);
    }

    @Override
    protected boolean shouldShowManagedByMenuItem(Tab currentTab) {
        Profile profile = Profile.fromWebContents(currentTab.getWebContents());
        return profile != null && ManagedBrowserUtils.isBrowserManaged(profile);
    }

    @Override
    public boolean shouldShowIconBeforeItem() {
        return true;
    }
}
