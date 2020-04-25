// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;

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
            setIconDrawable(BookmarkUtils.getFolderIcon(getContext()));
    }

    // BookmarkRow implementation.

    @Override
    public void onClick() {
        mDelegate.openFolder(mBookmarkId);
    }

    @Override
    BookmarkItem setBookmarkId(BookmarkId bookmarkId) {
        BookmarkItem item = super.setBookmarkId(bookmarkId);
        mTitleView.setText(item.getTitle());
        int childCount = mDelegate.getModel().getTotalBookmarkCount(bookmarkId);
        mDescriptionView.setText((childCount > 0)
                        ? getResources().getQuantityString(
                                  R.plurals.bookmarks_count, childCount, childCount)
                        : getResources().getString(R.string.no_bookmarks));
        if (ChromeApplication.isVivaldi()) {
            if (bookmarkId.equals(mDelegate.getModel().getTrashFolderId())) {
                setIconDrawable(VivaldiBookmarkUtils.getTrashFolderIcon(getContext()));
            } else {
                setIconDrawable(VivaldiBookmarkUtils.getFolderIcon(getContext()));
            }
        }
        return item;
    }

    @Override
    protected ColorStateList getDefaultIconTint() {
        if (ChromeApplication.isVivaldi())
            return null;
        else
        return AppCompatResources.getColorStateList(
                getContext(), BookmarkUtils.getFolderIconTint());
    }
}
