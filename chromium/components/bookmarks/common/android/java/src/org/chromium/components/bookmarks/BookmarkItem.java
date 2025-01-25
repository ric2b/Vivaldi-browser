// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.bookmarks;

import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** Contains data about a bookmark or bookmark folder. */
public class BookmarkItem {
    private final String mTitle;
    private final GURL mUrl;
    private final BookmarkId mId;
    private final boolean mIsFolder;
    private final BookmarkId mParentId;
    private final boolean mIsEditable;
    private final boolean mIsManaged;
    private final long mDateAdded;
    private final boolean mRead;
    private final long mDateLastOpened;
    private final boolean mIsAccountBookmark;

    private boolean mForceEditableForTesting;

    public BookmarkItem(
            BookmarkId id,
            String title,
            GURL url,
            boolean isFolder,
            BookmarkId parentId,
            boolean isEditable,
            boolean isManaged,
            long dateAdded,
            boolean read,
            long dateLastOpened,
            boolean isAccountBookmark) {
        mId = id;
        mTitle = title;
        mUrl = url;
        mIsFolder = isFolder;
        mParentId = parentId;
        mIsEditable = isEditable;
        mIsManaged = isManaged;
        mDateAdded = dateAdded;
        mRead = read;
        mDateLastOpened = dateLastOpened;
        mIsAccountBookmark = isAccountBookmark;
    }

    /** Returns the title of the bookmark item. */
    public String getTitle() {
        return mTitle;
    }

    /** Returns the url of the bookmark item. */
    public GURL getUrl() {
        return mUrl;
    }

    /** Returns the string to display for the item's url. */
    public String getUrlForDisplay() {
        return UrlFormatter.formatUrlForSecurityDisplay(
                getUrl(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
    }

    /** Returns whether item is a folder or a bookmark. */
    public boolean isFolder() {
        return mIsFolder;
    }

    /** Returns the parent id of the bookmark item. */
    public BookmarkId getParentId() {
        return mParentId;
    }

    /** Returns whether this bookmark can be edited. */
    public boolean isEditable() {
        return mForceEditableForTesting || mIsEditable;
    }

    /** Returns whether this bookmark's URL can be edited */
    public boolean isUrlEditable() {
        return isEditable() && mId.getType() == BookmarkType.NORMAL;
    }

    /** Returns whether this bookmark can be moved */
    public boolean isReorderable() {
        return isEditable() && mId.getType() == BookmarkType.NORMAL;
    }

    /** Returns whether this is a managed bookmark. */
    public boolean isManaged() {
        return mIsManaged;
    }

    /** Returns the {@link BookmarkId}. */
    public BookmarkId getId() {
        return mId;
    }

    /** Returns the timestamp in milliseconds since epoch that the bookmark is added. */
    public long getDateAdded() {
        return mDateAdded;
    }

    /**
     * Returns whether the bookmark is read. Only valid for {@link BookmarkType#READING_LIST}.
     * Defaults to "false" for other types.
     */
    public boolean isRead() {
        return mRead;
    }

    /**
     * Returns the timestamp in milliseconds since epoch that the bookmark was last opened. Folders
     * have a value of 0 for this, but should use the most recent child.
     */
    public long getDateLastOpened() {
        return mDateLastOpened;
    }

    /** Returns whether the bookmark is linked to your Google account. */
    public boolean isAccountBookmark() {
        return mIsAccountBookmark;
    }

    // TODO(crbug.com/40655824): Remove when BookmarkModel is stubbed in tests instead.
    public void forceEditableForTesting() {
        mForceEditableForTesting = true;
    }

        /** Vivaldi specific members. */
        private boolean mIsSpeedDial;
        private String mNickName;
        private String mDescription;
        private String mThumbnailPath;
        private int mThemeColor;
        private long mCreated;
        private String mGUID;

        /** Constructor used with Vivaldi. */
        public BookmarkItem(BookmarkId id, String title, GURL url, boolean isFolder,
                             BookmarkId parentId, boolean isEditable, boolean isManaged,
                             long dateAdded, boolean read, boolean isSpeedDial,
                             String nickName, String description, int themeColor,
                             long created, String thumbnailPath, String guid) {
            this(id, title, url, isFolder, parentId, isEditable, isManaged, dateAdded, read,
                    created, false);
            mIsSpeedDial = isSpeedDial;
            mNickName = nickName;
            mDescription = description;
            mThumbnailPath = thumbnailPath;
            mThemeColor = themeColor;
            mCreated = created;
            mGUID = guid;
        }

        /** @return (Vivaldi) Whether this is a speed dial bookmark. */
        public boolean isSpeeddial() { return mIsSpeedDial; }

        /** @return (Vivaldi) Nickname of the bookmark item. */
        public String getNickName() { return mNickName; }

        /** @return (Vivaldi) Description of the bookmark item. */
        public String getDescription() { return mDescription; }

        /** @return (Vivaldi) Thumbnail path of the bookmark item. */
        public String getThumbnailPath() { return mThumbnailPath; }

        /** @return (Vivaldi) Theme color of the bookmark item's site. */
        public int getThemeColor() { return mThemeColor; }

        /** @return (Vivaldi) Created date of the bookmark item */
        public long getCreated() { return mCreated; }

        /** @return (Vivaldi) GUID of item */
        public String getGUID() { return mGUID; }

        /** @return (Vivaldi) Whether this is a default (pre-installed) bookmark. */
        public boolean isDefaultBookmark() {
            return mThumbnailPath != null && mThumbnailPath.startsWith("/resources");
        }

        /** Vivaldi **/
        public void setThumbnailPath(String path) { mThumbnailPath = path; }
}
