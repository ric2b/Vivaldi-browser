// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkId;

import org.chromium.chrome.browser.ChromeApplication;
import org.vivaldi.browser.common.VivaldiBookmarkUtils;

/**
 * A row view that shows folder info in the bookmarks UI.
 */
public class BookmarkFolderRow extends BookmarkRow {

    /**
     * Constructor for inflating from XML.
     */
    public BookmarkFolderRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        if (!ChromeApplication.isVivaldi())
        setStartIconDrawable(BookmarkUtils.getFolderIcon(getContext()));
    }

    // BookmarkRow implementation.

    @Override
    public void onClick() {
        mDelegate.openFolder(mBookmarkId);
    }

    @Override
    BookmarkItem setBookmarkId(BookmarkId bookmarkId, @Location int location) {
        BookmarkItem item = super.setBookmarkId(bookmarkId, location);
        mTitleView.setText(item.getTitle());
        int childCount = mDelegate.getModel().getTotalBookmarkCount(bookmarkId);
        mDescriptionView.setText((childCount > 0)
                        ? getResources().getQuantityString(
                                  R.plurals.bookmarks_count, childCount, childCount)
                        : getResources().getString(R.string.no_bookmarks));

        if (ChromeApplication.isVivaldi()) {
            if (bookmarkId.equals(mDelegate.getModel().getTrashFolderId())) {
                setStartIconDrawable(VivaldiBookmarkUtils.getTrashFolderIcon(getContext()));
            } else {
                setStartIconDrawable(VivaldiBookmarkUtils.getFolderIcon(getContext()));
            }
        }

        return item;
    }

    @Override
    protected ColorStateList getDefaultStartIconTint() {
        if (ChromeApplication.isVivaldi())
            return null;
        else
        return AppCompatResources.getColorStateList(
                getContext(), BookmarkUtils.getFolderIconTint());
    }
}
