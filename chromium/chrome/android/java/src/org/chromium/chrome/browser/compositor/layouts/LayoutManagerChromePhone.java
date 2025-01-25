// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.phone.SimpleAnimationLayout;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

// Vivaldi
import android.content.SharedPreferences;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;

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
    /** A {@link TitleCache} instance that stores all title/favicon bitmaps as CC resources. */
    protected LayerTitleCache mLayerTitleCache;

    /**
     * Creates an instance of a {@link LayoutManagerChromePhone}.
     *
     * @param host A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param tabSwitcherSupplier Supplier for an interface to talk to the Grid Tab Switcher. Used
     *     to create overviewLayout if it has value, otherwise will use the accessibility overview
     *     layout.
     * @param tabModelSelectorSupplier Supplier for an interface to talk to the Tab Model Selector.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param hubLayoutDependencyHolder The dependency holder for creating {@link HubLayout}.
     */
    public LayoutManagerChromePhone(
            LayoutManagerHost host,
            ViewGroup contentContainer,
            Supplier<TabSwitcher> tabSwitcherSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            HubLayoutDependencyHolder hubLayoutDependencyHolder,
            ObservableSupplier<StripLayoutHelperManager.TabModelStartupInfo>  // Vivaldi
                    tabModelStartupInfoSupplier,  // Vivaldi
            ActivityLifecycleDispatcher lifecycleDispatcher, // Vivaldi
            MultiInstanceManager multiInstanceManager,  // Vivaldi
            DragAndDropDelegate dragAndDropDelegate, // Vivaldi
            View toolbarContainerView, //Vivaldi
            ViewStub tabHoverCardViewStub, // Vivaldi
            ViewStub tabHoverCardViewStubStack, // Vivaldi
            WindowAndroid windowAndroid, // Vivaldi
            ToolbarManager toolbarManager,
            DesktopWindowStateManager desktopWindowStateManager,
            ActionConfirmationManager actionConfirmationManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            ModalDialogManager modalDialogManager, // Vivaldi
            DataSharingTabManager dataSharingTabManager) { //Vivaldi
        super(
                host,
                contentContainer,
                tabSwitcherSupplier,
                tabModelSelectorSupplier,
                tabContentManagerSupplier,
                topUiThemeColorProvider,
                hubLayoutDependencyHolder);

        // Note(david@vivaldi.com): We create two tab strips here. The first one is the main strip.
        // The second one is the stack strip.
        for (int i = 0; i < 2; i++) {
            mTabStrips.add(new StripLayoutHelperManager(mHost.getContext(), host, this,
                    mHost.getLayoutRenderHost(), new ObservableSupplierImpl<>(mLayerTitleCache),
                    tabModelStartupInfoSupplier, lifecycleDispatcher, multiInstanceManager,
                    dragAndDropDelegate, toolbarContainerView,
                    i == 0 ? tabHoverCardViewStub : tabHoverCardViewStubStack,
                    tabContentManagerSupplier, browserControlsStateProvider, windowAndroid,
                    toolbarManager, desktopWindowStateManager, actionConfirmationManager,
                    modalDialogManager, dataSharingTabManager));
            mTabStrips.get(i).setIsStackStrip(i != 0);
            addObserver(mTabStrips.get(i).getTabSwitcherObserver());
        }
        updateGlobalSceneOverlay();
        // End Vivaldi
    }

    @Override
    public void init(
            TabModelSelector selector,
            TabCreatorManager creator,
            ControlContainer controlContainer,
            DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider,
            Supplier<Integer> bottomControlsOffsetSupplier) {
        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        // Build Layouts
        mSimpleAnimationLayout =
                new SimpleAnimationLayout(context, this, renderHost, getContentContainer());

        super.init(
                selector,
                creator,
                controlContainer,
                dynamicResourceLoader,
                topUiColorProvider,
                bottomControlsOffsetSupplier);

        // Initialize Layouts
        TabContentManager tabContentManager = mTabContentManagerSupplier.get();
        assert tabContentManager != null;
        mSimpleAnimationLayout.setTabModelSelector(selector);
        mSimpleAnimationLayout.setTabContentManager(tabContentManager);

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
    protected void tabClosed(int id, int nextId, boolean incognito, boolean tabRemoved) {
        boolean showOverview = nextId == Tab.INVALID_TAB_ID;

        // Vivaldi
        if (BuildConfig.IS_VIVALDI)
            showOverview = false;

        if (getActiveLayoutType() != LayoutType.TAB_SWITCHER && showOverview) {
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
        if (getActiveLayout() != null
                && !getActiveLayout().isStartingToHide()
                && overlaysHandleTabCreating()
                && getActiveLayout().handlesTabCreating()) {
            // If the current layout in the foreground, let it handle the tab creation animation.
            // This check allows us to switch from the StackLayout to the SimpleAnimationLayout
            // smoothly.
            getActiveLayout().onTabCreating(sourceId);
        } else if (animationsEnabled()) {
            if (!isLayoutVisible(LayoutType.TAB_SWITCHER)) {
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
        if (layout == null
                || layout.getLayoutTabsToRender() == null
                || layout.getLayoutTabsToRender().length != 1) {
            return false;
        }
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;
            if (mSceneOverlays.get(i).handlesTabCreating()) {
                // Prevent animation from happening if the overlay handles creation.
                startHiding();
                doneHiding();
                return true;
            }
        }
        return false;
    }

    // Vivaldi: Update the {@link SceneOverlay} according to the tab strip setting.
    private void updateGlobalSceneOverlay() {
        for (int i = 0; i < 2; i++) addSceneOverlay(mTabStrips.get(i));
        if (getTabModelSelector() != null)
            tabModelSwitched(getTabModelSelector().isIncognitoSelected());
    }

    // Vivaldi
    @Override
    public void destroy() {
        super.destroy();
        // Vivaldi
        for (int i = 0; i < 2; i++) mTabStrips.get(i).destroy();
        mTabStrips.clear();
    }

    // Vivaldi
    @Override
    public void initLayoutTabFromHost(final int tabId) {
        if (mLayerTitleCache != null) {
            mLayerTitleCache.removeTabTitle(tabId);
        }
        super.initLayoutTabFromHost(tabId);
    }

    // Vivaldi
    @Override
    public void releaseTabLayout(int id) {
        if (mLayerTitleCache != null) {
            mLayerTitleCache.removeTabTitle(id);
        }
        super.releaseTabLayout(id);
    }

    // Vivaldi
    @Override
    public void releaseResourcesForTab(int tabId) {
        super.releaseResourcesForTab(tabId);
        if (mLayerTitleCache != null) {
            mLayerTitleCache.removeTabTitle(tabId);
        }
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
