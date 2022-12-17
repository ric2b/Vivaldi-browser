// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.view.ViewGroup;

import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

// Vivaldi
import java.util.ArrayList;
import java.util.List;

/**
 * {@link LayoutManagerChromeTablet} is the specialization of {@link LayoutManagerChrome} for
 * the tablet.
 */
public class LayoutManagerChromeTablet extends LayoutManagerChrome {

    private StripLayoutHelperManager mTabStripLayoutHelperManager;

    // Vivaldi
    private final List<StripLayoutHelperManager> mTabStrips = new ArrayList<>();

    // Internal State
    /** A {@link TitleCache} instance that stores all title/favicon bitmaps as CC resources. */
    protected LayerTitleCache mLayerTitleCache;

    /**
     * Creates an instance of a {@link LayoutManagerChromePhone}.
     * @param host                     A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param startSurface An interface to talk to the Grid Tab Switcher.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param overviewModeBehaviorSupplier Supplier of the {@link OverviewModeBehavior}.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     */
    public LayoutManagerChromeTablet(LayoutManagerHost host, ViewGroup contentContainer,
            StartSurface startSurface,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            OneshotSupplierImpl<OverviewModeBehavior> overviewModeBehaviorSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider, JankTracker jankTracker) {
        super(host, contentContainer,
                TabUiFeatureUtilities.isGridTabSwitcherEnabled(host.getContext()), startSurface,
                tabContentManagerSupplier, overviewModeBehaviorSupplier, topUiThemeColorProvider,
                jankTracker);

        mTabStripLayoutHelperManager = new StripLayoutHelperManager(host.getContext(), this,
                mHost.getLayoutRenderHost(), () -> mLayerTitleCache);

        // Note(david@vivaldi.com): We create two tab strips here. The first one is the main strip.
        // The second one is the stack strip.
        for (int i = 0; i < 2; i++) {
            mTabStrips.add(new StripLayoutHelperManager(mHost.getContext(), this,
                    mHost.getLayoutRenderHost(), () -> mLayerTitleCache));
            mTabStrips.get(i).setIsStackStrip(i != 0);
            addSceneOverlay(mTabStrips.get(i));
        }

        addObserver(mTabStripLayoutHelperManager.getTabSwitcherObserver());

        setNextLayout(null, true);
    }

    @Override
    public void destroy() {
        super.destroy();
        // Vivaldi
        for (int i = 0; i < 2; i++) mTabStrips.get(i).destroy();
        mTabStrips.clear();
    }

    @Override
    protected void tabCreated(int id, int sourceId, @TabLaunchType int launchType,
            boolean incognito, boolean willBeSelected, float originX, float originY) {
        if (getBrowserControlsManager() != null) {
            getBrowserControlsManager().getBrowserVisibilityDelegate().showControlsTransient();
        }
        super.tabCreated(id, sourceId, launchType, incognito, willBeSelected, originX, originY);
    }

    @Override
    protected void tabModelSwitched(boolean incognito) {
        super.tabModelSwitched(incognito);
        getTabModelSelector().commitAllTabClosures();
    }

    @Override
    public void init(TabModelSelector selector, TabCreatorManager creator,
            ControlContainer controlContainer,
            DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider) {

        // Vivaldi
        for (int i = 0; i < 2; i++) mTabStrips.get(i).setTabModelSelector(selector, creator);

        super.init(selector, creator, controlContainer, dynamicResourceLoader, topUiColorProvider);

        if (DeviceClassManager.enableLayerDecorationCache()) {
            mLayerTitleCache = new LayerTitleCache(mHost.getContext(), getResourceManager());
            // TODO: TitleCache should be a part of the ResourceManager.
            mLayerTitleCache.setTabModelSelector(selector);
        }

        if (mTabStripLayoutHelperManager != null) {
            mTabStripLayoutHelperManager.setTabModelSelector(selector, creator);
        }
    }

    @Override
    protected void emptyCachesExcept(int tabId) {
        super.emptyCachesExcept(tabId);
        if (mLayerTitleCache != null) mLayerTitleCache.clearExcept(tabId);
    }

    @Override
    public void initLayoutTabFromHost(final int tabId) {
        if (mLayerTitleCache != null) {
            mLayerTitleCache.remove(tabId);
        }
        super.initLayoutTabFromHost(tabId);
    }

    @Override
    public void releaseTabLayout(int id) {
        mLayerTitleCache.remove(id);
        super.releaseTabLayout(id);
    }

    @Override
    public void releaseResourcesForTab(int tabId) {
        super.releaseResourcesForTab(tabId);
        mLayerTitleCache.remove(tabId);
    }

    @Override
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        return mTabStripLayoutHelperManager;
    }
}
