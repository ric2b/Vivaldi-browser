// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;
import org.chromium.ui.base.WindowAndroid;

// Vivaldi
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.url.GURL;
import org.vivaldi.browser.common.VivaldiUtils;
import org.vivaldi.browser.preferences.VivaldiPreferences;

/** Monitor changes that indicate a theme color change may be needed from tab contents. */
public class TabThemeColorHelper extends EmptyTabObserver {
    private final Callback<Integer> mUpdateCallback;

    // Vivaldi
    private SharedPreferences.OnSharedPreferenceChangeListener mPrefsListener;

    TabThemeColorHelper(Tab tab, Callback<Integer> updateCallback) {
        mUpdateCallback = updateCallback;
        tab.addObserver(this);

        // Vivaldi
        mPrefsListener = (sharedPrefs, key) -> {
            if (VivaldiPreferences.UI_ACCENT_COLOR_SETTING.equals(key)) updateIfNeeded(tab, false);
        };
        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(mPrefsListener);
    }

    /** Notifies the listeners of the tab theme color change. */
    private void updateIfNeeded(Tab tab, boolean didWebContentsThemeColorChange) {
        // Note(david@vivaldi.com): Calculating the actual theme colour here and don't bother what
        // Chromium is doing.
        if (ChromeApplicationImpl.isVivaldi()) {
            VivaldiUtils.calculateThemeColor(tab, mUpdateCallback);
            return;
        }

        int themeColor = tab.getThemeColor();
        if (didWebContentsThemeColorChange) themeColor = tab.getWebContents().getThemeColor();
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
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        if (navigation.errorCode() != NetError.OK) updateIfNeeded(tab, true);
    }

    @Override
    public void onDestroyed(Tab tab) {
        tab.removeObserver(this);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.

        // Note(david@vivaldi.com) We need to update here, otherwise a changed theme from the
        // settings won't be recognized.
        updateIfNeeded(tab,false);
    }

    /** Vivaldi **/
    @Override
    public void onFaviconUpdated(Tab tab, Bitmap icon, GURL iconUrl) {
        updateIfNeeded(tab, false);
    }
}
