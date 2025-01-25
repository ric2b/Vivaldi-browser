// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;
import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;

import android.content.Context;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// Vivaldi
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * The main coordinator for the Tab Switcher Action Menu, responsible for creating the popup menu
 * (popup window) in general and building a list of menu items.
 */
public class TabSwitcherActionMenuCoordinator {

    private static TabModelSelector mTabModelSelector; // Vivaldi

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        MenuItemType.DIVIDER,
        MenuItemType.CLOSE_TAB,
        MenuItemType.VIVALDI_CREATE_TAB_STACK,
        MenuItemType.NEW_TAB,
        MenuItemType.NEW_INCOGNITO_TAB
    })
    public @interface MenuItemType {
        int DIVIDER = 0;
        int CLOSE_TAB = 1;
        int NEW_TAB = 2;
        int NEW_INCOGNITO_TAB = 3;

        int VIVALDI_CLOSE_OTHER_TABS = 301; // Vivaldi
        int VIVALDI_CREATE_TAB_STACK = 302; // Vivaldi
    }

    /**
     * @param onItemClicked The clicked listener handling clicks on TabSwitcherActionMenu.
     * @param profile The {@link Profile} associated with the tabs.
     * @return a long click listener of the long press action of tab switcher button.
     */
    public static OnLongClickListener createOnLongClickListener(
            Callback<Integer> onItemClicked, Profile profile) {
        return createOnLongClickListener(
                new TabSwitcherActionMenuCoordinator(profile), onItemClicked);
    }

    // internal helper function to create a long click listener.
    protected static OnLongClickListener createOnLongClickListener(
            TabSwitcherActionMenuCoordinator menu, Callback<Integer> onItemClicked) {
        return (view) -> {
            Context context = view.getContext();
            menu.displayMenu(
                    context,
                    (ListMenuButton) view,
                    menu.buildMenuItems(),
                    (id) -> {
                        recordUserActions(id);
                        onItemClicked.onResult(id);
                    });
            return true;
        };
    }

    private static void recordUserActions(int id) {
        if (id == R.id.close_tab) {
            RecordUserAction.record("MobileMenuCloseTab.LongTapMenu");
        } else if (id == R.id.new_tab_menu_id) {
            RecordUserAction.record("MobileMenuNewTab.LongTapMenu");
        } else if (id == R.id.new_incognito_tab_menu_id) {
            RecordUserAction.record("MobileMenuNewIncognitoTab.LongTapMenu");
        }
    }

    private final Profile mProfile;

    // For test.
    private View mContentView;

    /** Construct a coordinator for the given {@link Profile}. */
    public TabSwitcherActionMenuCoordinator(Profile profile) {
        mProfile = profile;
    }

    /**
     * Created and display the tab switcher action menu anchored to the specified view.
     *
     * @param context The context of the TabSwitcherActionMenu.
     * @param anchorView The anchor {@link View} of the {@link PopupWindow}.
     * @param listItems The menu item models.
     * @param onItemClicked The clicked listener handling clicks on TabSwitcherActionMenu.
     */
    @VisibleForTesting
    public void displayMenu(
            final Context context,
            ListMenuButton anchorView,
            ModelList listItems,
            Callback<Integer> onItemClicked) {
        RectProvider rectProvider = MenuBuilderHelper.getRectProvider(anchorView);
        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        context,
                        listItems,
                        (model) -> {
                            onItemClicked.onResult(model.get(ListMenuItemProperties.MENU_ITEM_ID));
                        });

        mContentView = listMenu.getContentView();
        int verticalPadding =
                context.getResources()
                        .getDimensionPixelOffset(R.dimen.tab_switcher_menu_vertical_padding);
        ListView listView = listMenu.getListView();
        listView.setPaddingRelative(
                listView.getPaddingStart(),
                verticalPadding,
                listView.getPaddingEnd(),
                verticalPadding);
        ListMenuButtonDelegate delegate =
                new ListMenuButtonDelegate() {
                    @Override
                    public ListMenu getListMenu() {
                        return listMenu;
                    }

                    @Override
                    public RectProvider getRectProvider(View listMenuButton) {
                        return rectProvider;
                    }
                };

        anchorView.setDelegate(delegate, false);
        anchorView.showMenu();
    }

    @VisibleForTesting
    View getContentView() {
        return mContentView;
    }

    public ModelList buildMenuItems() {
        ModelList itemList = new ModelList();
        itemList.add(buildListItemByMenuItemType(MenuItemType.CLOSE_TAB));
        // Vivaldi
        if (mTabModelSelector != null && mTabModelSelector.getCurrentModel().getCount() > 1)
            itemList.add(buildListItemByMenuItemType(MenuItemType.VIVALDI_CLOSE_OTHER_TABS));
        itemList.add(buildListItemByMenuItemType(MenuItemType.DIVIDER));
        itemList.add(buildListItemByMenuItemType(MenuItemType.NEW_TAB));
        // Vivaldi
        if (ChromeSharedPreferences.getInstance().readBoolean("enable_tab_stack", true))
            itemList.add(buildListItemByMenuItemType(MenuItemType.VIVALDI_CREATE_TAB_STACK));
        itemList.add(buildListItemByMenuItemType(MenuItemType.NEW_INCOGNITO_TAB));
        return itemList;
    }

    protected ListItem buildListItemByMenuItemType(@MenuItemType int type) {
        switch (type) {
            case MenuItemType.CLOSE_TAB:
                return buildMenuListItem(R.string.close_tab, R.id.close_tab, R.drawable.btn_close);
            case MenuItemType.VIVALDI_CLOSE_OTHER_TABS:
                return buildMenuListItem(R.string.tabs_menu_close_other_tabs,
                        R.id.vivaldi_close_other_tabs, R.drawable.btn_close);
            case MenuItemType.NEW_TAB:
                return buildMenuListItem(
                        R.string.menu_new_tab, R.id.new_tab_menu_id, R.drawable.new_tab_icon);
            case MenuItemType.NEW_INCOGNITO_TAB:
                return buildMenuListItem(
                        R.string.menu_new_incognito_tab,
                        R.id.new_incognito_tab_menu_id,
                        R.drawable.incognito_simple,
                        IncognitoUtils.isIncognitoModeEnabled(mProfile));

            case MenuItemType.VIVALDI_CREATE_TAB_STACK:
                return buildMenuListItem(R.string.tabs_menu_create_new_tab_stack,
                        R.id.vivaldi_create_new_tab_stack, R.drawable.new_tab_stack_icon);

            case MenuItemType.DIVIDER:
            default:
                return buildMenuDivider();
        }
    }

    /** Vivaldi **/
    public static void setTabModel(TabModelSelector tabModel) {
        mTabModelSelector = tabModel;
    }
}
