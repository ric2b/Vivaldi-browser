// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;

// Vivaldi
import android.graphics.PorterDuff;

import androidx.core.widget.ImageViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.components.bookmarks.BookmarkType;

/**
 * A row view that shows bookmark info in the bookmarks UI.
 */
public class BookmarkItemRow extends BookmarkRow implements LargeIconCallback {
    private GURL mUrl;
    private RoundedIconGenerator mIconGenerator;
    private final int mMinIconSize;
    private final int mDisplayedIconSize;
    private boolean mFaviconCancelled;

    /**
     * Constructor for inflating from XML.
     */
    public BookmarkItemRow(Context context, AttributeSet attrs) {
        super(context, attrs);
        mMinIconSize = isVisualRefreshEnabled()
                ? getResources().getDimensionPixelSize(R.dimen.bookmark_refresh_min_start_icon_size)
                : getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size);

        mDisplayedIconSize = isVisualRefreshEnabled()
                ? getResources().getDimensionPixelSize(
                        R.dimen.bookmark_refresh_preferred_start_icon_size)
                : getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        if (isVisualRefreshEnabled()) {
            mIconGenerator = new RoundedIconGenerator(mDisplayedIconSize, mDisplayedIconSize,
                    mDisplayedIconSize / 2,
                    getContext().getColor(R.color.default_favicon_background_color),
                    getResources().getDimensionPixelSize(
                            R.dimen.bookmark_refresh_circular_monogram_text_size));
        } else {
            mIconGenerator = FaviconUtils.createCircularIconGenerator(context);
        }
    }

    // BookmarkRow implementation.

    @Override
    public void onClick() {
        switch (mDelegate.getCurrentState()) {
            case BookmarkUIState.STATE_FOLDER:
            case BookmarkUIState.STATE_SEARCHING:
                break;
            case BookmarkUIState.STATE_LOADING:
                assert false :
                        "The main content shouldn't be inflated if it's still loading";
                break;
            default:
                assert false : "State not valid";
                break;
        }
        mDelegate.openBookmark(mBookmarkId);
    }

    @Override
    BookmarkItem setBookmarkId(
            BookmarkId bookmarkId, @Location int location, boolean fromFilterView) {
        BookmarkItem item = super.setBookmarkId(bookmarkId, location, fromFilterView);
        mUrl = item.getUrl();
        mStartIconView.setImageDrawable(null);
        mTitleView.setText(item.getTitle());
        mDescriptionView.setText(item.getUrlForDisplay());
        mFaviconCancelled = false;
        mDelegate.getLargeIconBridge().getLargeIconForUrl(mUrl, mMinIconSize, this);
        // Vivaldi - change font color for read items in Reading List
        boolean isItemRead = bookmarkId.getType() == BookmarkType.READING_LIST && item.isRead();
        int color = isItemRead
                ? R.color.vivaldi_disabled_text_color
                : R.color.default_text_color_baseline;
        mTitleView.setTextColor(getResources().getColor(color));
        mDescriptionView.setTextColor(getResources().getColor(color));
        return item;
    }

    /** Allows cancellation of the favicon. */
    protected void cancelFavicon() {
        mFaviconCancelled = true;
    }

    // LargeIconCallback implementation.

    @Override
    public void onLargeIconAvailable(Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
            @IconType int iconType) {
        if (mFaviconCancelled) return;

        if (ChromeApplicationImpl.isVivaldi()) {
            largeIconAvailable(icon, fallbackColor, isFallbackColorDefault, iconType);
            return;
        }

        Drawable iconDrawable = FaviconUtils.getIconDrawableWithoutFilter(
                icon, mUrl, fallbackColor, mIconGenerator, getResources(), mDisplayedIconSize);
        setStartIconDrawable(iconDrawable);
    }

    protected boolean getFaviconCancelledForTesting() {
        return mFaviconCancelled;
    }

    void setRoundedIconGeneratorForTesting(RoundedIconGenerator roundedIconGenerator) {
        mIconGenerator = roundedIconGenerator;
    }

    // Vivaldi
    private void largeIconAvailable(Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
            @IconType int iconType) {
        int vivaldiIconSize =
                getResources().getDimensionPixelSize(R.dimen.panels_favicon_size);
        Drawable iconDrawable = FaviconUtils.getIconDrawableWithoutFilter(
                icon, mUrl.getSpec(), fallbackColor, mIconGenerator, getResources(),
                vivaldiIconSize);
        setStartIconDrawable(iconDrawable);
        if (mStartIconView != null) setReadingListIconTint();
    }

    /** Vivaldi - Sets the icon color as per the read status of Bookmark item **/
    private void setReadingListIconTint() {
        BookmarkId bookmarkId = this.getItem();
        BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(bookmarkId);
        if (this.getItem().getType() == BookmarkType.READING_LIST && bookmarkItem.isRead()) {
            mStartIconView.setColorFilter(getContext().getColor(
                    R.color.default_text_color_disabled_light), PorterDuff.Mode.MULTIPLY);
            if (mMoreIcon != null)
                mMoreIcon.setColorFilter(getContext().getColor(
                        R.color.default_text_color_disabled_light), PorterDuff.Mode.MULTIPLY);
        } else {
            mStartIconView.setColorFilter(null);
            ImageViewCompat.setImageTintList(mStartIconView, getDefaultStartIconTint());
            if (mMoreIcon != null) {
                mMoreIcon.setColorFilter(null);
                ImageViewCompat.setImageTintList(mMoreIcon, getDefaultStartIconTint());
            }
        }
    }
}
