// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.RectF;
import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.CompositorOnClickHandler;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.AreaGestureEventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.GestureHandler;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.url.GURL;

import java.util.List;

//Vivaldi
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.toolbar.top.TabSwitcherActionMenuCoordinator;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.vivaldi.browser.common.VivaldiUtils;

import android.support.v4.graphics.ColorUtils;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

/**
 * This class handles managing which {@link StripLayoutHelper} is currently active and dispatches
 * all input and model events to the proper destination.
 */
public class StripLayoutHelperManager implements SceneOverlay, PauseResumeWithNativeObserver {
    private static final long FADE_SCRIM_DURATION_MS = 200;
    // Caching Variables
    private final RectF mStripFilterArea = new RectF();

    // Model selector buttons constants.
    private static final float MODEL_SELECTOR_BUTTON_Y_OFFSET_DP = 10.f;
    private static final float MODEL_SELECTOR_BUTTON_PADDING_DP = 12.f;
    private static final float MODEL_SELECTOR_BUTTON_WIDTH_DP = 24.f;
    private static final float MODEL_SELECTOR_BUTTON_HEIGHT_DP = 24.f;
    private static final float MODEL_SELECTOR_BUTTON_CLICK_SLOP_DP = 12.f;

    // External influences
    private TabModelSelector mTabModelSelector;
    private final LayoutUpdateHost mUpdateHost;

    // Event Filters
    private final AreaGestureEventFilter mEventFilter;

    // Internal state
    private boolean mIsIncognito;
    private final StripLayoutHelper mNormalHelper;
    private final StripLayoutHelper mIncognitoHelper;

    // UI State
    private float mWidth;  // in dp units
    private final float mHeight;  // in dp units
    private int mOrientation;
    private final CompositorButton mModelSelectorButton;

    private Context mContext;
    private final StripScrim mStripScrim;
    private boolean mBrowserScrimShowing;
    private ValueAnimator mScrimFadeAnimation;

    private TabStripSceneLayer mTabStripTreeProvider;

