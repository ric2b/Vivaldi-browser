// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link TabGroupContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_STRIP_GROUP_CONTEXT_MENU,
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID
})
public class TabGroupContextMenuCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    private OnItemClickedCallback mOnItemClickedCallback;
    private int mTabId;
    @Mock private View mMenuView;
    @Mock private Supplier<TabModel> mTabModelSupplier;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private Profile mProfile;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private TabCreator mTabCreator;
    @Captor private ArgumentCaptor<Callback<Integer>> mConfirmationResultCaptor;
    @Mock private TabModel mTabModel;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mMenuView = inflater.inflate(R.layout.tab_strip_group_menu_layout, null);
        mTabId = 1;
        mOnItemClickedCallback =
                TabGroupContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabGroupModelFilter,
                        mActionConfirmationManager,
                        mTabCreator,
                        /* isTabGroupSyncEnabled= */ true);
        mTabGroupContextMenuCoordinator =
                new TabGroupContextMenuCoordinator(
                        mTabModelSupplier,
                        mTabGroupModelFilter,
                        mActionConfirmationManager,
                        mTabCreator,
                        mWindowAndroid,
                        /* shouldShowDeleteGroup= */ true);

        // Set groupRootId to bypass showMenu() call.
        mTabGroupContextMenuCoordinator.setGroupRootIdForTesting(mTabId);
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testListMenuItems() {
        // Build custom view first to setup menu view.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);

        ModelList modelList = new ModelList();
        mTabGroupContextMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ false,
                /* shouldShowDeleteGroup= */ true,
                /* hasCollaborationData= */ false);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 6, modelList.size());

        // Assert: verify item type or id.
        verifyNormalListItems(modelList);
        assertEquals(ListMenuItemType.DIVIDER, modelList.get(4).type);
        assertEquals(
                R.id.delete_tab, modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testListMenuItems_Incognito() {
        // Build custom view first to setup menu view.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);

        ModelList modelList = new ModelList();
        mTabGroupContextMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ true,
                /* shouldShowDeleteGroup= */ false,
                /* hasCollaborationData= */ false);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // Assert: verify divider or menu item id.
        verifyNormalListItems(modelList);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testCustomMenuItems() {
        // Build custom view.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);

        // Verify text input layout.
        EditText groupTitleEditText =
                mTabGroupContextMenuCoordinator.getGroupTitleEditTextForTesting();
        assertNotNull(groupTitleEditText);

        // Verify color picker.
        ColorPickerCoordinator colorPickerCoordinator =
                mTabGroupContextMenuCoordinator.getColorPickerCoordinatorForTesting();
        assertNotNull(colorPickerCoordinator);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_Ungroup() {
        // Initialize.
        setUpTabGroupModelFilter();

        // Verify tab group is ungrouped.
        mOnItemClickedCallback.onClick(R.id.ungroup_tab, mTabId, /* collaborationId= */ null);
        verify(mActionConfirmationManager)
                .processUngroupAttempt(mConfirmationResultCaptor.capture());
        mConfirmationResultCaptor.getValue().onResult(ConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mTabGroupModelFilter).moveTabOutOfGroup(mTabId);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testItemClicked_CloseGroup() {
        // Initialize.
        List<Tab> tabsInGroup = setUpTabGroupModelFilter();

        // Verify tab group closed.
        mOnItemClickedCallback.onClick(R.id.close_tab, mTabId, /* collaborationId= */ null);
        verify(mTabGroupModelFilter)
                .closeTabs(
                        argThat(
                                params ->
                                        params.tabs.get(0) == tabsInGroup.get(0)
                                                && params.allowUndo
                                                && params.hideTabGroups));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_DeleteGroup() {
        // Initialize.
        List<Tab> tabsInGroup = setUpTabGroupModelFilter();

        // Verify tab group deleted.
        mOnItemClickedCallback.onClick(R.id.delete_tab, mTabId, /* collaborationId= */ null);
        verify(mActionConfirmationManager)
                .processDeleteGroupAttempt(mConfirmationResultCaptor.capture());
        mConfirmationResultCaptor.getValue().onResult(ConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mTabGroupModelFilter)
                .closeTabs(
                        argThat(
                                params ->
                                        params.tabs.get(0) == tabsInGroup.get(0)
                                                && !params.allowUndo
                                                && !params.hideTabGroups));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_NewTabInGroup() {
        // Initialize.
        List<Tab> tabsInGroup = setUpTabGroupModelFilter();

        // Verify new tab opened in group.
        mOnItemClickedCallback.onClick(
                R.id.open_new_tab_in_group, mTabId, /* collaborationId= */ null);
        verify(mTabCreator)
                .createNewTab(any(), eq(TabLaunchType.FROM_TAB_GROUP_UI), eq(tabsInGroup.get(0)));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testUpdateGroupTitleOnKeyboardHide() {
        // Initialize
        setUpTabGroupModelFilter();
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        // Verify default group title.
        EditText groupTitleEditText =
                mTabGroupContextMenuCoordinator.getGroupTitleEditTextForTesting();
        assertEquals("1 tab", groupTitleEditText.getText().toString());

        // Update group title by flipping keyboard visibility to hide.
        String newTitle = "new title";
        groupTitleEditText.setText(newTitle);
        KeyboardVisibilityDelegate.KeyboardVisibilityListener keyboardVisibilityListener =
                mTabGroupContextMenuCoordinator.getKeyboardVisibilityListenerForTesting();
        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        // Verify the group title is updated.
        verify(mTabGroupModelFilter).setTabGroupTitle(mTabId, newTitle);

        // Remove the custom title set by the user by clearing the edit box.
        newTitle = "";
        groupTitleEditText.setText(newTitle);
        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        // Verify the previous title is deleted and is default to "N tabs"
        verify(mTabGroupModelFilter).deleteTabGroupTitle(mTabId);
        assertEquals("1 tab", groupTitleEditText.getText().toString());
    }

    private List<Tab> setUpTabGroupModelFilter() {
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        tabModel.addTab(mTabId);
        Tab tab = tabModel.getTabById(mTabId);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(tabModel);
        when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(eq(mTabId))).thenReturn(1);
        List<Tab> tabsInGroup = Arrays.asList(tab);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(eq(mTabId))).thenReturn(tabsInGroup);
        when(mTabGroupModelFilter.getRelatedTabList(eq(mTabId))).thenReturn(tabsInGroup);
        return tabsInGroup;
    }

    private void verifyNormalListItems(ModelList modelList) {
        assertEquals(ListMenuItemType.DIVIDER, modelList.get(0).type);
        assertEquals(
                R.id.open_new_tab_in_group,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.ungroup_tab, modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }
}
