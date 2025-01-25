// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.graphics.Canvas;
import android.os.Build;
import android.view.LayoutInflater;

import androidx.core.view.ViewCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPageView.IncognitoNewTabPageManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.user_prefs.UserPrefs;

/** Provides functionality when the user interacts with the Incognito NTP. */
public class IncognitoNewTabPage extends BasicNativePage
        implements InvalidationAwareThumbnailProvider {
    private final Activity mActivity;
    private final Profile mProfile;
    private final int mIncognitoNtpBackgroundColor;

    private String mTitle;
    protected IncognitoNewTabPageView mIncognitoNewTabPageView;

    private boolean mIsLoaded;

    private IncognitoNewTabPageManager mIncognitoNewTabPageManager;
    private IncognitoCookieControlsManager mCookieControlsManager;
    private IncognitoCookieControlsManager.Observer mCookieControlsObserver;

    private void showIncognitoLearnMore() {
        HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                .show(
                        mActivity,
                        mActivity.getString(R.string.help_context_incognito_learn_more),
                        null);
    }

    /**
     * Constructs an Incognito NewTabPage.
     *
     * @param activity The activity used to create the new tab page's View.
     * @param profile The profile associated with this incognito NTP.
     */
    public IncognitoNewTabPage(Activity activity, NativePageHost host, Profile profile) {
        super(host);

        mActivity = activity;
        mProfile = profile;
        if (!mProfile.isOffTheRecord()) {
            throw new IllegalStateException(
                    "Attempting to create an incognito NTP with a normal profile.");
        }

        mIncognitoNtpBackgroundColor = host.getContext().getColor(R.color.ntp_bg_incognito);

        mIncognitoNewTabPageManager =
                new IncognitoNewTabPageManager() {
                    @Override
                    public void loadIncognitoLearnMore() {
                        showIncognitoLearnMore();
                    }

                    @Override
                    public void initCookieControlsManager() {
                        mCookieControlsManager = new IncognitoCookieControlsManager();
                        mCookieControlsManager.initialize(mProfile);
                        mCookieControlsObserver =
                                new IncognitoCookieControlsManager.Observer() {
                                    @Override
                                    public void onUpdate(
                                            boolean checked,
                                            @CookieControlsEnforcement int enforcement) {
                                        mIncognitoNewTabPageView
                                                .setIncognitoCookieControlsToggleEnforcement(
                                                        enforcement);
                                        mIncognitoNewTabPageView
                                                .setIncognitoCookieControlsToggleChecked(checked);
                                    }
                                };
                        mCookieControlsManager.addObserver(mCookieControlsObserver);
                        mIncognitoNewTabPageView.setIncognitoCookieControlsToggleCheckedListener(
                                mCookieControlsManager);
                        mIncognitoNewTabPageView.setIncognitoCookieControlsIconOnclickListener(
                                mCookieControlsManager);
                        mCookieControlsManager.updateIfNecessary();
                    }

                    @Override
                    public boolean shouldCaptureThumbnail() {
                        return mCookieControlsManager.shouldCaptureThumbnail();
                    }

                    @Override
                    public boolean shouldShowTrackingProtectionNtp() {
                        return UserPrefs.get(mProfile)
                                        .getBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED)
                                || ChromeFeatureList.isEnabled(
                                        ChromeFeatureList.TRACKING_PROTECTION_3PCD);
                    }

                    @Override
                    public void destroy() {
                        if (mCookieControlsManager != null) {
                            mCookieControlsManager.removeObserver(mCookieControlsObserver);
                            mCookieControlsManager.destroy();
                        }
                    }

                    @Override
                    public void onLoadingComplete() {
                        mIsLoaded = true;
                    }
                };

        mTitle = host.getContext().getResources().getString(R.string.new_incognito_tab_title);

        LayoutInflater inflater = LayoutInflater.from(host.getContext());
        mIncognitoNewTabPageView =
                (IncognitoNewTabPageView) inflater.inflate(R.layout.new_tab_page_incognito, null);
        mIncognitoNewTabPageView.initialize(mIncognitoNewTabPageManager);

        // Work around https://crbug.com/943873 and https://crbug.com/963385 where default focus
        // highlight shows up after toggling dark mode.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mIncognitoNewTabPageView.setDefaultFocusHighlightEnabled(false);
        }

        initWithView(mIncognitoNewTabPageView);
    }

    /**
     * @return Whether the NTP has finished loaded.
     */
    public boolean isLoadedForTests() {
        return mIsLoaded;
    }

    // NativePage overrides

    @Override
    public void destroy() {
        assert !ViewCompat.isAttachedToWindow(getView())
                : "Destroy called before removed from window";
        mIncognitoNewTabPageManager.destroy();
        super.destroy();
    }

    @Override
    public String getUrl() {
        return UrlConstants.NTP_URL;
    }

    @Override
    public int getBackgroundColor() {
        return mIncognitoNtpBackgroundColor;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public boolean needsToolbarShadow() {
        return true;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public void updateForUrl(String url) {}

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        return mIncognitoNewTabPageView.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mIncognitoNewTabPageView.captureThumbnail(canvas);
    }
}
