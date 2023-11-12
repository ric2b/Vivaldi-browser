// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;
import android.os.Build;

import androidx.annotation.ColorInt;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripScrim;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;

import org.chromium.ui.util.ColorUtils;
import org.vivaldi.browser.common.VivaldiUtils;

/**
 * The Java component of what is basically a CC Layer that manages drawing the Tab Strip (which is
 * composed of {@link StripLayoutTab}s) to the screen.  This object keeps the layers up to date and
 * removes/creates children as necessary.  This object is built by its native counterpart.
 */
@JNINamespace("android")
public class TabStripSceneLayer extends SceneOverlayLayer {
    private static boolean sTestFlag;
    private long mNativePtr;
    private float mDpToPx;
    private SceneLayer mChildSceneLayer;
    private int mOrientation;
    private int mNumReaddBackground;

    // Vivaldi
    private boolean mShouldHideOverlay;
    private boolean mIsStackStrip;
    private float mMainYOffset;
    private Context mContext;

    public TabStripSceneLayer(Context context) {
        mDpToPx = context.getResources().getDisplayMetrics().density;
        // Vivaldi
        mShouldHideOverlay = false;
        mContext = context;
    }

    public static void setTestFlag(boolean testFlag) {
        sTestFlag = testFlag;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = TabStripSceneLayerJni.get().init(TabStripSceneLayer.this);
        }
        // Set flag for testing
        if (!sTestFlag) {
            assert mNativePtr != 0;
        }
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        TabStripSceneLayerJni.get().setContentTree(
                mNativePtr, TabStripSceneLayer.this, contentTree);
    }

    /**
     * Pushes all relevant {@link StripLayoutTab}s to the CC Layer tree.
     * This also pushes any other assets required to draw the Tab Strip.  This should only be called
     * when the Compositor has disabled ScheduleComposite calls as this will change the tree and
     * could subsequently cause unnecessary follow up renders.
     *
     * @param layoutHelper A layout helper for the tab strip.
     * @param layerTitleCache A layer title cache.
     * @param resourceManager A resource manager.
     * @param stripLayoutTabsToRender Array of strip layout tabs.
     * @param yOffset Current browser controls offset in dp.
     */
    public void pushAndUpdateStrip(StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager,
            StripLayoutTab[] stripLayoutTabsToRender, float yOffset, int selectedTabId) {
        if (mNativePtr == 0) return;

        // Note(david@vivaldi.com): If we have a stack the offset is twice as big. We also always
        // consider the |mMainYOffset| for calculating the tab strip visibility.
        float stackOffset = VivaldiUtils.isTabStackVisible() ? 2 * mDpToPx : 1;
        boolean visible = mMainYOffset > -(layoutHelper.getHeight() * stackOffset); // Vivaldi
        visible = visible && VivaldiUtils.isTabStripOn() && !mShouldHideOverlay;

        // Note(david@vivaldi.com): We only show the stacking strip when we are in a tab stack.
        if(mIsStackStrip && !VivaldiUtils.isTabStackVisible()) visible = false;
        // This will hide the tab strips if necessary.
        TabStripSceneLayerJni.get().beginBuildingFrame(
                mNativePtr, TabStripSceneLayer.this, visible);
        // When strip tabs are completely off screen, we don't need to update it.
        if (visible) {
            pushButtonsAndBackground(layoutHelper, resourceManager, yOffset);
            pushStripTabs(layoutHelper, layerTitleCache, resourceManager, stripLayoutTabsToRender,
                    selectedTabId);
        }
        TabStripSceneLayerJni.get().finishBuildingFrame(mNativePtr, TabStripSceneLayer.this);
    }

    /**
     * Updates tab strip scrim.
     * @param scrim - Scrim applied to tab strip.
     */
    public void updateStripScrim(StripScrim scrim) {
        if (mNativePtr == 0) return;

        TabStripSceneLayerJni.get().updateStripScrim(mNativePtr, TabStripSceneLayer.this,
                scrim.getX(), scrim.getY(), scrim.getWidth() * mDpToPx, scrim.getHeight() * mDpToPx,
                scrim.getColor(), scrim.getAlpha());
    }

    private boolean shouldReaddBackground(int orientation) {
        // Sometimes layer trees do not get updated on rotation on Nexus 10.
        // This is a workaround that readds the background to prevent it.
        // See https://crbug.com/503930 for more.
        if (Build.MODEL == null || !Build.MODEL.contains("Nexus 10")) return false;
        if (mOrientation != orientation) {
            // This is a random number. Empirically this is enough.
            mNumReaddBackground = 10;
            mOrientation = orientation;
        }
        mNumReaddBackground--;
        return mNumReaddBackground >= 0;
    }

    private void pushButtonsAndBackground(StripLayoutHelperManager layoutHelper,
            ResourceManager resourceManager, float yOffset) {
        final float width = layoutHelper.getWidth() * mDpToPx;
        float height = layoutHelper.getHeight() * mDpToPx; // Vivaldi
        // Note(david@vivaldi.com): This will fix the empty line which only occurs on certain
        // devices between the bottom tab strip bar and the navigation bar. Still not 100% sure why
        // that happens but by adding the offset it fixes it. See ref. VAB-4399.
        height += VivaldiUtils.isTopToolbarOn() ? 0 : 1;
        TabStripSceneLayerJni.get().updateTabStripLayer(mNativePtr, TabStripSceneLayer.this, width,
                height, yOffset * mDpToPx, shouldReaddBackground(layoutHelper.getOrientation()),
                layoutHelper.getBackgroundColor());

        updateStripScrim(layoutHelper.getStripScrim());

        TintedCompositorButton newTabButton = layoutHelper.getNewTabButton();
        CompositorButton modelSelectorButton = layoutHelper.getModelSelectorButton();
        boolean newTabButtonVisible = newTabButton.isVisible();
        boolean modelSelectorButtonVisible = modelSelectorButton.isVisible();
        TabStripSceneLayerJni.get().updateNewTabButton(mNativePtr, TabStripSceneLayer.this,
                newTabButton.getResourceId(), newTabButton.getBackgroundResourceId(),
                newTabButton.getX() * mDpToPx, newTabButton.getY() * mDpToPx,
                layoutHelper.getNewTabBtnTouchTargetOffset() * mDpToPx, newTabButtonVisible,
                newTabButton.getTint(), newTabButton.getBackgroundTint(), newTabButton.getOpacity(),
                resourceManager);

        TabStripSceneLayerJni.get().updateModelSelectorButton(mNativePtr, TabStripSceneLayer.this,
                modelSelectorButton.getResourceId(), modelSelectorButton.getX() * mDpToPx,
                modelSelectorButton.getY() * mDpToPx, modelSelectorButton.getWidth() * mDpToPx,
                modelSelectorButton.getHeight() * mDpToPx, modelSelectorButton.isIncognito(),
                modelSelectorButtonVisible, modelSelectorButton.getOpacity(), resourceManager);

        boolean tabStripImprovementsEnabled = ChromeFeatureList.sTabStripImprovements.isEnabled();
        boolean showLeftTabStripFade = ChromeFeatureList.sTabStripRedesign.isEnabled()
                || !tabStripImprovementsEnabled || LocalizationUtils.isLayoutRtl();
        boolean showRightTabStripFade = ChromeFeatureList.sTabStripRedesign.isEnabled()
                || !tabStripImprovementsEnabled || !LocalizationUtils.isLayoutRtl();

        int tab_strip_fade_short = tabStripImprovementsEnabled ? R.drawable.tab_strip_fade_short
                                                               : R.drawable.tab_strip_fade;
        int tab_strip_fade_long = tabStripImprovementsEnabled
                ? R.drawable.tab_strip_fade_long
                : R.drawable.tab_strip_fade_for_model_selector;

        if (showLeftTabStripFade) {
            int leftFadeDrawable = modelSelectorButtonVisible && LocalizationUtils.isLayoutRtl()
                    ? tab_strip_fade_long
                    : tab_strip_fade_short;
            TabStripSceneLayerJni.get().updateTabStripLeftFade(mNativePtr, TabStripSceneLayer.this,
                    leftFadeDrawable, layoutHelper.getLeftFadeOpacity(), resourceManager,
                    layoutHelper.getBackgroundColor());
        }

        if (showRightTabStripFade) {
            int rightFadeDrawable = modelSelectorButtonVisible && !LocalizationUtils.isLayoutRtl()
                    ? tab_strip_fade_long
                    : tab_strip_fade_short;
            TabStripSceneLayerJni.get().updateTabStripRightFade(mNativePtr, TabStripSceneLayer.this,
                    rightFadeDrawable, layoutHelper.getRightFadeOpacity(), resourceManager,
                    layoutHelper.getBackgroundColor());
        }
    }

    private void pushStripTabs(StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager,
            StripLayoutTab[] stripTabs, int selectedTabId) {
        final int tabsCount = stripTabs != null ? stripTabs.length : 0;

        for (int i = 0; i < tabsCount; i++) {
            final StripLayoutTab st = stripTabs[i];
            boolean isSelected = st.getId() == selectedTabId;
            TabStripSceneLayerJni.get().putStripTabLayer(mNativePtr, TabStripSceneLayer.this,
                    st.getId(), st.getCloseButton().getResourceId(), st.getDividerResourceId(),
                    st.getResourceId(), st.getOutlineResourceId(), st.getCloseButton().getTint(),
                    st.getDividerTint(), st.getTint(isSelected), st.getOutlineTint(isSelected),
                    isSelected, st.getClosePressed(), layoutHelper.getWidth() * mDpToPx,
                    st.getDrawX() * mDpToPx, st.getDrawY() * mDpToPx, st.getWidth() * mDpToPx,
                    st.getHeight() * mDpToPx, st.getContentOffsetX() * mDpToPx,
                    st.getDividerOffsetX() * mDpToPx, st.getBottomMargin() * mDpToPx,
                    st.getCloseButton().getOpacity(), st.getDividerOpacity(), st.isLoading(),
                    st.getLoadingSpinnerRotation(), st.getBrightness(), st.getOpacity(isSelected),
                    layerTitleCache, resourceManager,
                    // Note(david@vivaldi.com): From here we pass the Vivaldi parameters.
                    st.getAlpha(), layoutHelper.getActiveStripLayoutHelper().showTabsAsFavIcon(),
                    st.getTitleOffset() * mDpToPx);
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    // Vivaldi
    public void setTabStripBackgroundColor(int tabStripBackgroundColor) {
        boolean useLightForegroundOnBackground =
                ColorUtils.shouldUseLightForegroundOnBackground(tabStripBackgroundColor);
        TabStripSceneLayerJni.get().setTabStripBackgroundColor(
                mNativePtr, this, tabStripBackgroundColor, useLightForegroundOnBackground);
    }

    /** Vivaldi **/
    public void shouldHideOverlay(boolean value) {
        mShouldHideOverlay = value;
    }

    /** Vivaldi **/
    public void setIsStackStrip(boolean isStackStrip) {
        mIsStackStrip = isStackStrip;
        TabStripSceneLayerJni.get().setIsStackStrip(mNativePtr, this, mIsStackStrip);
    }

    /** Vivaldi **/
    public void setMainYOffset(float yOffset) {
        mMainYOffset = yOffset;
    }

    /** Vivaldi **/
    public void updateLoadingState(ResourceManager resourceManager,
            StripLayoutHelperManager layoutHelper, boolean isIncognito, int color) {
        boolean useDark = ColorUtils.shouldUseLightForegroundOnBackground(color);
        TabStripSceneLayerJni.get().updateLoadingState(mNativePtr, TabStripSceneLayer.this,
                VivaldiUtils.getLoadingTabsResource(
                        mContext, isIncognito, useDark, resourceManager),
                resourceManager, layoutHelper.getActiveStripLayoutHelper().shouldShowLoading());
    }

    @NativeMethods
    public interface Natives {
        long init(TabStripSceneLayer caller);
        void beginBuildingFrame(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, boolean visible);
        void finishBuildingFrame(long nativeTabStripSceneLayer, TabStripSceneLayer caller);
        void updateTabStripLayer(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                float width, float height, float yOffset, boolean shouldReadBackground,
                @ColorInt int backgroundColor);
        void updateStripScrim(long nativeTabStripSceneLayer, TabStripSceneLayer caller, float x,
                float y, float width, float height, int color, float alpha);
        void updateNewTabButton(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int resourceId, int backgroundResourceId, float x, float y, float touchTargetOffset,
                boolean visible, int tint, int backgroundTint, float buttonAlpha,
                ResourceManager resourceManager);
        void updateModelSelectorButton(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int resourceId, float x, float y, float width, float height, boolean incognito,
                boolean visible, float buttonAlpha, ResourceManager resourceManager);
        void updateTabStripLeftFade(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int resourceId, float opacity, ResourceManager resourceManager,
                @ColorInt int leftFadeColor);
        void updateTabStripRightFade(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int resourceId, float opacity, ResourceManager resourceManager,
                @ColorInt int rightFadeColor);
        void putStripTabLayer(long nativeTabStripSceneLayer, TabStripSceneLayer caller, int id,
                int closeResourceId, int dividerResourceId, int handleResourceId,
                int handleOutlineResourceId, int closeTint, int dividerTint, int handleTint,
                int handleOutlineTint, boolean foreground, boolean closePressed, float toolbarWidth,
                float x, float y, float width, float height, float contentOffsetX,
                float dividerOffsetX, float bottomOffsetY, float closeButtonAlpha,
                float dividerAlpha, boolean isLoading, float spinnerRotation, float brightness,
                float opacity, LayerTitleCache layerTitleCache, ResourceManager resourceManager,
                float tabAlpha, boolean isShownAsFavicon, float titleOffset); // Vivaldi
        void setContentTree(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, SceneLayer contentTree);
        // Vivaldi
        void setTabStripBackgroundColor(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int nativeTabStripBackgroundColor, boolean nativeUseLightForegroundOnBackground);
        // Vivaldi
        void setIsStackStrip(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, boolean isStackStrip);
        // Vivaldi
        void updateLoadingState(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int resourceId, ResourceManager resourceManager, boolean shouldShowLoading);
    }
}
