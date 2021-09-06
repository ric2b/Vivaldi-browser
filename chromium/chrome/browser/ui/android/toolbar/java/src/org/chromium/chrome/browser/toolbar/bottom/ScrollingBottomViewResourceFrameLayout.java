// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.util.AttributeSet;

import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

// Vivaldi
import android.annotation.SuppressLint;
import android.view.MotionEvent;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener;

/**
 * A {@link ViewResourceFrameLayout} that specifically handles redraw of the top shadow of the view
 * it represents.
 */
public class ScrollingBottomViewResourceFrameLayout extends ViewResourceFrameLayout {
    /** A cached rect to avoid extra allocations. */
    private final Rect mCachedRect = new Rect();

    /** The height of the shadow sitting above the bottom view in px. */
    private final int mTopShadowHeightPx;

    /** Vivaldi swipe recognizer for handling swipe gestures. */
    private SwipeGestureListener mSwipeGestureListener;

    public ScrollingBottomViewResourceFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mTopShadowHeightPx = getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);
    }

    @Override
    protected ViewResourceAdapter createResourceAdapter() {
        return new ViewResourceAdapter(this) {
            @Override
            public void onCaptureStart(Canvas canvas, Rect dirtyRect) {
                mCachedRect.set(dirtyRect);
                if (mCachedRect.intersect(0, 0, getWidth(), mTopShadowHeightPx)) {
                    canvas.save();

                    // Clip the canvas to only the section of the dirty rect that contains the top
                    // shadow of the view.
                    canvas.clipRect(mCachedRect);

                    // Clear the shadow so redrawing does not make it progressively darker.
                    canvas.drawColor(0, PorterDuff.Mode.CLEAR);

                    canvas.restore();
                }

                super.onCaptureStart(canvas, dirtyRect);
            }
        };
    }

    /**
     * @return The height of the view's top shadow in px.
     */
    public int getTopShadowHeight() {
        return mTopShadowHeightPx;
    }

    /**
     * Vivaldi
     * Handle intercept touch event.
     */
    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        return mSwipeGestureListener.onTouchEvent(event);
    }

    /**
     * Vivaldi
     * Handle touch event.
     */
    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Don't eat the event if we don't have a handler.
        if (mSwipeGestureListener == null) return false;

        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            return true;
        }

        return mSwipeGestureListener.onTouchEvent(event);
    }

    /**
     * Vivaldi
     * Set the swipe handler
     */
    public void setSwipeHandler(SwipeGestureListener.SwipeHandler handler) {
        mSwipeGestureListener = new SwipeGestureListener(getContext(), handler);
    }
}