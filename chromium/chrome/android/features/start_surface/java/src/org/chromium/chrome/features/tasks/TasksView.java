// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.coordinatorlayout.widget.CoordinatorLayout;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedStreamViewResizer;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.IncognitoDescriptionView;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;

/** The view of the tasks surface. Set public for testing. */
public class TasksView extends CoordinatorLayoutForPointer {

    private final Context mContext;
    private FrameLayout mCardTabSwitcherContainer;
    private AppBarLayout mHeaderView;
    private ViewGroup mMvTilesContainerLayout;
    @Nullable private ViewGroup mHomeModulesLayout;
    private SearchBoxCoordinator mSearchBoxCoordinator;
    private IncognitoDescriptionView mIncognitoDescriptionView;
    private View.OnClickListener mIncognitoDescriptionLearnMoreListener;
    private boolean mIncognitoCookieControlsToggleIsChecked;
    private OnCheckedChangeListener mIncognitoCookieControlsToggleCheckedListener;
    private @CookieControlsEnforcement int mIncognitoCookieControlsToggleEnforcement =
            CookieControlsEnforcement.NO_ENFORCEMENT;
    private View.OnClickListener mIncognitoCookieControlsIconClickListener;
    private UiConfig mUiConfig;
    private ObservableSupplier<Profile> mProfileSupplier;

    /** Default constructor needed to inflate via XML. */
    public TasksView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    public void initialize(
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            boolean isIncognito,
            WindowAndroid windowAndroid,
            ObservableSupplier<Profile> profileSupplier) {
        mProfileSupplier = profileSupplier;
        assert mSearchBoxCoordinator != null
                : "#onFinishInflate should be completed before the call to initialize.";

        mSearchBoxCoordinator.initialize(activityLifecycleDispatcher, isIncognito, windowAndroid);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCardTabSwitcherContainer = findViewById(R.id.tab_switcher_module_container);
        mMvTilesContainerLayout = findViewById(R.id.mv_tiles_container);
        mSearchBoxCoordinator = new SearchBoxCoordinator(getContext(), this);
        mHomeModulesLayout = findViewById(R.id.home_modules_recycler_view);

        mHeaderView = findViewById(R.id.task_surface_header);

        forceHeaderScrollable();

        mUiConfig = new UiConfig(this);
        setHeaderPadding();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mUiConfig.updateDisplayStyle();
    }

    public ViewGroup getCardTabSwitcherContainer() {
        return mCardTabSwitcherContainer;
    }

    public ViewGroup getBodyViewContainer() {
        return findViewById(R.id.tasks_surface_body);
    }

