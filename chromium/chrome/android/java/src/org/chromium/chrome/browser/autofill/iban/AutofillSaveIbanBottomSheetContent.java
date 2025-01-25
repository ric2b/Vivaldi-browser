// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.view.View;
import android.widget.ScrollView;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** This class is responsible for rendering the content for the Autofill save IBAN bottomsheet. */
/*package*/ class AutofillSaveIbanBottomSheetContent implements BottomSheetContent {
    private final ScrollView mScrollView;
    private final View mContentView;

    AutofillSaveIbanBottomSheetContent(View contentView, ScrollView scrollView) {
        mContentView = contentView;
        mScrollView = scrollView;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public void destroy() {}

    @Override
    public boolean hasCustomLifecycle() {
        // This bottom sheet should stay open during page navigation.
        return true;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mScrollView.getScrollY();
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO(b/309163431): Support a11y.
        return R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(b/309163431): Support a11y.
        return R.string.ok;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(b/309163431): Support a11y.
        return R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(b/309163431): Support a11y.
        return R.string.ok;
    }
}
