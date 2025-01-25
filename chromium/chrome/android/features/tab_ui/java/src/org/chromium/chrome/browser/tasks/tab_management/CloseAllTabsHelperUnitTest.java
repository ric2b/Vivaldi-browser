// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

/** Unit test for {@link CloseAllTabsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CloseAllTabsHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabGroupModelFilter mRegularTabGroupModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock private TabModel mRegularTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TabSwitcher mRegularTabSwitcher;
    @Mock private TabSwitcher mIncognitoTabSwitcher;
    @Mock private HubManager mHubManager;
    @Mock private PaneManager mPaneManager;
    @Mock private Pane mPane;

    private LazyOneshotSupplier<HubManager> mHubManagerSupplier;
    private final OneshotSupplierImpl<TabSwitcher> mRegularTabSwitcherSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<TabSwitcher> mIncognitoTabSwitcherSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHubVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Pane> mFocusedPaneSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false))
                .thenReturn(mRegularTabGroupModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true))
                .thenReturn(mIncognitoTabGroupModelFilter);
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        mRegularTabSwitcherSupplier.set(mRegularTabSwitcher);
        mIncognitoTabSwitcherSupplier.set(mIncognitoTabSwitcher);

        mHubVisibilitySupplier.set(false);
        when(mHubManager.getHubVisibilitySupplier()).thenReturn(mHubVisibilitySupplier);

        when(mPane.getPaneId()).thenReturn(PaneId.BOOKMARKS);
        mFocusedPaneSupplier.set(mPane);
        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(mFocusedPaneSupplier);
        when(mHubManager.getPaneManager()).thenReturn(mPaneManager);

        mHubManagerSupplier = LazyOneshotSupplier.fromValue(mHubManager);
        assertNotNull(mHubManagerSupplier.get());
    }

    private void setAnimationStateForTesting(
            boolean quickDeleteState, boolean customAnimationState) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.GTS_CLOSE_TAB_ANIMATION, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.GTS_CLOSE_TAB_ANIMATION,
                "close_all_quick_delete_animation",
                quickDeleteState ? "true" : "false");
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.GTS_CLOSE_TAB_ANIMATION,
                "close_all_custom_animation",
                customAnimationState ? "true" : "false");
        FeatureList.setTestValues(testValues);

        assertEquals(
                quickDeleteState,
                ChromeFeatureList.sGtsCloseTabAnimationCloseAllQuickDeleteAnimation.getValue());
        assertEquals(
                customAnimationState,
                ChromeFeatureList.sGtsCloseTabAnimationCloseAllCustomAnimation.getValue());
    }

    @Test
    public void testCloseAllTabsHidingTabGroups() {
        CloseAllTabsHelper.closeAllTabsHidingTabGroups(mTabModelSelector);

        verify(mRegularTabGroupModelFilter).closeAllTabs(false, true);
        verify(mIncognitoTabGroupModelFilter).closeAllTabs(false, true);
    }

    @Test
    public void testBuildCloseAllTabsRunnable_RegularDefault() {
        setAnimationStateForTesting(false, false);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ false);
        r.run();

        verify(mRegularTabGroupModelFilter).closeAllTabs(false, true);
        verify(mIncognitoTabGroupModelFilter).closeAllTabs(false, true);

        verifyNoInteractions(mRegularTabSwitcher);
        verifyNoInteractions(mIncognitoTabSwitcher);
        verify(mIncognitoTabModel, never()).closeAllTabs();
    }

    @Test
    public void testBuildCloseAllTabsRunnable_IncognitoDefault() {
        setAnimationStateForTesting(false, false);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ true);
        r.run();

        verify(mIncognitoTabModel).closeAllTabs(false);

        verifyNoInteractions(mRegularTabSwitcher);
        verifyNoInteractions(mIncognitoTabSwitcher);
        verify(mRegularTabGroupModelFilter, never()).closeAllTabs(anyBoolean(), anyBoolean());
        verify(mIncognitoTabGroupModelFilter, never()).closeAllTabs(anyBoolean(), anyBoolean());
    }

    @Test
    public void testBuildCloseAllTabsRunnable_AnimationFallback_HubVisibleWrongPane() {
        setAnimationStateForTesting(true, true);
        when(mPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mHubVisibilitySupplier.set(true);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ true);
        r.run();

        verify(mIncognitoTabModel).closeAllTabs(false);

        verifyNoInteractions(mRegularTabSwitcher);
        verifyNoInteractions(mIncognitoTabSwitcher);
        verify(mRegularTabGroupModelFilter, never()).closeAllTabs(anyBoolean(), anyBoolean());
        verify(mIncognitoTabGroupModelFilter, never()).closeAllTabs(anyBoolean(), anyBoolean());
    }

    @Test
    public void testBuildCloseAllTabsRunnable_AnimationFallback_HubInvisibleRightPane() {
        setAnimationStateForTesting(true, true);
        when(mPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ false);
        r.run();

        verify(mRegularTabGroupModelFilter).closeAllTabs(false, true);
        verify(mIncognitoTabGroupModelFilter).closeAllTabs(false, true);

        verifyNoInteractions(mRegularTabSwitcher);
        verifyNoInteractions(mIncognitoTabSwitcher);
        verify(mIncognitoTabModel, never()).closeAllTabs();
    }

    @Test
    public void testBuildCloseAllTabsRunnable_QuickDeleteAnimation_IncognitoOnly() {
        setAnimationStateForTesting(true, false);
        mHubVisibilitySupplier.set(true);
        when(mPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ true);

        doCallback(0, (Runnable onAnimationEnd) -> onAnimationEnd.run())
                .when(mIncognitoTabSwitcher)
                .showQuickDeleteAnimation(any(), any());
        r.run();

        verify(mIncognitoTabSwitcher).showQuickDeleteAnimation(any(), any());
        verify(mIncognitoTabModel).closeAllTabs(false);

        verifyNoInteractions(mRegularTabSwitcher);
        verify(mRegularTabGroupModelFilter, never()).closeAllTabs(anyBoolean(), anyBoolean());
        verify(mIncognitoTabGroupModelFilter, never()).closeAllTabs(anyBoolean(), anyBoolean());
    }

    @Test
    public void testBuildCloseAllTabsRunnable_QuickDeleteAnimation_All_RegularPane() {
        setAnimationStateForTesting(true, false);
        mHubVisibilitySupplier.set(true);
        when(mPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ false);

        doCallback(0, (Runnable onAnimationEnd) -> onAnimationEnd.run())
                .when(mRegularTabSwitcher)
                .showQuickDeleteAnimation(any(), any());
        r.run();

        verify(mRegularTabSwitcher).showQuickDeleteAnimation(any(), any());
        verify(mRegularTabGroupModelFilter).closeAllTabs(false, true);
        verify(mIncognitoTabGroupModelFilter).closeAllTabs(false, true);

        verifyNoInteractions(mIncognitoTabSwitcher);
        verify(mIncognitoTabModel, never()).closeAllTabs();
    }

    @Test
    public void testBuildCloseAllTabsRunnable_QuickDeleteAnimation_All_IncognitoPane() {
        setAnimationStateForTesting(true, false);
        mHubVisibilitySupplier.set(true);
        when(mPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ false);

        doCallback(0, (Runnable onAnimationEnd) -> onAnimationEnd.run())
                .when(mIncognitoTabSwitcher)
                .showQuickDeleteAnimation(any(), any());
        r.run();

        verify(mIncognitoTabSwitcher).showQuickDeleteAnimation(any(), any());
        verify(mRegularTabGroupModelFilter).closeAllTabs(false, true);
        verify(mIncognitoTabGroupModelFilter).closeAllTabs(false, true);

        verifyNoInteractions(mRegularTabSwitcher);
        verify(mIncognitoTabModel, never()).closeAllTabs();
    }

    @Test
    public void testBuildCloseAllTabsRunnable_CustomAnimation_IncognitoOnly() {
        setAnimationStateForTesting(false, true);
        mHubVisibilitySupplier.set(true);
        when(mPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ true);

        doCallback(0, (Runnable onAnimationEnd) -> onAnimationEnd.run())
                .when(mIncognitoTabSwitcher)
                .showCloseAllTabsAnimation(any());
        r.run();

        verify(mIncognitoTabSwitcher).showCloseAllTabsAnimation(any());
        verify(mIncognitoTabModel).closeAllTabs(false);

        verifyNoInteractions(mRegularTabSwitcher);
        verify(mRegularTabGroupModelFilter, never()).closeAllTabs(anyBoolean(), anyBoolean());
        verify(mIncognitoTabGroupModelFilter, never()).closeAllTabs(anyBoolean(), anyBoolean());
    }

    @Test
    public void testBuildCloseAllTabsRunnable_CustomAnimation_All_RegularPane() {
        setAnimationStateForTesting(false, true);
        mHubVisibilitySupplier.set(true);
        when(mPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ false);

        doCallback(0, (Runnable onAnimationEnd) -> onAnimationEnd.run())
                .when(mRegularTabSwitcher)
                .showCloseAllTabsAnimation(any());
        r.run();

        verify(mRegularTabSwitcher).showCloseAllTabsAnimation(any());
        verify(mRegularTabGroupModelFilter).closeAllTabs(false, true);
        verify(mIncognitoTabGroupModelFilter).closeAllTabs(false, true);

        verifyNoInteractions(mIncognitoTabSwitcher);
        verify(mIncognitoTabModel, never()).closeAllTabs();
    }

    @Test
    public void testBuildCloseAllTabsRunnable_CloseAllAnimation_All_IncognitoPane() {
        setAnimationStateForTesting(false, true);
        mHubVisibilitySupplier.set(true);
        when(mPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mHubManagerSupplier,
                        mRegularTabSwitcherSupplier,
                        mIncognitoTabSwitcherSupplier,
                        mTabModelSelector,
                        /* isIncognitoOnly= */ false);

        doCallback(0, (Runnable onAnimationEnd) -> onAnimationEnd.run())
                .when(mIncognitoTabSwitcher)
                .showCloseAllTabsAnimation(any());
        r.run();

        verify(mIncognitoTabSwitcher).showCloseAllTabsAnimation(any());
        verify(mRegularTabGroupModelFilter).closeAllTabs(false, true);
        verify(mIncognitoTabGroupModelFilter).closeAllTabs(false, true);

        verifyNoInteractions(mRegularTabSwitcher);
        verify(mIncognitoTabModel, never()).closeAllTabs();
    }
}
