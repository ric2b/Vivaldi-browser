// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.app.creator.CreatorActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.webfeed.WebFeedFaviconFetcher;
import org.chromium.chrome.browser.feed.webfeed.WebFeedMainMenuItem;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

import org.chromium.chrome.browser.ChromeApplicationImpl;

import org.vivaldi.browser.appmenu.AppMenuIconRow;
import org.vivaldi.browser.common.VivaldiUtils;

/** An {@link AppMenuPropertiesDelegateImpl} for ChromeTabbedActivity. */
public class TabbedAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {
    AppMenuDelegate mAppMenuDelegate;
    WebFeedSnackbarController.FeedLauncher mFeedLauncher;
    ModalDialogManager mModalDialogManager;
    SnackbarManager mSnackbarManager;

    // Vivaldi
    private final ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private AppMenuIconRow mMenuBarView;

    public TabbedAppMenuPropertiesDelegate(
            Context context,
            ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector,
            ToolbarManager toolbarManager,
            View decorView,
            AppMenuDelegate appMenuDelegate,
            OneshotSupplier<LayoutStateProvider> layoutStateProvider,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            WebFeedSnackbarController.FeedLauncher feedLauncher,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            @NonNull
                    OneshotSupplier<IncognitoReauthController>
                            incognitoReauthControllerOneshotSupplier,
            Supplier<ReadAloudController> readAloudControllerSupplier) {
        super(
                context,
                activityTabProvider,
                multiWindowModeStateDispatcher,
                tabModelSelector,
                toolbarManager,
                decorView,
                layoutStateProvider,
                bookmarkModelSupplier,
                incognitoReauthControllerOneshotSupplier,
                readAloudControllerSupplier);
        mAppMenuDelegate = appMenuDelegate;
        mFeedLauncher = feedLauncher;
        mModalDialogManager = modalDialogManager;
        mSnackbarManager = snackbarManager;

        // Vivaldi
        mBookmarkModelSupplier = bookmarkModelSupplier;
    }

    private boolean shouldShowWebFeedMenuItem() {
        Tab tab = mActivityTabProvider.get();
        if (tab == null || tab.isIncognito() || OfflinePageUtils.isOfflinePage(tab)) {
            return false;
        }
        if (!FeedFeatures.isWebFeedUIEnabled(tab.getProfile())) {
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
            return R.layout.icon_row_menu_layout;

        if (shouldShowWebFeedMenuItem()) {
            return R.layout.web_feed_main_menu_item;
        }
        return 0;
    }

    @Override
    public void onFooterViewInflated(AppMenuHandler appMenuHandler, View view) {
        if (view instanceof WebFeedMainMenuItem) {
            ((WebFeedMainMenuItem) view)
                    .initialize(
                            mActivityTabProvider.get(),
                            appMenuHandler,
                            WebFeedFaviconFetcher.createDefault(),
                            mFeedLauncher,
                            mModalDialogManager,
                            mSnackbarManager,
                            CreatorActivity.class);
        }

        if (ChromeApplicationImpl.isVivaldi() && view instanceof AppMenuIconRow) {
                ((AppMenuIconRow) view).initialize(appMenuHandler,
                        mBookmarkModelSupplier,
                                                         mActivityTabProvider.get(),
                                                         mAppMenuDelegate);
            mMenuBarView = (AppMenuIconRow)view;
        }
    }

    @Override
    public int getHeaderResourceId() {
        if (!mIsTablet && VivaldiUtils.isTopToolbarOn())
            return R.layout.icon_row_menu_layout;

        return 0;
    }

    // Vivaldi
    @Override
    public void onHeaderViewInflated(AppMenuHandler appMenuHandler, View view) {
        if (!mIsTablet && VivaldiUtils.isTopToolbarOn()) {
            ((AppMenuIconRow) view).initialize(appMenuHandler,
                    mBookmarkModelSupplier,
                    mActivityTabProvider.get(),
                    mAppMenuDelegate);

            mMenuBarView = (AppMenuIconRow)view;
        }
    }

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
        return ManagedBrowserUtils.isBrowserManaged(currentTab.getProfile());
    }

    @Override
    public boolean shouldShowIconBeforeItem() {
        return true;
    }

    // Vivaldi - Passes tab loading status to Vivaldi menu bar for updating refresh icon
    @Override
    public void loadingStateChanged(boolean isLoading) {
        super.loadingStateChanged(isLoading);
        if (mMenuBarView != null) mMenuBarView.loadingStateChanged(isLoading);
    }

    // Vivaldi
    @Override
    public boolean shouldShowHeader(int maxMenuHeight) {
        if (ChromeApplicationImpl.isVivaldi()) {
            boolean showHeader = VivaldiUtils.isTopToolbarOn();
            // Check if we have a current tab; if not we are in the tab switcher and should not
            // show a header (icon row menu).
            showHeader &= (mActivityTabProvider != null
                    && mActivityTabProvider.get() != null);
            return showHeader;
        }

        return super.shouldShowHeader(maxMenuHeight);
    }
}
