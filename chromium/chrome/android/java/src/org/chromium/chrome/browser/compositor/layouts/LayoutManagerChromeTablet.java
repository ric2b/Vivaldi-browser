// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.TabModelStartupInfo;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.hub.HubLayout;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

// Vivaldi
import java.util.ArrayList;
import java.util.List;
import org.chromium.chrome.browser.ChromeApplicationImpl;

/**
 * {@link LayoutManagerChromeTablet} is the specialization of {@link LayoutManagerChrome} for the
 * tablet.
 */
public class LayoutManagerChromeTablet extends LayoutManagerChrome {
    // Tab Strip
    private StripLayoutHelperManager mTabStripLayoutHelperManager;

    // Vivaldi
    private final List<StripLayoutHelperManager> mTabStrips = new ArrayList<>();

    // Internal State
    /** A {@link LayerTitleCache} instance that stores all title/favicon bitmaps as CC resources. */
    // This cache should not be cleared in LayoutManagerImpl#emptyCachesExcept(), since that method
    // is currently called when returning to the static layout, which is when these titles will be
    // visible. See https://crbug.com/1329293.
    protected LayerTitleCache mLayerTitleCache;

    protected ObservableSupplierImpl<LayerTitleCache> mLayerTitleCacheSupplier =
            new ObservableSupplierImpl<>();

    /**
     * Creates an instance of a {@link LayoutManagerChromePhone}.
     *
     * @param host A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param tabSwitcherSupplier Supplier for an interface to talk to the Grid Tab Switcher.
     * @param tabModelSelectorSupplier Supplier for an interface to talk to the Tab Model Selector.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for top
     *     controls.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param lifecycleDispatcher @{@link ActivityLifecycleDispatcher} to be passed to TabStrip
     *     helper.
     * @param hubLayoutDependencyHolder The dependency holder for creating {@link HubLayout}.
     * @param multiInstanceManager @{link MultiInstanceManager} passed to @{link StripLayoutHelper}
     *     to support tab drag and drop.
     * @param dragAndDropDelegate @{@link DragAndDropDelegate} passed to {@link
     *     StripLayoutHelperManager} to initiate tab drag and drop.
     * @param toolbarContainerView @{link View} passed to @{link StripLayoutHelper} to support tab
     *     drag and drop.
     * @param tabHoverCardViewStub The {@link ViewStub} representing the strip tab hover card.
     * @param toolbarManager The {@link ToolbarManager} instance.
     * @param desktopWindowStateProvider The {@link DesktopWindowStateProvider} for the app header.
     */
    public LayoutManagerChromeTablet(
            LayoutManagerHost host,
            ViewGroup contentContainer,
            Supplier<TabSwitcher> tabSwitcherSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            ObservableSupplier<TabModelStartupInfo> tabModelStartupInfoSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            HubLayoutDependencyHolder hubLayoutDependencyHolder,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragAndDropDelegate,
            View toolbarContainerView,
            @NonNull ViewStub tabHoverCardViewStub,
            @NonNull WindowAndroid windowAndroid,
            @NonNull ToolbarManager toolbarManager,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider,
            ActionConfirmationManager actionConfirmationManager,
            @NonNull ViewStub tabHoverCardViewStubStack) { // Vivaldi
        super(
                host,
                contentContainer,
                tabSwitcherSupplier,
                tabModelSelectorSupplier,
                tabContentManagerSupplier,
                topUiThemeColorProvider,
                hubLayoutDependencyHolder);
        if (!ChromeApplicationImpl.isVivaldi()) {
        mTabStripLayoutHelperManager =
                new StripLayoutHelperManager(
                        host.getContext(),
                        host,
                        this,
                        mHost.getLayoutRenderHost(),
                        mLayerTitleCacheSupplier,
                        tabModelStartupInfoSupplier,
                        lifecycleDispatcher,
                        multiInstanceManager,
                        dragAndDropDelegate,
                        toolbarContainerView,
                        tabHoverCardViewStub,
                        tabContentManagerSupplier,
                        browserControlsStateProvider,
                        windowAndroid,
                        toolbarManager,
                        desktopWindowStateProvider,
                        actionConfirmationManager);
        addSceneOverlay(mTabStripLayoutHelperManager);
        addObserver(mTabStripLayoutHelperManager.getTabSwitcherObserver());
        mDesktopWindowStateProvider = desktopWindowStateProvider;
        } // vivaldi

        // Note(david@vivaldi.com): We create two tab strips here. The first one is the main strip.
        // The second one is the stack strip.
        for (int i = 0; i < 2; i++) {
            mTabStrips.add(new StripLayoutHelperManager(host.getContext(), host, this,
                    mHost.getLayoutRenderHost(), new ObservableSupplierImpl<>(mLayerTitleCache),
                    tabModelStartupInfoSupplier, lifecycleDispatcher, multiInstanceManager,
                    dragAndDropDelegate, toolbarContainerView,
                    i == 0 ? tabHoverCardViewStub : tabHoverCardViewStubStack,
                    tabContentManagerSupplier, browserControlsStateProvider, windowAndroid,
                    toolbarManager, desktopWindowStateProvider, actionConfirmationManager));
            mTabStrips.get(i).setIsStackStrip(i != 0);
            addObserver(mTabStrips.get(i).getTabSwitcherObserver());
            addSceneOverlay(mTabStrips.get(i));
        }

        setNextLayout(null, true);
    }

