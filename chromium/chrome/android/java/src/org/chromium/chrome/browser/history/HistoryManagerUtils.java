// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;

import org.vivaldi.browser.panels.PanelUtils;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;

/**
 * Utility methods for the browsing history manager.
 */
public class HistoryManagerUtils {

    /**
     * Opens the browsing history manager.
     *
     * @param activity The {@link ChromeActivity} that owns the {@link HistoryManager}.
     * @param tab The {@link Tab} to used to display the native page version of the
     *            {@link HistoryManager}.
     */
    public static void showHistoryManager(ChromeActivity activity, Tab tab) {
        if (ChromeApplication.isVivaldi()) {
            showHistoryManagerForVivaldi(activity, tab);
            return;
        }
        Context appContext = ContextUtils.getApplicationContext();
        if (activity.isTablet()) {
            // History shows up as a tab on tablets.
            LoadUrlParams params = new LoadUrlParams(UrlConstants.NATIVE_HISTORY_URL);
                tab.loadUrl(params);
        } else {
            Intent intent = new Intent();
            intent.setClass(appContext, HistoryActivity.class);
            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            intent.putExtra(IntentHandler.EXTRA_INCOGNITO_MODE,
                    activity.getTabModelSelector().isIncognitoSelected());
            activity.startActivity(intent);
        }
    }

    public static void showHistoryManagerForVivaldi(ChromeActivity activity, Tab tab) {
        if (tab == null && activity instanceof ChromeTabbedActivity) {
            ChromeTabbedActivity chromeActivity = ((ChromeTabbedActivity) activity);
            tab = chromeActivity.getActivityTab();
        }
        Context appContext = ContextUtils.getApplicationContext();
        if (activity.isTablet()) {
            // History shows up as a tab on tablets.
            LoadUrlParams params = new LoadUrlParams(UrlConstants.NATIVE_HISTORY_URL);
            if (tab == null || !tab.isInitialized()) {
                // Open a new tab.
                TabDelegate delegate = new TabDelegate(false);
                delegate.createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
            } else {
                tab.loadUrl(params);
            }
        } else {
            if (ChromeApplication.isVivaldi()) {
                PanelUtils.showPanel(activity, UrlConstants.NATIVE_HISTORY_URL,
                        activity.getTabModelSelector().isIncognitoSelected());
                return;
            }
        }
    }
}