    /**
     * Set the visibility of the tasks surface body.
     * @param isVisible Whether it's visible.
     */
    void setSurfaceBodyVisibility(boolean isVisible) {
        getBodyViewContainer().setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the visibility of the tab carousel.
     *
     * @param isVisible Whether it's visible.
     */
    void setTabCardVisibility(boolean isVisible) {
        mCardTabSwitcherContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * @return The {@link SearchBoxCoordinator} representing the fake search box.
     */
    SearchBoxCoordinator getSearchBoxCoordinator() {
        return mSearchBoxCoordinator;
    }

    /** Set the visibility of the Most Visited Tiles. */
    void setMostVisitedVisibility(int visibility) {
        mMvTilesContainerLayout.setVisibility(visibility);
    }

    /** Set the visibility of the Magic stack. */
    void setMagicStackVisibility(int visibility) {
        mHomeModulesLayout.setVisibility(visibility);
    }

    /** Set the visibility of the Query Tiles. */
    void setQueryTilesVisibility(int visibility) {
        findViewById(R.id.query_tiles_container).setVisibility(visibility);
    }

    /**
     * Set the incognito state.
     * @param isIncognito Whether it's in incognito mode.
     */
    void setIncognitoMode(boolean isIncognito) {
        mSearchBoxCoordinator.setIncognitoMode(isIncognito);
        Drawable searchBackground;
        if (isIncognito) {
            searchBackground =
                    AppCompatResources.getDrawable(
                            mContext, R.drawable.fake_search_box_bg_incognito);
        } else {
            searchBackground =
                    AppCompatResources.getDrawable(
                            mContext, R.drawable.home_surface_search_box_background);
        }
        mSearchBoxCoordinator.setBackground(searchBackground);
    }

    /**
     * Initialize incognito description view. Note that this interface is supposed to be called only
     * once.
     */
    void initializeIncognitoDescriptionView() {
        assert mIncognitoDescriptionView == null;
        ViewStub containerStub = findViewById(R.id.incognito_description_container_layout_stub);
        View containerView = containerStub.inflate();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            containerView.setFocusable(true);
            containerView.setFocusableInTouchMode(true);
        }

        ViewStub incognitoDescriptionViewStub = findViewById(R.id.task_view_incognito_layout_stub);
        incognitoDescriptionViewStub.setLayoutResource(R.layout.incognito_description_layout);
        mIncognitoDescriptionView =
                (IncognitoDescriptionView) incognitoDescriptionViewStub.inflate();

        // Inflate the correct cookie/tracking protection card.
        ViewStub cardStub = findViewById(R.id.cookie_card_stub);
        if (cardStub == null) return;
        if (shouldShowTrackingProtectionNtp()) {
            cardStub.setLayoutResource(R.layout.incognito_tracking_protection_card);
        } else {
            cardStub.setLayoutResource(R.layout.incognito_cookie_controls_card);
        }
        cardStub.inflate();
        mIncognitoDescriptionView.formatTrackingProtectionText(getContext(), this);

        if (mIncognitoDescriptionLearnMoreListener != null) {
            setIncognitoDescriptionLearnMoreClickListener(mIncognitoDescriptionLearnMoreListener);
        }
        setIncognitoCookieControlsToggleChecked(mIncognitoCookieControlsToggleIsChecked);
        if (mIncognitoCookieControlsToggleCheckedListener != null) {
            setIncognitoCookieControlsToggleCheckedListener(
                    mIncognitoCookieControlsToggleCheckedListener);
        }
        setIncognitoCookieControlsToggleEnforcement(mIncognitoCookieControlsToggleEnforcement);
        if (mIncognitoCookieControlsIconClickListener != null) {
            setIncognitoCookieControlsIconClickListener(mIncognitoCookieControlsIconClickListener);
        }
    }

    /**
     * Set the visibility of the incognito description.
     * @param isVisible Whether it's visible or not.
     */
    void setIncognitoDescriptionVisibility(boolean isVisible) {
        ((View) mIncognitoDescriptionView).setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the incognito description learn more click listener.
     * @param listener The given click listener.
     */
    void setIncognitoDescriptionLearnMoreClickListener(View.OnClickListener listener) {
        mIncognitoDescriptionLearnMoreListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setLearnMoreOnclickListener(listener);
            mIncognitoDescriptionLearnMoreListener = null;
        }
    }

    /**
     * Set the toggle on the cookie controls card.
     * @param isChecked Whether it's checked or not.
     */
    void setIncognitoCookieControlsToggleChecked(boolean isChecked) {
        mIncognitoCookieControlsToggleIsChecked = isChecked;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsToggle(isChecked);
        }
    }

