// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.EditText;
import android.widget.ListAdapter;
import android.widget.ListView;

import androidx.annotation.DimenRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupOverflowMenuCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupTitleEditor;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.listmenu.ListSectionDividerProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on the group titles. It is
 * responsible for creating a list of menu items, setting up the menu and displaying the menu.
 */
public class TabGroupContextMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    private static final String MENU_USER_ACTION_PREFIX = "MobileToolbarTabGroupMenu.";
    private View mContentView;
    private EditText mGroupTitleEditText;
    private ColorPickerCoordinator mColorPickerCoordinator;
    private TabGroupModelFilter mTabGroupModelFilter;
    private int mGroupRootId;
    private Context mContext;

    // Title currently modified by the user through the edit box. This does not include previously
    // updated or default title.
    private String mCurrentModifiedTitle;
    private boolean mIsPresetTitleUsed;
    private WindowAndroid mWindowAndroid;
    private KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;

    /**
     * @param tabModelSupplier The supplier of the tab model.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to act on.
     * @param actionConfirmationManager Used to show a confirmation dialog.
     * @param tabCreator The {@link TabCreator} to use to create new tab.
     * @param isTabGroupSyncEnabled Whether tab group sync is enabled.
     */
    public TabGroupContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            ActionConfirmationManager actionConfirmationManager,
            TabCreator tabCreator,
            WindowAndroid windowAndroid,
            boolean isTabGroupSyncEnabled) {
        super(
                R.layout.tab_strip_group_menu_layout,
                getMenuItemClickedCallback(
                        tabGroupModelFilter,
                        actionConfirmationManager,
                        tabCreator,
                        isTabGroupSyncEnabled),
                tabModelSupplier,
                isTabGroupSyncEnabled,
                /* identityManager= */ null,
                /* tabGroupSyncService= */ null,
                /* dataSharingService= */ null);
        mTabGroupModelFilter = tabGroupModelFilter;
        mWindowAndroid = windowAndroid;
        mKeyboardVisibilityListener =
                isShowing -> {
                    if (!isShowing) updateTabGroupTitle();
                };
    }

    @VisibleForTesting
    static OnItemClickedCallback getMenuItemClickedCallback(
            TabGroupModelFilter tabGroupModelFilter,
            ActionConfirmationManager actionConfirmationManager,
            TabCreator tabCreator,
            boolean isTabGroupSyncEnabled) {
        return (menuId, tabId, collaborationId) -> {
            if (menuId == org.chromium.chrome.R.id.ungroup_tab) {
                TabUiUtils.ungroupTabGroup(
                        tabGroupModelFilter,
                        actionConfirmationManager,
                        tabId,
                        isTabGroupSyncEnabled);
                recordUserAction("Ungroup");
            } else if (menuId == org.chromium.chrome.R.id.close_tab) {
                TabUiUtils.closeTabGroup(
                        tabGroupModelFilter,
                        actionConfirmationManager,
                        tabId,
                        /* hideTabGroups= */ true,
                        isTabGroupSyncEnabled,
                        /* didCloseCallback= */ null);
                recordUserAction("CloseGroup");
            } else if (menuId == org.chromium.chrome.R.id.delete_tab) {
                TabUiUtils.closeTabGroup(
                        tabGroupModelFilter,
                        actionConfirmationManager,
                        tabId,
                        /* hideTabGroups= */ false,
                        isTabGroupSyncEnabled,
                        /* didCloseCallback= */ null);
                recordUserAction("DeleteGroup");
            } else if (menuId == org.chromium.chrome.R.id.open_new_tab_in_group) {
                TabUiUtils.openNtpInGroup(
                        tabGroupModelFilter, tabCreator, tabId, TabLaunchType.FROM_TAB_GROUP_UI);
                recordUserAction("NewTabInGroup");
            }
        };
    }

    /**
     * Show the context menu of the tab group.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates..
     * @param rootId The root id of the interacting tab group.
     */
    protected void showMenu(RectProvider anchorViewRectProvider, int rootId) {
        mGroupRootId = rootId;
        createAndShowMenu(
                anchorViewRectProvider,
                rootId,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ ResourcesCompat.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                mWindowAndroid.getActivity().get());
        recordUserAction("Shown");
    }

    @Override
    protected void buildCustomView(View contentView, boolean isIncognito) {
        mContentView = contentView;
        mContext = contentView.getContext();

        buildTitleEditor(isIncognito);

        buildColorEditor(isIncognito);
    }

    @Override
    protected void buildMenuActionItems(
            ModelList itemList,
            boolean isIncognito,
            boolean shouldShowDeleteGroup,
            boolean hasCollaborationData) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ListSectionDividerProperties.ALL_KEYS)
                        .with(
                                ListSectionDividerProperties.LEFT_PADDING_DIMEN_ID,
                                R.dimen.list_menu_item_horizontal_padding)
                        .with(
                                ListSectionDividerProperties.RIGHT_PADDING_DIMEN_ID,
                                R.dimen.list_menu_item_horizontal_padding);
        itemList.add(new ListItem(ListMenuItemType.DIVIDER, builder.build()));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.open_new_tab_in_group_context_menu_item,
                        R.id.open_new_tab_in_group,
                        R.drawable.ic_open_new_tab_in_group_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.ungroup_tab_group_menu_item,
                        R.id.ungroup_tab,
                        R.drawable.ic_ungroup_tabs_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.tab_grid_dialog_toolbar_close_group,
                        R.id.close_tab,
                        R.drawable.ic_tab_close_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));

        // Delete does not make sense for incognito since the tab group is not saved to sync.
        if (shouldShowDeleteGroup && !isIncognito && !hasCollaborationData) {
            itemList.add(new ListItem(ListMenuItemType.DIVIDER, builder.build()));
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.tab_grid_dialog_toolbar_delete_group,
                            R.id.delete_tab,
                            R.drawable.material_ic_delete_24dp,
                            R.color.default_icon_color_light_tint_list,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            isIncognito,
                            true));
        }

        setListViewHeightBasedOnChildren();
    }

    /**
     * Calculates and sets the total height of the action menu ListView to prevent the ListView from
     * collapsing when nested inside a parent ScrollView.
     */
    public void setListViewHeightBasedOnChildren() {
        assert mContentView != null : "Menu view should not be null";

        ListView listView = mContentView.findViewById(R.id.tab_group_action_menu_list);
        listView.setScrollContainer(false);

        ListAdapter listAdapter = listView.getAdapter();
        if (listAdapter == null) {
            return;
        }

        int totalHeight = 0;
        for (int i = 0; i < listAdapter.getCount(); i++) {
            View listItem = listAdapter.getView(i, null, listView);
            listItem.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
            totalHeight += listItem.getMeasuredHeight();
        }

        ViewGroup.LayoutParams params = listView.getLayoutParams();
        params.height = totalHeight + listView.getPaddingTop() + listView.getPaddingBottom();
        listView.setLayoutParams(params);
    }

    @Override
    public void buildCollaborationMenuItems(
            ModelList itemList,
            IdentityManager identityManager,
            GroupDataOrFailureOutcome outcome) {
        // Intentional no-op.
    }

    @Override
    protected void onMenuDismissed() {
        // TODO(Crbug.com/360044398) Record user action dismiss without any action taken.
        updateTabGroupTitle();
        mWindowAndroid
                .getKeyboardDelegate()
                .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
    }

    @Override
    protected @DimenRes int getMenuWidth() {
        return R.dimen.tab_strip_group_context_menu_max_width;
    }

    private void updateTabGroupColor() {
        @TabGroupColorId int newColor = mColorPickerCoordinator.getSelectedColorSupplier().get();
        if (TabUiUtils.updateTabGroupColor(mTabGroupModelFilter, mGroupRootId, newColor)) {
            recordUserAction("ColorChanged");
        }
    }

    @VisibleForTesting
    void updateTabGroupTitle() {
        String newTitle = mCurrentModifiedTitle;
        if (newTitle == null) {
            return;
        } else if (TextUtils.isEmpty(newTitle) || newTitle.equals(getDefaultTitle())) {
            mTabGroupModelFilter.deleteTabGroupTitle(mGroupRootId);
            recordUserAction("TitleReset");
            setExistingOrDefaultTitle(getDefaultTitle());
        } else if (TabUiUtils.updateTabGroupTitle(mTabGroupModelFilter, mGroupRootId, newTitle)) {
            recordUserAction("TitleChanged");
        }
        mCurrentModifiedTitle = null;
    }

    private void setExistingOrDefaultTitle(String s) {
        // Flip `IsPresetTitleUsed`to prevent `TextWatcher` from treating `#setText` as a title
        // update.
        mIsPresetTitleUsed = true;
        mGroupTitleEditText.setText(s);
    }

    private String getDefaultTitle() {
        return TabGroupTitleEditor.getDefaultTitle(
                mContext, mTabGroupModelFilter.getRelatedTabCountForRootId(mGroupRootId));
    }

    // TODO(crbug.com/358689769): Enable live editing and updating of the group title.
    private void buildTitleEditor(boolean isIncognito) {
        mGroupTitleEditText = mContentView.findViewById(R.id.tab_group_title);

        // Set incognito style.
        if (isIncognito) {
            mGroupTitleEditText.setBackgroundTintList(
                    AppCompatResources.getColorStateList(
                            mContext,
                            org.chromium.chrome.R.color.menu_edit_text_bg_tint_list_baseline));
            mGroupTitleEditText.setTextAppearance(
                    R.style.TextAppearance_TextLarge_Primary_Baseline_Light);
        }

        // Listen to title update as user types.
        mGroupTitleEditText.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        if (!mIsPresetTitleUsed) {
                            mCurrentModifiedTitle = s.toString();
                        }
                        mIsPresetTitleUsed = false;
                    }
                });

        // Set the initial text to the existing group title, defaulting to "N tabs" if no title name
        // is set.
        String curGroupTitle = mTabGroupModelFilter.getTabGroupTitle(mGroupRootId);
        if (curGroupTitle == null || curGroupTitle.isEmpty()) {
            setExistingOrDefaultTitle(getDefaultTitle());
        } else {
            setExistingOrDefaultTitle(curGroupTitle);
        }

        // Add listener to group title EditText to update group title when keyboard starts hiding.
        mWindowAndroid
                .getKeyboardDelegate()
                .addKeyboardVisibilityListener(mKeyboardVisibilityListener);
    }

    private void buildColorEditor(boolean isIncognito) {
        // TODO(crbug.com/359941567): Refactor layout to use uniform padding in xml and remove
        // custom padding here.
        // Set horizontal padding to custom view to match list items.
        int horizontalPadding =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding);

        // TODO(crbug.com/357104424): Consider create ColorPickerCoordinator once during the first
        // call, and reuse it for subsequent calls.
        mColorPickerCoordinator =
                new ColorPickerCoordinator(
                        mContext,
                        ColorPickerUtils.getTabGroupColorIdList(),
                        ((ViewStub) mContentView.findViewById(R.id.color_picker_stub)).inflate(),
                        ColorPickerType.TAB_GROUP,
                        isIncognito,
                        ColorPickerLayoutType.DYNAMIC,
                        this::updateTabGroupColor);
        mColorPickerCoordinator
                .getContainerView()
                .setPadding(horizontalPadding, 0, horizontalPadding, 0);

        // The color picker should select the current color of the tab group when it is displayed.
        @TabGroupColorId
        int curGroupColor = mTabGroupModelFilter.getTabGroupColorWithFallback(mGroupRootId);
        mColorPickerCoordinator.setSelectedColorItem(curGroupColor);
    }

    private static void recordUserAction(String action) {
        RecordUserAction.record(MENU_USER_ACTION_PREFIX + action);
    }

    EditText getGroupTitleEditTextForTesting() {
        return mGroupTitleEditText;
    }

    ColorPickerCoordinator getColorPickerCoordinatorForTesting() {
        return mColorPickerCoordinator;
    }

    KeyboardVisibilityDelegate.KeyboardVisibilityListener
            getKeyboardVisibilityListenerForTesting() {
        return mKeyboardVisibilityListener;
    }

    void setGroupRootIdForTesting(int id) {
        mGroupRootId = id;
    }
}
