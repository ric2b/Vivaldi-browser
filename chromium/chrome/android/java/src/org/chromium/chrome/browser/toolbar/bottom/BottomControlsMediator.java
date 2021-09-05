// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

import android.content.res.Configuration;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.tab.TabBrowserControlsOffsetHelper;
import org.vivaldi.browser.common.VivaldiUtils;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the bottom controls component,
 * and updating the model accordingly.
 */
class BottomControlsMediator implements BrowserControlsStateProvider.Observer,
                                        KeyboardVisibilityDelegate.KeyboardVisibilityListener,
                                        SceneChangeObserver,
                                        OverlayPanelManager.OverlayPanelManagerObserver {
    /** The model for the bottom controls component that holds all of its view state. */
    private final PropertyModel mModel;

    /** The fullscreen manager to observe fullscreen events. */
    private final FullscreenManager mFullscreenManager;

    /** The browser controls sizer/manager to observe browser controls events. */
    private final BrowserControlsSizer mBrowserControlsSizer;

    /**
     * The height of the bottom bar in pixels, not including the top shadow.
     */
    private int mBottomControlsHeight;

    /** A {@link WindowAndroid} for watching keyboard visibility events. */
    private WindowAndroid mWindowAndroid;

    /** The bottom controls visibility. */
    private boolean mIsBottomControlsVisible;

    /** Whether any overlay panel is showing. */
    private boolean mIsOverlayPanelShowing;

    /** Whether the swipe layout is currently active. */
    private boolean mIsInSwipeLayout;

    /** Whether the soft keyboard is visible. */
    private boolean mIsKeyboardVisible;

    // Vivaldi
    private ChromeActivity mChromeActivity;

    /**
     * Build a new mediator that handles events from outside the bottom controls component.
     * @param model The {@link BottomControlsProperties} that holds all the view state for the
     *         bottom controls component.
     @param controlsSizer The {@link BrowserControlsSizer} to manipulate browser controls.
     * @param fullscreenManager A {@link FullscreenManager} for events related to the browser
     *                          controls.
     * @param bottomControlsHeight The height of the bottom bar in pixels.
     */
    BottomControlsMediator(PropertyModel model, BrowserControlsSizer controlsSizer,
            FullscreenManager fullscreenManager, int bottomControlsHeight) {
        mModel = model;

        mFullscreenManager = fullscreenManager;
        mBrowserControlsSizer = controlsSizer;
        mBrowserControlsSizer.addObserver(this);

        mBottomControlsHeight = bottomControlsHeight;
    }

    void setResourceManager(ResourceManager resourceManager) {
        mModel.set(BottomControlsProperties.RESOURCE_MANAGER, resourceManager);
    }

    void setWindowAndroid(WindowAndroid windowAndroid) {
        assert mWindowAndroid == null : "#setWindowAndroid should only be called once per toolbar.";
        // Watch for keyboard events so we can hide the bottom toolbar when the keyboard is showing.
        mWindowAndroid = windowAndroid;
        mWindowAndroid.getKeyboardDelegate().addKeyboardVisibilityListener(this);
    }

    void setLayoutManager(LayoutManagerImpl layoutManager) {
        mModel.set(BottomControlsProperties.LAYOUT_MANAGER, layoutManager);
        layoutManager.addSceneChangeObserver(this);
        layoutManager.getOverlayPanelManager().addObserver(this);
    }

    void setBottomControlsVisible(boolean visible) {
        mIsBottomControlsVisible = visible;

        // Note(david@vivaldi.com): Apply bottom controls height as an offset. This will ensure that
        // the bottom toolbar will be in the same state after rotating device or being in fullscreen
        // mode.
        if (visible && BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsSizer)
                        && mChromeActivity != null
                || isInFullscreenMode()) {
            if (mChromeActivity.getTabModelSelector() != null) {
                if (mChromeActivity.getTabModelSelector().getCurrentTab() != null) {
                    TabBrowserControlsOffsetHelper
                            .get(mChromeActivity.getTabModelSelector().getCurrentTab())
                            .setBottomOffset(mBottomControlsHeight, 0);
                }
            }
        }

        updateCompositedViewVisibility();
        updateAndroidViewVisibility();
    }

    /**
     * Clean up anything that needs to be when the bottom controls component is destroyed.
     */
    void destroy() {
        mBrowserControlsSizer.removeObserver(this);
        if (mWindowAndroid != null) {
            mWindowAndroid.getKeyboardDelegate().removeKeyboardVisibilityListener(this);
            mWindowAndroid = null;
        }
        if (mModel.get(BottomControlsProperties.LAYOUT_MANAGER) != null) {
            LayoutManagerImpl manager = mModel.get(BottomControlsProperties.LAYOUT_MANAGER);
            manager.getOverlayPanelManager().removeObserver(this);
            manager.removeSceneChangeObserver(this);
        }
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        mModel.set(BottomControlsProperties.Y_OFFSET, bottomOffset);
        updateAndroidViewVisibility();
    }

    @Override
    public void onOverlayPanelShown() {
        mIsOverlayPanelShowing = true;
        updateAndroidViewVisibility();
    }

    @Override
    public void onOverlayPanelHidden() {
        mIsOverlayPanelShowing = false;
        updateAndroidViewVisibility();
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        mIsKeyboardVisible = isShowing;
        updateCompositedViewVisibility();
        updateAndroidViewVisibility();
    }

    @Override
    public void onTabSelectionHinted(int tabId) {}

    @Override
    public void onSceneChange(Layout layout) {
        mIsInSwipeLayout = layout instanceof ToolbarSwipeLayout;
        updateAndroidViewVisibility();
    }

    /**
     * @return Whether the browser is currently in fullscreen mode.
     */
    private boolean isInFullscreenMode() {
        return mFullscreenManager != null && mFullscreenManager.getPersistentFullscreenMode();
    }

    /**
     * The composited view is the composited version of the Android View. It is used to be able to
     * scroll the bottom controls off-screen synchronously. Since the bottom controls live below
     * the webcontents we re-size the webcontents through
     * {@link BrowserControlsSizer#setBottomControlsHeight(int,int)} whenever the composited view
     * visibility changes.
     */
    private void updateCompositedViewVisibility() {
        // Note(david@vivaldi.com): Don't update when in swipe layout.
        if (ChromeApplication.isVivaldi() && mIsInSwipeLayout) return;

        boolean isCompositedViewVisible =
                mIsBottomControlsVisible && !mIsKeyboardVisible && !isInFullscreenMode();

        if (!VivaldiUtils.isTopToolbarOn()) isCompositedViewVisible = false;

        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, isCompositedViewVisible);

        // Note(david@vivaldi.com): The bottom controls height will be set in the
        // |VivaldiTopToolbarCoordinator|
        if (!ChromeApplication.isVivaldi())
        mBrowserControlsSizer.setBottomControlsHeight(
                isCompositedViewVisible ? mBottomControlsHeight : 0,
                mBrowserControlsSizer.getBottomControlsMinHeight());
    }

    /**
     * The Android View is the interactive view. The composited view should always be behind the
     * Android view which means we hide the Android view whenever the composited view is hidden.
     * We also hide the Android view as we are scrolling the bottom controls off screen this is
     * done by checking if {@link BrowserControlsSizer#getBottomControlOffset()} is
     * non-zero.
     */
    private void updateAndroidViewVisibility() {
        // Note(david@vivaldi.com: With Chromium 84 the ability to swipe the bottom toolbar is not
        // supported anymore (it is not part of the SwipeLayout and the ToolbarSwipeLayout don't
        // handle swipe inputs). Therefore we will just show the AndroidView rather then the
        // compositor view while swiping.
        if (mChromeActivity != null) {
            if (mIsInSwipeLayout && !mChromeActivity.isTablet()
                            && mChromeActivity.getResources().getConfiguration().orientation
                                    == Configuration.ORIENTATION_PORTRAIT
                            && VivaldiUtils.isTopToolbarOn()
                    || mChromeActivity.isInOverviewMode()) {
                mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, true);
                return;
            }
        }

        // Note(david@vivaldi.com): Don't show android view when controls are off screen.
        if (BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsSizer)) {
            mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, false);
            return;
        }

        // Note(david@vivaldi.com): When toolbar is at the bottom we hide the android view.
        if(!VivaldiUtils.isTopToolbarOn()) {
            mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, false);
            return;
        }
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE,
                mIsBottomControlsVisible && !mIsKeyboardVisible && !mIsOverlayPanelShowing
                        && !mIsInSwipeLayout && mBrowserControlsSizer.getBottomControlOffset() == 0
                        && !isInFullscreenMode());
    }

    /** Vivaldi **/
    public void setChromeActivity(ChromeActivity activity)
    {
        mChromeActivity = activity;
    }

    /** Vivaldi **/
    void setToolbarSwipeHandler(EdgeSwipeHandler swipeHandler) {
        mModel.set(BottomControlsProperties.TOOLBAR_SWIPE_HANDLER, swipeHandler);
    }
}