    /**
     * Set the incognito cookie controls toggle checked change listener.
     * @param listener The given checked change listener.
     */
    void setIncognitoCookieControlsToggleCheckedListener(OnCheckedChangeListener listener) {
        mIncognitoCookieControlsToggleCheckedListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsToggleOnCheckedChangeListener(listener);
            mIncognitoCookieControlsToggleCheckedListener = null;
        }
    }

    /**
     * Set the enforcement rule for the incognito cookie controls toggle.
     * @param enforcement The enforcement enum to set.
     */
    void setIncognitoCookieControlsToggleEnforcement(@CookieControlsEnforcement int enforcement) {
        mIncognitoCookieControlsToggleEnforcement = enforcement;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsEnforcement(enforcement);
        }
    }

    /**
     * Set the incognito cookie controls icon click listener.
     * @param listener The given onclick listener.
     */
    void setIncognitoCookieControlsIconClickListener(OnClickListener listener) {
        mIncognitoCookieControlsIconClickListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsIconOnclickListener(listener);
            mIncognitoCookieControlsIconClickListener = null;
        }
    }

    /** Set the height of the top toolbar placeholder layout. */
    void setTopToolbarPlaceholderHeight(int height) {
        View topToolbarPlaceholder = findViewById(R.id.top_toolbar_placeholder);
        ViewGroup.LayoutParams lp = topToolbarPlaceholder.getLayoutParams();
        lp.height = height;
        topToolbarPlaceholder.setLayoutParams(lp);
    }

    /** Reset the scrolling position by expanding the {@link #mHeaderView}. */
    void resetScrollPosition() {
        if (mHeaderView != null && mHeaderView.getHeight() != mHeaderView.getBottom()) {
            mHeaderView.setExpanded(true, /* animate= */ false);
        }
    }

    /**
     * Add a header offset change listener.
     * @param onOffsetChangedListener The given header offset change listener.
     */
    public void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        if (mHeaderView != null) {
            mHeaderView.addOnOffsetChangedListener(onOffsetChangedListener);
        }
    }

    /**
     * Remove the given header offset change listener.
     * @param onOffsetChangedListener The header offset change listener which should be removed.
     */
    public void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        if (mHeaderView != null) {
            mHeaderView.removeOnOffsetChangedListener(onOffsetChangedListener);
        }
    }

    /**
     * Update the fake search box layout.
     *
     * @param height Current height of the fake search box layout.
     * @param topMargin Current top margin of the fake search box layout.
     * @param endPadding Current end padding of the fake search box layout.
     * @param translationX Current translationX of text view in fake search box layout.
     * @param searchTextSize Current size for the search text in the fake search box.
     */
    public void updateFakeSearchBox(
            int height, int topMargin, int endPadding, float translationX, float searchTextSize) {
        if (mSearchBoxCoordinator.getView().getVisibility() != View.VISIBLE) return;
        mSearchBoxCoordinator.setHeight(height);
        mSearchBoxCoordinator.setTopMargin(topMargin);
        mSearchBoxCoordinator.setEndPadding(endPadding);
        mSearchBoxCoordinator.setTextViewTranslationX(translationX);
        mSearchBoxCoordinator.setSearchTextSize(searchTextSize);
    }

    /**
     * Update both the fake search box height.
     * @param height Current height of the fake search box.
     */
    public void updateFakeSearchBoxHeight(int height) {
        mSearchBoxCoordinator.setHeight(height);
    }

    private void forceHeaderScrollable() {
        // TODO(crbug.com/40198436): Find out why scrolling was broken after
        // crrev.com/c/3025127. Force the header view to be draggable as a workaround.
        CoordinatorLayout.LayoutParams params =
                (CoordinatorLayout.LayoutParams) mHeaderView.getLayoutParams();
        AppBarLayout.Behavior behavior = new AppBarLayout.Behavior();
        behavior.setDragCallback(
                new AppBarLayout.Behavior.DragCallback() {
                    @Override
                    public boolean canDrag(AppBarLayout appBarLayout) {
                        return true;
                    }
                });
        params.setBehavior(behavior);
    }

    /**
     * Make the padding of header consistent with that of Feed recyclerview which is sized by {@link
     * FeedStreamViewResizer} in {@link FeedSurfaceCoordinator}
     */
    private void setHeaderPadding() {
        FeedStreamViewResizer.createAndAttach((Activity) mContext, mHeaderView, mUiConfig);
    }

    /**
     * Set the background color for Start Surface.
     * @param backgroundColor The drawable which contains the background color to set for the Start
     *         Surface.
     */
    void setStartSurfaceBackgroundColor(int backgroundColor) {
        setBackgroundColor(backgroundColor);
        mHeaderView.setBackgroundColor(backgroundColor);
    }

    boolean shouldShowTrackingProtectionNtp() {
        Profile profile = mProfileSupplier.get();
        assert !profile.isOffTheRecord();
        return (UserPrefs.get(profile).getBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_3PCD));
    }
}
