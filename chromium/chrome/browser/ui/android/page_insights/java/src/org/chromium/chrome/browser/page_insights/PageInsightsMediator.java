// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent.BOTTOM_SHEET_EXPANDED;
import static org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent.BOTTOM_SHEET_PEEKING;
import static org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent.DISMISSED_FROM_PEEKING_STATE;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.text.format.DateUtils;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.page_insights.proto.Config.PageInsightsConfig;
import org.chromium.chrome.browser.page_insights.proto.IntentParams.PageInsightsIntentParams;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.Page;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceRenderer;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceScope;
import org.chromium.chrome.browser.xsurface_provider.XSurfaceProcessScopeProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * PageInsights mediator component listening to various external events to update UI, internal
 * states accordingly:
 *
 * <ul>
 *   <li>Observes browser controls for hide-on-scroll behavior
 *   <li>Closes the sheet when the Tab page gets reloaded
 *   <li>Resizes contents upon Sheet offset/state changes
 *   <li>Adjusts the top corner radius to the sheet height
 * </ul>
 */
public class PageInsightsMediator extends EmptyTabObserver implements BottomSheetObserver {
    private static final String TAG = "PIMediator";

    static final int DEFAULT_TRIGGER_DELAY_MS = (int) DateUtils.MINUTE_IN_MILLIS;
    private static final float MINIMUM_CONFIDENCE = 0.5f;
    static final String PAGE_INSIGHTS_PEEK_DELAY_PARAM = "page_insights_peek_delay";
    static final String PAGE_INSIGHTS_CAN_PEEK_WHILE_IN_MOTION_PARAM =
            "page_insights_can_peek_while_in_motion";
    static final String PAGE_INSIGHTS_CAN_RETURN_TO_PEEK_AFTER_EXPANSION_PARAM =
            "page_insights_can_return_to_peek_after_expansion";
    private static final List<Integer> USER_DISMISSAL_REASONS =
            Arrays.asList(
                    StateChangeReason.SWIPE,
                    StateChangeReason.BACK_PRESS,
                    StateChangeReason.TAP_SCRIM);

    private final PageInsightsSheetContent mSheetContent;
    private final ManagedBottomSheetController mSheetController;
    private final Context mContext;

    // BottomSheetController for other bottom sheet UIs.
    private final BottomSheetController mOtherBottomSheetController;

    // Observers other bottom sheet UI state.
    private final BottomSheetObserver mOtherBottomSheetObserver;

    // Bottom browser controls resizer. Used to resize web contents to move up bottom-aligned
    // elements such as cookie dialog.
    private final BrowserControlsSizer mBrowserControlsSizer;

    private final BrowserControlsStateProvider mControlsStateProvider;

    // Browser controls observer. Monitors the browser controls offset changes to scroll
    // away the bottom sheet in sync with the controls.
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;

    private final ExpandedSheetHelper mExpandedSheetHelper;

    // Sheet background drawable whose top corners needs to be rounded.
    private GradientDrawable mBackgroundDrawable;

    private int mMaxCornerRadiusPx;
    private View mSheetContainer;

    private final BooleanSupplier mIsPageInsightsEnabledSupplier;
    private final PageInsightsCoordinator.ConfigProvider mPageInsightsConfigProvider;
    private final Handler mHandler;
    private final Runnable mAutoTriggerTimerRunnable = this::onAutoTriggerTimerFinished;
    private final Callback<Boolean> mInMotionCallback = inMotion -> maybeAutoTrigger();
    private final PageInsightsSheetContent.OnBottomSheetTouchHandler mOnBottomSheetTouchHandler =
            new PageInsightsSheetContent.OnBottomSheetTouchHandler() {
                @Override
                public boolean handleTap() {
                    return handleBottomSheetTap();
                }

                @Override
                public boolean shouldInterceptTouchEvents() {
                    return shouldInterceptBottomSheetTouchEvents();
                }
            };
    private final HashMap<String, Object> mSurfaceRendererContextValues;
    private final ObservableSupplier<Tab> mTabObservable;
    private final Supplier<Profile> mProfileSupplier;
    private final ObservableSupplierImpl<Boolean> mWillHandleBackPressSupplier;
    private final boolean mIsAccessibilityEnabled;
    private final boolean mCanAutoTrigger;
    private final boolean mCanAutoTriggerWhileInMotion;
    private final boolean mCanReturnToPeekAfterExpansion;
    @Nullable private final ObservableSupplier<Boolean> mInMotionSupplier;
    @Nullable private final BackPressManager mBackPressManager;
    @Nullable private final BackPressHandler mBackPressHandler;
    private final ObservableSupplierImpl<Integer> mSheetInset = new ObservableSupplierImpl<>();
    private final boolean mResizeInSync;

