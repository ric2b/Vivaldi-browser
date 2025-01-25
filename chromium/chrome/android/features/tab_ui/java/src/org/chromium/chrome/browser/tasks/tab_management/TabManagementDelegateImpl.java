// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.util.Pair;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;

/** Impl class that will resolve components for tab management. */
public class TabManagementDelegateImpl implements TabManagementDelegate {
    @Override
    public TabGroupUi createTabGroupUi(
            @NonNull Activity activity,
            @NonNull ViewGroup parentView,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull IncognitoStateProvider incognitoStateProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            ViewGroup rootView,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager) {
        return new TabGroupUiCoordinator(
                activity,
                parentView,
                browserControlsStateProvider,
                incognitoStateProvider,
                scrimCoordinator,
                omniboxFocusStateSupplier,
                bottomSheetController,
                tabModelSelector,
                tabContentManager,
                rootView,
                tabCreatorManager,
                layoutStateProviderSupplier,
                snackbarManager,
                modalDialogManager);
    }

    @Override
    public Pair<TabSwitcher, Pane> createTabSwitcherPane(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator rootUiScrimCoordinator,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull BottomSheetController bottomSheetController,
            @Nullable OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @NonNull OnClickListener newTabButtonOnClickListener,
            boolean isIncognito,
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull BackPressManager backPressManager) {
        // TODO(crbug.com/40946413): Consider making this an activity scoped singleton and possibly
        // hosting it in CTA/HubProvider.
        TabSwitcherPaneCoordinatorFactory factory =
                new TabSwitcherPaneCoordinatorFactory(
                        activity,
                        lifecycleDispatcher,
                        profileProviderSupplier,
                        tabModelSelector,
                        tabContentManager,
                        tabCreatorManager,
                        browserControlsStateProvider,
                        multiWindowModeStateDispatcher,
                        rootUiScrimCoordinator,
                        snackbarManager,
                        modalDialogManager,
                        bottomSheetController,
                        backPressManager);
        TabSwitcherPaneBase pane;
        if (isIncognito) {
            Supplier<TabModelFilter> incongitorTabModelFilterSupplier =
                    () -> tabModelSelector.getTabModelFilterProvider().getTabModelFilter(true);
            pane =
                    new IncognitoTabSwitcherPane(
                            activity,
                            factory,
                            incongitorTabModelFilterSupplier,
                            newTabButtonOnClickListener,
                            incognitoReauthControllerSupplier,
                            onToolbarAlphaChange);
        } else {
            Supplier<TabModelFilter> tabModelFilterSupplier =
                    () -> tabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
            OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
            profileProviderSupplier.onAvailable(
                    (profileProvider) -> profileSupplier.set(profileProvider.getOriginalProfile()));
            Handler handler = new Handler();
            UserEducationHelper userEducationHelper =
                    new UserEducationHelper(activity, profileSupplier, handler);
            pane =
                    new TabSwitcherPane(
                            activity,
                            ContextUtils.getAppSharedPreferences(),
                            profileProviderSupplier,
                            factory,
                            tabModelFilterSupplier,
                            newTabButtonOnClickListener,
                            new TabSwitcherPaneDrawableCoordinator(activity, tabModelSelector),
                            onToolbarAlphaChange,
                            userEducationHelper);
        }
        return Pair.create(pane, pane);
    }

    @Override
    public Pane createTabGroupsPane(
            @NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull OneshotSupplier<HubManager> hubManagerSupplier,
            @NonNull Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        LazyOneshotSupplier<TabModelFilter> tabModelFilterSupplier =
                LazyOneshotSupplier.fromSupplier(
                        () ->
                                tabModelSelector
                                        .getTabModelFilterProvider()
                                        .getTabModelFilter(false));
        return new TabGroupsPane(
                context,
                tabModelFilterSupplier,
                onToolbarAlphaChange,
                profileProviderSupplier,
                () -> hubManagerSupplier.get().getPaneManager(),
                tabGroupUiActionHandlerSupplier,
                modalDialogManagerSupplier);
    }
}
