// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.phone.SimpleAnimationLayout;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

// Vivaldi
import android.view.View;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.vivaldi.browser.preferences.VivaldiPreferences;

import java.util.ArrayList;
import java.util.List;

/**
 * {@link LayoutManagerChromePhone} is the specialization of {@link LayoutManagerChrome} for the
 * phone.
 */
public class LayoutManagerChromePhone extends LayoutManagerChrome {
    // Layouts
    private SimpleAnimationLayout mSimpleAnimationLayout;

    // Vivaldi
    private final List<StripLayoutHelperManager> mTabStrips = new ArrayList<>();
    private SharedPreferencesManager.Observer mPreferenceObserver;
    private boolean mTabStripAdded;
    /** A {@link TitleCache} instance that stores all title/favicon bitmaps as CC resources. */
    protected LayerTitleCache mLayerTitleCache;

    /**
     * Creates an instance of a {@link LayoutManagerChromePhone}.
     * @param host         A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param startSurfaceSupplier Supplier for an interface to talk to the Grid Tab Switcher when
     *         Start surface refactor is disabled. Used to create overviewLayout if it has value,
     *         otherwise will use the accessibility overview layout.
     * @param tabSwitcherSupplier Supplier for an interface to talk to the Grid Tab Switcher when
     *         Start surface refactor is enabled. Used to create overviewLayout if it has value,
     *         otherwise will use the accessibility overview layout.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for top
     *         controls.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     */
    public LayoutManagerChromePhone(LayoutManagerHost host, ViewGroup contentContainer,
            Supplier<StartSurface> startSurfaceSupplier, Supplier<TabSwitcher> tabSwitcherSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            ObservableSupplier<StripLayoutHelperManager.TabModelStartupInfo>
                    tabModelStartupInfoSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MultiInstanceManager multiInstanceManager, View toolbarContainerView) {
        super(host, contentContainer, startSurfaceSupplier, tabSwitcherSupplier,
                browserControlsStateProvider, tabContentManagerSupplier, topUiThemeColorProvider,
                null, null);

        // Note(david@vivaldi.com): We create two tab strips here. The first one is the main strip.
        // The second one is the stack strip.
        for (int i = 0; i < 2; i++) {
            mTabStrips.add(new StripLayoutHelperManager(mHost.getContext(), host, this,
                    mHost.getLayoutRenderHost(),
                    ()
                            -> mLayerTitleCache,
                    tabModelStartupInfoSupplier, lifecycleDispatcher, multiInstanceManager,
                    toolbarContainerView));
            mTabStrips.get(i).setIsStackStrip(i != 0);
            addObserver(mTabStrips.get(i).getTabSwitcherObserver());
        }

        mTabStripAdded = false;
        mPreferenceObserver = key -> {
            if (VivaldiPreferences.SHOW_TAB_STRIP.equals(key)
                    || VivaldiPreferences.ADDRESS_BAR_TO_BOTTOM.equals(key)) {
                updateGlobalSceneOverlay();
            }
        };
        SharedPreferencesManager.getInstance().addObserver(mPreferenceObserver);
        updateGlobalSceneOverlay();
        // End Vivaldi
    }

    @Override
    public void init(TabModelSelector selector, TabCreatorManager creator,
            ControlContainer controlContainer, DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider) {
        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        // Build Layouts
        mSimpleAnimationLayout = new SimpleAnimationLayout(context, this, renderHost);

        super.init(selector, creator, controlContainer, dynamicResourceLoader, topUiColorProvider);

        // Initialize Layouts
        TabContentManager tabContentManager = mTabContentManagerSupplier.get();
        assert tabContentManager != null;
        mSimpleAnimationLayout.setTabModelSelector(selector, tabContentManager);

        // Vivaldi
        for (int i = 0; i < 2; i++) mTabStrips.get(i).setTabModelSelector(selector, creator);

        // Vivaldi
        if (DeviceClassManager.enableLayerDecorationCache()) {
            mLayerTitleCache = new LayerTitleCache(mHost.getContext(), getResourceManager());
            mLayerTitleCache.setTabModelSelector(selector);
        }
    }

    @Override
    protected Layout getLayoutForType(int layoutType) {
        if (layoutType == LayoutType.SIMPLE_ANIMATION) {
            return mSimpleAnimationLayout;
        }
        return super.getLayoutForType(layoutType);
    }

    @Override
    public void onTabsAllClosing(boolean incognito) {
        // Vivaldi
        if (!BuildConfig.IS_VIVALDI)
        if (getActiveLayout() == mStaticLayout && !incognito) {
            startShowing(mTabSwitcherLayout != null ? mTabSwitcherLayout : mOverviewLayout,
                    /* animate= */ false);
        }
        super.onTabsAllClosing(incognito);
    }