    @Override
    public void destroy() {
        super.destroy();

        if (mLayerTitleCache != null) {
            mLayerTitleCache.shutDown();
            mLayerTitleCache = null;
        }

        if (mTabStripLayoutHelperManager != null) {
            removeObserver(mTabStripLayoutHelperManager.getTabSwitcherObserver());
            mTabStripLayoutHelperManager.destroy();
            mTabStripLayoutHelperManager = null;
        }

        // Vivaldi
        for (int i = 0; i < 2; i++) mTabStrips.get(i).destroy();
        mTabStrips.clear();

        if (mTopUiThemeColorProvider != null && mThemeColorObserver != null) {
            mTopUiThemeColorProvider.removeThemeColorObserver(mThemeColorObserver);
            mTopUiThemeColorProvider = null;
            mThemeColorObserver = null;
        }
    }

    @Override
    protected void tabCreated(
            int id,
            int sourceId,
            @TabLaunchType int launchType,
            boolean incognito,
            boolean willBeSelected,
            float originX,
            float originY) {
        if (getBrowserControlsManager() != null) {
            getBrowserControlsManager().getBrowserVisibilityDelegate().showControlsTransient();
        }
        super.tabCreated(id, sourceId, launchType, incognito, willBeSelected, originX, originY);
    }

    @Override
    public void onTabsAllClosing(boolean incognito) {
        if (getActiveLayout() == mStaticLayout && !incognito) {
            showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
        }
        super.onTabsAllClosing(incognito);
    }

    @Override
    protected void tabModelSwitched(boolean incognito) {
        super.tabModelSwitched(incognito);
        getTabModelSelector().commitAllTabClosures();
        if (getActiveLayout() == mStaticLayout
                && !incognito
                && getTabModelSelector().getModel(false).getCount() == 0
                && getNextLayoutType() != LayoutType.TAB_SWITCHER) {
            showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
        }
    }

    @Override
    public void init(
            TabModelSelector selector,
            TabCreatorManager creator,
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
            mLayerTitleCacheSupplier.set(mLayerTitleCache);
        }

        if (mTabStripLayoutHelperManager != null) {
            mTabStripLayoutHelperManager.setTabModelSelector(selector, creator);
        }
    }

    @Override
    public void releaseResourcesForTab(int tabId) {
        super.releaseResourcesForTab(tabId);
        // Vivaldi
        if (mLayerTitleCache != null)
        mLayerTitleCache.removeTabTitle(tabId);
    }

    @Override
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        // Vivaldi: We always return our main strip here.
        if (ChromeApplicationImpl.isVivaldi()) return mTabStrips.get(0);
        return mTabStripLayoutHelperManager;
    }
}