    private PageInsightsDataLoader mPageInsightsDataLoader;
    @Nullable private volatile PageInsightsSurfaceRenderer mSurfaceRenderer;
    @Nullable private PageInsightsMetadata mCurrentMetadata;
    @Nullable private PageInsightsConfig mCurrentConfig;
    @Nullable private View mCurrentFeedView;
    @Nullable private View mCurrentChildView;
    private boolean mIsShowingChildView;
    @Nullable private NavigationHandle mCurrentNavigationHandle;
    @Nullable private Tab mObservedTab;

    // Caches the sheet height at the current state. Avoids the repeated call to resize the content
    // if the size hasn't changed since.
    private int mCachedSheetHeight;

    // Amount of time to wait before triggering the sheet automatically. Can be overridden
    // for testing.
    private int mAutoTriggerDelayMs;

    private int mOldState = SheetState.NONE;

    // True if since the Page Insights component was created there has been at least one page load
    // started.
    private boolean mHasPageLoadBeenStartedSinceCreation;

    @IntDef({
        AutoTriggerStage.CANCELLED_OR_NOT_STARTED,
        AutoTriggerStage.AWAITING_TIMER,
        AutoTriggerStage.FETCHING_DATA,
        AutoTriggerStage.PREPARING,
        AutoTriggerStage.READY_FOR_AUTO_TRIGGER,
        AutoTriggerStage.AUTO_TRIGGERED
    })
    @interface AutoTriggerStage {
        int CANCELLED_OR_NOT_STARTED = 0;
        int AWAITING_TIMER = 1;
        int FETCHING_DATA = 2;
        int PREPARING = 3;
        int READY_FOR_AUTO_TRIGGER = 4;
        int AUTO_TRIGGERED = 5;
    }