    @Override
    protected void tabClosed(int id, int nextId, boolean incognito, boolean tabRemoved) {
        boolean showOverview = nextId == Tab.INVALID_TAB_ID;

        // Vivaldi
        if (BuildConfig.IS_VIVALDI)
            showOverview = false;

        if (getActiveLayoutType() != LayoutType.TAB_SWITCHER
                && getActiveLayoutType() != LayoutType.START_SURFACE && showOverview) {
            // Since there will be no 'next' tab to display, switch to
            // overview mode when the animation is finished.
            if (getActiveLayoutType() == LayoutType.SIMPLE_ANIMATION) {
                setNextLayout(getLayoutForType(LayoutType.TAB_SWITCHER), true);
                getActiveLayout().onTabClosed(time(), id, nextId, incognito);
            } else {
                super.tabClosed(id, nextId, incognito, tabRemoved);
            }
        }
    }

    @Override
    protected void tabCreating(int sourceId, boolean isIncognito) {
        if (getActiveLayout() != null && !getActiveLayout().isStartingToHide()
                && overlaysHandleTabCreating() && getActiveLayout().handlesTabCreating()) {
            // If the current layout in the foreground, let it handle the tab creation animation.
            // This check allows us to switch from the StackLayout to the SimpleAnimationLayout
            // smoothly.
            getActiveLayout().onTabCreating(sourceId);
        } else if (animationsEnabled()) {
            if (!isLayoutVisible(LayoutType.TAB_SWITCHER)
                    && !isLayoutVisible(LayoutType.START_SURFACE)) {
                if (getActiveLayout() != null && getActiveLayout().isStartingToHide()) {
                    setNextLayout(mSimpleAnimationLayout, true);
                    // The method Layout#doneHiding() will automatically show the next layout.
                    getActiveLayout().doneHiding();
                } else {
                    startShowing(mSimpleAnimationLayout, false);
                }
            }
            if (getActiveLayout() != null) {
                getActiveLayout().onTabCreating(sourceId);
            }
        }
    }

    /** @return Whether the {@link SceneOverlay}s handle tab creation. */
    private boolean overlaysHandleTabCreating() {
        Layout layout = getActiveLayout();
        if (layout == null || layout.getLayoutTabsToRender() == null
                || layout.getLayoutTabsToRender().length != 1) {
            return false;
        }
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;
            if (mSceneOverlays.get(i).handlesTabCreating()) {
                // Prevent animation from happening if the overlay handles creation.
                startHiding(layout.getLayoutTabsToRender()[0].getId(), false);
                doneHiding();
                return true;
            }
        }
        return false;
    }

    // Vivaldi: Update the {@link SceneOverlay} according to the tab strip setting.
    private void updateGlobalSceneOverlay() {
            if(!mTabStripAdded) {
                for (int i = 0; i < 2; i++) addSceneOverlay(mTabStrips.get(i));
                if (getTabModelSelector() != null)
                    tabModelSwitched(getTabModelSelector().isIncognitoSelected()); //!! TODO(jarle): CHECK IF THIS IS CORRECT
                mTabStripAdded = true;
            }
    }

    // Vivaldi
    @Override
    public void destroy() {
        super.destroy();
        // Vivaldi
        for (int i = 0; i < 2; i++) mTabStrips.get(i).destroy();
        mTabStrips.clear();

        SharedPreferencesManager.getInstance().removeObserver(mPreferenceObserver);
    }

    // Vivaldi
    @Override
    protected void emptyTabCachesExcept(int tabId) {
        super.emptyTabCachesExcept(tabId);
        if (mLayerTitleCache != null) mLayerTitleCache.clearExcept(tabId);
    }

    // Vivaldi
    @Override
    public void initLayoutTabFromHost(final int tabId) {
        if (mLayerTitleCache != null) {
            mLayerTitleCache.remove(tabId);
        }
        super.initLayoutTabFromHost(tabId);
    }

    // Vivaldi
    @Override
    public void releaseTabLayout(int id) {
        mLayerTitleCache.remove(id);
        super.releaseTabLayout(id);
    }

    // Vivaldi
    @Override
    public void releaseResourcesForTab(int tabId) {
        super.releaseResourcesForTab(tabId);
        mLayerTitleCache.remove(tabId);
    }

    @Override
    protected void tabModelSwitched(boolean incognito) {
        super.tabModelSwitched(incognito);
        getTabModelSelector().commitAllTabClosures();
    }

    /** Vivaldi **/
    @Override
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        // We always return our main strip here.
        return mTabStrips.get(0);
    }
}
