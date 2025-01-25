// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.View.OnClickListener;

import androidx.annotation.IdRes;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.core.view.MenuCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.util.ToolbarUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;

// Vivaldi
import android.text.SpannableString;
import android.text.style.AbsoluteSizeSpan;
import android.text.style.ForegroundColorSpan;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.components.bookmarks.BookmarkType;
// End Vivaldi

/**
 * Main toolbar of bookmark UI. It is responsible for displaying title and buttons associated with
 * the current context.
 */
public class BookmarkToolbar extends SelectableListToolbar<BookmarkId>
        implements OnMenuItemClickListener, OnClickListener {
    private SelectionDelegate<BookmarkId> mSelectionDelegate;

    private boolean mEditButtonVisible;
    private boolean mNewFolderButtonVisible;
    private boolean mNewFolderButtonEnabled;
    private boolean mSelectionShowEdit;
    private boolean mSelectionShowOpenInNewTab;
    private boolean mSelectionShowOpenInIncognito;
    private boolean mSelectionShowMove;
    private boolean mSelectionShowMarkRead;
    private boolean mSelectionShowMarkUnread;

    private List<Integer> mSortMenuIds;
    private boolean mSortMenuIdsEnabled;

    private Runnable mNavigateBackRunnable;
    private Function<Integer, Boolean> mMenuIdClickedFunction;

    // Vivaldi
    private ChromeTabbedActivity mChromeTabbedActivity;
    private boolean mSortButtonVisible;
    private boolean mAddToReadingListButtonVisible;

    public BookmarkToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setNavigationOnClickListener(this);

        if (ChromeApplicationImpl.isVivaldi()) {
            if (context instanceof ChromeTabbedActivity) {
                mChromeTabbedActivity = (ChromeTabbedActivity)context;
            }
        }

        inflateMenu(R.menu.bookmark_toolbar_menu_improved);
        MenuCompat.setGroupDividerEnabled(
                getMenu().findItem(R.id.normal_options_submenu).getSubMenu(), true);

	    if (ChromeApplicationImpl.isVivaldi()) {
            getMenu().removeItem(R.id.normal_options_submenu);
            getMenu().findItem(R.id.add_page_to_reading_list_menu_id).setVisible(false);
	    }

        setOnMenuItemClickListener(this);
    }

    void setBookmarkOpener(BookmarkOpener bookmarkOpener) {}

    void setSelectionDelegate(SelectionDelegate selectionDelegate) {
        mSelectionDelegate = selectionDelegate;
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, true);
    }

    void setBookmarkUiMode(@BookmarkUiMode int mode) {
        if (mode != BookmarkUiMode.LOADING) {
            showNormalView();
        }
    }

    void setSoftKeyboardVisible(boolean visible) {
        if (!visible) hideKeyboard();
    }

    void setIsDialogUi(boolean isDialogUi) {
        if (!ChromeApplicationImpl.isVivaldi())
        if (!isDialogUi) getMenu().removeItem(R.id.close_menu_id);
    }

    void setDragEnabled(boolean dragEnabled) {
        // Disable menu items while dragging.
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, !dragEnabled);
        ToolbarUtils.setOverFlowMenuEnabled(this, !dragEnabled);

        // Disable listeners while dragging.
        setNavigationOnClickListener(dragEnabled ? null : this);
        setOnMenuItemClickListener(dragEnabled ? null : this);
    }

    void setEditButtonVisible(boolean visible) {
        mEditButtonVisible = visible;
        getMenu().findItem(R.id.edit_menu_id).setVisible(visible);
    }

    void setNewFolderButtonVisible(boolean visible) {
        if (ChromeApplicationImpl.isVivaldi()) {
            getMenu().findItem(R.id.create_new_folder_menu_id).setVisible(false);
            return;
        }
        mNewFolderButtonVisible = visible;
        getMenu().findItem(R.id.create_new_folder_menu_id).setVisible(visible);
    }

    void setNewFolderButtonEnabled(boolean enabled) {
        if (ChromeApplicationImpl.isVivaldi()) {
            getMenu().findItem(R.id.create_new_folder_menu_id).setEnabled(false);
            return;
        }
        mNewFolderButtonEnabled = enabled;
        getMenu().findItem(R.id.create_new_folder_menu_id).setEnabled(enabled);
    }

    void setSelectionShowEdit(boolean show) {
        mSelectionShowEdit = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.selection_mode_edit_menu_id).setVisible(show);
    }

    void setSelectionShowOpenInNewTab(boolean show) {
        mSelectionShowOpenInNewTab = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.selection_open_in_new_tab_id).setVisible(show);
    }

    void setSelectionShowOpenInIncognito(boolean show) {
        mSelectionShowOpenInIncognito = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.selection_open_in_incognito_tab_id).setVisible(show);
    }

    void setSelectionShowMove(boolean show) {
        mSelectionShowMove = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.selection_mode_move_menu_id).setVisible(show);
    }

    void setSelectionShowMarkRead(boolean show) {
        mSelectionShowMarkRead = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.reading_list_mark_as_read_id).setVisible(show);
    }

    void setSelectionShowMarkUnread(boolean show) {
        mSelectionShowMarkUnread = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.reading_list_mark_as_unread_id).setVisible(show);
    }

    void setNavigationButtonState(@NavigationButton int navigationButtonState) {
        setNavigationButton(navigationButtonState);
    }

    void setCheckedSortMenuId(@IdRes int id) {
        if (ChromeApplicationImpl.isVivaldi()) {
            MenuItem item = getMenu().findItem(id);
            if (item != null && item.isVisible()
                ) item.setChecked(true);
        } else // End Vivaldi
        getMenu().findItem(id).setChecked(true);
    }

    void setSortMenuIds(List<Integer> sortMenuIds) {
        mSortMenuIds = sortMenuIds;
    }

    void setSortMenuIdsEnabled(boolean enabled) {
        if (ChromeApplicationImpl.isVivaldi()) return;
        mSortMenuIdsEnabled = enabled;
        for (Integer id : mSortMenuIds) {
            getMenu().findItem(id).setEnabled(enabled);
        }
    }

    void setCheckedViewMenuId(@IdRes int id) {
        if (ChromeApplicationImpl.isVivaldi()) return;
        getMenu().findItem(id).setChecked(true);
    }

    void setNavigateBackRunnable(Runnable navigateBackRunnable) {
        mNavigateBackRunnable = navigateBackRunnable;
    }

    void setMenuIdClickedFunction(Function<Integer, Boolean> menuIdClickedFunction) {
        mMenuIdClickedFunction = menuIdClickedFunction;
    }

    void fakeSelectionStateChange() {
        onSelectionStateChange(new ArrayList<>(mSelectionDelegate.getSelectedItems()));
    }

    // OnMenuItemClickListener implementation.

    @Override
    public boolean onMenuItemClick(MenuItem menuItem) {
        hideOverflowMenu();
        return mMenuIdClickedFunction.apply(menuItem.getItemId());
    }

    // SelectableListToolbar implementation.

    @Override
    public void onNavigationBack() {
        // The navigation button shouldn't be visible unless the current folder is non-null.
        mNavigateBackRunnable.run();
    }

    @Override
    protected void showNormalView() {
        super.showNormalView();

        // SelectableListToolbar will show/hide the entire group.
        setEditButtonVisible(mEditButtonVisible);
        setNewFolderButtonVisible(mNewFolderButtonVisible);
        setNewFolderButtonEnabled(mNewFolderButtonEnabled);
        setSortMenuIdsEnabled(mSortMenuIdsEnabled);
        // Vivaldi
        setAddToReadingListButtonVisible(mAddToReadingListButtonVisible);
        setSortButtonVisible(mSortButtonVisible);
    }

    @Override
    protected void showSelectionView(List<BookmarkId> selectedItems, boolean wasSelectionEnabled) {
        super.showSelectionView(selectedItems, wasSelectionEnabled);

        setSelectionShowEdit(mSelectionShowEdit);
        setSelectionShowOpenInNewTab(mSelectionShowOpenInNewTab);
        setSelectionShowOpenInIncognito(mSelectionShowOpenInIncognito);
        setSelectionShowMove(mSelectionShowMove);
        setSelectionShowMarkRead(mSelectionShowMarkRead);
        setSelectionShowMarkUnread(mSelectionShowMarkUnread);
    }

    /**
     * Vivaldi
     * Returns true if current page is reading list supported and not already on reading list
     */
    private boolean shouldEnableAddCurrentPageMenu(ChromeTabbedActivity activity) {
        if (activity == null) return false;
        if (activity.getActivityTab() == null) return false;
        if (!ReadingListUtils.isReadingListSupported(activity.getActivityTab().getUrl()))
            return false;
        BookmarkModel model = BookmarkModel.getForProfile(
                    ProfileManager.getLastUsedRegularProfile());
        BookmarkId existingBookmark =
                model.getUserBookmarkIdForTab(activity.getActivityTab());
        if (existingBookmark == null) return true;
        return existingBookmark.getType() != BookmarkType.READING_LIST;
    }

    void setSortButtonVisible(boolean visible) {
        mSortButtonVisible = visible;
        getMenu().findItem(R.id.sort_bookmarks_id).setVisible(visible);
    }

    void setCloseButtonVisible(boolean visible) {
        getMenu().findItem(R.id.close_menu_id).setVisible(visible);
    }

    void setAddToReadingListButtonVisible(boolean visible) {
        mAddToReadingListButtonVisible = visible;
        MenuItem addPageMenuItem = getMenu().findItem(R.id.add_page_to_reading_list_menu_id);
        if (addPageMenuItem == null) return;
        addPageMenuItem.setVisible(visible);
        if (!visible) return;
        addPageMenuItem.setEnabled(shouldEnableAddCurrentPageMenu(mChromeTabbedActivity));
        int textColor = addPageMenuItem.isEnabled()
                ? R.color.vivaldi_highlight
                : R.color.default_icon_color_disabled;
        SpannableString menuTitle = new SpannableString(
                getContext().getString(R.string.add_page_to_reading_list));
        menuTitle.setSpan(new AbsoluteSizeSpan( // Vivaldi Ref. VAB-8831
                        getContext().getResources().getDimensionPixelSize(R.dimen.text_size_large)),
                0,menuTitle.length(), 0);
        menuTitle.setSpan(new ForegroundColorSpan(
                getContext().getColor(textColor)), 0, menuTitle.length(), 0);
        addPageMenuItem.setTitle(menuTitle);
    }
}
