// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.res.Configuration;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

// Vivaldi
import android.os.Build;
import androidx.appcompat.app.AppCompatDelegate;
import org.chromium.base.Log;
import org.chromium.build.BuildConfig;

/**
 * Observes and keeps a record of the system night mode state (i.e. the night mode from the
 * application).
 */
public class SystemNightModeMonitor {
    private static SystemNightModeMonitor sInstance;

    /** Interface for callback when system night mode is changed. */
    public interface Observer {
        /** Called when system night mode is changed. */
        void onSystemNightModeChanged();
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private boolean mSystemNightModeOn;

    /**
     * @return The {@link SystemNightModeMonitor} that observes the system night mode state (i.e.
     *     the night mode from the application).
     */
    public static SystemNightModeMonitor getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SystemNightModeMonitor();
        }
        return sInstance;
    }

    public static void setInstanceForTesting(SystemNightModeMonitor instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    private SystemNightModeMonitor() {
        calculateSystemNightMode();
    }

    /** @return True if system night mode is on, and false otherwise. */
    public boolean isSystemNightModeOn() {
        return mSystemNightModeOn;
    }

    /** @param observer The {@link Observer} to be added for observing system night mode changes. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The {@link Observer} to be removed from observing system night mode changes.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /** Updates the system night mode state, and notifies observers if system night mode changes. */
    public void onApplicationConfigurationChanged() {
        final boolean oldNightMode = mSystemNightModeOn;
        calculateSystemNightMode();

        if (oldNightMode != mSystemNightModeOn) {
            for (Observer observer : mObservers) observer.onSystemNightModeChanged();
        }
    }

    private void calculateSystemNightMode() {
        // Vivaldi
        if (BuildConfig.IS_OEM_LYNKCO_BUILD && Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            final int defaultNightMode = AppCompatDelegate.getDefaultNightMode();
            Log.d("SystemNightModeMonitor", "< API29 defaultNightMode = " + defaultNightMode);
            mSystemNightModeOn = (defaultNightMode == AppCompatDelegate.MODE_NIGHT_UNSPECIFIED);
            return;
        }
        final int uiMode =
                ContextUtils.getApplicationContext().getResources().getConfiguration().uiMode;
        mSystemNightModeOn =
                (uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
    }
}
