// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import android.graphics.Bitmap;
import androidx.palette.graphics.Palette;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.vivaldi.browser.common.VivaldiColorUtils;
import org.vivaldi.browser.preferences.VivaldiPreferences;

/**
 * Monitor changes that indicate a theme color change may be needed from tab contents.
 */
public class TabThemeColorHelper extends EmptyTabObserver {
    private final Tab mTab;
    private final Callback mUpdateCallback;

        // Vivaldi
    private SharedPreferencesManager.Observer mPreferenceObserver;

    TabThemeColorHelper(Tab tab, Callback<Integer> updateCallback) {
        mTab = tab;
        mUpdateCallback = updateCallback;
        tab.addObserver(this);

        // Note(david@vivaldi.com): Update colors when toggle tab strip preference.
        mPreferenceObserver = key -> {
            if (VivaldiPreferences.SHOW_TAB_STRIP.equals(key)) {
                updateIfNeeded(mTab,false);
            }
        };
        SharedPreferencesManager.getInstance().addObserver(mPreferenceObserver);
    }

    /**
     * Notifies the listeners of the tab theme color change.
     */
    private void updateIfNeeded(Tab tab, boolean didWebContentsThemeColorChange) {
        int themeColor = tab.getThemeColor();
        if (didWebContentsThemeColorChange) themeColor = tab.getWebContents().getThemeColor();
        // NOTE(david@vivaldi.com): In Vivaldi we're fetching the favicon color in order to define
        // it as the |themeColor|.
        if (ChromeApplication.isVivaldi()) {
            Bitmap favicon = TabFavicon.getBitmap(tab);
            if (favicon != null && tab.isThemingAllowed()) {
                // NOTE(david@vivaldi.com): We might need to do that asynchronously, if we
                // experiencing any delays.
                Palette palette = Palette.from(favicon).generate();
                if (palette != null) {
                    Palette.Swatch swatch = null;
                    // This is the priority list for searching the most relevant color.
                    if (palette.getDominantSwatch() != null)
                        swatch = palette.getDominantSwatch();
                    else if (palette.getVibrantSwatch() != null)
                        swatch = palette.getVibrantSwatch();
                    else if (palette.getDarkVibrantSwatch() != null)
                        swatch = palette.getDarkVibrantSwatch();
                    // Fetch color...
                    if (swatch != null) {
                        int color = swatch.getRgb();
                        if (color != 0xFFe8e8e8) {
                            themeColor = color;
                        }
                    }
                }
            } else
                themeColor = VivaldiColorUtils.getDefaultToolbarColor(tab);
        }
        mUpdateCallback.onResult(themeColor);
    }

    // TabObserver

    @Override
    public void onSSLStateUpdated(Tab tab) {
        updateIfNeeded(tab, false);
    }

    @Override
    public void onUrlUpdated(Tab tab) {
        updateIfNeeded(tab, false);
    }

    @Override
    public void onDidFailLoad(Tab tab, boolean isMainFrame, int errorCode, GURL failingUrl) {
        updateIfNeeded(tab, true);
    }

    @Override
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
        if (navigation.errorCode() != NetError.OK) updateIfNeeded(tab, true);
    }

    @Override
    public void onDestroyed(Tab tab) {
        tab.removeObserver(this);
        // Vivaldi
        SharedPreferencesManager.getInstance().removeObserver(mPreferenceObserver);
    }

    /** Vivaldi **/
    @Override
    public void onFaviconUpdated(Tab tab, Bitmap icon) {
        updateIfNeeded(tab,false);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.

        // Note(david@vivaldi.com) We need to update here, otherwise a changed theme from the
        // settings won't be recognized.
        updateIfNeeded(tab,false);
    }
}
