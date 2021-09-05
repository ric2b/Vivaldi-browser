// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;

// Vivaldi
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.NavigationPopup;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A {@link LinearLayout} that displays a horizontal row of icons for page actions.
 */
public class AppMenuIconRowFooter extends LinearLayout implements View.OnClickListener,
        View.OnLongClickListener {
    private AppMenuHandler mAppMenuHandler;
    private AppMenuDelegate mAppMenuDelegate;

    private ImageButton mForwardButton;
    private ImageButton mBookmarkButton;
    private ImageButton mDownloadButton;
    private ImageButton mPageInfoButton;
    private ImageButton mReloadButton;

    private ImageButton mBackwardButton;

    public AppMenuIconRowFooter(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    // Vivaldi
    private Tab mCurrentTab;
    private NavigationPopup mNavigationPopup;

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mForwardButton = findViewById(R.id.forward_menu_id);
        mForwardButton.setOnClickListener(this);
        // Vivaldi
        mForwardButton.setOnLongClickListener(this);

        mBookmarkButton = findViewById(R.id.bookmark_this_page_id);
        mBookmarkButton.setOnClickListener(this);

        if (ChromeApplication.isVivaldi()) {
            mBackwardButton = findViewById(R.id.backward_menu_id);
            if (mBackwardButton != null) {
                mBackwardButton.setOnClickListener(this);
                mBackwardButton.setOnLongClickListener(this);
            }

            ImageButton homeButton = findViewById(R.id.home_menu_id);
            if (homeButton != null)
                homeButton.setOnClickListener(this);
        } else {
        mDownloadButton = findViewById(R.id.offline_page_id);
        mDownloadButton.setOnClickListener(this);

        mPageInfoButton = findViewById(R.id.info_menu_id);
        mPageInfoButton.setOnClickListener(this);
        }
        mReloadButton = findViewById(R.id.reload_menu_id);
        mReloadButton.setOnClickListener(this);

        // ImageView tinting doesn't work with LevelListDrawable, use Drawable tinting instead.
        // See https://crbug.com/891593 for details.
        Drawable icon = AppCompatResources.getDrawable(getContext(), R.drawable.btn_reload_stop);
        DrawableCompat.setTintList(icon,
                AppCompatResources.getColorStateList(
                        getContext(), R.color.default_icon_color_tint_list));
        mReloadButton.setImageDrawable(icon);
    }

    /**
     * Initializes the icons, setting enabled state, drawables, and content descriptions.
     * @param appMenuHandler The {@link AppMenu} that contains the icon row.
     * @param bookmarkBridge The {@link BookmarkBridge} used to retrieve information about
     *                       bookmarks.
     * @param currentTab The current activity {@link Tab}.
     * @param appMenuDelegate The AppMenuDelegate to handle options item selection.
     */
    public void initialize(AppMenuHandler appMenuHandler, BookmarkBridge bookmarkBridge,
            Tab currentTab, AppMenuDelegate appMenuDelegate) {
        mAppMenuHandler = appMenuHandler;
        mAppMenuDelegate = appMenuDelegate;

        mForwardButton.setEnabled(currentTab.canGoForward());

        updateBookmarkMenuItem(bookmarkBridge, currentTab);

        if (ChromeApplication.isVivaldi())
            if (mBackwardButton != null) mBackwardButton.setEnabled(currentTab.canGoBack());
        else
        mDownloadButton.setEnabled(DownloadUtils.isAllowedToDownloadPage(currentTab));

        loadingStateChanged(currentTab.isLoading());

        // Vivaldi
        mCurrentTab = currentTab;
    }

    @Override
    public void onClick(View v) {
        mAppMenuDelegate.onOptionsItemSelected(v.getId(), null);
        mAppMenuHandler.hideAppMenu();
    }

    /**
     * Called when the current tab's load state  has changed.
     * @param isLoading Whether the tab is currently loading.
     */
    public void loadingStateChanged(boolean isLoading) {
        mReloadButton.getDrawable().setLevel(isLoading
                        ? getResources().getInteger(R.integer.reload_button_level_stop)
                        : getResources().getInteger(R.integer.reload_button_level_reload));
        mReloadButton.setContentDescription(isLoading
                        ? getContext().getString(R.string.accessibility_btn_stop_loading)
                        : getContext().getString(R.string.accessibility_btn_refresh));
    }

    private void updateBookmarkMenuItem(BookmarkBridge bookmarkBridge, Tab currentTab) {
        // Vivaldi:
        // If the bookmarkBridge is unavailable, there is no way to determine the state,
        // so make the bookmark item passive/disabled.
        if (bookmarkBridge == null) {
            mBookmarkButton.setImageResource(R.drawable.btn_star);
            mBookmarkButton.setEnabled(false);
            return;
        }
        if (currentTab.isNativePage() || currentTab.getWebContents() == null)
            mBookmarkButton.setEnabled(false);
        else
        mBookmarkButton.setEnabled(bookmarkBridge.isEditBookmarksEnabled());

        if (bookmarkBridge.hasBookmarkIdForTab(currentTab)) {
            mBookmarkButton.setImageResource(R.drawable.btn_star_filled);
            mBookmarkButton.setContentDescription(getContext().getString(R.string.edit_bookmark));
            ApiCompatibilityUtils.setImageTintList(mBookmarkButton,
                    AppCompatResources.getColorStateList(getContext(), R.color.blue_mode_tint));
        } else {
            mBookmarkButton.setImageResource(R.drawable.btn_star);
            mBookmarkButton.setContentDescription(
                    getContext().getString(R.string.accessibility_menu_bookmark));
        }
    }

    // Vivaldi
    private final Runnable mNavigationPopupDismissRunnable = new Runnable() {
        @Override
        public void run() {
            mAppMenuHandler.hideAppMenu();
        }
    };

    // Vivaldi
    @Override
    public boolean onLongClick(View v) {
        if ((v == mBackwardButton) || (v == mForwardButton)) {
            if (mCurrentTab == null || mCurrentTab.getWebContents() == null) return false;
            // Context must be a ChromeActivity here for NavigationPopup to work.
            Context context = mCurrentTab.getWindowAndroid().getContext().get();
            assert context instanceof ChromeActivity;
            mNavigationPopup = new NavigationPopup(
                    Profile.fromWebContents(mCurrentTab.getWebContents()),
                    context,
                    mCurrentTab.getWebContents().getNavigationController(),
                    (v == mBackwardButton) ? NavigationPopup.Type.TABLET_BACK
                                           : NavigationPopup.Type.TABLET_FORWARD);

            // Close app menu when dismissing the navigation popup.
            mNavigationPopup.setOnDismissCallback(mNavigationPopupDismissRunnable);
            mNavigationPopup.show(this);

            return true;
        }
        return false;
    }

    // Vivaldi
    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        // Ensure the navigation popup is not shown after resuming activity from background.
        if (hasWindowFocus && mNavigationPopup != null) {
            mNavigationPopup.dismiss();
            mNavigationPopup = null;
        }
        super.onWindowFocusChanged(hasWindowFocus);
    }
}