    private volatile @AutoTriggerStage int mAutoTriggerStage =
            AutoTriggerStage.CANCELLED_OR_NOT_STARTED;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        PageInsightsEvent.USER_INVOKES_PIH,
        PageInsightsEvent.AUTO_PEEK_TRIGGERED,
        PageInsightsEvent.STATE_PEEK,
        PageInsightsEvent.STATE_EXPANDED,
        PageInsightsEvent.DISMISS_PEEK,
        PageInsightsEvent.DISMISS_EXPANDED,
        PageInsightsEvent.TAP_XSURFACE_VIEW_URL,
        PageInsightsEvent.TAP_XSURFACE_VIEW_SHARE,
        PageInsightsEvent.TAP_XSURFACE_VIEW_CHILD_PAGE,
        PageInsightsEvent.COUNT
    })
    @interface PageInsightsEvent {
        int USER_INVOKES_PIH = 0;
        int AUTO_PEEK_TRIGGERED = 1;
        int STATE_PEEK = 2;
        int STATE_EXPANDED = 3;
        int DISMISS_PEEK = 4;
        int DISMISS_EXPANDED = 5;
        // User interacts with a xSurface card with URL in PIH
        int TAP_XSURFACE_VIEW_URL = 6;
        // User interacts with a xSurface share functionality in PIH
        int TAP_XSURFACE_VIEW_SHARE = 7;
        // User interacts with a xSurface child page in PIH
        int TAP_XSURFACE_VIEW_CHILD_PAGE = 8;
        int COUNT = 9;
    }

    private static void logPageInsightsEvent(@PageInsightsEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.PageInsights.Event", event, PageInsightsEvent.COUNT);
    }

    public PageInsightsMediator(
            Context context,
            View layoutView,
            ObservableSupplier<Tab> tabObservable,
            Supplier<ShareDelegate> shareDelegateSupplier,
            Supplier<Profile> profileSupplier,
            ManagedBottomSheetController bottomSheetController,
            BottomSheetController otherBottomSheetController,
            ExpandedSheetHelper expandedSheetHelper,
            BrowserControlsStateProvider controlsStateProvider,
            BrowserControlsSizer browserControlsSizer,
            @Nullable BackPressManager backPressManager,
            @Nullable ObservableSupplier<Boolean> inMotionSupplier,
            ApplicationViewportInsetSupplier appViewportInsetSupplier,
            PageInsightsIntentParams intentParams,
            BooleanSupplier isPageInsightsEnabledSupplier,
            BooleanSupplier isGoogleBottomBarEnabledSupplier,
            PageInsightsCoordinator.ConfigProvider pageInsightsConfigProvider) {
        mContext = context;
        mTabObservable = tabObservable;
        mProfileSupplier = profileSupplier;
        mControlsStateProvider = controlsStateProvider;
        mInMotionSupplier = inMotionSupplier;
        mWillHandleBackPressSupplier = new ObservableSupplierImpl<>(false);
        mSheetContent =
                new PageInsightsSheetContent(
                        mContext,
                        intentParams,
                        layoutView,
                        this::loadUrl,
                        this::handleBackPress,
                        mWillHandleBackPressSupplier,
                        mOnBottomSheetTouchHandler);
        mSheetController = bottomSheetController;
        mOtherBottomSheetController = otherBottomSheetController;
        mExpandedSheetHelper = expandedSheetHelper;
        mHandler = new Handler(Looper.getMainLooper());
        mBrowserControlsSizer = browserControlsSizer;
        mBrowserControlsObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate,
                            boolean isVisibilityForced) {
                        bottomSheetController.setBrowserControlsHiddenRatio(
                                controlsStateProvider.getBrowserControlHiddenRatio());
                        maybeAutoTrigger();
                    }
                };
        controlsStateProvider.addObserver(mBrowserControlsObserver);
        bottomSheetController.addObserver(this);
        if (mInMotionSupplier != null) {
            mInMotionSupplier.addObserver(mInMotionCallback);
        }
        mOtherBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(@SheetState int newState, int reason) {
                        onOtherBottomSheetStateChanged(newState >= SheetState.PEEK);
                    }
                };
        otherBottomSheetController.addObserver(mOtherBottomSheetObserver);
        mIsPageInsightsEnabledSupplier = isPageInsightsEnabledSupplier;
        mPageInsightsConfigProvider = pageInsightsConfigProvider;
        mPageInsightsDataLoader = new PageInsightsDataLoader(profileSupplier);
        mIsAccessibilityEnabled = ChromeAccessibilityUtil.get().isAccessibilityEnabled();
        mSurfaceRendererContextValues =
                PageInsightsActionHandlerImpl.createContextValues(
                        new PageInsightsActionHandlerImpl(
                                tabObservable,
                                shareDelegateSupplier,
                                this::changeToChildPage,
                                PageInsightsMediator::logPageInsightsEvent));
        mCanAutoTrigger =
                ChromeFeatureList.sCctPageInsightsHubPeek.isEnabled()
                        && !intentParams.getShouldDisablePeek()
                        && !isGoogleBottomBarEnabledSupplier.getAsBoolean();
        mAutoTriggerDelayMs =
                intentParams.hasAutoTriggerDelayMs()
                        ? intentParams.getAutoTriggerDelayMs()
                        : ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB_PEEK,
                                PAGE_INSIGHTS_PEEK_DELAY_PARAM,
                                DEFAULT_TRIGGER_DELAY_MS);
        mCanAutoTriggerWhileInMotion =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB_PEEK,
                        PAGE_INSIGHTS_CAN_PEEK_WHILE_IN_MOTION_PARAM,
                        false);
        mCanReturnToPeekAfterExpansion =
                intentParams.hasCanReturnToPeekAfterExpansion()
                        ? intentParams.getCanReturnToPeekAfterExpansion()
                        : ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB_PEEK,
                                PAGE_INSIGHTS_CAN_RETURN_TO_PEEK_AFTER_EXPANSION_PARAM,
                                false);
        onTab(tabObservable.get());
        tabObservable.addObserver(this::onTab);
        mBackPressManager = backPressManager;
        if (BackPressManager.isEnabled()) {
            mBackPressHandler = bottomSheetController.getBottomSheetBackPressHandler();
            if (mBackPressHandler != null
                    && backPressManager != null
                    && !backPressManager.has(BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET)) {
                backPressManager.addHandler(
                        mBackPressHandler, BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET);
            }
        } else {
            mBackPressHandler = null;
        }

        // Native is ready by now. The feature flag can be cached from here.
        mResizeInSync = ChromeFeatureList.sPageInsightsResizeInSync.isEnabled();
        if (mResizeInSync) appViewportInsetSupplier.setBottomSheetInsetSupplier(mSheetInset);
    }

    void initView(View bottomSheetContainer) {
        mSheetContainer = bottomSheetContainer;
        View view = bottomSheetContainer.findViewById(R.id.background);
        mBackgroundDrawable = (GradientDrawable) view.getBackground();
        mMaxCornerRadiusPx =
                bottomSheetContainer
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_corner_radius);
        setCornerRadiusPx(0);

        // Initialize the hidden ratio, otherwise it won't be set until the first offset
        // change event occurs.
        mSheetController.setBrowserControlsHiddenRatio(
                mControlsStateProvider.getBrowserControlHiddenRatio());
    }

    void onOtherBottomSheetStateChanged(boolean opened) {
        if (opened && shouldHideContent()) {
            mSheetController.hideContent(mSheetContent, true);
        }
    }

    private void onTab(@Nullable Tab tab) {
        if (tab != null && !tab.hasObserver(this)) {
            Log.v(TAG, "New tab");
            onNewTabOrPage();
            tab.addObserver(this);
            if (mObservedTab != null && mObservedTab != tab) {
                mObservedTab.removeObserver(this);
            }
            mObservedTab = tab;
        }
    }

    private boolean shouldHideContent() {
        // See if we need to hide the sheet content temporarily while another bottom UI is
        // launched. No need to hide if not in peek/full state or in scrolled-away state,
        // hence not visible.
        return mSheetController.getSheetState() >= SheetState.PEEK && !isInScrolledAwayState();
    }

    private boolean isInScrolledAwayState() {
        return !MathUtils.areFloatsEqual(mControlsStateProvider.getBrowserControlHiddenRatio(), 0f);
    }

    private boolean handleBottomSheetTap() {
        if (mSheetController.getSheetState() == BottomSheetController.SheetState.PEEK) {
            mSheetController.expandSheet();
            return true;
        }
        return false;
    }

    private boolean shouldInterceptBottomSheetTouchEvents() {
        return mSheetController.getSheetState() == BottomSheetController.SheetState.PEEK;
    }

    private boolean handleBackPress() {
        if (mSheetController.getSheetState() != BottomSheetController.SheetState.FULL) {
            return false;
        }

        if (mIsShowingChildView) {
            mSheetContent.showFeedPage();
            mIsShowingChildView = false;
        } else if (!mSheetController.collapseSheet(true)) {
            mSheetController.hideContent(mSheetContent, true, StateChangeReason.BACK_PRESS);
        }
        return true;
    }

    private void cancelAutoTrigger() {
        Log.v(TAG, "Cancelling auto-trigger");
        mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
        mHandler.removeCallbacks(mAutoTriggerTimerRunnable);
    }

    // TabObserver

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        Log.v(TAG, "onPageLoadStarted");
        mHasPageLoadBeenStartedSinceCreation = true;
        onNewTabOrPage();
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(
            Tab tab, NavigationHandle navigationHandle) {
        Log.v(TAG, "onDidFinishNavigationInPrimaryMainFrame");
        mCurrentNavigationHandle = navigationHandle;
    }

    private void onNewTabOrPage() {
        mCurrentNavigationHandle = null;
        mCurrentMetadata = null;
        mCurrentConfig = null;
        cancelAutoTrigger();
        if (mPageInsightsDataLoader != null) {
            mPageInsightsDataLoader.cancelCallback();
        }
        if (mSheetContent == mSheetController.getCurrentSheetContent()) {
            mSheetController.hideContent(mSheetContent, true);
        }
        if (mCanAutoTrigger) {
            delayStartAutoTrigger(mAutoTriggerDelayMs);
        }
    }

    private void delayStartAutoTrigger(long delayMs) {
        Log.v(TAG, "Scheduling auto-trigger");
        mAutoTriggerStage = AutoTriggerStage.AWAITING_TIMER;
        if (delayMs > 0) {
            mHandler.postDelayed(mAutoTriggerTimerRunnable, delayMs);
        } else {
            mAutoTriggerTimerRunnable.run();
        }
    }

    @VisibleForTesting
    void onAutoTriggerTimerFinished() {
        Log.v(TAG, "Auto-trigger timer finished");
        if (mAutoTriggerStage == AutoTriggerStage.AWAITING_TIMER) {
            maybeFetchDataForAutoTrigger();
        }
    }

    private void maybeFetchDataForAutoTrigger() {
        Tab tab = mTabObservable.get();
        if (tab == null) {
            Log.e(TAG, "Cancelling auto-trigger because Tab is unexpectedly null.");
            mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
            return;
        }
        // mCurrentNavigationHandle may still be null by this point, probably because
        // onDidFinishNavigationInPrimaryMainFrame was called before PageInsightsMediator
        // was created. See b/325597773. Auto-triggering without the handle just causes
        // internal code to provide the most conservative PageInsightsConfig options -
        // triggering with these options is better than not triggering at all.
        PageInsightsConfig config =
                mPageInsightsConfigProvider.get(
                        new PageInsightsConfigRequest(
                                mCurrentNavigationHandle,
                                getLastCommittedNavigationEntry(tab),
                                mHasPageLoadBeenStartedSinceCreation));
        if (!shouldFetchDataForAutoTrigger(config)) {
            mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
            return;
        }

        mAutoTriggerStage = AutoTriggerStage.FETCHING_DATA;
        Log.v(TAG, "Fetching data for auto-trigger");
        mPageInsightsDataLoader.loadInsightsData(
                tab.getUrl(),
                /* isUserInitiated= */ false,
                config,
                metadata -> {
                    if (mAutoTriggerStage != AutoTriggerStage.FETCHING_DATA) {
                        // Don't proceed if something has changed since we started fetching data.
                        return;
                    }
                    if (metadata.getAutoPeekConditions().getConfidence() <= MINIMUM_CONFIDENCE) {
                        mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
                        Log.v(TAG, "Cancelling auto-trigger as confidence too low");
                        return;
                    }
                    mCurrentMetadata = metadata;
                    mCurrentConfig = config;
                    prepareForAutoTrigger();
                });
    }

    private void prepareForAutoTrigger() {
        Log.v(TAG, "Preparing for auto-trigger.");
        mAutoTriggerStage = AutoTriggerStage.PREPARING;
        PostTask.postTask(
                // Get surface renderer on background thread as it can be expensive
                TaskTraits.USER_VISIBLE_MAY_BLOCK,
                () -> {
                    if (mAutoTriggerStage != AutoTriggerStage.PREPARING) return;
                    getSurfaceRenderer();
                    PostTask.postTask(
                            TaskTraits.UI_USER_VISIBLE,
                            () -> {
                                if (mAutoTriggerStage != AutoTriggerStage.PREPARING) return;
                                Log.v(TAG, "Ready for auto-trigger.");
                                mAutoTriggerStage = AutoTriggerStage.READY_FOR_AUTO_TRIGGER;
                                maybeAutoTrigger();
                            });
                });
    }

    private void maybeAutoTrigger() {
        if (mAutoTriggerStage != AutoTriggerStage.READY_FOR_AUTO_TRIGGER) return;

        if (!BrowserControlsUtils.areBrowserControlsOffScreen(mControlsStateProvider)
                && !mIsAccessibilityEnabled) {
            Log.v(
                    TAG,
                    "Not auto-triggering because browser controls are not off screen and a11y is"
                            + " not enabled.");
            return;
        }
        if (!mCanAutoTriggerWhileInMotion
                && mInMotionSupplier != null
                && mInMotionSupplier.get() != null
                && mInMotionSupplier.get()) {
            Log.v(TAG, "Not auto-triggering because compositor is in motion.");
            return;
        }

        if (mSheetContent == mSheetController.getCurrentSheetContent()) {
            Log.v(
                    TAG,
                    "Cancelling auto-trigger because page insights sheet content already being"
                            + " shown.");
            mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
            return;
        }
        if (mCurrentMetadata == null || mCurrentConfig == null) {
            Log.e(TAG, "Cancelling  auto-trigger because metadata or config unexpectedly null.");
            mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
            return;
        }

        Log.v(TAG, "Auto-triggering.");
        if (mCurrentConfig.getShouldXsurfaceLog()) {
            getSurfaceRenderer()
                    .onSurfaceCreated(
                            PageInsightsLoggingParametersImpl.create(
                                    mProfileSupplier.get(), mCurrentMetadata));
        }
        initSheetContent(
                mCurrentMetadata,
                /* isPrivacyNoticeRequired= */ mCurrentConfig.getShouldXsurfaceLog(),
                /* shouldHavePeekState= */ true);
        logPageInsightsEvent(PageInsightsEvent.AUTO_PEEK_TRIGGERED);
        getSurfaceRenderer().onEvent(BOTTOM_SHEET_PEEKING);
        mSheetController.requestShowContent(mSheetContent, true);
        mAutoTriggerStage = AutoTriggerStage.AUTO_TRIGGERED;
    }

    private boolean shouldFetchDataForAutoTrigger(PageInsightsConfig config) {
        if (mSheetContent == mSheetController.getCurrentSheetContent()) {
            Log.v(
                    TAG,
                    "Not fetching data for auto-trigger because page insights sheet content"
                            + " already being shown.");
            return false;
        }
        if (!mIsPageInsightsEnabledSupplier.getAsBoolean()) {
            Log.v(TAG, "Not fetching data for auto-trigger because feature is disabled.");
            return false;
        }
        if (!config.getShouldAutoTrigger()) {
            Log.v(TAG, "Not fetching data for auto-trigger because auto-triggering is disabled.");
            return false;
        }
        return true;
    }

    void launch() {
        Tab tab = mTabObservable.get();
        if (tab == null) {
            Log.e(TAG, "Can't launch Page Insights because Tab is null.");
            return;
        }
        cancelAutoTrigger();
        mSheetContent.showLoadingIndicator();
        mSheetController.requestShowContent(mSheetContent, true);
        PageInsightsConfig config =
                mPageInsightsConfigProvider.get(
                        new PageInsightsConfigRequest(
                                mCurrentNavigationHandle,
                                getLastCommittedNavigationEntry(tab),
                                mHasPageLoadBeenStartedSinceCreation));
        mPageInsightsDataLoader.loadInsightsData(
                tab.getUrl(),
                /* isUserInitiated= */ true,
                config,
                metadata -> {
                    mCurrentMetadata = metadata;
                    mCurrentConfig = config;
                    if (config.getShouldXsurfaceLog()) {
                        getSurfaceRenderer()
                                .onSurfaceCreated(
                                        PageInsightsLoggingParametersImpl.create(
                                                mProfileSupplier.get(), metadata));
                    }
                    initSheetContent(
                            metadata,
                            /* isPrivacyNoticeRequired= */ config.getShouldXsurfaceLog(),
                            /* shouldHavePeekState= */ false);
                    setBackgroundColors(/* ratioOfCompletionFromPeekToExpanded= */ 1.0f);
                    setCornerRadiusPx(mMaxCornerRadiusPx);
                    logPageInsightsEvent(PageInsightsEvent.USER_INVOKES_PIH);
                    // We need to perform this logging here, even though we also do it when the
                    // sheet reaches expanded state, as XSurface logging is not initialised until
                    // now.
                    getSurfaceRenderer().onEvent(BOTTOM_SHEET_EXPANDED);
                    mSheetController.expandSheet();
                });
    }

    private void initSheetContent(
            PageInsightsMetadata metadata,
            boolean isPrivacyNoticeRequired,
            boolean shouldHavePeekState) {
        mCurrentFeedView = getXSurfaceView(metadata.getFeedPage().getElementsOutput());
        mSheetContent.initContent(mCurrentFeedView, isPrivacyNoticeRequired, shouldHavePeekState);
        mSheetContent.showFeedPage();
    }

    private View getXSurfaceView(ByteString elementsOutput) {
        return getSurfaceRenderer()
                .render(elementsOutput.toByteArray(), mSurfaceRendererContextValues);
    }

    private void changeToChildPage(int id) {
        if (mCurrentMetadata == null) {
            return;
        }

        for (int i = 0; i < mCurrentMetadata.getPagesCount(); i++) {
            Page currPage = mCurrentMetadata.getPages(i);
            if (id == currPage.getId().getNumber()) {
                if (mCurrentChildView != null) {
                    getSurfaceRenderer().unbindView(mCurrentChildView);
                }
                mCurrentChildView = getXSurfaceView(currPage.getElementsOutput());
                mSheetContent.showChildPage(mCurrentChildView, currPage.getTitle());
                mIsShowingChildView = true;
            }
        }
    }

    private void loadUrl(String url) {
        Tab tab = mTabObservable.get();
        if (tab != null) {
            tab.loadUrl(new LoadUrlParams(url));
        }
    }

    PageInsightsSheetContent getSheetContent() {
        return mSheetContent;
    }

    // BottomSheetObserver

    @Override
    public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        if (newState == SheetState.HIDDEN) {
            mWillHandleBackPressSupplier.set(false);
            if (mResizeInSync) mSheetInset.set(0);
            setBottomControlsHeight(mSheetController.getCurrentOffset());
            handleDismissal(mOldState, reason);
        } else if (newState == SheetState.PEEK) {
            mWillHandleBackPressSupplier.set(false);
            if (mResizeInSync) mSheetInset.set(0);
            setBottomControlsHeight(mSheetController.getCurrentOffset());
            setBackgroundColors(/* ratioOfCompletionFromPeekToExpanded= */ .0f);
            // The user should always be able to swipe to dismiss from peek state.
            mSheetContent.setSwipeToDismissEnabled(true);
            logPageInsightsEvent(PageInsightsEvent.STATE_PEEK);
            // We don't log peek state to XSurface here, as its BOTTOM_SHEET_PEEKING event is only
            // intended for when the feature initially auto-peeks.
        } else if (newState == SheetState.FULL) {
            mWillHandleBackPressSupplier.set(true);
            setBackgroundColors(/* ratioOfCompletionFromPeekToExpanded= */ 1.0f);
            if (mOldState == SheetState.PEEK && mCanReturnToPeekAfterExpansion) {
                // Disable swiping to dismiss, so that swiping/scrim-tapping returns to peek state
                // instead.
                mSheetContent.setSwipeToDismissEnabled(false);
            } else if (mOldState != SheetState.FULL) {
                // Enable swiping to dismiss, and also explicitly disable peek state. If peek state
                // remains enabled then some lighter swipes can return to it, even with
                // swipeToDismissEnabled true.
                mSheetContent.setSwipeToDismissEnabled(true);
                mSheetContent.setShouldHavePeekState(false);
            }
            logPageInsightsEvent(PageInsightsEvent.STATE_EXPANDED);
            getSurfaceRenderer().onEvent(BOTTOM_SHEET_EXPANDED);
        } else {
            mWillHandleBackPressSupplier.set(false);
        }

        if (newState != SheetState.NONE && newState != SheetState.SCROLLING) {
            mOldState = newState;
        }
    }

    private void handleDismissal(@SheetState int oldState, @StateChangeReason int reason) {
        mIsShowingChildView = false;

        if (mCurrentFeedView != null) {
            getSurfaceRenderer().unbindView(mCurrentFeedView);
            mCurrentFeedView = null;
        }
        if (mCurrentChildView != null) {
            getSurfaceRenderer().unbindView(mCurrentChildView);
            mCurrentChildView = null;
        }

        if (USER_DISMISSAL_REASONS.contains(reason)) {
            if (mOldState == SheetState.PEEK) {
                logPageInsightsEvent(PageInsightsEvent.DISMISS_PEEK);
                getSurfaceRenderer().onEvent(DISMISSED_FROM_PEEKING_STATE);
            } else if (mOldState >= SheetState.HALF) {
                logPageInsightsEvent(PageInsightsEvent.DISMISS_EXPANDED);
            }
        }
        getSurfaceRenderer().onSurfaceClosed();
    }

    private void setBottomControlsHeight(int height) {
        if (height == mCachedSheetHeight) return;
        mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(true);
        mBrowserControlsSizer.setBottomControlsHeight(height, 0);
        mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(false);
        mCachedSheetHeight = height;
    }

    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mExpandedSheetHelper.onSheetExpanded();
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        mExpandedSheetHelper.onSheetCollapsed();
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
        float peekHeightRatio = getPeekHeightRatio();
        if (mSheetController.getSheetState() == SheetState.SCROLLING) {
            if (mResizeInSync) {
                // Calling |setBottomControlsHeight| to resize WebContents per each offset change
                // is janky. While the sheet is being dragged, let the app-wide inset supplier
                // handle the resizing so the sheet and the contents move in sync smoothly.
                if (BrowserControlsUtils.areBrowserControlsFullyVisible(mControlsStateProvider)
                        && heightFraction < peekHeightRatio) {
                    setBottomControlsHeight(0);
                    mSheetInset.set((int) offsetPx);
                }
            } else if (heightFraction < peekHeightRatio) {
                // Set the content height to zero in advance when user drags/scrolls the sheet down
                // below the peeking state. This helps hide the white patch (blank bottom controls).
                // The actual value to be set is not 0 but 1, due to a limitation in browser
                // controls animation. See crbug.com/40941684
                setBottomControlsHeight(1);
            }
        }

        float ratioOfCompletionFromPeekToExpanded =
                (heightFraction - peekHeightRatio) / (1.f - peekHeightRatio);
        setBackgroundColors(ratioOfCompletionFromPeekToExpanded);
        if (0 <= ratioOfCompletionFromPeekToExpanded
                && ratioOfCompletionFromPeekToExpanded <= 1.f) {
            setCornerRadiusPx((int) (ratioOfCompletionFromPeekToExpanded * mMaxCornerRadiusPx));
        }
    }

    private float getPeekHeightRatio() {
        float fullHeight = mSheetContent.getContentView().getMeasuredHeight();
        return mSheetContent.getPeekHeight() / fullHeight;
    }

    void setCornerRadiusPx(int radius) {
        mBackgroundDrawable.mutate();
        mBackgroundDrawable.setCornerRadii(
                new float[] {radius, radius, radius, radius, 0, 0, 0, 0});
    }

    void setBackgroundColors(float ratioOfCompletionFromPeekToExpanded) {
        float colorRatio = 1.0f;
        if (0 <= ratioOfCompletionFromPeekToExpanded
                && ratioOfCompletionFromPeekToExpanded <= 0.5f) {
            colorRatio = 2 * ratioOfCompletionFromPeekToExpanded;
        } else if (ratioOfCompletionFromPeekToExpanded <= 0) {
            colorRatio = 0;
        }
        @ColorInt int surfaceColor = mContext.getColor(R.color.gm3_baseline_surface);
        @ColorInt
        int surfaceContainerColor = mContext.getColor(R.color.gm3_baseline_surface_container);
        mBackgroundDrawable.setColor(
                ColorUtils.getColorWithOverlay(surfaceContainerColor, surfaceColor, colorRatio));
        mSheetContent.setPrivacyCardColor(
                ColorUtils.getColorWithOverlay(surfaceColor, surfaceContainerColor, colorRatio));
    }

    @Override
    public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {}

    void destroy() {
        cancelAutoTrigger();
        mOtherBottomSheetController.removeObserver(mOtherBottomSheetObserver);
        mControlsStateProvider.removeObserver(mBrowserControlsObserver);
        mSheetController.removeObserver(this);
        if (mObservedTab != null) {
            mObservedTab.removeObserver(this);
            mObservedTab = null;
        }
        if (mInMotionSupplier != null) {
            mInMotionSupplier.removeObserver(mInMotionCallback);
        }
        if (mBackPressManager != null && mBackPressHandler != null) {
            mBackPressManager.removeHandler(mBackPressHandler);
        }
        if (mPageInsightsDataLoader != null) {
            mPageInsightsDataLoader.cancelCallback();
        }
    }

    float getCornerRadiusForTesting() {
        float[] radii = mBackgroundDrawable.getCornerRadii();
        assert radii[0] == radii[1] && radii[1] == radii[2] && radii[2] == radii[3];
        return radii[0];
    }

    void setPageInsightsDataLoaderForTesting(PageInsightsDataLoader pageInsightsDataLoader) {
        mPageInsightsDataLoader = pageInsightsDataLoader;
    }

    View getContainerForTesting() {
        return mSheetContainer;
    }

    ObservableSupplierImpl<Integer> getSheetInsetForTesting() {
        return mSheetInset;
    }

    private PageInsightsSurfaceRenderer getSurfaceRenderer() {
        if (mSurfaceRenderer != null) {
            return mSurfaceRenderer;
        }
        PageInsightsSurfaceScope surfaceScope =
                XSurfaceProcessScopeProvider.getProcessScope()
                        .obtainPageInsightsSurfaceScope(
                                new PageInsightsSurfaceScopeDependencyProviderImpl(mContext));
        mSurfaceRenderer = surfaceScope.provideSurfaceRenderer();
        return mSurfaceRenderer;
    }

    @Nullable
    private static NavigationEntry getLastCommittedNavigationEntry(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents == null) {
            return null;
        }
        NavigationController navigationController = webContents.getNavigationController();
        if (navigationController == null) {
            return null;
        }
        return navigationController.getEntryAtIndex(
                navigationController.getLastCommittedEntryIndex());
    }
}
