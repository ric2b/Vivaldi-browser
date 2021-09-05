// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListView;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.vivaldi.browser.common.VivaldiUtils;
import org.vivaldi.browser.preferences.VivaldiPreferences;
import org.vivaldi.browser.preferences.VivaldiSyncActivity;
import org.vivaldi.browser.sync.VivaldiProfileSyncService;
import org.vivaldi.browser.vivaldi_account_manager.VivaldiAccountManager;

/**
 * The native recent tabs page. Lists recently closed tabs, open windows and tabs from the user's
 * synced devices, and snapshot documents sent from Chrome to Mobile in an expandable list view.
 */
public class RecentTabsPage
        implements NativePage, ApplicationStatus.ActivityStateListener,
                   ExpandableListView.OnChildClickListener,
                   ExpandableListView.OnGroupCollapseListener,
                   ExpandableListView.OnGroupExpandListener, RecentTabsManager.UpdatedCallback,
                   View.OnAttachStateChangeListener, View.OnCreateContextMenuListener,
                   InvalidationAwareThumbnailProvider, BrowserControlsStateProvider.Observer {
    private final Activity mActivity;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ExpandableListView mListView;
    private final String mTitle;
    private final ViewGroup mView;

    private RecentTabsManager mRecentTabsManager;
    private RecentTabsRowAdapter mAdapter;
    private NativePageHost mPageHost;

    private boolean mSnapshotContentChanged;
    private int mSnapshotListPosition;
    private int mSnapshotListTop;
    private int mSnapshotWidth;
    private int mSnapshotHeight;

    /**
     * Whether the page is in the foreground and is visible.
     */
    private boolean mInForeground;

    /**
     * Whether {@link #mView} is attached to the application window.
     */
    private boolean mIsAttachedToWindow;

    //** Vivaldi */
    private VivaldiAccountManager.AccountStateObserver mAccountObserver;
    private SharedPreferencesManager.Observer mPreferenceObserver;

    /**
     * Constructor returns an instance of RecentTabsPage.
     *
     * @param activity The activity this view belongs to.
     * @param recentTabsManager The RecentTabsManager which provides the model data.
     * @param pageHost The NativePageHost used to provide a history navigation delegate object.
     */
    public RecentTabsPage(
            ChromeActivity activity, RecentTabsManager recentTabsManager, NativePageHost pageHost) {
        mActivity = activity;
        mRecentTabsManager = recentTabsManager;
        mPageHost = pageHost;
        Resources resources = activity.getResources();

        mTitle = resources.getString(R.string.recent_tabs);
        mRecentTabsManager.setUpdatedCallback(this);
        LayoutInflater inflater = LayoutInflater.from(activity);
        mView = (ViewGroup) inflater.inflate(R.layout.recent_tabs_page, null);
        mListView = (ExpandableListView) mView.findViewById(R.id.odp_listview);
        mAdapter = new RecentTabsRowAdapter(activity, recentTabsManager);
        mListView.setAdapter(mAdapter);
        mListView.setOnChildClickListener(this);
        mListView.setGroupIndicator(null);
        mListView.setOnGroupCollapseListener(this);
        mListView.setOnGroupExpandListener(this);
        mListView.setOnCreateContextMenuListener(this);

        mView.addOnAttachStateChangeListener(this);
        ApplicationStatus.registerStateListenerForActivity(this, activity);
        // {@link #mInForeground} will be updated once the view is attached to the window.

        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            mBrowserControlsStateProvider = activity.getFullscreenManager();
            mBrowserControlsStateProvider.addObserver(this);
            onBottomControlsHeightChanged(mBrowserControlsStateProvider.getBottomControlsHeight(),
                    mBrowserControlsStateProvider.getBottomControlsMinHeight());
        } else {
            mBrowserControlsStateProvider = null;
        }

        onUpdated();

        // Vivaldi
        mAccountObserver = () -> onUpdated();
        // Note(david@vivaldi.com): We need to adjust the margin when using tab strip.
        int initialHeight =
                ((ChromeTabbedActivity) mActivity).getFullscreenManager().getTopControlsHeight();
        int toolbarHeight =
                ((ChromeTabbedActivity) mActivity).getToolbarManager().getToolbar().getHeight();
        int tabStripHeight = (int) mActivity.getResources().getDimension(
                R.dimen.tab_strip_height);
        mPreferenceObserver = key -> {
            if (VivaldiPreferences.SHOW_TAB_STRIP.equals(key)) {
                if (mRecentTabsManager.getVivaldiRecentTabsManager() == null) {
                    int marginTop = SharedPreferencesManager.getInstance().readBoolean(
                            VivaldiPreferences.SHOW_TAB_STRIP, true)
                            ? (initialHeight > toolbarHeight) ? 0 : tabStripHeight
                            : (initialHeight > toolbarHeight) ? -tabStripHeight : 0;
                    VivaldiUtils.updateTopMarginForTabsOnPhoneUI(mView, marginTop);
                }
            }
        };
        SharedPreferencesManager.getInstance().addObserver(mPreferenceObserver);
        mView.setPadding(0,0,0,0);
        mView.findViewById(R.id.recent_tabs_root).setBackgroundColor(Color.TRANSPARENT);
    }

    /**
     * Updates whether the page is in the foreground based on whether the application is in the
     * foreground and whether {@link #mView} is attached to the application window. If the page is
     * no longer in the foreground, records the time that the page spent in the foreground to UMA.
     */
    private void updateForegroundState() {
        // Note(david@vivaldi.com): We need to adjust the margin when using tab strip.
        if (mRecentTabsManager.getVivaldiRecentTabsManager() != null) {
            VivaldiUtils.updateTopMarginForTabsOnPhoneUI(mView,
                    SharedPreferencesManager.getInstance().readBoolean(
                            VivaldiPreferences.SHOW_TAB_STRIP, true)
                            ? -(int) mActivity.getResources().getDimension(
                                      R.dimen.tab_strip_height)
                            : 0);
        }

        boolean inForeground = mIsAttachedToWindow
                && ApplicationStatus.getStateForActivity(mActivity) == ActivityState.RESUMED;
        if (mInForeground == inForeground) {
            return;
        }

        mInForeground = inForeground;
        if (mInForeground) {
            mRecentTabsManager.recordRecentTabMetrics();
        }
    }

    // NativePage overrides

    @Override
    public String getUrl() {
        return UrlConstants.RECENT_TABS_URL;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return Color.WHITE;
    }

    @Override
    public boolean needsToolbarShadow() {
        return true;
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public String getHost() {
        return UrlConstants.RECENT_TABS_HOST;
    }

    @Override
    public void destroy() {
        assert !mIsAttachedToWindow : "Destroy called before removed from window";
        mRecentTabsManager.destroy();
        mRecentTabsManager = null;
        mPageHost = null;
        mAdapter.notifyDataSetInvalidated();
        mAdapter = null;
        mListView.setAdapter((RecentTabsRowAdapter) null);

        mView.removeOnAttachStateChangeListener(this);
        ApplicationStatus.unregisterActivityStateListener(this);
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.removeObserver(this);
        }

        // Vivaldi
        SharedPreferencesManager.getInstance().removeObserver(mPreferenceObserver);
    }

    @Override
    public void updateForUrl(String url) {
    }

    // ApplicationStatus.ActivityStateListener
    @Override
    public void onActivityStateChange(Activity activity, int state) {
        // Called when the user locks the screen or moves Chrome to the background via the task
        // switcher.
        updateForegroundState();
    }

    // View.OnAttachStateChangeListener
    @Override
    public void onViewAttachedToWindow(View view) {
        // Called when the user opens the RecentTabsPage or switches back to the RecentTabsPage from
        // another tab.
        mIsAttachedToWindow = true;
        updateForegroundState();

        // Work around a bug on Samsung devices where the recent tabs page does not appear after
        // toggling the Sync quick setting.  For some reason, the layout is being dropped on the
        // flow and we need to force a root level layout to get the UI to appear.
        view.getRootView().requestLayout();

        //** Vivaldi */
        VivaldiAccountManager.get().addAccountStateObserver(mAccountObserver);
    }

    @Override
    public void onViewDetachedFromWindow(View view) {
        // Called when the user navigates from the RecentTabsPage or switches to another tab.
        mIsAttachedToWindow = false;
        updateForegroundState();

        //** Vivaldi */
        VivaldiAccountManager.get().removeAccountStateObserver(mAccountObserver);
    }

    // ExpandableListView.OnChildClickedListener
    @Override
    public boolean onChildClick(ExpandableListView parent, View v, int groupPosition,
            int childPosition, long id) {
        return mAdapter.getGroup(groupPosition).onChildClick(childPosition);
    }

    // ExpandableListView.OnGroupExpandedListener
    @Override
    public void onGroupExpand(int groupPosition) {
        mAdapter.getGroup(groupPosition).setCollapsed(false);
        mSnapshotContentChanged = true;
    }

    // ExpandableListView.OnGroupCollapsedListener
    @Override
    public void onGroupCollapse(int groupPosition) {
        mAdapter.getGroup(groupPosition).setCollapsed(true);
        mSnapshotContentChanged = true;
    }

    // RecentTabsManager.UpdatedCallback
    @Override
    public void onUpdated() {
        // NOTE (david@vivaldi.com): This only affects the sync tab page in the tab switcher which
        // will show a login button when we're not logged into sync.
        if (mRecentTabsManager.getVivaldiRecentTabsManager() != null) {
            if (mRecentTabsManager.getVivaldiRecentTabsManager().onlyShowForeignSessions()) {
                View view = mActivity.findViewById(R.id.sign_in_for_tabs);
                if (view == null) {
                    view = LayoutInflater.from(mActivity).inflate(
                            R.layout.sign_in_for_sync_tabs, null);
                    view.findViewById(R.id.no_sync_sign_in_button)
                            .setOnClickListener(v -> VivaldiSyncActivity.start(mView.getContext()));
                    if (mView.indexOfChild(view) == -1) mView.addView(view);
                }
                if (VivaldiAccountManager.get().getSimplifiedState()
                        == VivaldiAccountManager.SimplifiedState.LOGGED_IN) {
                    mView.findViewById(R.id.no_sync_sign_in_button).setVisibility(View.GONE);
                    if (VivaldiProfileSyncService.get().getCommitStatus()
                            != VivaldiProfileSyncService.CycleStatus.SUCCESS) {
                        ((android.widget.TextView) mView.findViewById(R.id.no_sync_text))
                                .setText(R.string.vivaldi_sync_in_progress_text);
                        return;
                    }
                    mView.removeView(view);
                } else
                    return;
                // Once we get here, remove the sign in view.
                mView.removeView(view);
            }
        }

        mAdapter.notifyDataSetChanged();
        for (int i = 0; i < mAdapter.getGroupCount(); i++) {
            if (mAdapter.getGroup(i).isCollapsed()) {
                mListView.collapseGroup(i);
            } else {
                mListView.expandGroup(i);
            }
        }
        mSnapshotContentChanged = true;
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        // Would prefer to have this context menu view managed internal to RecentTabsGroupView
        // Unfortunately, setting either onCreateContextMenuListener or onLongClickListener
        // disables the native onClick (expand/collapse) behaviour of the group view.
        ExpandableListView.ExpandableListContextMenuInfo info =
                (ExpandableListView.ExpandableListContextMenuInfo) menuInfo;

        int type = ExpandableListView.getPackedPositionType(info.packedPosition);
        int groupPosition = ExpandableListView.getPackedPositionGroup(info.packedPosition);

        if (type == ExpandableListView.PACKED_POSITION_TYPE_GROUP) {
            mAdapter.getGroup(groupPosition).onCreateContextMenuForGroup(menu, mActivity);
        } else if (type == ExpandableListView.PACKED_POSITION_TYPE_CHILD) {
            int childPosition = ExpandableListView.getPackedPositionChild(info.packedPosition);
            mAdapter.getGroup(groupPosition).onCreateContextMenuForChild(childPosition, menu,
                    mActivity);
        }
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        if (mView.getWidth() == 0 || mView.getHeight() == 0) return false;

        View topItem = mListView.getChildAt(0);
        return mSnapshotContentChanged
                || mSnapshotListPosition != mListView.getFirstVisiblePosition()
                || mSnapshotListTop != (topItem == null ? 0 : topItem.getTop())
                || mView.getWidth() != mSnapshotWidth
                || mView.getHeight() != mSnapshotHeight;
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        ViewUtils.captureBitmap(mView, canvas);
        mSnapshotContentChanged = false;
        mSnapshotListPosition = mListView.getFirstVisiblePosition();
        View topItem = mListView.getChildAt(0);
        mSnapshotListTop = topItem == null ? 0 : topItem.getTop();
        mSnapshotWidth = mView.getWidth();
        mSnapshotHeight = mView.getHeight();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        updateMargins();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateMargins();
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        updateMargins();
    }

    private void updateMargins() {
        final View recentTabsRoot = mView.findViewById(R.id.recent_tabs_root);
        final int topControlsHeight = mBrowserControlsStateProvider.getTopControlsHeight();
        final int contentOffset = mBrowserControlsStateProvider.getContentOffset();
        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) recentTabsRoot.getLayoutParams();
        int topMargin = layoutParams.topMargin;

        // If the top controls are at the resting position or their height is decreasing, we want to
        // update the margin. We don't do this if the controls height is increasing because changing
        // the margin shrinks the view height to its final value, leaving a gap at the bottom until
        // the animation finishes.
        if (contentOffset >= topControlsHeight) {
            topMargin = topControlsHeight;
        }

        // If the content offset is different from the margin, we use translationY to position the
        // view in line with the content offset.
        recentTabsRoot.setTranslationY(contentOffset - topMargin);

        final int bottomMargin = mBrowserControlsStateProvider.getBottomControlsHeight();
        if (topMargin != layoutParams.topMargin || bottomMargin != layoutParams.bottomMargin) {
            layoutParams.topMargin = topMargin;
            layoutParams.bottomMargin = bottomMargin;
            recentTabsRoot.setLayoutParams(layoutParams);
        }
    }
}
