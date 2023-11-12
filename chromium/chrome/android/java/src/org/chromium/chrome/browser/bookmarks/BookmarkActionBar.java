// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.View.OnClickListener;

import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.util.ToolbarUtils;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.List;

// Vivaldi
import android.text.SpannableString;
import android.text.style.AbsoluteSizeSpan;
import android.text.style.ForegroundColorSpan;
import android.view.View;
import android.widget.PopupMenu;
import java.util.HashSet;

import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.ui.base.DeviceFormFactor;

import org.vivaldi.browser.bookmarks.BookmarkDialogDelegate;
import org.vivaldi.browser.common.VivaldiBookmarkUtils;
import org.vivaldi.browser.panels.PanelUtils;

/**
 * Main action bar of bookmark UI. It is responsible for displaying title and buttons
 * associated with the current context.
 */
public class BookmarkActionBar extends SelectableListToolbar<BookmarkId>
        implements BookmarkUIObserver, OnMenuItemClickListener, OnClickListener,
                   DragReorderableListAdapter.DragListener {

    private BookmarkItem mCurrentFolder;
    private BookmarkDelegate mDelegate;
    private ChromeTabbedActivity mTabbedActivity; // Vivaldi

    /** Vivaldi **/
    private BookmarkDialogDelegate mBookmarkDialogDelegate;
    private BookmarkModelObserver mBookmarkModelObserver =
            new BookmarkModelObserver() {
                @Override
                public void bookmarkModelChanged() {
                    onSelectionStateChange(
                            mDelegate.getSelectionDelegate().getSelectedItemsAsList());
                }
            };
    public void setBookmarkDialogDelegate(BookmarkDialogDelegate delegate) { mBookmarkDialogDelegate = delegate; }

    public BookmarkActionBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setNavigationOnClickListener(this);
        inflateMenu(R.menu.bookmark_action_bar_menu);
        setOnMenuItemClickListener(this);

        getMenu().findItem(R.id.selection_mode_edit_menu_id).setTitle(R.string.edit_bookmark);
        getMenu().findItem(R.id.selection_mode_move_menu_id)
                .setTitle(R.string.bookmark_action_bar_move);
        getMenu().findItem(R.id.selection_mode_delete_menu_id)
                .setTitle(R.string.bookmark_action_bar_delete);

        getMenu()
                .findItem(R.id.selection_open_in_incognito_tab_id)
                .setTitle(R.string.contextmenu_open_in_incognito_tab);
        // Wait to enable the selection mode group until the BookmarkDelegate is set. The
        // SelectionDelegate is retrieved from the BookmarkDelegate.
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, false);

        // Vivaldi - Reading list
        if (getContext() instanceof ChromeTabbedActivity)
            mTabbedActivity = (ChromeTabbedActivity)getContext();
    }

    @Override
    public void onNavigationBack() {
        if (isSearching()) {
            super.onNavigationBack();
            return;
        }

        mDelegate.openFolder(mCurrentFolder.getParentId());
    }

    @Override
    public boolean onMenuItemClick(MenuItem menuItem) {
        hideOverflowMenu();

        if (menuItem.getItemId() == R.id.edit_menu_id) {
            if (isVivaldiTablet()) {
                mBookmarkDialogDelegate.startEditFolder(
                        mCurrentFolder.getId());
            }
            else {
                BookmarkAddEditFolderActivity.startEditFolderActivity(getContext(),
                        mCurrentFolder.getId());
            }
            return true;
        } else if (menuItem.getItemId() == R.id.close_menu_id) {
            if (ChromeApplicationImpl.isVivaldi()) {
                PanelUtils.closePanel(getContext());
                return true;
            }
            BookmarkUtils.finishActivityOnPhone(getContext());
            return true;
        } else if (menuItem.getItemId() == R.id.search_menu_id) {
            mDelegate.openSearchUI();
            return true;
        }

        SelectionDelegate<BookmarkId> selectionDelegate = mDelegate.getSelectionDelegate();
        if (menuItem.getItemId() == R.id.selection_mode_edit_menu_id) {
            List<BookmarkId> list = selectionDelegate.getSelectedItemsAsList();
            assert list.size() == 1;
            BookmarkItem item = mDelegate.getModel().getBookmarkById(list.get(0));
            if (item.isFolder()) {
                if (isVivaldiTablet())
                    mBookmarkDialogDelegate
                            .startEditFolder(item.getId());
                else
                BookmarkAddEditFolderActivity.startEditFolderActivity(getContext(), item.getId());
            } else {
                if (isVivaldiTablet())
                    mBookmarkDialogDelegate.openEditBookmark(
                                    item.getId(), mCurrentFolder.getId());
                else
                BookmarkUtils.startEditActivity(getContext(), item.getId());
            }
            return true;
        } else if (menuItem.getItemId() == R.id.selection_mode_move_menu_id) {
            List<BookmarkId> list = selectionDelegate.getSelectedItemsAsList();
            if (list.size() >= 1) {
                if (isVivaldiTablet())
                    mBookmarkDialogDelegate.chooseFolder(getContext(),
                                    list.toArray(new BookmarkId[list.size()]));
                else
                BookmarkFolderSelectActivity.startFolderSelectActivity(getContext(),
                        list.toArray(new BookmarkId[list.size()]));
                RecordUserAction.record("MobileBookmarkManagerMoveToFolderBulk");
            }
            return true;
        } else if (menuItem.getItemId() == R.id.selection_mode_delete_menu_id) {
            mDelegate.getModel().deleteBookmarks(
                    selectionDelegate.getSelectedItems().toArray(new BookmarkId[0]));
            RecordUserAction.record("MobileBookmarkManagerDeleteBulk");
            return true;
        } else if (menuItem.getItemId() == R.id.selection_open_in_new_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInNewTab");
            RecordHistogram.recordCount1000Histogram(
                    "Bookmarks.Count.OpenInNewTab", mSelectionDelegate.getSelectedItems().size());
            mDelegate.openBookmarksInNewTabs(
                    selectionDelegate.getSelectedItemsAsList(), /*incognito=*/false);
            return true;
        } else if (menuItem.getItemId() == R.id.selection_open_in_incognito_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInIncognito");
            RecordHistogram.recordCount1000Histogram("Bookmarks.Count.OpenInIncognito",
                    mSelectionDelegate.getSelectedItems().size());
            mDelegate.openBookmarksInNewTabs(
                    selectionDelegate.getSelectedItemsAsList(), /*incognito=*/true);
            return true;
        } else if (menuItem.getItemId() == R.id.reading_list_mark_as_read_id
                || menuItem.getItemId() == R.id.reading_list_mark_as_unread_id) {
            // Handle the seclection "mark as" buttons in the same block because the behavior is
            // the same other than one boolean flip.
            for (int i = 0; i < selectionDelegate.getSelectedItemsAsList().size(); i++) {
                BookmarkId bookmark = selectionDelegate.getSelectedItemsAsList().get(i);
                if (bookmark.getType() != BookmarkType.READING_LIST) continue;

                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(bookmark);
                mDelegate.getModel().setReadStatusForReadingList(bookmarkItem.getUrl(),
                        /*read=*/menuItem.getItemId() == R.id.reading_list_mark_as_read_id);
            }
            selectionDelegate.clearSelection();
            return true;
        } else if (menuItem.getItemId() == R.id.select_all_menu_id) {
            toggleSelectAll();
            return true;
        } else if (ChromeApplicationImpl.isVivaldi()) {
            if (menuItem.getItemId() == R.id.sort_bookmarks_id) {
                BookmarkItemsAdapter.SortOrder order = mDelegate.getSortOrder();
                if (order == BookmarkItemsAdapter.SortOrder.MANUAL) {
                    menuItem.getSubMenu().findItem(R.id.sort_manual_id).setChecked(true);
                } else if (order == BookmarkItemsAdapter.SortOrder.TITLE) {
                    menuItem.getSubMenu().findItem(R.id.sort_by_title_id).setChecked(true);
                } else if (order == BookmarkItemsAdapter.SortOrder.ADDRESS) {
                    menuItem.getSubMenu().findItem(R.id.sort_by_address_id).setChecked(true);
                } else if (order == BookmarkItemsAdapter.SortOrder.NICK) {
                    menuItem.getSubMenu().findItem(R.id.sort_by_nickname_id).setChecked(true);
                } else if (order == BookmarkItemsAdapter.SortOrder.DESCRIPTION) {
                    menuItem.getSubMenu().findItem(R.id.sort_by_description_id).setChecked(true);
                } else if (order == BookmarkItemsAdapter.SortOrder.DATE) {
                    menuItem.getSubMenu().findItem(R.id.sort_by_date_id).setChecked(true);
                }
                return true;
            } else if (menuItem.getItemId() == R.id.sort_manual_id) {
                mDelegate.setSortOrder(
                        BookmarkItemsAdapter.SortOrder.forNumber(BookmarkItemsAdapter.
                                SortOrder.MANUAL.getNumber()));
                return true;
            } else if (menuItem.getItemId() == R.id.sort_by_title_id) {
                mDelegate.setSortOrder(
                        BookmarkItemsAdapter.SortOrder.forNumber(BookmarkItemsAdapter.
                                SortOrder.TITLE.getNumber()));
                return true;
            } else if (menuItem.getItemId() == R.id.sort_by_address_id) {
                mDelegate.setSortOrder(
                        BookmarkItemsAdapter.SortOrder.forNumber(BookmarkItemsAdapter.
                                SortOrder.ADDRESS.getNumber()));
                return true;
            } else if (menuItem.getItemId() == R.id.sort_by_nickname_id) {
                mDelegate.setSortOrder(
                        BookmarkItemsAdapter.SortOrder.forNumber(BookmarkItemsAdapter.
                                SortOrder.NICK.getNumber()));
                return true;
            } else if (menuItem.getItemId() == R.id.sort_by_description_id) {
                mDelegate.setSortOrder(
                        BookmarkItemsAdapter.SortOrder.forNumber(BookmarkItemsAdapter.
                                SortOrder.DESCRIPTION.getNumber()));
                return true;
            } else if (menuItem.getItemId() == R.id.sort_by_date_id) {
                mDelegate.setSortOrder(
                        BookmarkItemsAdapter.SortOrder.forNumber(BookmarkItemsAdapter.
                                SortOrder.DATE.getNumber()));
                return true;
            } else if (menuItem.getItemId() == R.id.add_page_to_reading_list_menu_id) {
                if (mTabbedActivity != null && mTabbedActivity.getActivityTab() != null
                        && ReadingListUtils.isReadingListSupported(
                                   mTabbedActivity.getActivityTab().getUrl())) {
                    mTabbedActivity.getTabBookMarker().addToReadingList(
                            mTabbedActivity.getActivityTab());
                }
                return true;
            }
        }

        assert false : "Unhandled menu click.";
        return false;
    }

    void toggleSelectAll() {
        HashSet<BookmarkId> idSet = new HashSet<>(mDelegate.getModel().getChildIDs(
                mCurrentFolder.getId()));
        mDelegate.getSelectionDelegate().setSelectedItems(idSet);
    }


    void showLoadingUi() {
        setTitle(null);
        setNavigationButton(NAVIGATION_BUTTON_NONE);
        getMenu().findItem(R.id.search_menu_id).setVisible(false);
        getMenu().findItem(R.id.edit_menu_id).setVisible(false);
    }

    @Override
    protected void showNormalView() {
        super.showNormalView();

        if (mDelegate == null) {
            getMenu().findItem(R.id.search_menu_id).setVisible(false);
            getMenu().findItem(R.id.edit_menu_id).setVisible(false);
        }
    }

    /**
     * Sets the delegate to use to handle UI actions related to this action bar.
     * @param delegate A {@link BookmarkDelegate} instance to handle all backend interaction.
     */
    public void onBookmarkDelegateInitialized(BookmarkDelegate delegate) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);
        if (!ChromeApplicationImpl.isVivaldi())
        if (!delegate.isDialogUi()) getMenu().removeItem(R.id.close_menu_id);
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, true);
        /** Vivaldi **/
        delegate.getModel().addObserver(mBookmarkModelObserver);
    }

    // BookmarkUIObserver implementations.

    @Override
    public void onDestroy() {
        if (mDelegate == null) return;

        mDelegate.removeUIObserver(this);
        /** Vivaldi **/
        mDelegate.getModel().removeObserver(mBookmarkModelObserver);
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        mCurrentFolder = mDelegate.getModel().getBookmarkById(folder);
        getMenu().findItem(R.id.search_menu_id).setVisible(true);
        getMenu().findItem(R.id.edit_menu_id).setVisible(mCurrentFolder.isEditable());
        // Vivaldi
        MenuItem addPageMenuItem = getMenu().findItem(R.id.add_page_to_reading_list_menu_id);
        addPageMenuItem.setVisible(false);

        // If this is the root folder, we can't go up anymore.
        if (folder.equals(mDelegate.getModel().getRootFolderId())) {
            setTitle(R.string.bookmarks);
            setNavigationButton(NAVIGATION_BUTTON_NONE);
            return;
        }

        if (folder.equals(BookmarkId.SHOPPING_FOLDER)) {
            setTitle(R.string.price_tracking_bookmarks_filter_title);
        } else if (mDelegate.getModel().getTopLevelFolderParentIDs().contains(
                           mCurrentFolder.getParentId())
                && TextUtils.isEmpty(mCurrentFolder.getTitle())) {
            setTitle(R.string.bookmarks);
        } else {
            setTitle(mCurrentFolder.getTitle());
        }

        setNavigationButton(NAVIGATION_BUTTON_BACK);

        // Vivaldi needs this for adding new bookmarks
        if (!mCurrentFolder.getId().equals(mDelegate.getModel().getTrashFolderId()))
            BookmarkUtils.setLastUsedParent(getContext(), mCurrentFolder.getId());

        // Update menus for Reading list
        boolean isReadingListFolder =
                mCurrentFolder.getId().equals(mDelegate.getModel().getReadingListFolder());
        if (isReadingListFolder && mTabbedActivity != null) {
            if (!PanelUtils.isPanelOpen(mTabbedActivity)) {
                setNavigationButton(NAVIGATION_BUTTON_NONE);
                getMenu().findItem(R.id.close_menu_id).setVisible(false);
            }
            getMenu().findItem(R.id.search_menu_id).setVisible(false);
            addPageMenuItem.setVisible(true);
            addPageMenuItem.setEnabled(shouldEnableAddCurrentPageMenu(mTabbedActivity));
            int textColor = addPageMenuItem.isEnabled()
                    ? R.color.vivaldi_highlight
                    : R.color.default_icon_color_disabled;
            SpannableString menuTitle = new SpannableString(
                    getContext().getString(R.string.add_page_to_reading_list));
            menuTitle.setSpan(new AbsoluteSizeSpan(
                    getContext().getResources().getDimensionPixelSize(R.dimen.text_size_small)),
                    0,menuTitle.length(), 0);
            menuTitle.setSpan(new ForegroundColorSpan(
                    getContext().getColor(textColor)), 0, menuTitle.length(), 0);
            addPageMenuItem.setTitle(menuTitle);
        }
        getMenu().findItem(R.id.sort_bookmarks_id).setVisible(!isReadingListFolder);
    }

    @Override
    public void onSearchStateSet() {}

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        super.onSelectionStateChange(selectedBookmarks);

        // The super class registers itself as a SelectionObserver before
        // #onBookmarkDelegateInitialized() is called. Return early if mDelegate has not been set.
        if (mDelegate == null) return;

        if (mIsSelectionEnabled) {
            // Editing a bookmark action on multiple selected items doesn't make sense. So disable.
            getMenu().findItem(R.id.selection_mode_edit_menu_id).setVisible(
                    selectedBookmarks.size() == 1);
            getMenu()
                    .findItem(R.id.selection_open_in_incognito_tab_id)
                    .setVisible(IncognitoUtils.isIncognitoModeEnabled());

            // It does not make sense to open a folder in new tab.
            for (BookmarkId bookmark : selectedBookmarks) {
                BookmarkItem item = mDelegate.getModel().getBookmarkById(bookmark);
                if (item != null && item.isFolder()) {
                    getMenu().findItem(R.id.selection_open_in_new_tab_id).setVisible(false);
                    getMenu().findItem(R.id.selection_open_in_incognito_tab_id).setVisible(false);
                    break;
                }
            }

            boolean hasPartnerBoomarkSelected = false;
            // Partner bookmarks can't move, so if the selection includes a partner bookmark,
            // disable the move button.
            for (BookmarkId bookmark : selectedBookmarks) {
                if (bookmark.getType() == BookmarkType.PARTNER) {
                    hasPartnerBoomarkSelected = true;
                    getMenu().findItem(R.id.selection_mode_move_menu_id).setVisible(false);
                    break;
                }
            }

            // Compute whether all selected bookmarks are reading list items and add up the number
            // of read items.
            int numReadingListItems = 0;
            int numRead = 0;
            for (int i = 0; i < selectedBookmarks.size(); i++) {
                BookmarkId bookmark = selectedBookmarks.get(i);
                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(bookmark);
                if (bookmark.getType() == BookmarkType.READING_LIST) {
                    numReadingListItems++;
                    if (bookmarkItem.isRead()) numRead++;
                }
            }

            // Don't show the move/edit buttons if there are also partner bookmarks selected since
            // these bookmarks can't be moved or edited. If there are no reading list items
            // selected, then use default behavior.
            if (numReadingListItems > 0) {
                getMenu()
                        .findItem(R.id.selection_mode_move_menu_id)
                        .setVisible(!hasPartnerBoomarkSelected);
                getMenu()
                        .findItem(R.id.selection_mode_edit_menu_id)
                        .setVisible(selectedBookmarks.size() == 1 && !hasPartnerBoomarkSelected);

                getMenu().findItem(R.id.selection_open_in_new_tab_id).setVisible(true);
                getMenu().findItem(R.id.selection_open_in_incognito_tab_id).setVisible(true);
            }

            // Only show the "mark as" options when all selections are reading list items and
            // have the same read state.
            boolean onlyReadingListSelected =
                    selectedBookmarks.size() > 0 && numReadingListItems == selectedBookmarks.size();
            getMenu()
                    .findItem(R.id.reading_list_mark_as_read_id)
                    .setVisible(onlyReadingListSelected && numRead == 0);
            getMenu()
                    .findItem(R.id.reading_list_mark_as_unread_id)
                    .setVisible(onlyReadingListSelected && numRead == selectedBookmarks.size());
        } else {
            mDelegate.notifyStateChange(this);
        }
    }

    // DragListener implementation.

    /**
     * Called when there is a drag in the bookmarks list.
     *
     * @param drag Whether drag is currently on.
     */
    @Override
    public void onDragStateChange(boolean drag) {
        // Disable menu items while dragging.
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, !drag);
        ToolbarUtils.setOverFlowMenuEnabled(this, !drag);

        // Disable listeners while dragging.
        setNavigationOnClickListener(drag ? null : this);
        setOnMenuItemClickListener(drag ? null : this);
    }

    // Vivaldi
    private boolean isVivaldiTablet() {
        return ChromeApplicationImpl.isVivaldi() &&
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext());
    }

    /** Vivaldi
     * Returns true if current page is reading list supported and not already on reading list **/
    private boolean shouldEnableAddCurrentPageMenu(ChromeTabbedActivity activity) {
        if (activity.getActivityTab() == null) return false;
        if (!ReadingListUtils.isReadingListSupported(activity.getActivityTab().getUrl()))
            return false;
        BookmarkId existingBookmark =
                mDelegate.getModel().getUserBookmarkIdForTab(activity.getActivityTab());
        if (existingBookmark == null) return true;
        return existingBookmark.getType() != BookmarkType.READING_LIST;
    }
}