    private TabStripEventHandler mTabStripEventHandler;
    private TabSwitcherLayoutObserver mTabSwitcherLayoutObserver;

    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver =
            new TabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    tabModelSwitched(newModel.isIncognito());
                }
            };

    private TabModelObserver mTabModelObserver;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;

    private final String mDefaultTitle;
    private Supplier<LayerTitleCache> mLayerTitleCacheSupplier;

    // Vivaldi
    private final ChromeTabbedActivity mActivity;
    private float mViewportHeightOffset;
    private boolean mShouldHideOverlay;
    private LayerTitleCache mLayerTitleCache;
    private boolean mIsStackStrip;

    private class TabStripEventHandler implements GestureHandler {
        @Override
        public void onDown(float x, float y, boolean fromMouse, int buttons) {
            // Note(david@vivaldi.com): We need to manipulate the y value of the event when the
            // toolbar is at the bottom in order to handle the input events.
            if (ChromeApplicationImpl.isVivaldi()) y = getValueOfY(y);
            if (mModelSelectorButton.onDown(x, y)) return;
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().onDown(time(), x, y, fromMouse, buttons);
        }

        @Override
        public void onUpOrCancel() {
            if (ChromeApplicationImpl.isVivaldi())
                if (mModelSelectorButton.onUpOrCancel()) return;
            else
            if (mModelSelectorButton.onUpOrCancel() && mTabModelSelector != null) {
                getActiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
                if (!mModelSelectorButton.isVisible()) return;
                mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
                return;
            }
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().onUpOrCancel(time());
        }

        @Override
        public void drag(float x, float y, float dx, float dy, float tx, float ty) {
            mModelSelectorButton.drag(x, y);
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().drag(time(), x, y, dx, dy, tx, ty);
        }

        @Override
        public void click(float x, float y, boolean fromMouse, int buttons) {
            // Vivaldi - consider the offset of y when the toolbar is at the bottom
            if (ChromeApplicationImpl.isVivaldi()) y = getValueOfY(y);
            long time = time();
            if (mModelSelectorButton.click(x, y)) {
                mModelSelectorButton.handleClick(time);
                return;
            }
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().click(time(), x, y, fromMouse, buttons);
        }

        @Override
        public void fling(float x, float y, float velocityX, float velocityY) {
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().fling(time(), x, y, velocityX, velocityY);
        }

        @Override
        public void onLongPress(float x, float y) {
            if (mStripScrim.isVisible()) return;

            /** Vivaldi - Handling long click event on + button in tab strip **/
            if (ChromeApplicationImpl.isVivaldi()) {
                if (isNewTabButtonClicked(x, y)) {
                    showTabSwitcherPopupMenu();
                    return;
                }
            }
            /** Vivaldi - Consider the offset in case of bottom address bar **/
            getActiveStripLayoutHelper().onLongPress(time(), x, getValueOfY(y));
        }

        @Override
        public void onPinch(float x0, float y0, float x1, float y1, boolean firstEvent) {
            // Not implemented.
        }

        private long time() {
            return LayoutManagerImpl.time();
        }
    }

    /**
     * Observer for Tab Switcher layout events.
     */
    class TabSwitcherLayoutObserver implements LayoutStateObserver {
        @Override
        public void onStartedShowing(@LayoutType int layoutType, boolean showToolbar) {
            if (layoutType != LayoutType.TAB_SWITCHER) return;
            updateScrimVisibility(true);
        }

        @Override
        public void onStartedHiding(
                @LayoutType int layoutType, boolean showToolbar, boolean delayAnimation) {
            if (layoutType != LayoutType.TAB_SWITCHER) return;
            updateScrimVisibility(false);
        }

        @Override
        public void onTabSelectionHinted(int tabId) {
            LayoutStateObserver.super.onTabSelectionHinted(tabId);
        }

        private void updateScrimVisibility(boolean visibility) {
            // Handled by separate scrim over entire browser in the polished version.
            if (isGridTabSwitcherPolishEnabled()) {
                // Scrim doesn't actually show if the a11y list switcher is showing.
                mBrowserScrimShowing =
                        visibility && !DeviceClassManager.enableAccessibilityLayout(mContext);
                return;
            }

            if (!isGridTabSwitcherNonPolishEnabled()) return;

            if (mScrimFadeAnimation != null && mScrimFadeAnimation.isRunning()) {
                mScrimFadeAnimation.cancel();
            }

            float startAlpha = visibility ? 0.f : 1.f;
            float endAlpha = visibility ? 1.f : 0.f;
            mScrimFadeAnimation = ValueAnimator.ofFloat(startAlpha, endAlpha);
            mScrimFadeAnimation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
            mScrimFadeAnimation.setDuration(FADE_SCRIM_DURATION_MS);
            mScrimFadeAnimation.addUpdateListener((anim -> {
                final float currentAlpha = (float) anim.getAnimatedValue();
                mStripScrim.setAlpha(currentAlpha);
                mTabStripTreeProvider.updateStripScrim(mStripScrim);
            }));
            mScrimFadeAnimation.start();
            mStripScrim.setVisible(visibility);
        }
    }

    /**
     * @return Returns layout observer for tab switcher.
     */
    public TabSwitcherLayoutObserver getTabSwitcherObserver() {
        return mTabSwitcherLayoutObserver;
    }

    /**
     * Creates an instance of the {@link StripLayoutHelperManager}.
     * @param context The current Android {@link Context}.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The {@link LayoutRenderHost}.
     * @param layerTitleCacheSupplier A supplier of the cache that holds the title textures.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for registering this class
     *         to lifecycle events.
     */
    public StripLayoutHelperManager(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, Supplier<LayerTitleCache> layerTitleCacheSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mUpdateHost = updateHost;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mTabStripTreeProvider = new TabStripSceneLayer(context);
        mTabStripEventHandler = new TabStripEventHandler();
        mTabSwitcherLayoutObserver = new TabSwitcherLayoutObserver();
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mDefaultTitle = context.getString(R.string.tab_loading_default_title);
        mEventFilter =
                new AreaGestureEventFilter(context, mTabStripEventHandler, null, false, false);

        // Vivaldi - Note: We are abusing the |mModelSelectorButton| to handle new tab + button
        // click events in tab strip.
        CompositorOnClickHandler selectorClickHandler = new CompositorOnClickHandler() {
            @Override
            public void onClick(long time) {
                handleModelSelectorButtonClick();
            }
        };
        mModelSelectorButton = new CompositorButton(context, MODEL_SELECTOR_BUTTON_WIDTH_DP,
                MODEL_SELECTOR_BUTTON_HEIGHT_DP, selectorClickHandler);
        mModelSelectorButton.setIncognito(false);
        mModelSelectorButton.setVisible(false);
        // Pressed resources are the same as the unpressed resources.
        if(ChromeApplicationImpl.isVivaldi()) {
            mModelSelectorButton.setResources(R.drawable.btn_tabstrip_new_tab,
                    R.drawable.btn_tabstrip_new_tab, R.drawable.btn_tabstrip_new_tab,
                    R.drawable.btn_tabstrip_new_tab);
            mModelSelectorButton.setVisible(true);
        } else {
        mModelSelectorButton.setResources(R.drawable.btn_tabstrip_switch_normal,
                R.drawable.btn_tabstrip_switch_normal, R.drawable.location_bar_incognito_badge,
                R.drawable.location_bar_incognito_badge);
        }
        mModelSelectorButton.setY(MODEL_SELECTOR_BUTTON_Y_OFFSET_DP);
        mModelSelectorButton.setClickSlop(MODEL_SELECTOR_BUTTON_CLICK_SLOP_DP);

        Resources res = context.getResources();
        mHeight = res.getDimension(R.dimen.tab_strip_height) / res.getDisplayMetrics().density;
        mModelSelectorButton.setAccessibilityDescription(
                res.getString(R.string.accessibility_tabstrip_btn_incognito_toggle_standard),
                res.getString(R.string.accessibility_tabstrip_btn_incognito_toggle_incognito));

        mStripScrim = new StripScrim(context, mWidth, mHeight);
        mStripScrim.setVisible(false);
        mBrowserScrimShowing = false;

        mNormalHelper =
                new StripLayoutHelper(context, updateHost, renderHost, false, mModelSelectorButton);
        mIncognitoHelper =
                new StripLayoutHelper(context, updateHost, renderHost, true, mModelSelectorButton);

        onContextChanged(context);

        // Vivaldi
        mActivity = (ChromeTabbedActivity)context;
        mShouldHideOverlay = false;
        mContext = context;
    }

    /**
     * @return Return scrim to be applied on tab strip.
     */
    public StripScrim getStripScrim() {
        return mStripScrim;
    }

    /**
     * Cleans up internal state.
     */
    public void destroy() {
        if (mScrimFadeAnimation != null) {
            mScrimFadeAnimation.cancel();
        }
        mTabStripTreeProvider.destroy();
        mTabStripTreeProvider = null;
        mIncognitoHelper.destroy();
        mNormalHelper.destroy();
        mLifecycleDispatcher.unregister(this);
        if (mTabModelSelector != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);

            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelectorTabModelObserver.destroy();
            mTabModelSelectorTabObserver.destroy();
        }
    }

    @Override
    public void onResumeWithNative() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return;
        getStripLayoutHelper(currentTab.isIncognito())
                .scrollTabToView(LayoutManagerImpl.time(), true);
    }

    @Override
    public void onPauseWithNative() {}

    private void handleModelSelectorButtonClick() {
        // Note(david@vivaldi.com): We are abusing the |mModelSelectorButton| to create a new tab.
        if (ChromeApplicationImpl.isVivaldi()) {
            createNewTab();
            return;
        }
        if (mTabModelSelector == null) return;
        getActiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
        if (!mModelSelectorButton.isVisible()) return;
        mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
    }

    @VisibleForTesting
    public void simulateClick(float x, float y, boolean fromMouse, int buttons) {
        mTabStripEventHandler.click(x, y, fromMouse, buttons);
    }

    @VisibleForTesting
    public void simulateLongPress(float x, float y) {
        mTabStripEventHandler.onLongPress(x, y);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        assert mTabStripTreeProvider != null;

        // Note(david@vivaldi.com): Apply the correct |yOffset| according to the toolbar position.
        float dpToPx = (1.f / mActivity.getResources().getDisplayMetrics().density);
        if (!VivaldiUtils.isTopToolbarOn() && !mShouldHideOverlay) {
            mViewportHeightOffset = viewport.height() * dpToPx;
            yOffset += mViewportHeightOffset - mHeight;
            // Pass the yOffset of the main strip here as it is used to calculate the visibility of
            // the tab strip container.
            mTabStripTreeProvider.setMainYOffset(yOffset);
            // Place the stack strip above the main strip.
            if (mIsStackStrip) yOffset = -getHeight();
        } else {
            mViewportHeightOffset = 0;
            // Add vertical offset when there is an active panel.
            if (mActivity.getLayoutManager() != null) {
                OverlayPanel panel =
                        mActivity.getLayoutManager().getOverlayPanelManager().getActivePanel();
                if (panel != null) yOffset += panel.getBasePageY();
            }
            yOffset += mActivity.getBrowserControlsManager().getTopControlsMinHeight() * dpToPx;
            // Pass the yOffset of the main strip here as it is used to calculate the visibility of
            // the tab strip container.
            mTabStripTreeProvider.setMainYOffset(yOffset);
            // Place the stack strip under the main strip.
            if (mIsStackStrip) yOffset = getHeight();
        }

        Tab selectedTab = mTabModelSelector.getCurrentModel().getTabAt(
                mTabModelSelector.getCurrentModel().index());
        // Note(david@vivaldi.com): We show a loading text while restoring tabs.
        if (selectedTab != null)
            mTabStripTreeProvider.updateLoadingState(
                    resourceManager, this, getBackgroundStripColor(selectedTab.getThemeColor()));
        int selectedTabId = selectedTab == null ? TabModel.INVALID_TAB_INDEX : selectedTab.getId();
        mTabStripTreeProvider.pushAndUpdateStrip(this, mLayerTitleCacheSupplier.get(),
                resourceManager, getActiveStripLayoutHelper().getStripLayoutTabsToRender(), yOffset,
                selectedTabId);
        return mTabStripTreeProvider;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        // TODO(mdjones): This matches existing behavior but can be improved to return false if
        // the browser controls offset is equal to the browser controls height.
        return true;
    }

    @Override
    public EventFilter getEventFilter() {
        return mEventFilter;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {
        mWidth = width;
        boolean orientationChanged = false;
        if (mOrientation != orientation) {
            mOrientation = orientation;
            orientationChanged = true;
        }
        if (!LocalizationUtils.isLayoutRtl()) {
            mModelSelectorButton.setX(
                    mWidth - MODEL_SELECTOR_BUTTON_WIDTH_DP - MODEL_SELECTOR_BUTTON_PADDING_DP);
        } else {
            mModelSelectorButton.setX(MODEL_SELECTOR_BUTTON_PADDING_DP);
        }

        updateStripScrim();

        // Note(david@vivaldi.com): We need to take the orientation into account.
        if (ChromeApplicationImpl.isVivaldi()) {
            mNormalHelper.onSizeChanged(mWidth, mHeight, visibleViewportOffsetY, orientation);
            mIncognitoHelper.onSizeChanged(mWidth, mHeight, visibleViewportOffsetY, orientation);
        } else {
        mNormalHelper.onSizeChanged(mWidth, mHeight, orientationChanged, LayoutManagerImpl.time());
        mIncognitoHelper.onSizeChanged(
                mWidth, mHeight, orientationChanged, LayoutManagerImpl.time());
        }

        // Note(david@vivaldi.com): Apply the correct clicking area for all possible scenarios.
        if (ChromeApplicationImpl.isVivaldi()) {
            if (VivaldiUtils.isTopToolbarOn()) {
                float top = mIsStackStrip ? getHeight() : 0;
                mStripFilterArea.set(0, top, mWidth, mIsStackStrip ? getHeight() * 2 : getHeight());
            } else {
                mStripFilterArea.set(0,
                        mIsStackStrip ? getHeight() : height - getHeight(), mWidth,
                        mIsStackStrip ? getHeight() * 2 : height);
                if (!VivaldiUtils.isTabStackVisible())
                    mStripFilterArea.set(0, 0, mWidth, getHeight());
            }
        } else
        mStripFilterArea.set(0, 0, mWidth, Math.min(getHeight(), visibleViewportOffsetY));
        mEventFilter.setEventArea(mStripFilterArea);
    }

    private void updateStripScrim() {
        if (!isGridTabSwitcherNonPolishEnabled()) return;
        // Update width
        float scrimWidth = mModelSelectorButton.isVisible()
                ? mWidth - getModelSelectorButtonWidthWithPadding()
                : mWidth;
        mStripScrim.setWidth(scrimWidth);

        // Update drawX
        float drawX = 0;
        if (LocalizationUtils.isLayoutRtl() && mModelSelectorButton.isVisible()) {
            drawX = getModelSelectorButtonWidthWithPadding();
        }
        mStripScrim.setX(drawX);
    }

    private float getModelSelectorButtonWidthWithPadding() {
        return MODEL_SELECTOR_BUTTON_WIDTH_DP + (MODEL_SELECTOR_BUTTON_PADDING_DP * 2);
    }

    public TintedCompositorButton getNewTabButton() {
        return getActiveStripLayoutHelper().getNewTabButton();
    }

    /**
     * @return The touch target offset to be applied to the new tab button.
     */
    public float getNewTabBtnTouchTargetOffset() {
        return getActiveStripLayoutHelper().getNewTabButtonTouchTargetOffset();
    }

    public CompositorButton getModelSelectorButton() {
        return mModelSelectorButton;
    }

    @Override
    public void getVirtualViews(List<VirtualView> views) {
        if (mBrowserScrimShowing) return;

        if (!mStripScrim.isVisible()) getActiveStripLayoutHelper().getVirtualViews(views);
        if (mModelSelectorButton.isVisible()) views.add(mModelSelectorButton);
    }

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        // Note(david@vivaldi.com): We might need to hide the overlay when there is an active panel.
        // This only applies when toolbar is at the bottom.
        if (!VivaldiUtils.isTopToolbarOn()) {
            mShouldHideOverlay =
                    mActivity.getLayoutManager().getOverlayPanelManager().getActivePanel() != null;
            mTabStripTreeProvider.shouldHideOverlay(mShouldHideOverlay);
            return mShouldHideOverlay;
        }
        return false;
    }

    /**
     * @return The opacity to use for the fade on the left side of the tab strip.
     */
    public float getLeftFadeOpacity() {
        return getActiveStripLayoutHelper().getLeftFadeOpacity();
    }

    /**
     * @return The opacity to use for the fade on the right side of the tab strip.
     */
    public float getRightFadeOpacity() {
        return getActiveStripLayoutHelper().getRightFadeOpacity();
    }

    /** Update the title cache for the available tabs in the model. */
    private void updateTitleCacheForInit() {
        LayerTitleCache titleCache = mLayerTitleCacheSupplier.get();
        if (mTabModelSelector == null || titleCache == null) return;

        // Make sure any tabs already restored get loaded into the title cache.
        List<TabModel> models = mTabModelSelector.getModels();
        for (int i = 0; i < models.size(); i++) {
            TabModel model = models.get(i);
            for (int j = 0; j < model.getCount(); j++) {
                Tab tab = model.getTabAt(j);
                if (tab != null) {
                    titleCache.getUpdatedTitle(
                            tab, tab.getContext().getString(R.string.tab_loading_default_title));
                }
            }
        }
    }

    /**
     * Sets the {@link TabModelSelector} that this {@link StripLayoutHelperManager} will visually
     * represent, and various objects associated with it.
     * @param modelSelector The {@link TabModelSelector} to visually represent.
     * @param tabCreatorManager The {@link TabCreatorManager}, used to create new tabs.
     */
    public void setTabModelSelector(TabModelSelector modelSelector,
            TabCreatorManager tabCreatorManager) {
        if (mTabModelSelector == modelSelector) return;

        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int launchType, @TabCreationState int creationState) {
                updateTitleForTab(tab);
            }
        };
        modelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        mTabModelSelector = modelSelector;

        updateTitleCacheForInit();

        if (mTabModelSelector.isTabStateInitialized()) {
            updateModelSwitcherButton();
        } else {
            mTabModelSelector.addObserver(new TabModelSelectorObserver() {
                @Override
                public void onTabStateInitialized() {
                    updateModelSwitcherButton();
                    new Handler().post(() -> mTabModelSelector.removeObserver(this));
                }
            });
        }

        // Note(david@vivaldi.com): Always start off with the correct background color as the tab
        // strip will be recreated when themes has been changed (see VAB-2809).
        if (modelSelector.getCurrentTab() != null)
            mTabStripTreeProvider.setTabStripBackgroundColor(
                    getBackgroundStripColor(modelSelector.getCurrentTab().getThemeColor()));

        mNormalHelper.setTabModel(mTabModelSelector.getModel(false),
                tabCreatorManager.getTabCreator(false));
        mIncognitoHelper.setTabModel(mTabModelSelector.getModel(true),
                tabCreatorManager.getTabCreator(true));
        if (TabUiFeatureUtilities.isTabletTabGroupsEnabled(mContext)) {
            TabModelFilterProvider provider = mTabModelSelector.getTabModelFilterProvider();
            mNormalHelper.setTabGroupModelFilter(
                    (TabGroupModelFilter) provider.getTabModelFilter(false));
            mIncognitoHelper.setTabGroupModelFilter(
                    (TabGroupModelFilter) provider.getTabModelFilter(true));
        }
        tabModelSwitched(mTabModelSelector.isIncognitoSelected());

        mTabModelSelectorTabModelObserver = new TabModelSelectorTabModelObserver(modelSelector) {
            /**
             * @return The actual current time of the app in ms.
             */
            public long time() {
                return SystemClock.uptimeMillis();
            }

            @Override
            public void tabRemoved(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosed(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                // For right-direction move, layout helper re-ordering logic
                // expects destination index = position + 1
                getStripLayoutHelper(tab.isIncognito())
                        .tabMoved(time(), tab.getId(), curIndex,
                                newIndex > curIndex ? newIndex + 1 : newIndex);
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosureCancelled(time(), tab.getId());
                updateModelSwitcherButton();
                // Note(david@vivaldi.com): In case we undo a tab which was part of a stack, we need
                // to update the title for the current select tab.
                updateTitleForTab(modelSelector.getCurrentTab());
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                if (mLayerTitleCacheSupplier.hasValue()) {
                    mLayerTitleCacheSupplier.get().remove(tab.getId());
                }
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosed(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void onFinishingTabClosure(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosed(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void willCloseAllTabs(boolean incognito) {
                getStripLayoutHelper(incognito).willCloseAllTabs();
                updateModelSwitcherButton();
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (tab.getId() == lastId) return;
                getStripLayoutHelper(tab.isIncognito())
                        .tabSelected(time(), tab.getId(), lastId, false);
                // Vivaldi
                updateTitleForTab(tab);
            }

            @Override
            public void didAddTab(Tab tab, int type, int creationState) {
                boolean selected = type != TabLaunchType.FROM_LONGPRESS_BACKGROUND
                        || (mTabModelSelector.isIncognitoSelected() && tab.isIncognito());
                boolean onStartup = type == TabLaunchType.FROM_RESTORE;
                getStripLayoutHelper(tab.isIncognito())
                        .tabCreated(time(), tab.getId(), mTabModelSelector.getCurrentTabId(),
                                selected, false, onStartup);

                // Vivaldi
                updateModelSwitcherButton();
            }

            /** Vivaldi **/
            @Override
            public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
                // Make sure we update the title of all related tabs.
                List<Tab> tabs = modelSelector.getTabModelFilterProvider()
                                         .getCurrentTabModelFilter()
                                         .getRelatedTabList(tab.getId());
                for (Tab relatedTab : tabs) updateTitleForTab(relatedTab);
            }
        };

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                if (params.getTransitionType() == PageTransition.HOME_PAGE
                        || (params.getTransitionType() & PageTransition.FROM_ADDRESS_BAR)
                                == PageTransition.FROM_ADDRESS_BAR) {
                    getStripLayoutHelper(tab.isIncognito())
                            .scrollTabToView(LayoutManagerImpl.time(), false);
                }
            }

            @Override
            public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                getStripLayoutHelper(tab.isIncognito()).tabLoadStarted(tab.getId());
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                getStripLayoutHelper(tab.isIncognito()).tabLoadFinished(tab.getId());
            }

            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadStarted(tab.getId());
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadFinished(tab.getId());
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadFinished(tab.getId());
            }

            @Override
            public void onCrash(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadFinished(tab.getId());
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                updateTitleForTab(tab);
            }

            @Override
            public void onFaviconUpdated(Tab tab, Bitmap icon, GURL iconUrl) {
                updateTitleForTab(tab);
            }

            // Vivaldi
            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                if (mTabModelSelector.getCurrentTab() == tab)
                    mTabStripTreeProvider.setTabStripBackgroundColor(
                            getBackgroundStripColor(color));
            }
        };

        // Vivaldi
        new ActivityTabProvider.ActivityTabTabObserver(mActivity.getActivityTabProvider()) {
            @Override
            public void onObservingDifferentTab(Tab tab, boolean hint) {
                if (tab != null && !tab.isBeingRestored() && mTabStripTreeProvider != null)
                    mTabStripTreeProvider.setTabStripBackgroundColor(
                            getBackgroundStripColor(tab.getThemeColor()));
            }
        };

        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        // Note(david@vivaldi.com): Since we can have more tab strips this class needs it's own
        // |LayerTitleCache|. We also pass the tab model selector to the strip layout helper.
        mLayerTitleCache =
                new LayerTitleCache(mContext, mActivity.getLayoutManager().getResourceManager());
        mLayerTitleCache.setTabModelSelector(mTabModelSelector);
        mLayerTitleCache.setIsStackStrip(mIsStackStrip);
        mLayerTitleCacheSupplier = () -> mLayerTitleCache;
        mNormalHelper.setTabModelSelector(mTabModelSelector);
        mIncognitoHelper.setTabModelSelector(mTabModelSelector);
    }

    public void updateTitleForTab(Tab tab) { // Vivaldi
        if (mLayerTitleCacheSupplier.get() == null) return;

        String title = mLayerTitleCacheSupplier.get().getUpdatedTitle(tab, mDefaultTitle);

        // Note(david@vivaldi.com): Update the layer title cache as well.
        mLayerTitleCacheSupplier.get().getUpdatedTitle(tab, tab.getTitle());

        getStripLayoutHelper(tab.isIncognito()).tabTitleChanged(tab.getId(), title);
        mUpdateHost.requestUpdate();
    }

    public float getHeight() {
        return mHeight;
    }

    public float getWidth() {
        return mWidth;
    }

    public int getOrientation() {
        return mOrientation;
    }

    /**
     * Updates all internal resources and dimensions.
     * @param context The current Android {@link Context}.
     */
    public void onContextChanged(Context context) {
        mContext = context;
        mNormalHelper.onContextChanged(context);
        mIncognitoHelper.onContextChanged(context);
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        getInactiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
        return getActiveStripLayoutHelper().updateLayout(time);
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public boolean handlesTabCreating() {
        return false;
    }

    private void tabModelSwitched(boolean incognito) {
        if (incognito == mIsIncognito) return;
        mIsIncognito = incognito;

        mIncognitoHelper.tabModelSelected(mIsIncognito);
        mNormalHelper.tabModelSelected(!mIsIncognito);

        updateModelSwitcherButton();

        mUpdateHost.requestUpdate();
    }

    private void updateModelSwitcherButton() {
        mModelSelectorButton.setIncognito(mIsIncognito);
        if (mTabModelSelector != null) {
            boolean isVisible = mTabModelSelector.getModel(true).getCount() != 0;
            // Note (david@vivaldi.com): The ModelSelectorButton is always visible since this is our
            // new tab button but we don't occupy any margins with setting |isVisible| to false;
            if (ChromeApplicationImpl.isVivaldi()) {
                mModelSelectorButton.setVisible(true);
                isVisible = false;
            } else {
            mModelSelectorButton.setVisible(isVisible);
            }

            float endMargin = isVisible ? getModelSelectorButtonWidthWithPadding() : 0.0f;

            mNormalHelper.setEndMargin(endMargin);
            mIncognitoHelper.setEndMargin(endMargin);
            updateStripScrim();
        }
    }

    private boolean isGridTabSwitcherPolishEnabled() {
        return ChromeFeatureList.sGridTabSwitcherForTablets.isEnabled()
                && TabUiFeatureUtilities.GRID_TAB_SWITCHER_FOR_TABLETS_POLISH.getValue();
    }

    private boolean isGridTabSwitcherNonPolishEnabled() {
        return ChromeFeatureList.sGridTabSwitcherForTablets.isEnabled()
                && !TabUiFeatureUtilities.GRID_TAB_SWITCHER_FOR_TABLETS_POLISH.getValue();
    }

    /**
     * @param incognito Whether or not you want the incognito StripLayoutHelper
     * @return The requested StripLayoutHelper.
     */
    @VisibleForTesting
    public StripLayoutHelper getStripLayoutHelper(boolean incognito) {
        return incognito ? mIncognitoHelper : mNormalHelper;
    }

    /**
     * @return The currently visible strip layout helper.
     */
    @VisibleForTesting
    public StripLayoutHelper getActiveStripLayoutHelper() {
        return getStripLayoutHelper(mIsIncognito);
    }

    private StripLayoutHelper getInactiveStripLayoutHelper() {
        return mIsIncognito ? mNormalHelper : mIncognitoHelper;
    }

    /** Vivaldi **/
    public void setIsStackStrip(boolean isStackStrip) {
        mIsStackStrip = isStackStrip;
        mTabStripTreeProvider.setIsStackStrip(mIsStackStrip);
        mNormalHelper.setIsStackStrip(mIsStackStrip);
        mIncognitoHelper.setIsStackStrip(mIsStackStrip);
    }

    /** Vivaldi **/
    private void createNewTab() {
        TabModel currentTabModel = mTabModelSelector.getCurrentModel();
        if (currentTabModel == null) return;
        if (!currentTabModel.isIncognito()) currentTabModel.commitAllTabClosures();
        // Open tab within a group.
        if (mIsStackStrip) {
            List<Tab> relatedTabs = mTabModelSelector.getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()
                                            .getRelatedTabList(mTabModelSelector.getCurrentTabId());
            assert relatedTabs.size() > 0;
            Tab parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
            mActivity.getTabCreator(currentTabModel.isIncognito())
                    .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL),
                            TabLaunchType.FROM_TAB_GROUP_UI, parentTabToAttach);
        } else // Open a normal one.
            mActivity.getTabCreator(currentTabModel.isIncognito()).launchNTP();
    }

    /** Vivaldi **/
    public boolean isSceneOffScreen() {
        boolean isOffScreen = mActivity.getBrowserControlsManager().areBrowserControlsAtMinHeight()
                || !VivaldiUtils.isTabStripOn();
        if (mActivity.isInOverviewMode()) return true;
        if (mIsStackStrip && !VivaldiUtils.isTabStackVisible()) return true;
        return isOffScreen;
    }

    /** Vivaldi **/
    private boolean isNewTabButtonClicked(float x, float y) {
        if (VivaldiUtils.isTopToolbarOn()) {
            if (mIsStackStrip) y -= getHeight();
        } else {
            y += mIsStackStrip ? getHeight() : 0;
            y = mViewportHeightOffset - y; // consider the offset in case of bottom address bar
        }

        return mModelSelectorButton.checkClicked(x, y);
    }

    /** Vivaldi - Handling long click event on + button in tab strip **/
    private void showTabSwitcherPopupMenu() {
        // For now we don't show the popup on the stack strip.
        if (mIsStackStrip) return;
        TabSwitcherActionMenuCoordinator menu = new TabSwitcherActionMenuCoordinator();
        View anchorView = mActivity.findViewById(R.id.tab_switcher_menu_helper_button);
        // Note(nagamani@vivaldi.com): We are using dummy button with 0dp width and 0dp height as
        // anchorView
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(0, 0);
        params.gravity = Gravity.TOP | Gravity.END;
        // Apply correct margin when we have double tab bar.
        int margin =
                !mIsStackStrip && VivaldiUtils.isTabStackVisible() ? -((int) getHeight() * 2) : 0;
        if (VivaldiUtils.isTopToolbarOn())
            params.topMargin = margin;
        else
            params.bottomMargin = margin;

        // Note(nagamani@vivaldi.com): change layout gravity according to address bar location
        if (!VivaldiUtils.isTopToolbarOn()) {
            params.gravity = Gravity.BOTTOM | Gravity.END;
        }
        anchorView.setLayoutParams(params);

        menu.displayMenu(mContext, (ListMenuButton) anchorView, menu.buildMenuItems(),
                (id) -> { mActivity.onOptionsItemSelected(id, null); });
    }

    /** Vivaldi - Function to return value of y with correct offset value **/
    private float getValueOfY(float y) {
        if (!VivaldiUtils.isTopToolbarOn()) {
            y += mIsStackStrip ? getHeight() : 0;
            return mViewportHeightOffset - y;
        }
        float dpToPx = (1.f / mActivity.getResources().getDisplayMetrics().density);
        if (mIsStackStrip) y -= getHeight();
        return y - (mActivity.getBrowserControlsManager().getTopControlsMinHeight() *dpToPx);
    }

    /** Vivaldi - Get the right background color for the current strip **/
    private int getBackgroundStripColor(int color) {
        int newColor = color;
        // The stack strip color is slightly darker.
        if (mIsStackStrip) newColor = ColorUtils.blendARGB(color, 0xFF000000, 0.2f);
        return newColor;
    }
}
