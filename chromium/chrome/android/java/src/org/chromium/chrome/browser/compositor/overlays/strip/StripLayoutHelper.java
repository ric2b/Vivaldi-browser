// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.FloatProperty;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ListPopupWindow;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.CompositorOnClickHandler;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackScroller;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle.StripLayoutGroupTitleDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab.StripLayoutTabDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncIphController;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupTitleEditor;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.MotionEventUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

// Vivaldi
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.vivaldi.browser.common.VivaldiUtils;
import org.vivaldi.browser.compositor.overlay.strip.VivaldiTabMenu;
import org.vivaldi.browser.preferences.TabWidthPreference;
import org.vivaldi.browser.preferences.VivaldiPreferences;
import org.vivaldi.browser.toolbar.VivaldiTopToolbarCoordinator;
import org.chromium.ui.base.DeviceFormFactor;
import android.app.Activity;
import org.chromium.ui.KeyboardVisibilityDelegate;
import android.view.WindowManager;
import org.chromium.build.BuildConfig;

/**
 * This class handles managing the positions and behavior of all tabs in a tab strip. It is
 * responsible for both responding to UI input events and model change notifications, adjusting and
 * animating the tab strip as required.
 *
 * <p>The stacking and visual behavior is driven by setting a {@link StripStacker}.
 */
public class StripLayoutHelper implements StripLayoutTabDelegate, StripLayoutGroupTitleDelegate {
    /** A property for animations to use for changing the X offset of the tab. */
    public static final FloatProperty<StripLayoutHelper> SCROLL_OFFSET =
            new FloatProperty<StripLayoutHelper>("scrollOffset") {
                @Override
                public void setValue(StripLayoutHelper object, float value) {
                    object.setScrollOffset(value);
                }

                @Override
                public Float get(StripLayoutHelper object) {
                    return object.getScrollOffset();
                }
            };

    // Drag Constants
    private static final int REORDER_SCROLL_NONE = 0;
    private static final int REORDER_SCROLL_LEFT = 1;
    private static final int REORDER_SCROLL_RIGHT = 2;

    // Behavior Constants
    private static final float EPSILON = 0.001f;
    private static final float REORDER_OVERLAP_SWITCH_PERCENTAGE = 0.53f;
    private static final float DROP_INTO_GROUP_MAX_OFFSET = 36.f;

    // Animation/Timer Constants
    private static final int RESIZE_DELAY_MS = 1500;
    private static final int SPINNER_UPDATE_DELAY_MS = 66;
    // Degrees per millisecond.
    private static final float SPINNER_DPMS = 0.33f;
    private static final int SCROLL_DURATION_MS = 250;
    private static final int SCROLL_DURATION_MS_MEDIUM = 350;
    private static final int SCROLL_DURATION_MS_LONG = 450;
    private static final int ANIM_FOLIO_DETACH_MS = 75;
    private static final int ANIM_TAB_CREATED_MS = 150;
    private static final int ANIM_TAB_CLOSED_MS = 150;
    private static final int ANIM_TAB_RESIZE_MS = 250;
    private static final int ANIM_TAB_DRAW_X_MS = 250;
    private static final int ANIM_TAB_MOVE_MS = 125;
    private static final int ANIM_TAB_SLIDE_OUT_MS = 250;
    private static final int ANIM_BUTTONS_FADE_MS = 150;
    private static final int SCROLL_DISTANCE_SHORT = 960;
    private static final int SCROLL_DISTANCE_MEDIUM = 1920;
    private static final long INVALID_TIME = 0L;
    private static final int ANIM_HOVERED_TAB_CONTAINER_FADE_MS = 200;
    static final long DROP_INTO_GROUP_MS = 300L;

    // Visibility Constants
    private static final float TAB_OVERLAP_WIDTH_LARGE_DP = 28.f;
    private static final float TAB_WIDTH_MEDIUM = 156.f;
    private static final float REORDER_EDGE_SCROLL_MAX_SPEED_DP = 1000.f;
    private static final float REORDER_EDGE_SCROLL_START_MIN_DP = 87.4f;
    private static final float REORDER_EDGE_SCROLL_START_MAX_DP = 18.4f;
    private static final float NEW_TAB_BUTTON_BACKGROUND_Y_OFFSET_DP = 3.f;
    private static final float NEW_TAB_BUTTON_CLICK_SLOP_DP = 8.f;
    private static final float NEW_TAB_BUTTON_BACKGROUND_WIDTH_DP = 32.f;
    private static final float NEW_TAB_BUTTON_BACKGROUND_HEIGHT_DP = 32.f;
    @VisibleForTesting static final float FOLIO_ATTACHED_BOTTOM_MARGIN_DP = 0.f;
    private static final float FOLIO_ANIM_INTERMEDIATE_MARGIN_DP = -12.f;
    @VisibleForTesting static final float FOLIO_DETACHED_BOTTOM_MARGIN_DP = 4.f;
    private static final float BUTTON_DESIRED_TOUCH_TARGET_SIZE = 48.f;

    // Desired spacing between new tab button and tabs when tab strip is not full.
    private static final float NEW_TAB_BUTTON_X_OFFSET_TOWARDS_TABS = 4.f;
    private static final float DESIRED_PADDING_BETWEEN_NEW_TAB_BUTTON_AND_TABS = 2.f;
    private static final float NEW_TAB_BUTTON_DEFAULT_PRESSED_OPACITY = 0.2f;
    private static final float NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.12f;
    private static final float NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.08f;
    static final float TAB_OPACITY_HIDDEN = 0.f;
    static final float TAB_OPACITY_VISIBLE_BACKGROUND = 0.55f;
    static final float TAB_OPACITY_VISIBLE_FOREGROUND = 1.f;
    static final float FADE_FULL_OPACITY_THRESHOLD_DP = 24.f;
    private static final float TAB_STRIP_TAB_WIDTH = 108.f;
    private static final float NEW_TAB_BUTTON_WITH_MODEL_SELECTOR_BUTTON_PADDING = 8.f;

    // The bottom indicator should align with the contents of the last tab in group. This value is
    // calculated as:
    // closeButtonEndPadding(10) + tabContainerEndPadding(16) + groupTitleStartMargin(13)
    //         - overlap(28-16) =
    @VisibleForTesting static final float TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET = 27.f;

    private static final int MESSAGE_RESIZE = 1;
    private static final int MESSAGE_UPDATE_SPINNER = 2;
    private static final float CLOSE_BTN_VISIBILITY_THRESHOLD_START = 96.f;
    private static final long TAB_SWITCH_METRICS_MAX_ALLOWED_SCROLL_INTERVAL =
            DateUtils.MINUTE_IN_MILLIS;

    // Histogram Constants
    private static final String PLACEHOLDER_LEFTOVER_TABS_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripLeftoverTabsCount";
    private static final String PLACEHOLDER_TABS_CREATED_DURING_RESTORE_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripTabsCreatedDuringRestoreCount";
    private static final String PLACEHOLDER_TABS_NEEDED_DURING_RESTORE_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripTabsNeededDuringRestoreCount";
    private static final String PLACEHOLDER_VISIBLE_DURATION_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripVisibleDuration";

    // An observer that is notified of changes to a {@link TabGroupModelFilter} object.
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver =
            new TabGroupModelFilterObserver() {
                int mSourceRootId = Tab.INVALID_TAB_ID;

                @Override
                public void willMoveTabGroup(int tabModelOldIndex, int tabModelNewIndex) {
                    mMovingGroup = true;
                }

                @Override
                public void didMoveTabGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    mMovingGroup = false;
                }

                @Override
                public void didMergeTabToGroup(Tab movedTab, int selectedTabIdInGroup) {
                    int rootId = movedTab.getRootId();
                    updateGroupTitleText(rootId);
                    // Removing the tab at the end of a group through the GTS will result in the
                    // width of a group changing without a tab moving. This means a rebuild won't
                    // occur, and we'll need to manually update the bottom indicator here.
                    if (!mInReorderMode) {
                        finishAnimations();
                        computeAndUpdateTabOrders(false, false);
                        mRenderHost.requestRender();
                    }

                    // Tab merging should not automatically expand a collapsed tab group. If the
                    // target group is collapsed, the tab being merged should also be collapsed.
                    StripLayoutGroupTitle groupTitle = findGroupTitle(rootId);
                    if (groupTitle != null) {
                        updateTabCollapsed(
                                findTabById(movedTab.getId()), groupTitle.isCollapsed(), false);
                    }
                }

                @Override
                public void willMoveTabOutOfGroup(Tab movedTab, int newRootId) {
                    // TODO(crbug.com/326494015): Refactor #didMoveTabOutOfGroup to pass in previous
                    //  root ID.
                    mSourceRootId = movedTab.getRootId();
                }

                @Override
                public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                    updateGroupTitleText(mSourceRootId);
                    // Removing the tab at the end of a group through the GTS will result in the
                    // width of a group changing without a tab moving. This means a rebuild won't
                    // occur, and we'll need to manually update the bottom indicator here.
                    if (!mInReorderMode
                            || mTabGroupModelFilter.getTabGroupCount()
                                    != mStripGroupTitles.length) {
                        finishAnimations();
                        computeAndUpdateTabOrders(false, false);
                        mRenderHost.requestRender();
                    }

                    // Expand the tab if necessary.
                    StripLayoutTab tab = findTabById(movedTab.getId());
                    if (tab != null && tab.isCollapsed()) {
                        updateTabCollapsed(tab, false, false);
                        resizeTabStrip(true, false, false);
                    }
                }

                @Override
                public void didMoveWithinGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    updateGroupAccessibilityDescription(findGroupTitle(movedTab.getRootId()));
                }

                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                    rebuildStripViews();
                }

                @Override
                public void didChangeTabGroupTitle(int rootId, String newTitle) {
                    final StripLayoutGroupTitle groupTitle = findGroupTitle(rootId);
                    if (groupTitle == null) return;

                    updateGroupTitleText(groupTitle, newTitle);
                    mRenderHost.requestRender();
                }

                @Override
                public void didChangeTabGroupColor(int rootId, @TabGroupColorId int newColor) {
                    updateGroupTitleTint(findGroupTitle(rootId), newColor);
                }

                @Override
                public void didChangeTabGroupCollapsed(int rootId, boolean isCollapsed) {
                    final StripLayoutGroupTitle groupTitle = findGroupTitle(rootId);
                    if (groupTitle == null) return;

                    updateTabGroupCollapsed(groupTitle, isCollapsed, true);
                }

                @Override
                public void didChangeGroupRootId(int oldRootId, int newRootId) {
                    releaseResourcesForGroupTitle(oldRootId);

                    StripLayoutGroupTitle groupTitle = findGroupTitle(oldRootId);
                    if (groupTitle != null) {
                        groupTitle.updateRootId(newRootId);
                        // Refresh properties since removing the root tab may have cleared the ones
                        // associated with the oldRootId before updating to the newRootId here.
                        updateGroupTitleText(groupTitle);
                        updateGroupTitleTint(groupTitle);
                    }

                    // Update LastSyncedGroupId to prevent the IPH from being dismissed when the
                    // synced rootId changes.
                    if (oldRootId == mLastSyncedGroupId) {
                        mLastSyncedGroupId = newRootId;
                    }
                }

                @Override
                public void didRemoveTabGroup(
                        int oldRootId,
                        @Nullable Token oldTabGroupId,
                        @DidRemoveTabGroupReason int removalReason) {
                    releaseResourcesForGroupTitle(oldRootId);

                    // dismiss the iph text bubble when the synced tab group is unsynced.
                    if (oldRootId == mLastSyncedGroupId) {
                        dismissTabGroupSyncIph();
                    }
                }
            };

    // External influences
    private final LayoutUpdateHost mUpdateHost;
    private final LayoutRenderHost mRenderHost;
    private final LayoutManagerHost mManagerHost;
    private final WindowAndroid mWindowAndroid;
    private TabModel mModel;
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabCreator mTabCreator;
    private LayerTitleCache mLayerTitleCache;
    private ActionConfirmationManager mActionConfirmationManager;
    private StripStacker mStripStacker = new ScrollingStripStacker();

    // Internal State
    private StripLayoutView[] mStripViews = new StripLayoutView[0];
    private StripLayoutTab[] mStripTabs = new StripLayoutTab[0];
    private StripLayoutTab[] mStripTabsVisuallyOrdered = new StripLayoutTab[0];
    private StripLayoutTab[] mStripTabsToRender = new StripLayoutTab[0];
    private StripLayoutGroupTitle[] mStripGroupTitles = new StripLayoutGroupTitle[0];
    private StripLayoutGroupTitle[] mStripGroupTitlesToRender = new StripLayoutGroupTitle[0];
    private StripLayoutTab mTabAtPositionForTesting;
    private final StripTabEventHandler mStripTabEventHandler = new StripTabEventHandler();
    private final TabLoadTrackerCallback mTabLoadTrackerHost = new TabLoadTrackerCallbackImpl();
    private final RectF mTouchableRect = new RectF();

    // Common state used for animations on the strip triggered by independent actions including and
    // not limited to tab closure, tab creation/selection, and tab reordering. Not intended to be
    // used for hover actions. Consider using setAndStartRunningAnimator() to set and start this
    // animator.
    private Animator mRunningAnimator;

    private final TintedCompositorButton mNewTabButton;
    private final CompositorButton mModelSelectorButton;

    // Layout Constants
    private final float mTabOverlapWidth;
    private final float mNewTabButtonWidth;
    private final float mMinTabWidth;
    private final float mMaxTabWidth;
    private final ListPopupWindow mTabMenu;

    // All views are overlapped by mTabOverlapWidth. Group titles do not need to be overlapped by
    // this much, so we offset the drawX.
    private final float mGroupTitleDrawXOffset;
    // The effective overlap width for group titles. This is the "true" overlap width, but adjusted
    // to account for the start offset above.
    private final float mGroupTitleOverlapWidth;

    // Strip State
    private StackScroller mScroller;
    private float mScrollOffset;
    private float mMinScrollOffset;
    private float mCachedTabWidth;

    // Reorder State
    private int mReorderState = REORDER_SCROLL_NONE;
    private boolean mInReorderMode;
    private float mLastReorderX;
    private float mTabMarginWidth;
    private float mHalfTabWidth;
    private float mStripStartMarginForReorder;
    private float mReorderExtraMinScrollOffset;
    private long mLastReorderScrollTime;
    private long mLastUpdateTime;
    private long mHoverStartTime;
    private float mHoverStartOffset;
    private boolean mHoveringOverGroup;
    private boolean mMovingGroup;

    // Tab switch efficiency
    private Long mTabScrollStartTime;
    private Long mMostRecentTabScroll;

    // UI State
    private StripLayoutTab mInteractingTab;
    private float mWidth;
    private float mHeight;
    private long mLastSpinnerUpdate;
    // The margins on the tab strip used when positioning tabs. Tabs within these margins are not
    // touchable, but other strip widgets (e.g new tab button) could be.
    private float mLeftMargin;
    private float mRightMargin;
    private float mLeftFadeWidth;
    private float mRightFadeWidth;
    // Padding regions on both ends of the strip where strip touch events are blocked. Different
    // than margins, no strip widgets should be drawn within the padding region.
    private float mLeftPadding;
    private float mRightPadding;
    // Set during onDown called via BUTTON_PRIMARY. Cleared in onUpOrCancel.
    private boolean mOnDownWithButtonPrimary;

    // New tab button with tab strip end padding
    private final float mFixedEndPadding;
    private float mReservedEndMargin;

    // 3-dots menu button with tab strip end padding
    private final boolean mIncognito;
    private boolean mIsFirstLayoutPass;
    private boolean mAnimationsDisabledForTesting;
    // Whether tab strip scrolling is in progress
    private boolean mIsStripScrollInProgress;

    // Tab menu item IDs
    public static final int ID_CLOSE_ALL_TABS = 0;

    private Context mContext;

    // Animation states. True while the relevant animations are running, and false otherwise.
    private boolean mMultiStepTabCloseAnimRunning;
    private boolean mNewTabButtonAnimRunning;
    private boolean mTabGroupMarginAnimRunning;
    private boolean mTabResizeAnimRunning;
    private boolean mGroupTitleSliding;

    // TabModel info available before the tab state is actually initialized. Determined from frozen
    // tab metadata.
    private boolean mTabStateInitialized;
    private boolean mPlaceholderStripReady;
    private boolean mSelectedOnStartup;
    private boolean mCreatedTabOnStartup;
    private boolean mActiveTabReplaced;
    private int mTabCountOnStartup;
    private int mActiveTabIndexOnStartup;
    private int mCurrentPlaceholderIndex;

    private long mPlaceholderCreationTime;
    private int mTabsCreatedDuringRestore;
    private int mPlaceholdersNeededDuringRestore;

    // Tab Drag and Drop state to hold clicked tab being dragged.
    private View mToolbarContainerView;
    @Nullable private final TabDragSource mTabDragSource;
    private StripLayoutTab mActiveClickedTab;

    // Tab Drag and Drop state to set correct reorder state when dragging on/off tab strip.
    private boolean mReorderingForTabDrop;
    private float mLastOffsetX;
    private float mLastTrailingMargin;

    // Tab hover state.
    private StripLayoutTab mLastHoveredTab;
    private StripTabHoverCardView mTabHoverCardView;

    // Tab Group Sync.
    private float mTabStripHeight;
    private TabGroupSyncIphController mTabGroupSyncIphController;
    private int mLastSyncedGroupId = Tab.INVALID_TAB_ID;
    private final Supplier<Boolean> mTabStripVisibleSupplier;

    // Tab group delete dialog.
    private int mTabGroupIdToHide = Tab.INVALID_TAB_ID;
    private PrefService mPrefService;

    /** Vivaldi **/
    private TabModelObserver mModelObserver;
    private SharedPreferences.OnSharedPreferenceChangeListener mPrefsListener =
            (sharedPrefs, key) -> {
        if (VivaldiPreferences.SHOW_TAB_AS_FAVICON.equals(key)
                || VivaldiPreferences.ENABLE_TAB_STACK.equals(key)
                || VivaldiPreferences.MIN_TAB_WIDTH.equals(key)) {
            computeAndUpdateTabOrders();
            updateScrollOffsetLimits();
            scrollToSelectedTab(false);
        }
    };
    private int mOrientation;
    private boolean mIsStackStrip;
    private TabModelSelector mTabModelSelector;
    private boolean mShowTabsAsFavIcon;
    private Boolean mIsRestoreInProgress;

    private static float NO_X_BUTTON_OFFSET_DP = 20;
    // End Vivaldi

    /**
     * Creates an instance of the {@link StripLayoutHelper}.
     *
     * @param context The current Android {@link Context}.
     * @param managerHost The parent {@link LayoutManagerHost}.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The {@link LayoutRenderHost}.
     * @param incognito Whether or not this tab strip is incognito.
     * @param modelSelectorButton The {@link CompositorButton} used to toggle between regular and
     *     incognito models.
     * @param tabDragSource The @{@link TabDragSource} instance to initiate drag and drop.
     * @param toolbarContainerView The @{link View} passed to @{link TabDragSource} for drag and
     *     drop.
     * @param windowAndroid The @{@link WindowAndroid} instance to access Activity.
     * @param tabStripHeight The height of current tab strip.
     * @param tabStripVisibleSupplier Supplier of the boolean indicating whether the tab strip is
     *     visible. The tab strip can be hidden due to the tab switcher being displayed or the
     *     window width is less than 600dp.
     */
    public StripLayoutHelper(
            Context context,
            LayoutManagerHost managerHost,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            boolean incognito,
            CompositorButton modelSelectorButton,
            @Nullable TabDragSource tabDragSource,
            @NonNull View toolbarContainerView,
            @NonNull WindowAndroid windowAndroid,
            ActionConfirmationManager actionConfirmationManager,
            int tabStripHeight,
            Supplier<Boolean> tabStripVisibleSupplier) {
        // Note (david@vivaldi.com): In Vivaldi there is 1dp gap between the tabs.
        if (ChromeApplicationImpl.isVivaldi())
            mTabOverlapWidth = 20.f;
        else
        mTabOverlapWidth = TAB_OVERLAP_WIDTH_LARGE_DP;
        mGroupTitleDrawXOffset = mTabOverlapWidth - StripLayoutTab.FOLIO_FOOT_LENGTH_DP;
        mGroupTitleOverlapWidth = StripLayoutTab.FOLIO_FOOT_LENGTH_DP - mGroupTitleDrawXOffset;
        mNewTabButtonWidth = NEW_TAB_BUTTON_BACKGROUND_WIDTH_DP;
        mModelSelectorButton = modelSelectorButton;
        mToolbarContainerView = toolbarContainerView;
        mTabDragSource = tabDragSource;
        mWindowAndroid = windowAndroid;
        mActionConfirmationManager = actionConfirmationManager;
        mTabStripHeight = tabStripHeight;
        mTabStripVisibleSupplier = tabStripVisibleSupplier;

        // Use toolbar menu button padding to align NTB with menu button.
        mFixedEndPadding =
                context.getResources().getDimension(R.dimen.button_end_padding)
                        / context.getResources().getDisplayMetrics().density;
        mReservedEndMargin = mFixedEndPadding + mNewTabButtonWidth;
        updateMargins(false);

        mMinTabWidth = TAB_STRIP_TAB_WIDTH;

        mMaxTabWidth = TabUiThemeUtil.getMaxTabStripTabWidthDp();
        mManagerHost = managerHost;
        mUpdateHost = updateHost;
        mRenderHost = renderHost;
        CompositorOnClickHandler newTabClickHandler =
                new CompositorOnClickHandler() {
                    @Override
                    public void onClick(long time) {
                        handleNewTabClick();
                    }
                };

        // Set new tab button background resource.
        mNewTabButton =
                new TintedCompositorButton(
                        context,
                        NEW_TAB_BUTTON_BACKGROUND_WIDTH_DP,
                        NEW_TAB_BUTTON_BACKGROUND_HEIGHT_DP,
                        newTabClickHandler,
                        R.drawable.ic_new_tab_button);
        mNewTabButton.setBackgroundResourceId(R.drawable.bg_circle_tab_strip_button);

        int apsBackgroundHoveredTint =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultTextColor(context),
                        NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);
        int apsBackgroundPressedTint =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultTextColor(context),
                        NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);

        int apsBackgroundIncognitoHoveredTint =
                ColorUtils.setAlphaComponentWithFloat(
                        context.getColor(R.color.tab_strip_button_hover_bg_color),
                        NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);
        int apsBackgroundIncognitoPressedTint =
                ColorUtils.setAlphaComponentWithFloat(
                        context.getColor(R.color.tab_strip_button_hover_bg_color),
                        NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);

        // Primary container for default bg color.
        int BackgroundDefaultTint = TabUiThemeProvider.getDefaultNTBContainerColor(context);

        // Primary @ 20% for default pressed bg color.
        int BackgroundPressedTint =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultIconColorAccent1(context),
                        NEW_TAB_BUTTON_DEFAULT_PRESSED_OPACITY);

        // Surface-2 baseline for incognito bg color.
        int BackgroundIncognitoDefaultTint =
                context.getColor(R.color.default_bg_color_dark_elev_2_baseline);

        // Surface-5 baseline for incognito pressed bg color
        int BackgroundIncognitoPressedTint =
                context.getColor(R.color.default_bg_color_dark_elev_5_baseline);

        // Tab strip redesign new tab button night mode bg color.
        if (ColorUtils.inNightMode(context)) {
            // Surface-1 for night mode bg color.
            BackgroundDefaultTint =
                    ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_1);

            // Surface 5 for pressed night mode bg color.
            BackgroundPressedTint =
                    ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_5);
        }
        mNewTabButton.setBackgroundTint(
                BackgroundDefaultTint,
                BackgroundPressedTint,
                BackgroundIncognitoDefaultTint,
                BackgroundIncognitoPressedTint,
                apsBackgroundHoveredTint,
                apsBackgroundPressedTint,
                apsBackgroundIncognitoHoveredTint,
                apsBackgroundIncognitoPressedTint);

        // No pressed state color change for new tab button icon.
        mNewTabButton.setTintResources(
                R.color.default_icon_color_tint_list,
                R.color.default_icon_color_tint_list,
                R.color.modern_white,
                R.color.modern_white);

        // y-offset  = lowered tab container + (tab container size - bg size)/2 -
        // Tab title y-offset = 2 + (38 - 32)/2 - 2 = 3dp
        mNewTabButton.setDrawY(NEW_TAB_BUTTON_BACKGROUND_Y_OFFSET_DP);

        mNewTabButton.setIncognito(incognito);
        mNewTabButton.setClickSlop(NEW_TAB_BUTTON_CLICK_SLOP_DP);
        Resources res = context.getResources();
        mNewTabButton.setAccessibilityDescription(
                res.getString(R.string.accessibility_toolbar_btn_new_tab),
                res.getString(R.string.accessibility_toolbar_btn_new_incognito_tab));

        // Vivaldi
        updateNewTabButtonState();
        mShowTabsAsFavIcon = false;

        mContext = context;
        mIncognito = incognito;

        // Create tab menu
        // Vivaldi
        mTabMenu = new VivaldiTabMenu(context);
        mTabMenu.setAdapter(
                new ArrayAdapter<String>(
                        mContext,
                        android.R.layout.simple_list_item_1,
                        new String[] {
                            mContext.getString(
                                    !mIncognito
                                            ? R.string.menu_close_all_tabs
                                            : R.string.menu_close_all_incognito_tabs)
                        }));
        if (!ChromeApplicationImpl.isVivaldi()) // Listener is implemented in |VivaldiTabMenu|.
        mTabMenu.setOnItemClickListener(
                new OnItemClickListener() {
                    @Override
                    public void onItemClick(
                            AdapterView<?> parent, View view, int position, long id) {
                        mTabMenu.dismiss();
                        if (position == ID_CLOSE_ALL_TABS) {
                            mTabGroupModelFilter.closeAllTabs(
                                    /* uponExit= */ false, /* hideTabGroups= */ true);
                            RecordUserAction.record("MobileToolbarCloseAllTabs");
                        }
                    }
                });

        int menuWidth = mContext.getResources().getDimensionPixelSize(R.dimen.menu_width);
        mTabMenu.setWidth(menuWidth);
        mTabMenu.setModal(true);

        mIsFirstLayoutPass = true;
        // Vivaldi;
        mIsRestoreInProgress = false;
        mModelObserver = new TabModelObserver() {
            @Override
            public void restoreCompleted() {
                mIsRestoreInProgress = false;
                scrollToSelectedTab(false);
                updateVisualTabOrdering();
                updateStrip();
            }
        };

        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(mPrefsListener);
        if (BuildConfig.IS_VIVALDI) { // Ref. VAB-9347
            mTabMenu.setOnDismissListener(this::onDismiss);
        }
    }

    /** Cleans up internal state. */
    public void destroy() {
        mStripTabEventHandler.removeCallbacksAndMessages(null);
        if (mTabHoverCardView != null) {
            mTabHoverCardView.destroy();
            mTabHoverCardView = null;
        }
        if (mTabGroupModelFilter != null) {
            mTabGroupModelFilter.removeTabGroupObserver(mTabGroupModelFilterObserver);
            mTabGroupModelFilter = null;
        }

        // Vivaldi
        if (mModelObserver != null) mModel.removeObserver(mModelObserver);
    }

    /**
     * Get a list of virtual views for accessibility.
     *
     * @param views A List to populate with virtual views.
     */
    public void getVirtualViews(List<VirtualView> views) {
        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView view = mStripViews[i];
            view.getVirtualViews(views);
        }
        if (mNewTabButton.isVisible()) mNewTabButton.getVirtualViews(views);
    }

    /**
     * Get the touchable area within the strip, presented as a {@link RectF}, where (0,0) is the
     * top-left point of the StripLayoutHelper. The area will include the tabs and new tab button.
     */
    RectF getTouchableRect() {
        return mTouchableRect;
    }

    /**
     * @return The visually ordered list of visible {@link StripLayoutTab}s.
     */
    public StripLayoutTab[] getStripLayoutTabsToRender() {
        return mStripTabsToRender;
    }

    /**
     * @return The visually ordered list of visible {@link StripLayoutGroupTitle}s.
     */
    public StripLayoutGroupTitle[] getStripLayoutGroupTitlesToRender() {
        return mStripGroupTitlesToRender;
    }

    /**
     * @return A {@link TintedCompositorButton} that represents the positioning of the new tab
     *         button.
     */
    public TintedCompositorButton getNewTabButton() {
        return mNewTabButton;
    }

    /**
     * @return The visual offset to be applied to the new tab button.
     */
    protected float getNewTabButtonVisualOffset() {
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        float newTabButtonTouchTargetOffset;
        if (isTabStripFull()) {
            newTabButtonTouchTargetOffset = 0;
        } else {
            newTabButtonTouchTargetOffset = getNtbVisualOffsetHorizontal();
        }
        return isRtl ? newTabButtonTouchTargetOffset : -newTabButtonTouchTargetOffset;
    }

    /**
     * Check whether the tab strip is full by checking whether tab width has decreased to fit more
     * tabs.
     *
     * @return Whether the tab strip is full.
     */
    private boolean isTabStripFull() {
        return mCachedTabWidth < TabUiThemeUtil.getMaxTabStripTabWidthDp();
    }

    /**
     * Determine How far to shift new tab button icon visually towards the tab in order to achieve
     * the desired spacing between new tab button and tabs when tab strip is not full.
     *
     * @return Visual offset of new tab button icon.
     */
    protected float getNtbVisualOffsetHorizontal() {
        return (BUTTON_DESIRED_TOUCH_TARGET_SIZE - mNewTabButtonWidth) / 2
                - DESIRED_PADDING_BETWEEN_NEW_TAB_BUTTON_AND_TABS;
    }

    /**
     * @return The opacity to use for the fade on the left side of the tab strip.
     */
    public float getLeftFadeOpacity() {
        return getFadeOpacity(true);
    }

    /**
     * @return The opacity to use for the fade on the right side of the tab strip.
     */
    public float getRightFadeOpacity() {
        return getFadeOpacity(false);
    }

    /**
     * When the {@link ScrollingStripStacker} is being used, a fade is shown at the left and
     * right edges to indicate there is tab strip content off screen. As the scroll position
     * approaches the edge of the screen, the fade opacity is lowered.
     *
     * @param isLeft Whether the opacity for the left or right side should be returned.
     * @return The opacity to use for the fade.
     */
    private float getFadeOpacity(boolean isLeft) {
        // In RTL, scroll position 0 is on the right side of the screen, whereas in LTR scroll
        // position 0 is on the left. Account for that in the offset calculation.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        boolean useUnadjustedScrollOffset = isRtl != isLeft;
        float offset =
                -(useUnadjustedScrollOffset ? mScrollOffset : (mMinScrollOffset - mScrollOffset));

        if (offset <= 0.f) {
            return 0.f;
        } else if (offset >= FADE_FULL_OPACITY_THRESHOLD_DP) {
            return 1.f;
        } else {
            return offset / FADE_FULL_OPACITY_THRESHOLD_DP;
        }
    }

    /**
     * Set the scroll offset.
     * @param scrollOffset The scroll offset.
     */
    void setScrollOffset(float scrollOffset) {
        mScrollOffset = scrollOffset;
    }

    /**
     * @return The strip's current scroll offset.
     */
    float getScrollOffset() {
        return mScrollOffset;
    }

    float getVisibleLeftBound() {
        return mLeftPadding;
    }

    float getVisibleRightBound() {
        return mWidth - mRightPadding;
    }

    /**
     * @param msbTouchTargetSize The touch target size for the model selector button.
     */
    public void updateEndMarginForStripButtons(float msbTouchTargetSize) {
        // When MSB is not visible we add strip end padding here. When MSB is visible strip end
        // padding will be included in MSB margin, so just add padding between NTB and MSB here.
        mReservedEndMargin =
                msbTouchTargetSize
                        + mNewTabButtonWidth
                        + (mModelSelectorButton.isVisible()
                                ? NEW_TAB_BUTTON_WITH_MODEL_SELECTOR_BUTTON_PADDING
                                : mFixedEndPadding);
        updateMargins(true);
    }

    private void updateMargins(boolean recalculateTabWidth) {
        if (LocalizationUtils.isLayoutRtl()) {
            mLeftMargin = mReservedEndMargin + mLeftPadding;
            mRightMargin = mRightPadding;
        } else {
            mLeftMargin = mLeftPadding;
            mRightMargin = mReservedEndMargin + mRightPadding;
        }
        if (recalculateTabWidth) computeAndUpdateTabWidth(false, false, null);
    }

    /**
     * Sets the left fade width based on which fade is showing.
     *
     * @param fadeWidth The width of the left fade.
     */
    public void setLeftFadeWidth(float fadeWidth) {
        if (mLeftFadeWidth != fadeWidth) {
            mLeftFadeWidth = fadeWidth;
            bringSelectedTabToVisibleArea(LayoutManagerImpl.time(), false);
        }
    }

    /**
     * Sets the right fade width based on which fade is showing.
     * @param fadeWidth The width of the right fade.
     */
    public void setRightFadeWidth(float fadeWidth) {
        if (mRightFadeWidth != fadeWidth) {
            mRightFadeWidth = fadeWidth;
            bringSelectedTabToVisibleArea(LayoutManagerImpl.time(), false);
        }
    }

    /**
     * Updates the size of the virtual tab strip, making the tabs resize and move accordingly.
     *
     * @param width The new available width.
     * @param height The new height this stack should be.
     * @param orientationChanged Whether the screen orientation was changed.
     * @param time The current time of the app in ms.
     * @param leftPadding The new left padding.
     * @param rightPadding The new right padding.
     */
    public void onSizeChanged(
            float width,
            float height,
            boolean orientationChanged,
            long time,
            float leftPadding,
            float rightPadding) {
        if (mWidth == width
                && mHeight == height
                && leftPadding == mLeftPadding
                && rightPadding == mRightPadding) {
            return;
        }

        StripLayoutTab selectedTab = getSelectedStripTab();
        boolean wasSelectedTabVisible = selectedTab != null && selectedTab.isVisible();
        boolean recalculateTabWidth =
                mWidth != width || mLeftPadding != leftPadding || mRightPadding != rightPadding;

        mWidth = width;
        mHeight = height;
        mLeftPadding = leftPadding;
        mRightPadding = rightPadding;

        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView view = mStripViews[i];
            view.setHeight(mHeight);
        }

        updateMargins(recalculateTabWidth);
        if (mStripViews.length > 0) mUpdateHost.requestUpdate();

        // Dismiss tab menu, similar to how the app menu is dismissed on orientation change
        mTabMenu.dismiss();

        // Dismiss iph on orientation change, as its position might become incorrect.
        dismissTabGroupSyncIph();

        if ((orientationChanged && wasSelectedTabVisible) || !mTabStateInitialized) {
            bringSelectedTabToVisibleArea(time, mTabStateInitialized);
        }
    }

    /**
     * Updates all internal resources and dimensions.
     * @param context The current Android {@link Context}.
     */
    public void onContextChanged(Context context) {
        mScroller = new StackScroller(context);
        mContext = context;
    }

    /**
     * Notify the a title has changed.
     *
     * @param tabId     The id of the tab that has changed.
     * @param title     The new title.
     */
    public void tabTitleChanged(int tabId, String title) {
        Tab tab = getTabById(tabId);
        if (tab != null) setAccessibilityDescription(findTabById(tabId), title, tab.isHidden());
    }

    /**
     * Sets the {@link TabModel} that this {@link StripLayoutHelper} will visually represent.
     * @param model The {@link TabModel} to visually represent.
     * @param tabCreator The {@link TabCreator}, used to create new tabs.
     * @param tabStateInitialized Whether the tab model's tab state is fully initialized after
     *                            startup or not.
     */
    public void setTabModel(TabModel model, TabCreator tabCreator, boolean tabStateInitialized) {
        if (mModel == model) return;
        mModel = model;
        mTabCreator = tabCreator;
        mTabStateInitialized = tabStateInitialized;

        // If the tabs are still restoring and the refactoring experiment is enabled, we'll create a
        // placeholder strip. This means we don't need to call computeAndUpdateTabOrders() to
        // generate "real" strip tabs.
        if (!mTabStateInitialized && ChromeFeatureList.sTabStripStartupRefactoring.isEnabled()) {
            // If the placeholder strip is ready, replace the matching placeholders for the tabs
            // that have already been restored.
            mSelectedOnStartup = mModel.isActiveModel();
            if (mPlaceholderStripReady) replacePlaceholdersForRestoredTabs();
        } else {
            RecordHistogram.recordMediumTimesHistogram(
                    PLACEHOLDER_VISIBLE_DURATION_HISTOGRAM_NAME, 0L);

            computeAndUpdateTabOrders(false, false);
            resizeTabStrip(false, false, false);
        }
    }

    /** Called to notify that the tab state has been initialized. */
    protected void onTabStateInitialized() {
        mTabStateInitialized = true;

        if (ChromeFeatureList.sTabStripStartupRefactoring.isEnabled() && mPlaceholderStripReady) {
            int numLeftoverPlaceholders = 0;
            for (int i = 0; i < mStripTabs.length; i++) {
                if (mStripTabs[i].getIsPlaceholder()) numLeftoverPlaceholders++;
            }

            RecordHistogram.recordCount1000Histogram(
                    PLACEHOLDER_LEFTOVER_TABS_HISTOGRAM_NAME, numLeftoverPlaceholders);
            RecordHistogram.recordCount1000Histogram(
                    PLACEHOLDER_TABS_CREATED_DURING_RESTORE_HISTOGRAM_NAME,
                    mTabsCreatedDuringRestore);
            RecordHistogram.recordCount1000Histogram(
                    PLACEHOLDER_TABS_NEEDED_DURING_RESTORE_HISTOGRAM_NAME,
                    mPlaceholdersNeededDuringRestore);
            RecordHistogram.recordMediumTimesHistogram(
                    PLACEHOLDER_VISIBLE_DURATION_HISTOGRAM_NAME,
                    SystemClock.uptimeMillis() - mPlaceholderCreationTime);
        }

        // Recreate the StripLayoutTabs from the TabModel, now that all of the real Tabs have been
        // restored. This will reuse valid tabs, discard invalid tabs, and correct tab orders.
        computeAndUpdateTabOrders(false, false);

        // Vivaldi
        mModel.addObserver(mModelObserver);
    }

    /**
     * Sets the {@link TabGroupModelFilter} that will access the internal tab group state.
     *
     * @param tabGroupModelFilter The {@link TabGroupModelFilter}.
     */
    public void setTabGroupModelFilter(TabGroupModelFilter tabGroupModelFilter) {
        if (mTabGroupModelFilter != null) {
            mTabGroupModelFilter.removeTabGroupObserver(mTabGroupModelFilterObserver);
        }

        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupModelFilter.addTabGroupObserver(mTabGroupModelFilterObserver);

        updateTitleCacheForInit();
        rebuildStripViews();
    }

    TabGroupModelFilterObserver getTabGroupModelFilterObserverForTesting() {
        return mTabGroupModelFilterObserver;
    }

    /**
     * Sets the {@link LayerTitleCache} for the tab strip bitmaps.
     *
     * @param layerTitleCache The {@link LayerTitleCache}.
     */
    public void setLayerTitleCache(LayerTitleCache layerTitleCache) {
        mLayerTitleCache = layerTitleCache;
        updateTitleCacheForInit();
        mRenderHost.requestRender();
    }

    private void updateTitleCacheForInit() {
        if (mTabGroupModelFilter == null || mLayerTitleCache == null) return;

        for (int i = 0; i < mStripGroupTitles.length; ++i) {
            final StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            updateGroupTitleText(groupTitle, groupTitle.getTitle());
        }
    }

    /** Dismiss the iph text bubble for synced tab group. */
    private void dismissTabGroupSyncIph() {
        if (mLastSyncedGroupId != Tab.INVALID_TAB_ID && mTabGroupSyncIphController != null) {
            mTabGroupSyncIphController.dismissTextBubble();
            mLastSyncedGroupId = Tab.INVALID_TAB_ID;
        }
    }

    /**
     * Helper-specific updates. Cascades the values updated by the animations and flings.
     *
     * @param time The current time of the app in ms.
     * @return Whether or not animations are done.
     */
    public boolean updateLayout(long time) {
        mLastUpdateTime = time;

        // 1. Handle any Scroller movements (flings).
        updateScrollOffset(time);

        // 2. Handle reordering automatically scrolling the tab strip.
        handleReorderAutoScrolling(time);

        // 3. Update tab spinners.
        updateSpinners(time);

        final boolean doneAnimating = mRunningAnimator == null || !mRunningAnimator.isRunning();
        updateStrip();

        // If this is the first layout pass, scroll to the selected tab so that it is visible.
        // This is needed if the ScrollingStripStacker is being used because the selected tab is
        // not guaranteed to be visible.
        if (mIsFirstLayoutPass) {
            bringSelectedTabToVisibleArea(time, false);
            mIsFirstLayoutPass = false;
        }

        // 4. Show iph text bubble for synced tab group if necessary.
        if (doneAnimating && mScroller.isFinished()) {
            showTabGroupSyncIph();
        }
        return doneAnimating;
    }

    private void showTabGroupSyncIph() {
        // Return early if no tab group is being synced, or profile is invalid, or if in incognito
        // mode.
        if (mLastSyncedGroupId == Tab.INVALID_TAB_ID
                || mModel.isIncognito()
                || mModel.getProfile() == null) {
            return;
        }
        // Return early if the tab strip is not visible on screen.
        if (Boolean.FALSE.equals(mTabStripVisibleSupplier.get())) {
            return;
        }
        // Skip initialization if testing value has been set.
        if (mTabGroupSyncIphController == null) {
            UserEducationHelper userEducationHelper =
                    new UserEducationHelper(
                            mWindowAndroid.getActivity().get(),
                            mModel.getProfile(),
                            new Handler(Looper.getMainLooper()));
            Tracker tracker = TrackerFactory.getTrackerForProfile(mModel.getProfile());
            mTabGroupSyncIphController =
                    new TabGroupSyncIphController(
                            mContext.getResources(),
                            userEducationHelper,
                            R.string.newly_synced_tab_group_iph,
                            tracker);
        }
        StripLayoutGroupTitle groupTitle = findGroupTitle(mLastSyncedGroupId);

        // Display iph only when synced tab group title is fully visible.
        if (groupTitle == null
                || !groupTitle.isVisible()
                || groupTitle.getPaddedX() + groupTitle.getPaddedWidth()
                        >= mNewTabButton.getDrawX()) {
            return;
        }
        float dpToPx = mContext.getResources().getDisplayMetrics().density;
        if (groupTitle != null) {
            mTabGroupSyncIphController.maybeShowIphOnTabStrip(
                    mToolbarContainerView,
                    groupTitle.getDrawX() * dpToPx,
                    0.f,
                    (mWidth - groupTitle.getDrawX() - groupTitle.getWidth()) * dpToPx,
                    mToolbarContainerView.getHeight() - mTabStripHeight);
        }
    }

    void setLastSyncedGroupIdForTesting(int id) {
        mLastSyncedGroupId = id;
    }

    void setTabGroupSyncIphControllerForTesting(
            TabGroupSyncIphController tabGroupSyncIphController) {
        mTabGroupSyncIphController = tabGroupSyncIphController;
    }

    void setIsFirstLayoutPassForTesting(boolean isFirstLayoutPass) {
        mIsFirstLayoutPass = isFirstLayoutPass;
    }

    /**
     * Called when a new tab model is selected.
     *
     * @param selected If the new tab model selected is the model that this strip helper associated
     *     with.
     */
    public void tabModelSelected(boolean selected) {
        if (selected) {
            bringSelectedTabToVisibleArea(0, false);
        } else {
            mTabMenu.dismiss();
        }
    }

    /**
     * Called when a tab get selected.
     * @param time   The current time of the app in ms.
     * @param id     The id of the selected tab.
     * @param prevId The id of the previously selected tab.
     * @param skipAutoScroll Whether autoscroll to bring selected tab to view can be skipped.
     */
    public void tabSelected(long time, int id, int prevId, boolean skipAutoScroll) {
        // Note(david@vivaldi.com): It can happen that the stack strip is not yet created and the
        // tab won't be visible after selecting the stack. Due to that we update the strip first.
        if (mIsStackStrip) updateStrip();
        StripLayoutTab stripTab = findTabById(id);
        if (stripTab == null) {
            tabCreated(time, id, prevId, true, false, false);
        } else {
            updateVisualTabOrdering();
            updateCloseButtons();

            Tab tab = getTabById(id);
            if (tab != null && mTabGroupModelFilter.getTabGroupCollapsed(tab.getRootId())) {
                mTabGroupModelFilter.deleteTabGroupCollapsed(tab.getRootId());
            }

            if (!skipAutoScroll && !mInReorderMode) {
                // If the tab was selected through a method other than the user tapping on the
                // strip, it may not be currently visible. Scroll if necessary.
                bringSelectedTabToVisibleArea(time, true);
            }

            mUpdateHost.requestUpdate();

            setAccessibilityDescription(stripTab, getTabById(id));
            setAccessibilityDescription(findTabById(prevId), getTabById(prevId));
        }
    }

    /**
     * Called when a tab has been moved in the tabModel.
     * @param time     The current time of the app in ms.
     * @param id       The id of the Tab.
     * @param oldIndex The old index of the tab in the {@link TabModel}.
     * @param newIndex The new index of the tab in the {@link TabModel}.
     */
    public void tabMoved(long time, int id, int oldIndex, int newIndex) {
        // Note(david@vivaldi.com): The strip will be updated with the next layout update.
        if (ChromeApplicationImpl.isVivaldi()) {
            updateVisualTabOrdering();
            mUpdateHost.requestUpdate();
            return;
        }
        reorderTab(id, oldIndex, newIndex, false);

        updateVisualTabOrdering();
        mUpdateHost.requestUpdate();
    }

    /**
     * Called when a tab will be closed. When called, the closing tab will be part of the model.
     *
     * @param time The current time of the app in ms.
     * @param tab The tab that will be closed.
     */
    public void willCloseTab(long time, Tab tab) {
        if (tab != null) updateGroupTitleText(tab.getRootId());
    }

    /**
     * Called when a tab is being closed. When called, the closing tab will not be part of the
     * model.
     *
     * @param time The current time of the app in ms.
     * @param id The id of the tab being closed.
     */
    public void tabClosed(long time, int id) {
        if (findTabById(id) == null) return;

        // 1. Find out if we're closing the last tab.  This determines if we resize immediately.
        // We know mStripTabs.length >= 1 because findTabById did not return null.
        boolean closingLastTab = mStripTabs[mStripTabs.length - 1].getId() == id;

        // 2. Rebuild the strip.
        computeAndUpdateTabOrders(!closingLastTab, false);

        mUpdateHost.requestUpdate();
    }

    /** Called when all tabs are closed at once. */
    public void willCloseAllTabs() {
        computeAndUpdateTabOrders(true, false);
        mUpdateHost.requestUpdate();
    }

    /**
     * Called when a tab close has been undone and the tab has been restored. This also re-selects
     * the last tab the user was on before the tab was closed.
     * @param time The current time of the app in ms.
     * @param id   The id of the Tab.
     */
    public void tabClosureCancelled(long time, int id) {
        final boolean selected = TabModelUtils.getCurrentTabId(mModel) == id;
        tabCreated(time, id, Tab.INVALID_TAB_ID, selected, true, false);
    }

    /**
     * Called when a tab is created from the top left button.
     * @param time             The current time of the app in ms.
     * @param id               The id of the newly created tab.
     * @param prevId           The id of the source tab.
     * @param selected         Whether the tab will be selected.
     * @param closureCancelled Whether the tab was restored by a tab closure cancellation.
     * @param onStartup        Whether the tab is being unfrozen during startup.
     */
    public void tabCreated(
            long time,
            int id,
            int prevId,
            boolean selected,
            boolean closureCancelled,
            boolean onStartup) {
        // Note(david@vivaldi.com): We don't do anything when tab is restored. This will give us
        // some performance improvements. The tabs will be updated anyway with the next update loop.
        Tab newTab = mModel.getTabById(id);
        if (newTab != null && newTab.getLaunchType() == TabLaunchType.FROM_RESTORE) return;

        if (findTabById(id) != null) return;

        // 1. If tab state is still initializing, replace the matching placeholder tab.
        if (!mTabStateInitialized && ChromeFeatureList.sTabStripStartupRefactoring.isEnabled()) {
            replaceNextPlaceholder(id, selected, onStartup);

            return;
        }

        // Otherwise, 2. Build any tabs that are missing. Determine if it will be collapsed.
        finishAnimationsAndPushTabUpdates();
        List<Animator> animationList = computeAndUpdateTabOrders(false, !onStartup);
        Tab tab = getTabById(id);
        boolean collapsed = false;
        if (tab != null) {
            int rootId = tab.getRootId();
            updateGroupTitleText(rootId);
            if (mTabGroupModelFilter.getTabGroupCollapsed(rootId)) {
                if (selected) {
                    mTabGroupModelFilter.deleteTabGroupCollapsed(rootId);
                } else {
                    collapsed = true;
                }
            }
        }

        // 3. Start an animation for the newly created tab, unless it is collapsed.
        if (animationList == null) animationList = new ArrayList<>();
        StripLayoutTab stripTab = findTabById(id);
        if (stripTab != null) {
            updateTabCollapsed(stripTab, collapsed, false);
            if (!onStartup && !collapsed) runTabAddedAnimator(animationList, stripTab);
        }

        // 4. If the new tab will be selected, scroll it to view. If the new tab will not be
        // selected, scroll the currently selected tab to view. Skip auto-scrolling if the tab is
        // being created due to a tab closure being undone.
        if (stripTab != null && !closureCancelled && !collapsed) {
            boolean animate = !onStartup && !mAnimationsDisabledForTesting;
            if (selected) {
                float delta = calculateDeltaToMakeTabVisible(stripTab);
                setScrollForScrollingTabStacker(delta, animate, time);
            } else {
                bringSelectedTabToVisibleArea(time, animate);
            }
        }

        mUpdateHost.requestUpdate();
    }

    private void runTabAddedAnimator(@NonNull List<Animator> animationList, StripLayoutTab tab) {
        animationList.add(
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.Y_OFFSET,
                        tab.getHeight(),
                        0f,
                        ANIM_TAB_CREATED_MS));

        startAnimationList(animationList, /* listener= */ null);
    }

    /**
     * Set the relevant tab model metadata prior to the tab state initialization.
     *
     * @param activeTabIndexOnStartup What the active tab index should be after tabs finish
     *     restoring.
     * @param tabCountOnStartup What the tab count should be after tabs finish restoring.
     * @param createdTabOnStartup If an additional tab was created on startup (e.g. through intent).
     */
    protected void setTabModelStartupInfo(
            int tabCountOnStartup, int activeTabIndexOnStartup, boolean createdTabOnStartup) {
        if (!ChromeFeatureList.sTabStripStartupRefactoring.isEnabled()) return;

        mTabCountOnStartup = tabCountOnStartup;
        mActiveTabIndexOnStartup = activeTabIndexOnStartup;
        mCreatedTabOnStartup = createdTabOnStartup;

        // Avoid creating the placeholder strip if we have an invalid active tab index.
        if (mActiveTabIndexOnStartup < 0 || mActiveTabIndexOnStartup >= mTabCountOnStartup) return;

        // If tabs are still being restored on startup, create placeholder tabs to mitigate jank.
        if (!mTabStateInitialized) {
            prepareEmptyPlaceholderStripLayout();

            // If the TabModel has already been set, then replace placeholders for restored tabs.
            if (mModel != null) replacePlaceholdersForRestoredTabs();
        }
    }

    /**
     * Creates the placeholder tabs that will be shown on startup before the tab state is
     * initialized.
     */
    private void prepareEmptyPlaceholderStripLayout() {
        // TODO(crbug.com/41497111): Investigate if we can update for tab group indicators.
        if (mPlaceholderStripReady || mTabStateInitialized) return;

        // 1. Fill with placeholder tabs.
        mStripTabs = new StripLayoutTab[mTabCountOnStartup];
        for (int i = 0; i < mStripTabs.length; i++) {
            mStripTabs[i] = createPlaceholderStripTab();
        }
        rebuildStripViews();

        // 2. Initialize the draw parameters.
        computeAndUpdateTabWidth(false, false, null);
        updateVisualTabOrdering();

        // 3. Scroll the strip to bring the selected tab to view and ensure that the active tab
        // container is visible.
        if (mActiveTabIndexOnStartup != TabModel.INVALID_TAB_INDEX) {
            bringSelectedTabToVisibleArea(LayoutManagerImpl.time(), false);

            mStripTabs[mActiveTabIndexOnStartup].setContainerOpacity(
                    TAB_OPACITY_VISIBLE_FOREGROUND);
        }

        // 4. Mark that the placeholder strip layout is ready and request a visual update.
        mPlaceholderStripReady = true;
        mPlaceholderCreationTime = SystemClock.uptimeMillis();
        mUpdateHost.requestUpdate();
    }

    /**
     * Replace placeholders for all tabs that have already been restored. Do so by updating all
     * relevant properties in the StripLayoutTab (id).
     */
    private void replacePlaceholdersForRestoredTabs() {
        if (!mPlaceholderStripReady || mTabStateInitialized) return;

        // If the number of tabs is less than the expected active tab index, it means that there
        // will need to be placeholders before the active tab. If this is the case, replace the
        // active tab later to ensure it's at the correct index.
        int numTabsToCopy = mModel.getCount();
        if (mCreatedTabOnStartup) numTabsToCopy--;
        boolean needPlaceholdersBeforeActiveTab =
                numTabsToCopy <= mActiveTabIndexOnStartup && mSelectedOnStartup;
        if (needPlaceholdersBeforeActiveTab && numTabsToCopy > 0) numTabsToCopy--;
        mCurrentPlaceholderIndex = numTabsToCopy;

        // There should not be more restored tabs than the allotted placeholder tabs.
        assert numTabsToCopy <= mStripTabs.length;

        // 1. Replace the placeholder tabs by updating the relevant properties.
        for (int i = 0; i < numTabsToCopy; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            final Tab tab = mModel.getTabAt(i);

            pushPropertiesToPlaceholder(stripTab, tab);
        }
        if (!needPlaceholdersBeforeActiveTab) mActiveTabReplaced = true;

        // 2. If a new tab was created on startup (e.g. through intent), copy it over now.
        if (mCreatedTabOnStartup) {
            final StripLayoutTab stripTab = mStripTabs[mStripTabs.length - 1];
            final Tab tab = mModel.getTabAt(mModel.getCount() - 1);

            pushPropertiesToPlaceholder(stripTab, tab);
        }

        // 3. If the active tab could not be copied earlier, copy it over now at the correct index.
        if (needPlaceholdersBeforeActiveTab) {
            int prevActiveIndex = mModel.getCount() - 1;
            if (mCreatedTabOnStartup) prevActiveIndex--;

            if (prevActiveIndex >= 0) {
                final StripLayoutTab stripTab = mStripTabs[mActiveTabIndexOnStartup];
                final Tab tab = mModel.getTabAt(prevActiveIndex);

                pushPropertiesToPlaceholder(stripTab, tab);

                mActiveTabReplaced = true;
            }
        }

        // 4. Request new frame.
        mRenderHost.requestRender();
    }

    private void replaceNextPlaceholder(int id, boolean selected, boolean onStartup) {
        assert !mTabStateInitialized;

        // Placeholders are not yet ready. This strip tab will instead be created when we
        // prepare the placeholder strip.
        if (!mPlaceholderStripReady) return;

        // The active tab is handled separately.
        if (mCurrentPlaceholderIndex == mActiveTabIndexOnStartup && mSelectedOnStartup) {
            mCurrentPlaceholderIndex++;
        }

        // Tab manually created while tabs were still restoring on startup.
        if (!onStartup) {
            mTabsCreatedDuringRestore++;
            return;
        }

        // Unexpectedly ran out of placeholders.
        if (mCurrentPlaceholderIndex >= mStripTabs.length && !selected) {
            mPlaceholdersNeededDuringRestore++;
            return;
        }

        // Replace the matching placeholder.
        int replaceIndex;
        if (selected || !mActiveTabReplaced) {
            replaceIndex = mActiveTabIndexOnStartup;
            mActiveTabReplaced = true;
        } else {
            // Should match the index in the model.
            replaceIndex = mCurrentPlaceholderIndex++;
            assert replaceIndex == mModel.indexOf(getTabById(id));
        }

        if (replaceIndex >= 0 && replaceIndex < mStripTabs.length) {
            final StripLayoutTab placeholderTab = mStripTabs[replaceIndex];
            final Tab tab = getTabById(id);

            pushPropertiesToPlaceholder(placeholderTab, tab);

            if (placeholderTab.isVisible()) {
                mRenderHost.requestRender();
            }
        }
    }

    /**
     * @return The expected tab count after tabs finish restoring.
     */
    protected int getTabCountOnStartupForTesting() {
        return mTabCountOnStartup;
    }

    /**
     * @return The expected active tab index after tabs finish restoring.
     */
    protected int getActiveTabIndexOnStartupForTesting() {
        return mActiveTabIndexOnStartup;
    }

    /**
     * @return Whether a non-restored tab was created during startup (e.g. through intent).
     */
    protected boolean getCreatedTabOnStartupForTesting() {
        return mCreatedTabOnStartup;
    }

    /**
     * Called to hide close tab buttons when tab width is <156dp when min tab width is 108dp or for
     * partially visible tabs at the edge of the tab strip when min tab width is set to >=156dp.
     */
    private void updateCloseButtons() {
        int count = mStripTabs.length;
        int selectedIndex = getSelectedStripTabIndex();

        // Note(david@vivaldi.com): In Vivaldi we have two tab strips. Due to that we need to
        // consider the length for each individual strip here.
        count = mStripTabs.length;

        for (int i = 0; i < count; i++) {
            final StripLayoutTab tab = mStripTabs[i];
            boolean tabSelected = selectedIndex == i;
            boolean canShowCloseButton =
                    tab.getWidth() >= TAB_WIDTH_MEDIUM
                            || (tabSelected && shouldShowCloseButton(tab, i));
            mStripTabs[i].setCanShowCloseButton(canShowCloseButton, !mIsFirstLayoutPass);
        }
    }

    private void setTabContainerVisible(StripLayoutTab tab, boolean selected, boolean hovered) {
        // Don't interrupt a hovered tab container visibility animation, this will be handled in the
        // #onHover* methods.
        if (hovered) return;
        // Don't interrupt tab group background tab visibility.
        if (tab.getContainerOpacity() == TAB_OPACITY_VISIBLE_BACKGROUND) return;

        // The container will be visible if the tab is selected or is a placeholder tab.
        float containerOpacity =
                selected || tab.getIsPlaceholder()
                        ? TAB_OPACITY_VISIBLE_FOREGROUND
                        : TAB_OPACITY_HIDDEN;
        tab.setContainerOpacity(containerOpacity);
    }

    /**
     * Called to show/hide dividers and the foreground/hovered tab container. Dividers are only
     * necessary between tabs that both do not have a visible tab container (foreground or
     * background).
     */
    private void updateTabContainersAndDividers() {
        if (mStripTabs.length < 1) return;

        int hoveredId = mLastHoveredTab != null ? mLastHoveredTab.getId() : Tab.INVALID_TAB_ID;

        // Divider is never shown for the first tab.
        if (mStripViews[0] instanceof StripLayoutTab tab) {
            tab.setStartDividerVisible(false);
            setTabContainerVisible(tab, isSelectedTab(tab.getId()), hoveredId == tab.getId());

            boolean currContainerHidden = tab.getContainerOpacity() == TAB_OPACITY_HIDDEN;
            boolean endDividerVisible;
            if (ChromeFeatureList.sTabStripGroupIndicators.isEnabled()) {
                // End divider should only be shown if the following view is a group indicator.
                endDividerVisible =
                        currContainerHidden
                                && mStripViews.length > 1
                                && mStripViews[1] instanceof StripLayoutGroupTitle;
            } else {
                // End divider for first tab is only shown in reorder mode when tab has trailing
                // margin and container is not visible.
                endDividerVisible =
                        mInReorderMode && currContainerHidden && tab.getTrailingMargin() > 0;
            }
            tab.setEndDividerVisible(endDividerVisible);
        }

        for (int i = 1; i < mStripViews.length; i++) {
            if (!(mStripViews[i] instanceof StripLayoutTab currTab)) continue;

            boolean currTabSelected = isSelectedTab(currTab.getId());
            boolean currTabHovered = hoveredId == currTab.getId();

            // Set container opacity.
            setTabContainerVisible(currTab, currTabSelected, currTabHovered);

            /**
             * Start divider should be visible when: 1. prevTab is dragged off of the strip OR 2.
             * currTab container is hidden and (a) prevTab has trailing margin (ie: currTab is start
             * of group or an individual tab) OR (b) prevTab container is also hidden.
             */
            boolean currContainerHidden = currTab.getContainerOpacity() == TAB_OPACITY_HIDDEN;
            boolean startDividerVisible;
            if (mStripViews[i - 1] instanceof StripLayoutTab prevTab) {
                boolean prevTabNotLeftMostAndDraggedOffStrip = prevTab.isDraggedOffStrip() && i > 1;
                boolean prevContainerHidden = prevTab.getContainerOpacity() == TAB_OPACITY_HIDDEN;
                boolean prevTabHasMargin = prevTab.getTrailingMargin() > 0;
                startDividerVisible =
                        prevTabNotLeftMostAndDraggedOffStrip
                                || (currContainerHidden
                                        && (prevContainerHidden || prevTabHasMargin));
            } else {
                startDividerVisible = false;
            }
            currTab.setStartDividerVisible(startDividerVisible);

            /**
             * End divider should be applied when: 1. currTab container is hidden and (a) currTab's
             * trailing margin > 0 (i.e. is last tab in group) OR (b) currTab is last tab in strip
             * (as the last tab does not have trailing margin)
             */
            boolean currIsLastTab = i == (mStripViews.length - 1);
            boolean endDividerVisible;
            if (ChromeFeatureList.sTabStripGroupIndicators.isEnabled()) {
                // End divider should be shown if the following view is a group indicator.
                endDividerVisible =
                        currContainerHidden
                                && (currIsLastTab
                                        || mStripViews[i + 1] instanceof StripLayoutGroupTitle);
            } else {
                boolean currTabHasMargin = currTab.getTrailingMargin() > 0;
                endDividerVisible = currContainerHidden && (currTabHasMargin || currIsLastTab);
            }
            currTab.setEndDividerVisible(endDividerVisible);
        }
    }

    private void updateTouchableRect() {
        // Make the entire strip touchable when during dragging / reordering mode.
        boolean isTabDraggingInProgress =
                mTabDragSource != null && mTabDragSource.isTabDraggingInProgress();
        if (isTabStripFull() || mInReorderMode || isTabDraggingInProgress) {
            mTouchableRect.set(getVisibleLeftBound(), 0, getVisibleRightBound(), mHeight);
            return;
        }

        // When the tab strip is not full and not in recording mode, NTB is always showing after
        // the last visible tab on strip.
        RectF touchableRect = new RectF(0, 0, 0, mHeight);
        RectF ntbTouchRect = new RectF();
        getNewTabButton().getTouchTarget(ntbTouchRect);
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        if (isRtl) {
            touchableRect.right = getVisibleRightBound();
            touchableRect.left = Math.max(ntbTouchRect.left, getVisibleLeftBound());
        } else {
            touchableRect.left = getVisibleLeftBound();
            touchableRect.right = Math.min(ntbTouchRect.right, getVisibleRightBound());
        }
        mTouchableRect.set(touchableRect);
    }

    @VisibleForTesting
    protected int getStripTabRootId(StripLayoutTab stripTab) {
        if (mModel == null || stripTab == null || getTabById(stripTab.getId()) == null) {
            return Tab.INVALID_TAB_ID;
        }
        return getTabById(stripTab.getId()).getRootId();
    }

    private boolean isStripTabInTabGroup(StripLayoutTab stripTab) {
        if (stripTab == null || getTabById(stripTab.getId()) == null) {
            return false;
        }
        return mTabGroupModelFilter.isTabInTabGroup(getTabById(stripTab.getId()));
    }

    /**
     * Checks whether a tab at the edge of the strip is partially hidden, in which case the close
     * button will be hidden to avoid accidental clicks.
     *
     * @param tab The tab to check.
     * @param index The index of the tab.
     * @return Whether the close button should be shown for this tab.
     */
    private boolean shouldShowCloseButton(StripLayoutTab tab, int index) {
        boolean tabStartHidden;
        boolean tabEndHidden;
        boolean isLastTab = index == mStripTabs.length - 1;
        if (LocalizationUtils.isLayoutRtl()) {
            if (isLastTab) {
                tabStartHidden =
                        tab.getDrawX() + mTabOverlapWidth
                                < getVisibleLeftBound()
                                        + mNewTabButton.getDrawX()
                                        + mNewTabButton.getWidth();
            } else {
                tabStartHidden =
                        tab.getDrawX() + mTabOverlapWidth
                                < getVisibleLeftBound() + getCloseBtnVisibilityThreshold(false);
            }
            tabEndHidden =
                    tab.getDrawX() > getVisibleRightBound() - getCloseBtnVisibilityThreshold(true);
        } else {
            tabStartHidden =
                    tab.getDrawX() + tab.getWidth()
                            < getVisibleLeftBound() + getCloseBtnVisibilityThreshold(true);
            if (isLastTab) {
                tabEndHidden =
                        tab.getDrawX() + tab.getWidth() - mTabOverlapWidth
                                > getVisibleLeftBound() + mNewTabButton.getDrawX();
            } else {
                tabEndHidden =
                        (tab.getDrawX() + tab.getWidth() - mTabOverlapWidth
                                > getVisibleRightBound() - getCloseBtnVisibilityThreshold(false));
            }
        }
        return !tabStartHidden && !tabEndHidden;
    }

    /**
     * Called when a tab has started loading resources.
     *
     * @param id The id of the Tab.
     */
    public void tabLoadStarted(int id) {
        StripLayoutTab tab = findTabById(id);
        if (tab != null) tab.loadingStarted();
    }

    /**
     * Called when a tab has stopped loading resources.
     * @param id The id of the Tab.
     */
    public void tabLoadFinished(int id) {
        StripLayoutTab tab = findTabById(id);
        if (tab != null) tab.loadingFinished();
    }

    /**
     * Called on touch drag event.
     *
     * @param time The current time of the app in ms.
     * @param x The x coordinate of the end of the drag event.
     * @param y The y coordinate of the end of the drag event.
     * @param deltaX The number of pixels dragged in the x direction.
     */
    public void drag(long time, float x, float y, float deltaX) {
        resetResizeTimeout(false);

        mLastUpdateTime = time;
        deltaX = MathUtils.flipSignIf(deltaX, LocalizationUtils.isLayoutRtl());

        // Vivaldi: When there is any movement we dismiss the tab menu.
        mTabMenu.dismiss();

        // 1. Reset the button state.
        mNewTabButton.drag(x, y);

        // 2. If a tab was pressed in onDown and is now dragged, start tab drag/reorder.
        // This is to enable tab drag with BUTTON_PRIMARY (mouse / trackpad) via onDown.
        // Tab drags for touch events are handled via onLongPress.
        if (mOnDownWithButtonPrimary && mInteractingTab != null && !mInReorderMode) {
            startDragOrReorderTab(time, x, y, mInteractingTab);
        }

        if (mInReorderMode) {
            // 3.a. Handle reordering tabs.
            // This isn't the accumulated delta since the beginning of the drag.  It accumulates
            // the delta X until a threshold is crossed and then the event gets processed.
            float accumulatedDeltaX = x - mLastReorderX;

            if (Math.abs(accumulatedDeltaX) >= 1.f) {
                if (!LocalizationUtils.isLayoutRtl()) {
                    if (deltaX >= 1.f) {
                        mReorderState |= REORDER_SCROLL_RIGHT;
                    } else if (deltaX <= -1.f) {
                        mReorderState |= REORDER_SCROLL_LEFT;
                    }
                } else {
                    if (deltaX >= 1.f) {
                        mReorderState |= REORDER_SCROLL_LEFT;
                    } else if (deltaX <= -1.f) {
                        mReorderState |= REORDER_SCROLL_RIGHT;
                    }
                }

                mLastReorderX = x;
                if (mReorderingForTabDrop) {
                    updateReorderPositionForTabDrop(x);
                } else {
                    updateReorderPosition(accumulatedDeltaX);
                }
            }
        } else if (!mScroller.isFinished()) {
            // 3.b. Still scrolling, update the scroll destination here.
            mScroller.setFinalX((int) (mScroller.getFinalX() + deltaX));
        } else {
            // 3.c. Not scrolling.
            if (!mIsStripScrollInProgress) {
                mIsStripScrollInProgress = true;
                RecordUserAction.record("MobileToolbarSlideTabs");
                onStripScrollStart();
            }
            updateScrollOffsetPosition(mScrollOffset + deltaX);
        }

        // If we're scrolling at all we aren't interacting with any particular tab.
        // We already kicked off a fast expansion earlier if we needed one.  Reorder mode will
        // repopulate this if necessary.
        if (!mInReorderMode) mInteractingTab = null;
        mUpdateHost.requestUpdate();
    }

    void dragForTabDrop(long time, float x, float y, float deltaX, boolean draggedTabIncognito) {
        if (mIncognito == draggedTabIncognito) {
            drag(time, x, y, deltaX);
        }
    }

    private void onStripScrollStart() {
        long currentTime = SystemClock.elapsedRealtime();

        // If last scroll is within the max allowed interval, do not reset start time.
        if (mMostRecentTabScroll != null
                && currentTime - mMostRecentTabScroll
                        <= TAB_SWITCH_METRICS_MAX_ALLOWED_SCROLL_INTERVAL) {
            mMostRecentTabScroll = currentTime;
            return;
        }

        mTabScrollStartTime = currentTime;
        mMostRecentTabScroll = currentTime;
    }

    /**
     * Called on touch fling event. This is called before the onUpOrCancel event.
     * @param time      The current time of the app in ms.
     * @param x         The y coordinate of the start of the fling event.
     * @param y         The y coordinate of the start of the fling event.
     * @param velocityX The amount of velocity in the x direction.
     * @param velocityY The amount of velocity in the y direction.
     */
    public void fling(long time, float x, float y, float velocityX, float velocityY) {
        resetResizeTimeout(false);

        velocityX = MathUtils.flipSignIf(velocityX, LocalizationUtils.isLayoutRtl());

        // 1. If we're currently in reorder mode, don't allow the user to fling.
        if (mInReorderMode) return;

        // 2. If we're fast scrolling, figure out the destination of the scroll so we can apply it
        // to the end of this fling.
        int scrollDeltaRemaining = 0;
        if (!mScroller.isFinished()) {
            scrollDeltaRemaining = mScroller.getFinalX() - Math.round(mScrollOffset);

            mInteractingTab = null;
            mScroller.forceFinished(true);
        }

        // 3. Kick off the fling.
        mScroller.fling(
                Math.round(mScrollOffset),
                0,
                (int) velocityX,
                0,
                (int) mMinScrollOffset,
                0,
                0,
                0,
                0,
                0,
                time);
        mScroller.setFinalX(mScroller.getFinalX() + scrollDeltaRemaining);
        mUpdateHost.requestUpdate();
    }

    /**
     * Called on onDown event.
     * @param time      The time stamp in millisecond of the event.
     * @param x         The x position of the event.
     * @param y         The y position of the event.
     * @param fromMouse Whether the event originates from a mouse.
     * @param buttons   State of all buttons that are pressed.
     */
    public void onDown(long time, float x, float y, boolean fromMouse, int buttons) {
        // Prepare for drag and drop beyond the StripLayout view, if needed.
        // The first onDown is passed by the Chrome pipeline directly by GestureHandler. The
        // subsequent ones may be simulated by the DragDrop handler if the pointer goes beyond the
        // strip layout view.
        mActiveClickedTab = null;
        mLastOffsetX = 0.f;
        resetResizeTimeout(false);

        if (mNewTabButton.onDown(x, y, fromMouse, buttons)) {
            mRenderHost.requestRender();
            return;
        }

        final StripLayoutTab clickedTab = getTabAtPosition(x);
        final int index =
                clickedTab != null
                        ? TabModelUtils.getTabIndexById(mModel, clickedTab.getId())
                        : TabModel.INVALID_TAB_INDEX;
        // http://crbug.com/472186 : Needs to handle a case that index is invalid.
        // The case could happen when the current tab is touched while we're inflating the rest of
        // the tabs from disk.
        mInteractingTab =
                index != TabModel.INVALID_TAB_INDEX && index < mStripTabs.length
                        ? mStripTabs[index]
                        : null;
        boolean clickedClose = clickedTab != null && clickedTab.checkCloseHitTest(x, y);
        if (clickedClose) {
            clickedTab.setClosePressed(true, fromMouse);
            mRenderHost.requestRender();
        }

        if (!mScroller.isFinished()) {
            mScroller.forceFinished(true);
            mInteractingTab = null;
        }

        // If event is from primary button click, set flag to use during drag.
        if (MotionEventUtils.isPrimaryButton(buttons) && !clickedClose && clickedTab != null) {
            mOnDownWithButtonPrimary = true;
        }
    }

    /**
     * Called on long press touch event.
     * @param time The current time of the app in ms.
     * @param x    The x coordinate of the position of the press event.
     * @param y    The y coordinate of the position of the press event.
     */
    public void onLongPress(long time, float x, float y) {
        final StripLayoutTab clickedTab = getTabAtPosition(x);
        if (clickedTab != null) { // Vivaldi: Long press on the entire tab is supported.
            clickedTab.setClosePressed(false, false);
            mRenderHost.requestRender();
            showTabMenu(clickedTab);
            // Vivaldi: Always start reorder.
            startReorderTab(time, x, x);
        } else {
            resetResizeTimeout(false);
            startDragOrReorderTab(time, x, y, clickedTab);
        }
    }

    private void startDragOrReorderTab(long time, float x, float y, StripLayoutTab clickedTab) {
        // Allow the user to drag the selected tab out of the tab toolbar.
        if (clickedTab != null) {
            boolean res = startDragAndDropTab(clickedTab, new PointF(x, y));
            // If tab drag did not succeed, fallback to reorder within strip.
            if (!res) {
                startReorderTab(time, x, x);
            }
        } else {
            // Broadcast to start moving the window instance as the user has long pressed on the
            // open space of the tab strip.
            sendMoveWindowBroadcast(mToolbarContainerView, x, y);
        }
    }

    /**
     * Called on hover enter event.
     *
     * @param x The x coordinate of the position of the hover enter event.
     */
    public void onHoverEnter(float x, float y) {
        StripLayoutTab hoveredTab = getTabAtPosition(x);

        // Hovered into a tab on the strip.
        if (hoveredTab != null) {
            updateLastHoveredTab(hoveredTab);

            // Check whether the close button on the hovered tab is being hovered on.
            hoveredTab.setCloseHovered(hoveredTab.checkCloseHitTest(x, y));
        } else {
            // Check whether new tab button or model selector button is being hovered.
            updateCompositorButtonHoverState(x, y);
        }
        mUpdateHost.requestUpdate();
    }

    /**
     * Called on hover move event.
     *
     * @param x The x coordinate of the position of the hover move event.
     */
    public void onHoverMove(float x, float y) {
        // Check whether new tab button or model selector button is being hovered.
        updateCompositorButtonHoverState(x, y);

        StripLayoutTab hoveredTab = getTabAtPosition(x);
        // Hovered into a non-tab region within the strip.
        if (hoveredTab == null) {
            clearLastHoveredTab();
            mUpdateHost.requestUpdate();
            return;
        }

        // Hovered within the same tab that was last hovered into and close button hover state
        // remains unchanged.
        boolean isCloseHit = hoveredTab.checkCloseHitTest(x, y);
        if (hoveredTab == mLastHoveredTab && hoveredTab.isCloseHovered() == isCloseHit) {
            return;
        } else if (hoveredTab == mLastHoveredTab) {
            // Hovered within the same tab that was last hovered into, but close button hover state
            // has changed.
            hoveredTab.setCloseHovered(isCloseHit);
        } else {
            // Hovered from one tab to another tab on the strip.
            clearLastHoveredTab();
            updateLastHoveredTab(hoveredTab);
        }

        mUpdateHost.requestUpdate();
    }

    /** Called on hover exit event. */
    public void onHoverExit() {
        clearLastHoveredTab();

        // Clear tab strip button (NTB and MSB) hover state.
        clearCompositorButtonHoverStateIfNotClicked();

        mUpdateHost.requestUpdate();
    }

    /** Called in post delay task in q#onDown to clear tab hover state. */
    protected void clearTabHoverState() {
        clearLastHoveredTab();
        mUpdateHost.requestUpdate();
    }

    /** Check whether model selector button or new tab button is being hovered. */
    private void updateCompositorButtonHoverState(float x, float y) {
        // Model selector button is being hovered.
        mModelSelectorButton.setHovered(mModelSelectorButton.checkClickedOrHovered(x, y));
        // There's a delay in updating NTB's position/touch target when MSB initially appears on the
        // strip, taking over NTB's position and moving NTB closer to the tabs. Consequently, hover
        // highlights are observed on both NTB and MSB. To address this, this check is added to
        // ensure only one button can be hovered at a time.
        if (!mModelSelectorButton.isHovered()) {
            mNewTabButton.setHovered(
                    ((CompositorButton) mNewTabButton).checkClickedOrHovered(x, y));
        } else {
            mNewTabButton.setHovered(false);
        }
    }

    /** Clear button hover state */
    private void clearCompositorButtonHoverStateIfNotClicked() {
        mNewTabButton.setHovered(false);
        mModelSelectorButton.setHovered(false);
    }

    @VisibleForTesting
    void setTabHoverCardView(StripTabHoverCardView tabHoverCardView) {
        mTabHoverCardView = tabHoverCardView;
    }

    StripTabHoverCardView getTabHoverCardViewForTesting() {
        return mTabHoverCardView;
    }

    void setLastHoveredTabForTesting(StripLayoutTab tab) {
        mLastHoveredTab = tab;
    }

    StripLayoutTab getLastHoveredTab() {
        return mLastHoveredTab;
    }

    private void clearLastHoveredTab() {
        if (mLastHoveredTab == null) return;
        assert mTabHoverCardView != null : "Hover card view should not be null.";

        // Clear close button hover state.
        mLastHoveredTab.setCloseHovered(false);

        // Remove the highlight from the last hovered tab.
        updateHoveredTabAttachedState(mLastHoveredTab, false);
        mTabHoverCardView.hide();
        mLastHoveredTab = null;
    }

    @VisibleForTesting
    void updateLastHoveredTab(StripLayoutTab hoveredTab) {
        if (hoveredTab == null) return;

        // Do nothing if attempting to update the hover state of a tab while a tab strip animation
        // is running. This is to avoid applying the tab hover state during animations triggered for
        // some actions on the strip, for example, resizing the strip after tab closure, that might
        // cause the hover state to show / stick undesirably.
        if (mRunningAnimator != null && mRunningAnimator.isRunning()) return;

        // Do nothing if hovering into a drawn tab that is for example, hidden behind the model
        // selector button.
        if (isViewCompletelyHidden(hoveredTab)) return;

        mLastHoveredTab = hoveredTab;
        if (!mAnimationsDisabledForTesting) {
            CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            hoveredTab,
                            StripLayoutTab.OPACITY,
                            hoveredTab.getContainerOpacity(),
                            TAB_OPACITY_VISIBLE_FOREGROUND,
                            ANIM_HOVERED_TAB_CONTAINER_FADE_MS)
                    .start();
        } else {
            hoveredTab.setContainerOpacity(TAB_OPACITY_VISIBLE_FOREGROUND);
        }
        updateHoveredTabAttachedState(mLastHoveredTab, true);

        // Show the tab hover card.
        int hoveredTabIndex = findIndexForTab(mLastHoveredTab.getId());
        mTabHoverCardView.show(
                mModel.getTabAt(hoveredTabIndex),
                isSelectedTab(mLastHoveredTab.getId()),
                mLastHoveredTab.getDrawX(),
                mLastHoveredTab.getWidth(),
                mHeight);
    }

    private void updateHoveredTabAttachedState(StripLayoutTab tab, boolean hovered) {
        if (tab == null) return;

        // Do not update the attached state of a selected tab that is hovered on.
        if (isSelectedTab(tab.getId())) return;

        // If a tab is hovered on, detach its container.
        tab.setFolioAttached(!hovered);
        tab.setBottomMargin(
                hovered ? FOLIO_DETACHED_BOTTOM_MARGIN_DP : FOLIO_ATTACHED_BOTTOM_MARGIN_DP);
    }

    private void handleNewTabClick() {
        if (mModel == null) return;

        if (!mModel.isIncognito()) mModel.commitAllTabClosures();
        mTabCreator.launchNtp();
    }

    @Override
    public void handleCloseButtonClick(final StripLayoutTab tab, long time) {
        // Placeholder tabs are expected to have invalid tab ids.
        if (tab == null || tab.isDying() || tab.getId() == Tab.INVALID_TAB_ID) return;

        int tabId = tab.getId();
        int rootId = getTabById(tabId).getRootId();
        if (isLastTabInGroup(tabId)) {
            showDeleteGroupDialogAndProcessTabAction(
                    tabId,
                    rootId,
                    /* draggingLastTabOffStrip= */ false,
                    /* closeTab= */ true,
                    () -> handleCloseTab(tab, time));
        } else {
            handleCloseTab(tab, time);
        }
    }

    private void handleCloseTab(final StripLayoutTab tab, long time) {
        mMultiStepTabCloseAnimRunning = false;
        finishAnimationsAndPushTabUpdates();

        // When a tab is closed #resizeStripOnTabClose will run animations for the new tab offset
        // and tab x offsets. When there is only 1 tab remaining, we do not need to run those
        // animations, so #resizeTabStrip() is used instead.
        boolean runImprovedTabAnimations = mStripTabs.length > 1;

        // 1. Set the dying state of the tab.
        tab.setIsDying(true);

        // 2. Start the tab closing animator with a listener to resize/move tabs after the closure.
        AnimatorListener listener =
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (runImprovedTabAnimations) {
                            // This removes any closed tabs from the tabModel.
                            finishAnimationsAndPushTabUpdates();
                            resizeStripOnTabClose(getTabById(tab.getId()));
                        } else {
                            mMultiStepTabCloseAnimRunning = false;
                            mNewTabButtonAnimRunning = false;
                            // Resize the tabs appropriately.
                            resizeTabStrip(true, false, false);
                        }
                    }
                };
        runTabRemovalAnimation(tab, listener);

        // 3. If we're closing the selected tab, attempt to select the next expanded tab now. If
        // none exists, we'll default to the normal auto-selection behavior (i.e. selecting the
        // closest collapsed tab, or opening the GTS if none exist).
        if (getSelectedTabId() == tab.getId()) {
            int nextIndex = getNearbyExpandedTabIndex();
            if (nextIndex != TabModel.INVALID_TAB_INDEX) {
                TabModelUtils.setIndex(mModel, nextIndex);
            }
        }
    }

    private void runTabRemovalAnimation(StripLayoutTab tab, AnimatorListener listener) {
        // 1. Setup the close animation.
        List<Animator> tabClosingAnimators = new ArrayList<>();
        tabClosingAnimators.add(
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.Y_OFFSET,
                        tab.getOffsetY(),
                        tab.getHeight(),
                        ANIM_TAB_CLOSED_MS));

        // 2. Start the animation.
        mNewTabButtonAnimRunning = true;
        mMultiStepTabCloseAnimRunning = true;
        startAnimationList(tabClosingAnimators, listener);
    }

    private void resizeStripOnTabClose(Tab closedTab) {
        List<Animator> tabStripAnimators = new ArrayList<>();

        // 1. Add tabs expanding animators to expand remaining tabs to fill scrollable area.
        List<Animator> tabExpandAnimators = computeAndUpdateTabWidth(true, true, closedTab);
        if (tabExpandAnimators != null) tabStripAnimators.addAll(tabExpandAnimators);

        // 2. Calculate new mScrollOffset and idealX for tab offset animation.
        updateScrollOffsetLimits();
        computeTabInitialPositions();

        // 3. Animate the tabs sliding to their idealX.
        for (int i = 0; i < mStripViews.length; ++i) {
            final StripLayoutView view = mStripViews[i];
            if (view.getDrawX() == view.getIdealX()) {
                // Don't animate views that won't change location.
                continue;
            } else if (isViewCompletelyHidden(view) && willViewBeCompletelyHidden(view)) {
                // Don't animate views that won't be seen by the user (i.e. not currently visible
                // and won't be visible after moving) - just set the draw X immediately.
                view.setDrawX(view.getIdealX());
                continue;
            }

            CompositorAnimator drawXAnimator =
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            view,
                            StripLayoutView.X_OFFSET,
                            view.getDrawX() - view.getIdealX(),
                            0.f,
                            ANIM_TAB_DRAW_X_MS);
            tabStripAnimators.add(drawXAnimator);
        }

        // 4. Add new tab button offset animation.
        tabStripAnimators.add(getLastTabClosedNtbAnimator());

        // 5. Add animation completion listener and start animations.
        startAnimationList(
                tabStripAnimators,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mMultiStepTabCloseAnimRunning = false;
                        mNewTabButtonAnimRunning = false;
                    }
                });
    }

    @Override
    public void handleTabClick(StripLayoutTab tab) {
        if (tab == null || tab.isDying()) return;

        int newIndex = TabModelUtils.getTabIndexById(mModel, tab.getId());

        // Early return, since placeholder tabs are known to not have tab ids.
        if (newIndex == Tab.INVALID_TAB_ID) return;

        TabModelUtils.setIndex(mModel, newIndex);
    }

    /**
     * Called on click. This is called before the onUpOrCancel event.
     *
     * @param time The current time of the app in ms.
     * @param x The x coordinate of the position of the click.
     * @param y The y coordinate of the position of the click.
     * @param fromMouse Whether the event originates from a mouse.
     * @param buttons State of all buttons that were pressed when onDown was invoked.
     */
    public void click(long time, float x, float y, boolean fromMouse, int buttons) {
        resetResizeTimeout(false);

        if (mNewTabButton.click(x, y, fromMouse, buttons)) {
            RecordUserAction.record("MobileToolbarNewTab");
            mNewTabButton.handleClick(time);
            return;
        }

        final StripLayoutView clickedView = getViewAtPositionX(x, true);
        if (clickedView == null) return;
        if (clickedView instanceof StripLayoutTab clickedTab) {
            if (clickedTab.isDying()) return;
            if (clickedTab.checkCloseHitTest(x, y)
                    || (fromMouse && (buttons & MotionEvent.BUTTON_TERTIARY) != 0)) {
                RecordUserAction.record("MobileToolbarCloseTab");
                clickedTab.getCloseButton().handleClick(time);
            } else {
                RecordUserAction.record("MobileTabSwitched.TabletTabStrip");
                recordTabSwitchTimeHistogram();
                clickedTab.handleClick(time);
            }
        } else if (clickedView instanceof StripLayoutGroupTitle clickedGroupTitle) {
            clickedGroupTitle.handleClick(time);
        }
    }

    private void recordTabSwitchTimeHistogram() {
        if (mTabScrollStartTime == null || mMostRecentTabScroll == null) return;

        long endTime = SystemClock.elapsedRealtime();
        long duration = endTime - mTabScrollStartTime;
        long timeFromLastInteraction = endTime - mMostRecentTabScroll;

        // Discard sample if last scroll was over the max allowed interval.
        if (timeFromLastInteraction <= TAB_SWITCH_METRICS_MAX_ALLOWED_SCROLL_INTERVAL) {
            RecordHistogram.recordMediumTimesHistogram(
                    "Android.TabStrip.TimeToSwitchTab", duration);
        }

        mTabScrollStartTime = null;
        mMostRecentTabScroll = null;
    }

    /**
     * Called on up or cancel touch events. This is called after the click and fling event if any.
     *
     * @param time The current time of the app in ms.
     */
    public void onUpOrCancel(long time) {
        // 1. Stop any reordering that is happening.
        stopReorderMode();

        // 2. Reset state
        mInteractingTab = null;
        mReorderState = REORDER_SCROLL_NONE;
        if (mNewTabButton.onUpOrCancel() && mModel != null) {
            if (!mModel.isIncognito()) mModel.commitAllTabClosures();
            mTabCreator.launchNtp();
        }
        mIsStripScrollInProgress = false;
        mOnDownWithButtonPrimary = false;
    }

    /**
     * @return Whether or not the tabs are moving.
     */
    public boolean isAnimatingForTesting() {
        return (mRunningAnimator != null && mRunningAnimator.isRunning())
                || !mScroller.isFinished();
    }

    private void finishAnimations() {
        // Force any outstanding animations to finish. Need to recurse as some animations (like the
        // multi-step tab close animation) kick off another animation once the first ends.
        while (mRunningAnimator != null && mRunningAnimator.isRunning()) {
            mRunningAnimator.end();
        }
        mRunningAnimator = null;
    }

    @VisibleForTesting
    void startAnimationList(@Nullable List<Animator> animationList, AnimatorListener listener) {
        AnimatorSet set = new AnimatorSet();
        set.playTogether(animationList);
        if (listener != null) set.addListener(listener);

        finishAnimations();
        setAndStartRunningAnimator(set);
    }

    private void setAndStartRunningAnimator(Animator animator) {
        mRunningAnimator = animator;
        mRunningAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Clear any persisting tab hover state after tab strip animations have
                        // ended. This is to prevent the hover state from sticking after an action
                        // on the strip, including and not limited to tab closure and tab
                        // reordering.
                        clearTabHoverState();
                    }
                });
        mRunningAnimator.start();
    }

    /**
     * Finishes any outstanding animations and propagates any related changes to the
     * {@link TabModel}.
     */
    public void finishAnimationsAndPushTabUpdates() {
        if (mRunningAnimator == null) return;

        // 1. Finish animations.
        finishAnimations();

        // 2. Figure out which tabs need to be closed.
        ArrayList<StripLayoutTab> tabsToRemove = new ArrayList<StripLayoutTab>();
        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];
            if (tab.isDying()) tabsToRemove.add(tab);
        }

        if (tabsToRemove.isEmpty()) return;

        // 3. Pass the close notifications to the model if the tab isn't already closing.
        //    Do this as a post task as if more tabs are added inside commit all tab closures that
        //    is a concurrent modification exception.

        for (StripLayoutTab tab : tabsToRemove) tab.setIsClosed(true);

        // Note(david@vivaldi.com): We remove the post task here as Vivaldi needs the close
        // tab information instantly in order to update all tab strip animations correctly.
        //PostTask.postTask(
        //        TaskTraits.UI_DEFAULT,
        //        () -> {
                    for (StripLayoutTab tab : tabsToRemove) {
                        TabModelUtils.closeTabById(mModel, tab.getId(), true);
                    }

                    if (!tabsToRemove.isEmpty()) mUpdateHost.requestUpdate();
        //        });
    }

    private void updateSpinners(long time) {
        long diff = time - mLastSpinnerUpdate;
        float degrees = diff * SPINNER_DPMS;
        boolean tabsToLoad = false;
        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];
            // TODO(clholgat): Only update if the tab is visible.
            if (tab.isLoading()) {
                tab.addLoadingSpinnerRotation(degrees);
                tabsToLoad = true;
            }
        }
        mLastSpinnerUpdate = time;
        if (tabsToLoad) {
            mStripTabEventHandler.removeMessages(MESSAGE_UPDATE_SPINNER);
            mStripTabEventHandler.sendEmptyMessageDelayed(
                    MESSAGE_UPDATE_SPINNER, SPINNER_UPDATE_DELAY_MS);
        }
    }

    private void updateScrollOffsetPosition(float pos) {
        float oldScrollOffset = mScrollOffset;
        mScrollOffset = MathUtils.clamp(pos, mMinScrollOffset - mReorderExtraMinScrollOffset, 0);

        if (mInReorderMode && mScroller.isFinished()) {
            float delta =
                    MathUtils.flipSignIf(
                            oldScrollOffset - mScrollOffset, LocalizationUtils.isLayoutRtl());
            if (mReorderingForTabDrop) {
                updateReorderPositionForTabDrop(mLastReorderX);
            } else {
                updateReorderPosition(delta);
            }
        }
    }

    private void updateScrollOffset(long time) {
        if (mScroller.computeScrollOffset(time)) {
            updateScrollOffsetPosition(mScroller.getCurrX());
            mUpdateHost.requestUpdate();
        }
    }

    @VisibleForTesting
    void updateScrollOffsetLimits() {
        // 1. Compute the width of the available space for all tabs.
        float stripWidth = mWidth - mLeftMargin - mRightMargin;

        // 2. Compute the effective width of every strip view (i.e. tabs, title indicators).
        float tabsWidth = 0.f;
        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView view = mStripViews[i];
            if (view instanceof final StripLayoutTab tab) {
                if (tab.isCollapsed()) {
                    // Need to use real width here (which gets animated to effectively 0), so we
                    // don't "jump", but instead smoothly scroll when collapsing near the end of a
                    // full tab strip.
                    tabsWidth += tab.getWidth() - mTabOverlapWidth;
                } else if (!tab.isClosed() && !tab.isDraggedOffStrip()) {
                    tabsWidth += mCachedTabWidth - mTabOverlapWidth;
                }
            } else if (view instanceof StripLayoutGroupTitle groupTitle) {
                tabsWidth += (groupTitle.getWidth() - mGroupTitleOverlapWidth);
            }
        }

        if (mInReorderMode || mTabGroupMarginAnimRunning) {
            tabsWidth += mStripStartMarginForReorder;
            for (int i = 0; i < mStripTabs.length; i++) {
                final StripLayoutTab tab = mStripTabs[i];
                tabsWidth += tab.getTrailingMargin();
            }
        }

        // Note(david@vivaldi): Substract the offset here in regards to our setting. This
        // applies only when tab is shown as favicon.
        if (mShowTabsAsFavIcon) {
            tabsWidth = !showXButtonForBackgroundTabs()
                    ? tabsWidth - ((mStripTabs.length - 1) * NO_X_BUTTON_OFFSET_DP)
                    : tabsWidth;
        } // End Vivaldi

        // 3. Correct fencepost error in tabswidth;
        tabsWidth = tabsWidth + mTabOverlapWidth;

        // 4. Calculate the minimum scroll offset.  Round > -EPSILON to 0.
        mMinScrollOffset = Math.min(0.f, stripWidth - tabsWidth);
        if (mMinScrollOffset > -EPSILON) mMinScrollOffset = 0.f;

        // 5. Clamp mScrollOffset to make sure it's in the valid range.
        updateScrollOffsetPosition(mScrollOffset);
    }

    private List<Animator> computeAndUpdateTabOrders(boolean delayResize, boolean deferAnimations) {
        // Note(david@vivaldi.com): Always update.
        if (ChromeApplicationImpl.isVivaldi()) {
            computeAndUpdateTabOrders();
            return null;
        }

        final int count = mModel.getCount();
        StripLayoutTab[] tabs = new StripLayoutTab[count];

        for (int i = 0; i < count; i++) {
            final Tab tab = mModel.getTabAt(i);
            final int id = tab.getId();
            final StripLayoutTab oldTab = findTabById(id);
            tabs[i] = oldTab != null ? oldTab : createStripTab(id);
            setAccessibilityDescription(tabs[i], tab);
        }

        int oldTabsLength = mStripTabs.length;
        mStripTabs = tabs;
        rebuildStripViews();

        List<Animator> animationList = null;
        // If multi-step animation is running, the resize will be handled elsewhere.
        if (mStripTabs.length != oldTabsLength && !mMultiStepTabCloseAnimRunning) {
            computeTabInitialPositions();
            animationList = resizeTabStrip(true, delayResize, deferAnimations);
        }

        updateVisualTabOrdering();
        return animationList;
    }

    private String buildGroupAccessibilityDescription(@NonNull StripLayoutGroupTitle groupTitle) {
        final String contentDescriptionSeparator = " - ";
        Resources res = mContext.getResources();
        StringBuilder builder = new StringBuilder();

        String groupDescription = groupTitle.getTitle();
        if (TextUtils.isEmpty(groupDescription)) {
            // TODO(crbug.com/349696415): Investigate if we can indeed remove this now that we
            // default to showing "N tabs" for unnamed groups.
            // "Unnamed group"
            int titleRes = R.string.accessibility_tabstrip_group_identifier_unnamed;
            groupDescription = res.getString(titleRes);
        }
        builder.append(groupDescription);

        List<Tab> relatedTabs =
                mTabGroupModelFilter.getRelatedTabListForRootId(groupTitle.getRootId());
        int relatedTabsCount = relatedTabs.size();
        if (relatedTabsCount > 0) {
            // " - "
            builder.append(contentDescriptionSeparator);

            String firstTitle = relatedTabs.get(0).getTitle();
            String tabsDescription;
            if (relatedTabsCount == 1) {
                // <title>
                tabsDescription = firstTitle;
            } else {
                // <title> and <num> other tabs
                int descriptionRes = R.string.accessibility_tabstrip_group_identifier_multiple_tabs;
                tabsDescription = res.getString(descriptionRes, firstTitle, relatedTabsCount - 1);
            }
            builder.append(tabsDescription);
        }

        return builder.toString();
    }

    private void updateGroupAccessibilityDescription(StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null) return;
        groupTitle.setAccessibilityDescription(buildGroupAccessibilityDescription(groupTitle));
    }

    @Override
    public void releaseResourcesForGroupTitle(int rootId) {
        mLayerTitleCache.removeGroupTitle(rootId);
    }

    private AnimatorListener getCollapseAnimatorListener(
            StripLayoutGroupTitle collapsedGroupTitle) {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (collapsedGroupTitle != null) collapsedGroupTitle.setBottomIndicatorWidth(0.f);
            }
        };
    }

    private ArrayList<StripLayoutTab> getGroupedTabs(int rootId) {
        ArrayList<StripLayoutTab> groupedTabs = new ArrayList<>();
        for (int i = 0; i < mStripTabs.length; ++i) {
            final StripLayoutTab stripTab = mStripTabs[i];
            final Tab tab = getTabById(stripTab.getId());
            if (tab != null && tab.getRootId() == rootId) groupedTabs.add(stripTab);
        }
        return groupedTabs;
    }

    void collapseTabGroupForTesting(StripLayoutGroupTitle groupTitle, boolean isCollapsed) {
        updateTabGroupCollapsed(groupTitle, isCollapsed, true);
    }

    @Override
    public void handleGroupTitleClick(StripLayoutGroupTitle groupTitle) {
        if (!ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) return;
        if (groupTitle == null) return;

        int rootId = groupTitle.getRootId();
        boolean isCollapsed = mTabGroupModelFilter.getTabGroupCollapsed(rootId);
        assert isCollapsed == groupTitle.isCollapsed();

        mTabGroupModelFilter.setTabGroupCollapsed(rootId, !isCollapsed);
        RecordHistogram.recordBooleanHistogram("Android.TabStrip.TabGroupCollapsed", !isCollapsed);
    }

    private Animator updateTabCollapsed(StripLayoutTab tab, boolean isCollapsed, boolean animate) {
        tab.setCollapsed(isCollapsed);

        // The tab expand will be handled when the tab strip resizes, since we'll need to first
        // update mCachedTabWidth.
        if (!isCollapsed) return null;

        // Set to the tab overlap width so the tab effectively takes up no space. If we instead
        // animate to 0, the following tabs will unexpectedly be shifted as this tab takes up
        // "negative" space.
        if (!animate) {
            tab.setWidth(mTabOverlapWidth);
            return null;
        }

        return CompositorAnimator.ofFloatProperty(
                mUpdateHost.getAnimationHandler(),
                tab,
                StripLayoutTab.WIDTH,
                tab.getWidth(),
                mTabOverlapWidth,
                ANIM_TAB_RESIZE_MS);
    }

    private void updateTabGroupCollapsed(
            StripLayoutGroupTitle groupTitle, boolean isCollapsed, boolean animate) {
        if (!ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) return;
        if (groupTitle.isCollapsed() == isCollapsed) return;

        List<Animator> collapseAnimationList = null;
        if (animate && !mAnimationsDisabledForTesting) collapseAnimationList = new ArrayList<>();

        finishAnimations();
        groupTitle.setCollapsed(isCollapsed);
        for (StripLayoutTab tab : getGroupedTabs(groupTitle.getRootId())) {
            if (collapseAnimationList != null) {
                Animator animator = updateTabCollapsed(tab, isCollapsed, true);
                if (animator != null) collapseAnimationList.add(animator);
            } else {
                updateTabCollapsed(tab, isCollapsed, false);
            }
        }

        // Similar to bottom indicator collapse animation, the expansion animation should also begin
        // from the padded width of the group title.
        if (!isCollapsed) {
            groupTitle.setBottomIndicatorWidth(groupTitle.getPaddedWidth());
        }

        List<Animator> resizeAnimationList = resizeTabStrip(animate, false, animate);
        if (collapseAnimationList != null) {
            StripLayoutGroupTitle collapsedGroupTitle = null;
            if (isCollapsed) {
                // Animate bottom indicator down to the group title padded width when collapsing,
                // and then hide the remaining portion under the group title.
                collapsedGroupTitle = groupTitle;
                collapseAnimationList.add(
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                groupTitle,
                                StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                                groupTitle.getBottomIndicatorWidth(),
                                groupTitle.getPaddedWidth(),
                                ANIM_TAB_RESIZE_MS));
            }

            if (resizeAnimationList != null) collapseAnimationList.addAll(resizeAnimationList);
            startAnimationList(
                    collapseAnimationList, getCollapseAnimatorListener(collapsedGroupTitle));
        } else {
            if (isCollapsed) {
                groupTitle.setBottomIndicatorWidth(0.f);
            }
        }

        // Select an adjacent expanded tab if the current selected tab is being collapsed, If all
        // tabs are collapsed, open a ntp.
        if (isCollapsed) {
            Tab selectedTab = getTabById(getSelectedTabId());
            if (selectedTab != null && selectedTab.getRootId() == groupTitle.getRootId()) {
                int nextIndex = getNearbyExpandedTabIndex();
                if (nextIndex != TabModel.INVALID_TAB_INDEX) {
                    TabModelUtils.setIndex(mModel, nextIndex);
                } else {
                    mTabCreator.launchNtp();
                }
            }
        }
    }

    /**
     * Handles edge cases such as merging a selected tab into a collapsed tab group through GTS,
     * followed by exiting GTS with a back gesture. The tab group containing the selected tab should
     * be expanded.
     */
    protected void expandGroupOnGtsExit() {
        StripLayoutTab selectedTab = getSelectedStripTab();
        if (selectedTab == null) {
            return;
        }
        Tab tab = getTabById(selectedTab.getId());
        if (tab != null && mTabGroupModelFilter.getTabGroupCollapsed(tab.getRootId())) {
            mTabGroupModelFilter.deleteTabGroupCollapsed(tab.getRootId());
        }
    }

    /**
     * @return The index of the nearby expanded tab to the selected tab. Prioritizes tabs before the
     *     selected tab. If none are found, return an invalid index.
     */
    private int getNearbyExpandedTabIndex() {
        int index = getSelectedStripTabIndex();

        for (int i = index - 1; i >= 0; --i) {
            if (!mStripTabs[i].isCollapsed()) return i;
        }

        for (int i = index + 1; i < mStripTabs.length; ++i) {
            if (!mStripTabs[i].isCollapsed()) return i;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Called to refresh the group title bitmap when it may have changed (text, color, etc.).
     *
     * @param groupTitle The group title to refresh the bitmap for.
     */
    private void updateGroupTitleBitmapIfNeeded(@NonNull StripLayoutGroupTitle groupTitle) {
        if (groupTitle.isVisible()) {
            mLayerTitleCache.getUpdatedGroupTitle(
                    groupTitle.getRootId(), groupTitle.getTitle(), mIncognito);
            mRenderHost.requestRender();
        }
    }

    private void updateGroupTitleTint(StripLayoutGroupTitle groupTitle) {
        int colorId = mTabGroupModelFilter.getTabGroupColor(groupTitle.getRootId());
        // If the color is invalid, temporarily assign a default placeholder color.
        if (colorId == TabGroupColorUtils.INVALID_COLOR_ID) colorId = TabGroupColorId.GREY;
        updateGroupTitleTint(groupTitle, colorId);
    }

    private void updateGroupTitleTint(
            StripLayoutGroupTitle groupTitle, @TabGroupColorId int newColor) {
        if (groupTitle == null) return;

        groupTitle.updateTint(
                ColorPickerUtils.getTabGroupColorPickerItemColor(mContext, newColor, mIncognito));
        updateGroupTitleBitmapIfNeeded(groupTitle);
    }

    /**
     * @param rootId The root ID of the relevant tab group.
     * @param titleText The tab group's title text, if any. Null otherwise.
     * @return The provided title text if it isn't empty. Otherwise, returns the default title.
     */
    private String getDefaultGroupTitleTextIfEmpty(int rootId, @Nullable String titleText) {
        if (TextUtils.isEmpty(titleText)) {
            int numTabs = mTabGroupModelFilter.getRelatedTabCountForRootId(rootId);
            titleText = TabGroupTitleEditor.getDefaultTitle(mContext, numTabs);
        }
        return titleText;
    }

    @VisibleForTesting
    void updateGroupTitleText(int rootId) {
        updateGroupTitleText(findGroupTitle(rootId));
    }

    private void updateGroupTitleText(StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null) return;
        updateGroupTitleText(
                groupTitle, mTabGroupModelFilter.getTabGroupTitle(groupTitle.getRootId()));
    }

    /**
     * Sets a non-empty title text for the given group indicator. Also updates the title text
     * bitmap, accessibility description, and tab/indicator sizes if necessary.
     *
     * @param groupTitle The {@link StripLayoutGroupTitle} that we're update the title text for.
     * @param titleText The title text to apply. If empty, use a default title text.
     */
    private void updateGroupTitleText(StripLayoutGroupTitle groupTitle, String titleText) {
        assert groupTitle != null;

        // 1. Update indicator text and width.
        titleText = getDefaultGroupTitleTextIfEmpty(groupTitle.getRootId(), titleText);
        int widthPx = mLayerTitleCache.getGroupTitleWidth(mIncognito, titleText);
        float widthDp = widthPx / mContext.getResources().getDisplayMetrics().density;
        float oldWidth = groupTitle.getWidth();
        groupTitle.updateTitle(titleText, widthDp);
        updateGroupAccessibilityDescription(groupTitle);

        // 2. Update title text bitmap if needed.
        updateGroupTitleBitmapIfNeeded(groupTitle);

        // 3. Handle indicator size change if needed.
        if (groupTitle.getWidth() != oldWidth) {
            if (groupTitle.isVisible()) {
                // If on-screen, this may result in the ideal tab width changing.
                resizeTabStrip(false, false, false);
            } else {
                // If off-screen, request an update so we re-calculate tab initial positions and the
                // minimum scroll offset.
                mUpdateHost.requestUpdate();
            }
        }
    }

    private StripLayoutGroupTitle findGroupTitle(int rootId) {
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            final StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            if (groupTitle.getRootId() == rootId) return groupTitle;
        }
        return null;
    }

    private StripLayoutGroupTitle findOrCreateGroupTitle(int rootId) {
        StripLayoutGroupTitle groupTitle = findGroupTitle(rootId);
        return groupTitle == null ? createGroupTitle(rootId) : groupTitle;
    }

    private StripLayoutGroupTitle createGroupTitle(int rootId) {
        // Delay setting the collapsed state, since mStripViews may not yet be up to date.
        StripLayoutGroupTitle groupTitle =
                new StripLayoutGroupTitle(/* delegate= */ this, mIncognito, rootId);
        pushPropertiesToGroupTitle(groupTitle);
        // Must pass in the group title instead of rootId, since the StripLayoutGroupTitle has not
        // been added to mStripViews yet.
        updateGroupTitleText(groupTitle);
        updateGroupTitleTint(groupTitle);

        return groupTitle;
    }

    private void pushPropertiesToGroupTitle(StripLayoutGroupTitle groupTitle) {
        groupTitle.setDrawY(0);
        groupTitle.setHeight(mHeight);
    }

    @VisibleForTesting
    void rebuildStripViews() {
        if (mTabGroupModelFilter != null
                && mTabStateInitialized
                && ChromeFeatureList.sTabStripGroupIndicators.isEnabled()) {
            copyTabsWithGroupTitles();
            buildBottomIndicator();
        } else {
            copyTabs();
        }
        mUpdateHost.requestUpdate();
    }

    private int getTabGroupCount() {
        Set<Integer> groupRootIds = new HashSet<>();

        for (int i = 0; i < mStripTabs.length; ++i) {
            final StripLayoutTab stripTab = mStripTabs[i];
            final Tab tab = getTabById(stripTab.getId());
            if (mTabGroupModelFilter.isTabInTabGroup(tab)
                    && !groupRootIds.contains(tab.getRootId())) {
                groupRootIds.add(tab.getRootId());
            }
        }

        return groupRootIds.size();
    }

    private void buildBottomIndicator() {
        if (mStripTabs.length == 0 || mTabResizeAnimRunning) {
            return;
        }
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            if (groupTitle == null
                    || groupTitle.isCollapsed()
                    || groupTitle.getRootId() == mTabGroupIdToHide) {
                continue;
            }

            // Calculate the bottom indicator width.
            float bottomIndicatorWidth =
                    calculateBottomIndicatorWidth(groupTitle, getNumOfTabsInGroup(groupTitle));

            // Update the bottom indicator width.
            if (groupTitle.getBottomIndicatorWidth() != bottomIndicatorWidth) {
                groupTitle.setBottomIndicatorWidth(bottomIndicatorWidth);
            }
        }
    }

    /**
     * @param groupTitle The tab group title indicator {@link StripLayoutGroupTitle}.
     * @param numOfTabsInGroup Number of tabs in the tab group.
     * @return The total width of the group title and the number of tabs associated with it.
     */
    private float calculateBottomIndicatorWidth(
            StripLayoutGroupTitle groupTitle, int numOfTabsInGroup) {
        if (groupTitle == null || groupTitle.isCollapsed() || numOfTabsInGroup == 0) {
            return 0.f;
        }
        float tabWidth = mCachedTabWidth - mTabOverlapWidth;
        float totalTabWidth = tabWidth * numOfTabsInGroup - TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;
        float bottomIndicatorWidth = groupTitle.getWidth() + totalTabWidth;

        return bottomIndicatorWidth;
    }

    public int getNumOfTabsInGroup(StripLayoutGroupTitle stripLayoutGroupTitle) {
        if (stripLayoutGroupTitle == null) {
            return 0;
        }
        return mTabGroupModelFilter.getRelatedTabCountForRootId(stripLayoutGroupTitle.getRootId());
    }

    protected boolean isLastTabInGroup(int tabId) {
        Tab tab = getTabById(tabId);
        if (tab == null) {
            return false;
        }
        return mTabGroupModelFilter.isTabInTabGroup(tab)
                && mTabGroupModelFilter.getRelatedTabCountForRootId(tab.getRootId()) == 1;
    }

    private void copyTabsWithGroupTitles() {
        if (mStripTabs.length == 0) return;

        int numGroups = getTabGroupCount();

        // If we have tab group to hide due to running tab group delete dialog, then skip the tab
        // group when rebuilding StripViews.
        if (mTabGroupIdToHide != Tab.INVALID_TAB_ID && numGroups > 0) {
            numGroups -= 1;
        }

        int groupTitleIndex = 0;
        StripLayoutGroupTitle[] groupTitles = new StripLayoutGroupTitle[numGroups];

        int numViews = mStripTabs.length + numGroups;
        if (numViews != mStripViews.length) {
            mStripViews = new StripLayoutView[numViews];
        }

        int viewIndex = 0;
        // First view will be tab group title if first tab is grouped.
        Tab firstTab = getTabById(mStripTabs[0].getId());
        if (mTabGroupModelFilter.isTabInTabGroup(firstTab)) {
            int rootId = firstTab.getRootId();
            StripLayoutGroupTitle groupTitle = findOrCreateGroupTitle(rootId);
            if (rootId != mTabGroupIdToHide) {
                if (firstTab.getLaunchType() == TabLaunchType.FROM_SYNC_BACKGROUND) {
                    mLastSyncedGroupId = rootId;
                }
                groupTitles[groupTitleIndex++] = groupTitle;
                mStripViews[viewIndex++] = groupTitle;
            }
        }
        // Copy the StripLayoutTabs and create group titles where needed.
        for (int i = 0; i < mStripTabs.length - 1; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            mStripViews[viewIndex++] = stripTab;

            Tab currTab = getTabById(stripTab.getId());
            Tab nextTab = getTabById(mStripTabs[i + 1].getId());
            int nextRootId = nextTab.getRootId();
            boolean nextTabInGroup = mTabGroupModelFilter.isTabInTabGroup(nextTab);
            boolean areRelatedTabs = currTab.getRootId() == nextRootId;
            if (nextTabInGroup && !areRelatedTabs) {
                StripLayoutGroupTitle groupTitle = findOrCreateGroupTitle(nextRootId);
                if (nextRootId != mTabGroupIdToHide) {
                    if (nextTab.getLaunchType() == TabLaunchType.FROM_SYNC_BACKGROUND) {
                        mLastSyncedGroupId = nextRootId;
                    }
                    groupTitles[groupTitleIndex++] = groupTitle;
                    mStripViews[viewIndex++] = groupTitle;
                }
            }
        }
        // Final view will be the last tab.
        assert viewIndex == mStripViews.length - 1 : "Did not find all tab groups.";
        mStripViews[viewIndex] = mStripTabs[mStripTabs.length - 1];

        int oldGroupCount = mStripGroupTitles.length;
        mStripGroupTitles = groupTitles;
        if (mStripGroupTitles.length != oldGroupCount) {
            for (int i = 0; i < mStripGroupTitles.length; ++i) {
                final StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
                boolean isCollapsed =
                        mTabGroupModelFilter.getTabGroupCollapsed(groupTitle.getRootId());
                updateTabGroupCollapsed(groupTitle, isCollapsed, false);
            }
            resizeTabStrip(true, false, false);
        }
    }

    private void copyTabs() {
        int numViews = mStripTabs.length;
        if (numViews != mStripViews.length) {
            mStripViews = new StripLayoutView[numViews];
        }
        for (int i = 0; i < mStripViews.length; i++) {
            mStripViews[i] = mStripTabs[i];
        }
    }

    private List<Animator> resizeTabStrip(boolean animate, boolean delay, boolean deferAnimations) {
        List<Animator> animationList = null;

        if (delay) {
            resetResizeTimeout(true);
        } else {
            animationList = computeAndUpdateTabWidth(animate, deferAnimations, null);
        }

        return animationList;
    }

    private void updateVisualTabOrdering() {
        // Note(david@vivaldi.com): Don't update while tabs are restoring.
        if (mIsRestoreInProgress) return;

        if (mStripTabs.length != mStripTabsVisuallyOrdered.length) {
            mStripTabsVisuallyOrdered = new StripLayoutTab[mStripTabs.length];
        }

        mStripStacker.createVisualOrdering(
                getSelectedStripTabIndex(), mStripTabs, mStripTabsVisuallyOrdered);
    }

    private StripLayoutTab createPlaceholderStripTab() {
        StripLayoutTab tab =
                new StripLayoutTab(
                        mContext,
                        Tab.INVALID_TAB_ID,
                        this,
                        mTabLoadTrackerHost,
                        mUpdateHost,
                        mIncognito);

        tab.setIsPlaceholder(true);
        tab.setContainerOpacity(TAB_OPACITY_VISIBLE_FOREGROUND);

        // TODO(crbug.com/40942588): Added placeholder a11y descriptions to prevent crash due
        //  to invalid a11y node. Replace with official strings when available.
        String description = "Placeholder Tab";
        String title = "Placeholder";
        tab.setAccessibilityDescription(description, title, ResourcesCompat.ID_NULL);

        tab.setRenderHost(mRenderHost); // Vivaldi

        pushPropertiesToTab(tab);

        return tab;
    }

    @VisibleForTesting
    StripLayoutTab createStripTab(int id) {
        // TODO: Cache these
        StripLayoutTab tab =
                new StripLayoutTab(
                        mContext, id, this, mTabLoadTrackerHost, mUpdateHost, mIncognito);

        tab.setRenderHost(mRenderHost); // Vivaldi

        if (isSelectedTab(id)) {
            tab.setContainerOpacity(TAB_OPACITY_VISIBLE_FOREGROUND);
        }

        pushPropertiesToTab(tab);

        return tab;
    }

    private void pushPropertiesToPlaceholder(StripLayoutTab placeholderTab, Tab tab) {
        placeholderTab.setId(tab.getId());
        placeholderTab.setIsPlaceholder(false);
        placeholderTab.setContainerOpacity(TAB_OPACITY_HIDDEN);

        setAccessibilityDescription(placeholderTab, tab);
    }

    private void pushPropertiesToTab(StripLayoutTab tab) {
        // The close button is visible by default. If it should be hidden on tab creation, do not
        // animate the fade-out. See (https://crbug.com/1342654).
        boolean shouldShowCloseButton = mCachedTabWidth >= TAB_WIDTH_MEDIUM;
        tab.setCanShowCloseButton(shouldShowCloseButton, false);
        tab.setHeight(mHeight);
    }

    /**
     * @param id The Tab id.
     * @return The StripLayoutTab that corresponds to that tabid.
     */
    @VisibleForTesting
    public @Nullable StripLayoutTab findTabById(int id) {
        if (mStripTabs == null) return null;
        for (int i = 0; i < mStripTabs.length; i++) {
            if (mStripTabs[i].getId() == id) return mStripTabs[i];
        }
        return null;
    }

    private int findIndexForTab(int id) {
        if (mStripTabs == null || id == Tab.INVALID_TAB_ID) return TabModel.INVALID_TAB_INDEX;
        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            if (stripTab.getId() == id) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    private int findStripViewIndexForStripTab(int curIndexInStripTab) {
        if (curIndexInStripTab == TabModel.INVALID_TAB_INDEX) {
            return TabModel.INVALID_TAB_INDEX;
        }
        assert curIndexInStripTab < mStripTabs.length;
        StripLayoutTab curTab = mStripTabs[curIndexInStripTab];
        if (mStripViews == null || curTab == null) return TabModel.INVALID_TAB_INDEX;
        for (int i = 0; i < mStripViews.length; i++) {
            if (mStripViews[i] instanceof StripLayoutTab tab && curTab == tab) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    private int getNumLiveTabs() {
        int numLiveTabs = 0;

        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab tab = mStripTabs[i];
            if (!tab.isClosed() && !tab.isDraggedOffStrip() && !tab.isCollapsed()) numLiveTabs++;
        }

        return numLiveTabs;
    }

    /**
     * Computes and updates the tab width when resizing the tab strip.
     *
     * @param animate Whether to animate the update.
     * @param deferAnimations Whether to defer animations.
     * @param closedTab The tab that is closing. This value should be non-null, if the resize is
     *     caused by tab closing.
     * @return A list of animators for the tab width update.
     */
    private List<Animator> computeAndUpdateTabWidth(
            boolean animate, boolean deferAnimations, Tab closedTab) {
        // Skip updating the tab width when the tab strip width is unavailable.
        if (mWidth == 0) {
            return null;
        }

        // Remove any queued resize messages.
        mStripTabEventHandler.removeMessages(MESSAGE_RESIZE);

        int numTabs = Math.max(getNumLiveTabs(), 1);

        // 1. Compute the width of the available space for all tabs.
        float stripWidth = mWidth - mLeftMargin - mRightMargin;
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            final StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            stripWidth -= (groupTitle.getWidth() - mGroupTitleOverlapWidth);
        }

        // 2. Compute additional width we gain from overlapping the tabs.
        float overlapWidth = mTabOverlapWidth * (numTabs - 1);

        // 3. Calculate the optimal tab width.
        float optimalTabWidth = (stripWidth + overlapWidth) / numTabs;

        // 4. Calculate the realistic tab width.
        // Note(david@vivaldi.com): Read user defined minimum tab width.
        int mMinTabWidth = getMinTabWidth();
        mCachedTabWidth = MathUtils.clamp(optimalTabWidth, mMinTabWidth, mMaxTabWidth);
        mHalfTabWidth = (mCachedTabWidth - mTabOverlapWidth) * REORDER_OVERLAP_SWITCH_PERCENTAGE;

        // 5. Prepare animations and propagate width to all tabs.
        finishAnimationsAndPushTabUpdates();
        ArrayList<Animator> resizeAnimationList = null;
        // Note(david@vivaldi.com): It can happen that |mWidth| is zero in some cases, in particular
        // when changing themes or changing any others UI related settings. Make sure we don't
        // create |resizeAnimationList| in this case. This will fix VAB-2985.
        if (mWidth > 0)
        if (animate && !mAnimationsDisabledForTesting) resizeAnimationList = new ArrayList<>();

        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];

            // Note(david@vivaldi.com): Calculate the correct tab width here.
            setShowTabAsFavIcon();
            mCachedTabWidth =
                    mShowTabsAsFavIcon ? TabWidthPreference.MIN_TAB_WIDTH_DP : mCachedTabWidth;
            float newWidth = mCachedTabWidth;
            tab.setTabModelSelector(mTabModelSelector);
            if (mShowTabsAsFavIcon && !tab.isCloseButtonVisible())
                newWidth -= NO_X_BUTTON_OFFSET_DP;

            if (tab.isClosed()) tab.setWidth(mTabOverlapWidth);
            if (tab.isDying() || tab.isCollapsed()) continue;
            if (resizeAnimationList != null) {
                if (mCachedTabWidth > 0f && tab.getWidth() == mCachedTabWidth) {
                    // No need to create an animator to animate to the width we're already at.
                    continue;
                }
                CompositorAnimator animator =
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                tab,
                                StripLayoutTab.WIDTH,
                                tab.getWidth(),
                                newWidth, // Vivaldi: Apply new width.
                                ANIM_TAB_RESIZE_MS);
                resizeAnimationList.add(animator);
            } else {
                mStripTabs[i].setWidth(newWidth); // Vivaldi: Apply new width.
            }
        }

        // Return early if there is no animation to run.
        if (resizeAnimationList == null) {
            buildBottomIndicator();
            mUpdateHost.requestUpdate();
            return null;
        }

        // 6. Animate bottom indicator when tab width change.
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            if (groupTitle == null) {
                continue;
            }
            if (groupTitle.isCollapsed()) {
                continue;
            }
            float bottomIndicatorStartWidth = groupTitle.getBottomIndicatorWidth();
            float bottomIndicatorEndWidth;

            // When a grouped tab is closed, the bottom indicator end width needs to subtract the
            // width of the closed tab.
            if (closedTab != null && closedTab.getRootId() == groupTitle.getRootId()) {
                bottomIndicatorEndWidth =
                        calculateBottomIndicatorWidth(
                                groupTitle, getNumOfTabsInGroup(groupTitle) - 1);
            } else {
                bottomIndicatorEndWidth =
                        calculateBottomIndicatorWidth(groupTitle, getNumOfTabsInGroup(groupTitle));
            }

            if (bottomIndicatorEndWidth > 0f
                    && bottomIndicatorStartWidth == bottomIndicatorEndWidth) {
                // No need to create an animator to animate to the width we're already at.
                continue;
            }

            resizeAnimationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            groupTitle,
                            StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                            bottomIndicatorStartWidth,
                            bottomIndicatorEndWidth,
                            ANIM_TAB_RESIZE_MS));
        }

        if (deferAnimations) return resizeAnimationList;
        startAnimationList(resizeAnimationList, getTabResizeAnimatorListener());

        return null;
    }

    private AnimatorListener getTabResizeAnimatorListener() {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mTabResizeAnimRunning = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mTabResizeAnimRunning = false;
            }
        };
    }

    private void updateStrip() {
        // Note(david@vivaldi.com: We don't update when there is any scrolling.
        if (ChromeApplicationImpl.isVivaldi()) {
            BrowserControlsManager controlsManagerSupplier = null;
            if (mTabModelSelector.getCurrentTab() != null)
                controlsManagerSupplier = BrowserControlsManagerSupplier.getValueOrNullFrom(
                        mTabModelSelector.getCurrentTab().getWindowAndroid());
            if (controlsManagerSupplier != null
                    && controlsManagerSupplier.getBrowserControlHiddenRatio() != 0)
                return;
            computeAndUpdateTabOrders(false,false);
            // Check if we need to show the tab stack bar in the user interface.
            ChromeActivity activity = (ChromeActivity) mContext;
            if (activity != null && activity.getToolbarManager() != null)
                ((VivaldiTopToolbarCoordinator) activity.getToolbarManager().getToolbar())
                        .updateTabStackVisibility();
        } else
        // TODO(dtrainor): Remove this once tabCreated() is refactored to be called even from
        // restore.
        if (mTabStateInitialized
                && (mStripTabs == null || mModel.getCount() != mStripTabs.length)) {
            computeAndUpdateTabOrders(false, false);
        }

        // 1. Update the scroll offset limits
        updateScrollOffsetLimits();

        // 2. Calculate the ideal tab positions
        computeTabInitialPositions();

        // 3. Calculate the tab stacking and ensure that tabs are sized correctly.
        mStripStacker.setViewOffsets(
                mStripViews, mMultiStepTabCloseAnimRunning, mGroupTitleSliding, mCachedTabWidth);

        // 4. Calculate which tabs are visible.
        float stripWidth = getVisibleRightBound() - getVisibleLeftBound();
        mStripStacker.performOcclusionPass(mStripViews, getVisibleLeftBound(), stripWidth);

        // 5. Create render list.
        createRenderList();

        // 6. Figure out where to put the new tab button. If a tab is being closed, the new tab
        // button position will be updated with the tab resize and drawX animations.
        if (!mNewTabButtonAnimRunning) updateNewTabButtonState();

        // 7. Invalidate the accessibility provider in case the visible virtual views have changed.
        mRenderHost.invalidateAccessibilityProvider();

        // 8. Hide close buttons if tab width gets lower than 156dp.
        updateCloseButtons();

        // 9. Show dividers between inactive tabs.
        updateTabContainersAndDividers();

        // 10. Update the touchable rect.
        updateTouchableRect();
    }

    private float getTabPositionStart() {
        // Shift all of the tabs over by the the left margin because we're
        // no longer base lined at 0
        if (!LocalizationUtils.isLayoutRtl()) {
            return mScrollOffset + mLeftMargin + mStripStartMarginForReorder;
        } else {
            return mWidth
                    - mCachedTabWidth
                    - mScrollOffset
                    - mRightMargin
                    - mStripStartMarginForReorder;
        }
    }

    private void computeTabInitialPositions() {
        float tabPosition = getTabPositionStart();
        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView view = mStripViews[i];

            float delta;
            if (view instanceof StripLayoutTab tab) {
                if (tab.isClosed()) continue;
                // idealX represents where a tab should be placed in the tab strip.
                view.setIdealX(tabPosition);
                delta =
                        tab.isDying()
                                ? mCachedTabWidth - mTabOverlapWidth
                                : (tab.getWidth() - mTabOverlapWidth) * tab.getWidthWeight();
                if (mInReorderMode || mTabGroupMarginAnimRunning) {
                    delta += tab.getTrailingMargin();
                }
            } else {
                // Offset to "undo" the tab overlap width as that doesn't apply to non-tab views.
                // Also applies the desired overlap with the previous tab.
                float drawXOffset = mGroupTitleDrawXOffset;
                // Adjust for RTL.
                if (LocalizationUtils.isLayoutRtl()) {
                    drawXOffset = mCachedTabWidth - view.getWidth() - drawXOffset;
                }

                if (!mGroupTitleSliding) {
                    view.setIdealX(tabPosition + drawXOffset);
                }
                delta = view.getWidth() - mGroupTitleOverlapWidth;
            }

            delta = MathUtils.flipSignIf(delta, LocalizationUtils.isLayoutRtl());
            tabPosition += delta;
        }
    }

    private int getVisibleViewCount(StripLayoutView[] views) {
        int renderCount = 0;
        for (int i = 0; i < views.length; ++i) {
            if (views[i].isVisible()) renderCount++;
        }
        return renderCount;
    }

    private void populateVisibleViews(StripLayoutView[] allViews, StripLayoutView[] viewsToRender) {
        int renderIndex = 0;
        for (int i = 0; i < allViews.length; ++i) {
            final StripLayoutView view = allViews[i];
            if (view.isVisible()) viewsToRender[renderIndex++] = view;
        }
    }

    private void createRenderList() {
        // 1. Figure out how many tabs will need to be rendered.
        int tabRenderCount = getVisibleViewCount(mStripTabsVisuallyOrdered);
        int groupTitleRenderCount = getVisibleViewCount(mStripGroupTitles);

        // 2. Reallocate the render list if necessary.
        if (mStripTabsToRender.length != tabRenderCount) {
            mStripTabsToRender = new StripLayoutTab[tabRenderCount];
        }
        if (mStripGroupTitlesToRender.length != groupTitleRenderCount) {
            mStripGroupTitlesToRender = new StripLayoutGroupTitle[groupTitleRenderCount];
        }

        // 3. Populate it with the visible tabs.
        populateVisibleViews(mStripTabsVisuallyOrdered, mStripTabsToRender);
        populateVisibleViews(mStripGroupTitles, mStripGroupTitlesToRender);

        int renderIndex = 0;
        for (int i = 0; i < mStripTabsVisuallyOrdered.length; ++i) {
            // Note(david@vivaldi.com): Pass required parameters to the tab.
            mStripTabsVisuallyOrdered[i].setIsStackStrip(mIsStackStrip);
            mStripTabsVisuallyOrdered[i].setTabModelSelector(mTabModelSelector);
            if (mStripTabsVisuallyOrdered[i].isVisible()) {
                mStripTabsToRender[renderIndex++] = mStripTabsVisuallyOrdered[i];
            }
        }
    }

    private float adjustNewTabButtonOffsetIfFull(float offset) {
        if (!isTabStripFull()) {
            // Move NTB close to tabs by 4 dp when tab strip is not full.
            boolean isLtr = !LocalizationUtils.isLayoutRtl();
            offset += MathUtils.flipSignIf(NEW_TAB_BUTTON_X_OFFSET_TOWARDS_TABS, isLtr);
        }
        return offset;
    }

    private CompositorAnimator getLastTabClosedNtbAnimator() {
        // TODO(crbug.com/338332428): Unify with the stacker methods.
        float viewsWidth =
                getNumLiveTabs() * (mCachedTabWidth - mTabOverlapWidth) + mTabOverlapWidth;
        for (int i = 0; i < mStripViews.length; ++i) {
            final StripLayoutView view = mStripViews[i];
            if (!(view instanceof StripLayoutTab)) viewsWidth += view.getWidth();
        }

        boolean rtl = LocalizationUtils.isLayoutRtl();
        float offset = getTabPositionStart() + MathUtils.flipSignIf(viewsWidth, rtl);
        if (rtl) offset += mCachedTabWidth - mNewTabButtonWidth;
        offset = adjustNewTabButtonOffsetIfFull(offset);

        CompositorAnimator animator =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        mNewTabButton,
                        StripLayoutView.DRAW_X,
                        mNewTabButton.getDrawX(),
                        offset,
                        ANIM_TAB_RESIZE_MS);
        return animator;
    }

    private void updateNewTabButtonState() {
        // Note(david@vivaldi.com): We never show the new tab button.
        if (ChromeApplicationImpl.isVivaldi())
            mNewTabButton.setVisible(false);
        // 1. The NTB is faded out upon entering reorder mode and hidden when the model is empty.
        boolean isEmpty = mStripTabs.length == 0;
        if (!ChromeApplicationImpl.isVivaldi())
        mNewTabButton.setVisible(!isEmpty);
        if (isEmpty) return;

        // 2. Get offset from strip stacker.
        // Note: This method anchors the NTB to either a static position at the end of the strip OR
        // right next to the final tab in the strip. This only WAI if the final view in the strip is
        // guaranteed to be a tab. If this changes (e.g. we allow empty tab groups), then this will
        // need to be updated.
        float offset =
                mStripStacker.computeNewTabButtonOffset(
                        mStripTabs,
                        mTabOverlapWidth,
                        mLeftMargin,
                        mRightMargin,
                        mWidth,
                        mNewTabButtonWidth);
        offset = adjustNewTabButtonOffsetIfFull(offset);

        // 3. Hide the new tab button if it's not visible on the screen.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        if ((isRtl && offset + mNewTabButtonWidth < getVisibleLeftBound())
                || (!isRtl && offset > getVisibleRightBound())) {
            mNewTabButton.setVisible(false);
            return;
        }
        // Note(david@vivaldi.com): We are abusing the ModelSelectorButton of the
        // |StripLayoutHelperManager| to create a new tab, instead of using this one here.
        if (!ChromeApplicationImpl.isVivaldi())
        mNewTabButton.setVisible(true);

        // 4. Position the new tab button.
        mNewTabButton.setDrawX(offset);
    }

    /**
     * @param tab The tab to make fully visible.
     * @return Scroll delta to make the tab fully visible.
     */
    private float calculateDeltaToMakeTabVisible(StripLayoutTab tab) {
        if (tab == null) return 0.f;

        // Note(david@vivaldi.com): We have a more specific way on how to calculate the offset.
        if (ChromeApplicationImpl.isVivaldi()) return calculateOffsetToMakeTabVisible(tab);

        // 1. Calculate offsets to fully show the tab at the start and end of the strip.
        final boolean isRtl = LocalizationUtils.isLayoutRtl();
        final float tabWidth = mCachedTabWidth - mTabOverlapWidth;
        // TODO(wenyufu): Account for offsetX{Left,Right} result too much offset. Is this expected?
        final float startOffset = (isRtl ? mRightFadeWidth : mLeftFadeWidth);
        final float endOffset = (isRtl ? mLeftFadeWidth : mRightFadeWidth);
        final float tabPosition = tab.getIdealX() - mScrollOffset + mLeftMargin;

        final float optimalStart = startOffset - tabPosition;
        final float optimalEnd = mWidth - endOffset - tabWidth - tabPosition - mTabOverlapWidth;

        // 2. Return the scroll delta to make the given tab fully visible with the least scrolling.
        // This will result in the tab being at either the start or end of the strip.
        final float deltaToOptimalStart = optimalStart - mScrollOffset;
        final float deltaToOptimalEnd = optimalEnd - mScrollOffset;

        // 3. If the delta to the optimal start is negative and the delta to the optimal end is
        // positive, the given index is already completely in the visible area of the strip.
        if ((deltaToOptimalStart < 0) && (deltaToOptimalEnd > 0)) return 0.f;

        return Math.abs(deltaToOptimalStart) < Math.abs(deltaToOptimalEnd)
                ? deltaToOptimalStart
                : deltaToOptimalEnd;
    }

    /**
     * @param index The index of the tab to make fully visible.
     * @return Scroll delta to make the tab at the given index fully visible.
     */
    private float calculateDeltaToMakeIndexVisible(int index) {
        if (index == TabModel.INVALID_TAB_INDEX) return 0.f;

        return calculateDeltaToMakeTabVisible(mStripTabs[index]);
    }

    void setTabAtPositionForTesting(StripLayoutTab tab) {
        mTabAtPositionForTesting = tab;
    }

    int getTabIndexForTabDrop(float x) {
        float halfTabWidth = mCachedTabWidth / 2;
        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];

            if (LocalizationUtils.isLayoutRtl()) {
                if (x > stripTab.getTouchTargetRight() - halfTabWidth) return i;
            } else {
                if (x < stripTab.getTouchTargetLeft() + halfTabWidth) return i;
            }
        }

        return mStripTabs.length;
    }

    int getTabDropId() {
        if (!mReorderingForTabDrop || mInteractingTab == null) return Tab.INVALID_TAB_ID;

        Tab tab = getTabById(mInteractingTab.getId());
        return mTabGroupModelFilter.isTabInTabGroup(tab) ? tab.getId() : Tab.INVALID_TAB_ID;
    }

    void mergeToGroupForTabDropIfNeeded(int destTabId, int draggedTabId, int index) {
        if (destTabId == Tab.INVALID_TAB_ID) return;

        Tab destTab = getTabById(destTabId);
        StripLayoutGroupTitle groupTitle = findGroupTitle(destTab.getRootId());

        // Animate bottom indicator when merging a new tab into group.
        if (groupTitle != null) {
            List<Animator> animators = new ArrayList<>();
            animators.add(
                    getBottomIndicatorAnimatorForMergeOrMoveOutOfGroup(groupTitle, false, false));
            startAnimationList(animators, null);
        }

        mTabGroupModelFilter.mergeTabsToGroup(draggedTabId, destTabId, true);
        mModel.moveTab(draggedTabId, index);
    }

    StripLayoutTab getTabAtPosition(float x) {
        return (StripLayoutTab) getViewAtPositionX(x, false);
    }

    StripLayoutView getViewAtPositionX(float x, boolean includeGroupTitles) {
        if (mTabAtPositionForTesting != null) {
            return mTabAtPositionForTesting;
        }

        for (int i = 0; i < mStripViews.length; ++i) {
            final StripLayoutView view = mStripViews[i];

            float leftEdge;
            float rightEdge;
            if (view instanceof StripLayoutTab tab) {
                leftEdge = tab.getTouchTargetLeft();
                rightEdge = tab.getTouchTargetRight();
                if (mInReorderMode) {
                    if (LocalizationUtils.isLayoutRtl()) {
                        leftEdge -= tab.getTrailingMargin();
                    } else {
                        rightEdge += tab.getTrailingMargin();
                    }
                }
            } else {
                if (!includeGroupTitles) continue;
                leftEdge = view.getDrawX();
                rightEdge = leftEdge + view.getWidth();
            }

            if (view.isVisible() && leftEdge <= x && x <= rightEdge) {
                return view;
            }
        }

        return null;
    }

    /**
     * @param tab The StripLayoutTab to look for.
     * @return The index of the tab in the visual ordering.
     */
    public int visualIndexOfTabForTesting(StripLayoutTab tab) {
        for (int i = 0; i < mStripTabsVisuallyOrdered.length; i++) {
            if (mStripTabsVisuallyOrdered[i] == tab) {
                return i;
            }
        }
        return -1;
    }

    /**
     * @param tab The StripLayoutTab you're looking at.
     * @return Whether or not this tab is the foreground tab.
     */
    public boolean isForegroundTabForTesting(StripLayoutTab tab) {
        return tab == mStripTabsVisuallyOrdered[mStripTabsVisuallyOrdered.length - 1];
    }

    @VisibleForTesting
    void updateTabAttachState(
            StripLayoutTab tab, boolean attached, @Nullable ArrayList<Animator> animationList) {
        float startValue =
                attached ? FOLIO_DETACHED_BOTTOM_MARGIN_DP : FOLIO_ATTACHED_BOTTOM_MARGIN_DP;
        float intermediateValue = FOLIO_ANIM_INTERMEDIATE_MARGIN_DP;
        float endValue =
                attached ? FOLIO_ATTACHED_BOTTOM_MARGIN_DP : FOLIO_DETACHED_BOTTOM_MARGIN_DP;

        if (animationList == null) {
            tab.setBottomMargin(endValue);
            tab.setFolioAttached(attached);
            return;
        }

        ArrayList<Animator> attachAnimationList = new ArrayList<>();
        CompositorAnimator dropAnimation =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.BOTTOM_MARGIN,
                        startValue,
                        intermediateValue,
                        ANIM_FOLIO_DETACH_MS,
                        Interpolators.EMPHASIZED_ACCELERATE);
        CompositorAnimator riseAnimation =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.BOTTOM_MARGIN,
                        intermediateValue,
                        endValue,
                        ANIM_FOLIO_DETACH_MS,
                        Interpolators.EMPHASIZED_DECELERATE);
        dropAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        tab.setFolioAttached(attached);
                    }
                });
        attachAnimationList.add(dropAnimation);
        attachAnimationList.add(riseAnimation);

        AnimatorSet set = new AnimatorSet();
        set.playSequentially(attachAnimationList);
        animationList.add(set);
    }

    public boolean getInReorderModeForTesting() {
        return mInReorderMode;
    }

    public float getStripStartMarginForReorderForTesting() {
        return mStripStartMarginForReorder;
    }

    public void startReorderModeAtIndexForTesting(int index) {
        StripLayoutTab tab = mStripTabs[index];
        updateStrip();
        startReorderTab(INVALID_TIME, 0f, tab.getDrawX() + (tab.getWidth() / 2));
    }

    public void stopReorderModeForTesting() {
        stopReorderMode();
    }

    @VisibleForTesting
    void startReorderTab(long time, float currentX, float startX) {
        if (mInReorderMode) return;
        RecordUserAction.record("MobileToolbarStartReorderTab");

        // 1. Only start reorder mode if we have a valid (non-null, non-dying, non-placeholder) tab
        // and if the tab state is initialized.
        mInteractingTab = mActiveClickedTab == null ? getTabAtPosition(startX) : mActiveClickedTab;
        if (mInteractingTab == null
                || mInteractingTab.isDying()
                || mInteractingTab.getId() == Tab.INVALID_TAB_ID
                || !mTabStateInitialized) {
            return;
        }
        mInteractingTab.setIsReordering(true);

        // 2. Set mInReorderMode to true before selecting this tab to prevent unnecessary triggering
        // of #bringSelectedTabToVisibleArea for edge tabs when the tab strip is full.
        mInReorderMode = true;

        // 3. Select this tab so that it is always in the foreground.
        TabModelUtils.setIndex(
                mModel, TabModelUtils.getTabIndexById(mModel, mInteractingTab.getId()));

        // 4. Set initial state.
        ArrayList<Animator> animationList = updateStripForReorder(startX);

        // 5. Lift the container off the toolbar and perform haptic feedback.
        Tab tab = getTabById(mInteractingTab.getId());
        updateTabAttachState(mInteractingTab, false, animationList);
        performHapticFeedback(tab);

        // 6. Kick-off animations and request an update.
        if (animationList != null) {
            startAnimationList(animationList, getTabGroupMarginAnimatorListener(false));
        }
        mUpdateHost.requestUpdate();
    }

    void updateStripForExternalTabDrop(float startX) {
        // 1. StartX indicates the position where the tab drag entered the destination tab strip.
        // Adjust by a half tab-width so that we target the nearest tab gap.
        startX = adjustXForTabDrop(startX);

        // 2. Mark the "interacting" tab. This is not the DnD dragged tab, but rather the tab in the
        // strip that is currently being hovered by the DnD drag.
        StripLayoutTab hoveredTab = getTabAtPosition(startX);
        if (hoveredTab == null) hoveredTab = mStripTabs[mStripTabs.length - 1];
        mInteractingTab = hoveredTab;

        // 3. Set initial state.
        mInReorderMode = true;
        mReorderingForTabDrop = true;
        ArrayList<Animator> animationList = updateStripForReorder(startX);

        // 4. Add a tab group margin to the "interacting" tab to indicate where the tab will be
        // inserted should the drag be dropped.
        setTrailingMarginForTab(mInteractingTab, mTabMarginWidth, animationList);

        // 5. Kick-off animations and request an update.
        if (animationList != null) {
            startAnimationList(animationList, getTabGroupMarginAnimatorListener(false));
        }
        mUpdateHost.requestUpdate();
    }

    private ArrayList<Animator> updateStripForReorder(float startX) {
        // 1. Set initial state parameters.
        finishAnimationsAndPushTabUpdates();
        ArrayList<Animator> animationList =
                mAnimationsDisabledForTesting ? null : new ArrayList<>();
        mLastReorderScrollTime = INVALID_TIME;
        mHoverStartTime = INVALID_TIME;
        mHoverStartOffset = 0;
        mReorderState = REORDER_SCROLL_NONE;
        mLastReorderX = startX;
        mTabMarginWidth = mCachedTabWidth / 2;
        mHoveringOverGroup = false;

        // 2. Fade-out model selector and new tab buttons.
        setCompositorButtonsVisible(false);

        // 3. The selected tab will already be visible, so update tab group and background
        // container. For Tab Group Indicators, skip background container highlight and autoscroll
        // to match desktop behavior.
        if (!ChromeFeatureList.sTabStripGroupIndicators.isEnabled()) {
            Tab tab = getTabById(mInteractingTab.getId());

            // Vivaldi
            if (tab == null) return null;

            if (mTabGroupModelFilter.isTabInTabGroup(tab)) {
                setTabGroupBackgroundContainersVisible(tab.getRootId(), true);
            }
            if (!ChromeApplicationImpl.isVivaldi())
            computeAndUpdateTabGroupMargins(true, animationList);
        } else {
            computeAndUpdateStartAndEndMargins(true, animationList);
        }

        return animationList;
    }

    private void stopReorderMode() {
        if (!mInReorderMode) return;
        ArrayList<Animator> animationList = null;
        if (!mAnimationsDisabledForTesting) animationList = new ArrayList<>();

        // 1. Reset the state variables.
        mReorderState = REORDER_SCROLL_NONE;
        mInReorderMode = false;

        // 2. Clear any drag offset.
        finishAnimationsAndPushTabUpdates();
        if (mInteractingTab != null) {
            if (animationList != null) {
                animationList.add(
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                mInteractingTab,
                                StripLayoutView.X_OFFSET,
                                mInteractingTab.getOffsetX(),
                                0f,
                                ANIM_TAB_MOVE_MS));
            } else {
                mInteractingTab.setOffsetX(0f);
            }
        }

        // 3. Reset the background tabs and fade-in the new tab & model selector buttons.
        setBackgroundTabContainersVisible(false);
        setCompositorButtonsVisible(true);

        // 4. Clear any tab group margins.
        resetTabGroupMargins(animationList);

        // 5. Reattach the folio container to the toolbar.
        if (mInteractingTab != null) {
            mInteractingTab.setIsReordering(false);

            // Skip reattachment for tab drop to avoid exposing bottom indicator underneath the tab
            // container.
            if (!mReorderingForTabDrop || !mInteractingTab.getFolioAttached()) {
                updateTabAttachState(mInteractingTab, true, animationList);
            }
        }

        // 6. Reset the tab drop state. Must occur after the rest of the state is reset, since some
        // logic depends on these values.
        mReorderingForTabDrop = false;
        mLastTrailingMargin = 0;

        // 7. Request an update.
        startAnimationList(animationList, getTabGroupMarginAnimatorListener(true));
        mUpdateHost.requestUpdate();
    }

    /**
     * Sets the trailing margin for the current tab. Update bottom indicator width for Tab Group
     * Indicators and animates if necessary.
     *
     * @param tab The tab to update.
     * @param trailingMargin The given tab's new trailing margin.
     * @param animationList The list to add the animation to, or {@code null} if not animating.
     * @return Whether or not the trailing margin for the given tab actually changed.
     */
    private boolean setTrailingMarginForTab(
            StripLayoutTab tab, float trailingMargin, @Nullable List<Animator> animationList) {
        if (tab.getTrailingMargin() != trailingMargin) {
            StripLayoutGroupTitle groupTitle = findGroupTitle(getStripTabRootId(tab));

            if (animationList != null) {
                animationList.add(
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                tab,
                                StripLayoutTab.TRAILING_MARGIN,
                                tab.getTrailingMargin(),
                                trailingMargin,
                                ANIM_TAB_SLIDE_OUT_MS));

                if (groupTitle != null) {
                    float defaultWidth =
                            calculateBottomIndicatorWidth(
                                    groupTitle, getNumOfTabsInGroup(groupTitle));
                    float startWidth = groupTitle.getBottomIndicatorWidth();
                    float endWidth =
                            trailingMargin == 0 ? defaultWidth : defaultWidth + trailingMargin;

                    animationList.add(
                            CompositorAnimator.ofFloatProperty(
                                    mUpdateHost.getAnimationHandler(),
                                    groupTitle,
                                    StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                                    startWidth,
                                    endWidth,
                                    ANIM_TAB_SLIDE_OUT_MS));
                }
            } else {
                tab.setTrailingMargin(trailingMargin);
                if (groupTitle != null) {
                    float defaultWidth =
                            calculateBottomIndicatorWidth(
                                    groupTitle, getNumOfTabsInGroup(groupTitle));
                    float endWidth =
                            trailingMargin == 0 ? defaultWidth : defaultWidth + trailingMargin;
                    groupTitle.setBottomIndicatorWidth(endWidth);
                }
            }
            return true;
        }
        return false;
    }

    /**
     * Sets the new tab strip's start margin and autoscrolls the required amount to make it appear
     * as though the interacting tab does not move.
     *
     * @param startMarginDelta The change in start margin for the tab strip.
     * @param numMarginsToSlide The number of margins to slide to make it appear as through the
     *     interacting tab does not move.
     * @param animationList The list to add the animation to, or {@code null} if not animating.
     */
    private void autoScrollForTabGroupMargins(
            float startMarginDelta, int numMarginsToSlide, List<Animator> animationList) {
        float delta = (numMarginsToSlide * mTabMarginWidth);
        float startValue = mScrollOffset - startMarginDelta;
        float endValue = startValue - delta;

        // If the current tab width is at its max, this means there are not enough tabs to fill the
        // visible area on the tab strip. In this case, there is not enough room to auto-scroll for
        // tab group margins. Allocate additional space to account for this. See
        // http://crbug.com/1374918 for additional details.
        if (mCachedTabWidth == mMaxTabWidth) {
            mReorderExtraMinScrollOffset = mStripStartMarginForReorder + Math.abs(delta);
        }

        if (animationList != null) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            this,
                            SCROLL_OFFSET,
                            startValue,
                            endValue,
                            ANIM_TAB_SLIDE_OUT_MS));
        } else {
            mScrollOffset = endValue;
        }
    }

    private AnimatorListener getTabGroupMarginAnimatorListener(boolean resetExtraMinScrollOffset) {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mTabGroupMarginAnimRunning = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mTabGroupMarginAnimRunning = false;
                if (resetExtraMinScrollOffset) mReorderExtraMinScrollOffset = 0.f;
            }
        };
    }

    private void computeAndUpdateTabGroupMargins(
            boolean autoScroll, ArrayList<Animator> animationList) {
        // 1. Update the trailing margins for each tab.
        boolean pastInteractingTab = false;
        int numMarginsToSlide = 0;
        for (int i = 0; i < mStripTabs.length - 1; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            if (stripTab == mInteractingTab) pastInteractingTab = true;

            // 1.a. Calculate the current tab's trailing margin.
            float trailingMargin = 0f;
            Tab currTab = getTabById(stripTab.getId());
            Tab nextTab = getTabById(mStripTabs[i + 1].getId());
            if (notRelatedAndEitherTabInGroup(currTab, nextTab)) {
                trailingMargin = mTabMarginWidth;
            }

            // 1.b. Attempt to update the current tab's trailing margin.
            float oldMargin = stripTab.getTrailingMargin();
            boolean didChangeTrailingMargin =
                    setTrailingMarginForTab(stripTab, trailingMargin, animationList);
            if (didChangeTrailingMargin && !pastInteractingTab) {
                numMarginsToSlide += oldMargin < trailingMargin ? 1 : -1;
            }

            if (stripTab == mInteractingTab) mLastTrailingMargin = trailingMargin;
        }

        // 2. Set the starting and trailing margin for the tab strip.
        boolean firstTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(getTabById(mStripTabs[0].getId()));
        boolean lastTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(
                        getTabById(mStripTabs[mStripTabs.length - 1].getId()));
        float startMargin =
                firstTabIsInGroup && !ChromeFeatureList.sTabStripGroupIndicators.isEnabled()
                        ? mTabMarginWidth
                        : 0f;
        float startMarginDelta = startMargin - mStripStartMarginForReorder;
        mStripStartMarginForReorder = startMargin;
        mStripTabs[mStripTabs.length - 1].setTrailingMargin(
                (lastTabIsInGroup || mReorderingForTabDrop) ? mTabMarginWidth : 0f);

        // 3. Adjust the scroll offset accordingly to prevent the interacting tab from shifting away
        // from where the user long-pressed.
        if (autoScroll) {
            autoScrollForTabGroupMargins(startMarginDelta, numMarginsToSlide, animationList);
        }

        // 4. Begin slide-out and scroll animation. Update tab positions.
        if (animationList == null) computeTabInitialPositions();
    }

    private void computeAndUpdateStartAndEndMargins(
            boolean autoScroll, List<Animator> animationList) {
        // 1. Set the starting and trailing margin for the tab strip.
        boolean firstTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(getTabById(mStripTabs[0].getId()));
        boolean lastTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(
                        getTabById(mStripTabs[mStripTabs.length - 1].getId()));
        float startMargin =
                firstTabIsInGroup ? mHalfTabWidth * REORDER_OVERLAP_SWITCH_PERCENTAGE : 0f;

        float startMarginDelta = startMargin - mStripStartMarginForReorder;
        mStripStartMarginForReorder = startMargin;
        mStripTabs[mStripTabs.length - 1].setTrailingMargin(
                (lastTabIsInGroup || mReorderingForTabDrop)
                        ? calculateTabGroupThreshold(mStripTabs.length - 1, true, true)
                        : 0f);

        // 2. Adjust the scroll offset accordingly to prevent the interacting tab from shifting away
        // from where the user long-pressed.
        if (autoScroll && !mAnimationsDisabledForTesting) {
            autoScrollForTabGroupMargins(startMarginDelta, 0, animationList);
        } else {
            if (mCachedTabWidth == mMaxTabWidth) {
                mReorderExtraMinScrollOffset = mStripStartMarginForReorder;
            }
            mScrollOffset = mScrollOffset - startMarginDelta;
        }
    }

    private void resetTabGroupMargins(@Nullable ArrayList<Animator> animationList) {
        assert !mInReorderMode;

        // 1. Update the trailing margins for each tab.
        boolean pastInteractingTab = false;
        int numMarginsToSlide = 0;
        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            if (stripTab == mInteractingTab) pastInteractingTab = true;

            boolean didChangeTrailingMargin = setTrailingMarginForTab(stripTab, 0f, animationList);
            if (didChangeTrailingMargin && !pastInteractingTab) numMarginsToSlide--;
        }

        // 2. Adjust the scroll offset accordingly to prevent the interacting tab from shifting away
        // from where the user long-pressed.
        autoScrollForTabGroupMargins(
                -mStripStartMarginForReorder, numMarginsToSlide, animationList);
        mStripStartMarginForReorder = 0f;
    }

    private void setCompositorButtonsVisible(boolean visible) {
        float endOpacity = visible ? 1.f : 0.f;

        CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        mNewTabButton,
                        CompositorButton.OPACITY,
                        mNewTabButton.getOpacity(),
                        endOpacity,
                        ANIM_BUTTONS_FADE_MS)
                .start();
        CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        mModelSelectorButton,
                        CompositorButton.OPACITY,
                        mModelSelectorButton.getOpacity(),
                        endOpacity,
                        ANIM_BUTTONS_FADE_MS)
                .start();
    }

    private void setBackgroundTabContainerVisible(StripLayoutTab tab, boolean visible) {
        if (mReorderingForTabDrop || tab != mInteractingTab) {
            float opacity = visible ? TAB_OPACITY_VISIBLE_BACKGROUND : TAB_OPACITY_HIDDEN;
            tab.setContainerOpacity(opacity);

            updateTabAttachState(tab, !visible, null);
        }
    }

    private void setBackgroundTabContainersVisible(boolean visible) {
        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab tab = mStripTabs[i];
            setBackgroundTabContainerVisible(tab, visible);
        }
    }

    private void setTabGroupBackgroundContainersVisible(int groupId, boolean visible) {
        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab tab = mStripTabs[i];

            if (getTabById(tab.getId()).getRootId() == groupId) {
                setBackgroundTabContainerVisible(tab, visible);
            }
        }
    }

    /**
     * This method checks whether or not interacting tab has met the conditions to be moved out of
     * its tab group. It moves tab out of group if so and returns the new index for the interacting
     * tab.
     *
     * @param offset The distance the interacting tab has been dragged from its ideal x-position.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @return The new index for the interacting tab if it has been removed from its tab group and
     *         the INVALID_TAB_INDEX otherwise.
     */
    private int maybeMoveOutOfGroup(float offset, int curIndex, boolean towardEnd) {
        // If past threshold, un-dim hovered group and trigger reorder.
        if (Math.abs(offset) > mTabMarginWidth * REORDER_OVERLAP_SWITCH_PERCENTAGE) {
            final int tabId = mInteractingTab.getId();

            setTabGroupBackgroundContainersVisible(getTabById(tabId).getRootId(), false);
            mTabGroupModelFilter.moveTabOutOfGroupInDirection(tabId, towardEnd);
            RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
            return curIndex;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * This method checks whether or not interacting tab has met the conditions to be moved out of
     * its tab group for Tab Group Indicators. It moves tab out of group if so and returns the new
     * index for the interacting tab.
     *
     * @param offset The distance the interacting tab has been dragged from its ideal x-position.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @param threshold the drag distance threshold to determine whether a tab is moving out of the
     *     tab group.
     * @param interactingGroupTitle The title of the tab group the tab is dragging past, which
     *     occurs when a tab is being dragged to merge into or move out of the tab group through
     *     group title.
     * @return The new index for the interacting tab if it has been removed from its tab group and
     *     the INVALID_TAB_INDEX otherwise.
     */
    private int maybeMoveOutOfGroupForTabGroupIndicators(
            float offset,
            int curIndex,
            boolean towardEnd,
            float threshold,
            StripLayoutGroupTitle interactingGroupTitle) {
        // If past threshold, trigger reorder.
        if (Math.abs(offset) > threshold) {
            final int tabId = mInteractingTab.getId();
            int rootId = getTabById(tabId).getRootId();

            // Get the target group title.
            Tab destinationTab = getTabById(mStripTabs[curIndex].getId());
            StripLayoutGroupTitle targetGroupTitle = findGroupTitle(destinationTab.getRootId());
            // Run indicator animations.
            if (targetGroupTitle != null) {
                runIndicatorAnimationForMergeOrMoveOutOfGroup(
                        targetGroupTitle, interactingGroupTitle, curIndex, true, towardEnd);
            }

            if ((isLastTabInGroup(tabId)) && mTabGroupIdToHide == Tab.INVALID_TAB_ID) {
                // When dragging the last tab out of group on strip, the tab group delete dialog
                // will show and we will hide the indicators for the interacting tab group until the
                // user confirms the next action. e.g delete tab group when user confirms the
                // delete, or restore indicators back on strip when user cancel the delete.
                showDeleteGroupDialogAndProcessTabAction(
                        tabId,
                        rootId,
                        /* draggingLastTabOffStrip= */ false,
                        /* closeTab= */ false,
                        () -> {
                            mTabGroupModelFilter.moveTabOutOfGroupInDirection(tabId, towardEnd);
                            RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
                        });
            } else if (getNumOfTabsInGroup(targetGroupTitle) > 1) {
                mTabGroupModelFilter.moveTabOutOfGroupInDirection(tabId, towardEnd);
                RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
            }
            return curIndex;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    @VisibleForTesting
    boolean isTabRemoveDialogSkipped() {
        if (mPrefService == null) {
            mPrefService = UserPrefs.get(mModel.getProfile());
        }
        return mPrefService.getBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE);
    }

    void setPrefServiceForTesting(PrefService prefService) {
        mPrefService = prefService;
    }

    /**
     * This method checks whether or not interacting tab has met the conditions to be merged into a
     * neighbouring tab group. It merges tab to group if so and returns the new index for the
     * interacting tab.
     *
     * @param offset The distance the interacting tab has been dragged from its ideal x-position.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @return The new index for the interacting tab if it has been moved into a neighboring tab
     *     group and the INVALID_TAB_INDEX otherwise.
     */
    private int maybeMergeToGroup(float offset, int curIndex, boolean towardEnd) {
        // 1. Only attempt to merge if hovering a group for a valid amount of time.
        if (!mHoveringOverGroup) return TabModel.INVALID_TAB_INDEX;

        // 2. Set initial hover variables if we have not yet started or if we have moved too far
        // from the initial hover. Since we have just started a new hover, do not trigger a
        // reorder.
        if (mHoverStartTime == INVALID_TIME
                || Math.abs(mHoverStartOffset - offset) > DROP_INTO_GROUP_MAX_OFFSET) {
            mHoverStartTime = mLastUpdateTime;
            mHoverStartOffset = offset;

            return TabModel.INVALID_TAB_INDEX;
        }

        // 3. If we have not yet hovered for the required amount of time, keep waiting and do not
        // trigger a reorder.
        if (mLastUpdateTime - mHoverStartTime < DROP_INTO_GROUP_MS) {
            mUpdateHost.requestUpdate();

            return TabModel.INVALID_TAB_INDEX;
        }

        // 4. We have hovered for the required time, so trigger a reorder.
        int direction = towardEnd ? 1 : -1;
        StripLayoutTab destTab = mStripTabs[curIndex + direction];
        float effectiveWidth = mCachedTabWidth - mTabOverlapWidth;
        float flipThreshold = effectiveWidth * REORDER_OVERLAP_SWITCH_PERCENTAGE;
        float minFlipOffset = mTabMarginWidth + flipThreshold;
        int numTabsToSkip =
                1 + (int) Math.floor((Math.abs(offset) - minFlipOffset) / effectiveWidth);
        mTabGroupModelFilter.mergeTabsToGroup(mInteractingTab.getId(), destTab.getId(), true);
        RecordUserAction.record("MobileToolbarReorderTab.TabAddedToGroup");

        return towardEnd ? curIndex + 1 + numTabsToSkip : curIndex - numTabsToSkip;
    }

    /**
     * This method merges the interacting tab into a neighboring tab group for Tab Group Indicators
     * and returns the new index for interacting tab.
     *
     * @param offset The distance the interacting tab has been dragged from its ideal x-position.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @param threshold the drag distance threshold to determine whether a tab is merging into the
     *     tab group.
     * @param interactingGroupTitle The title of the tab group the tab is dragging past, which
     *     occurs when a tab is being dragged to merge into or move out of the tab group through
     *     group title.
     * @return The new index for the interacting tab if it has been moved into a neighboring tab
     *     group and the INVALID_TAB_INDEX otherwise.
     */
    @VisibleForTesting
    protected int maybeMergeToGroupForTabGroupIndicators(
            float offset,
            int curIndex,
            boolean towardEnd,
            float threshold,
            StripLayoutGroupTitle interactingGroupTitle) {
        if (Math.abs(offset) < threshold) {
            return TabModel.INVALID_TAB_INDEX;
        }

        // Trigger a reorder
        int direction = towardEnd ? 1 : -1;
        StripLayoutTab destTab = mStripTabs[curIndex + direction];

        // Get the target group title.
        Tab destinationTab = getTabById(destTab.getId());
        StripLayoutGroupTitle targetGroupTitle = findGroupTitle(destinationTab.getRootId());

        // Run indicator animations.
        if (targetGroupTitle != null) {
            runIndicatorAnimationForMergeOrMoveOutOfGroup(
                    targetGroupTitle, interactingGroupTitle, curIndex, false, towardEnd);
        }
        mTabGroupModelFilter.mergeTabsToGroup(mInteractingTab.getId(), destTab.getId(), true);

        RecordUserAction.record("MobileToolbarReorderTab.TabAddedToGroup");

        return curIndex;
    }

    private AnimatorListener getGroupTitleSlidingAnimatorListener() {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mGroupTitleSliding = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mGroupTitleSliding = false;
            }
        };
    }

    private void runIndicatorAnimationForMergeOrMoveOutOfGroup(
            StripLayoutGroupTitle targetGroupTitle,
            StripLayoutGroupTitle interactingGroupTitle,
            int curIndex,
            boolean isMovingOutOfGroup,
            boolean towardEnd) {
        List<Animator> animators = new ArrayList();

        // Add the group title swapping animation if the tab is merging into or moving out of tab
        // group through group title.
        boolean throughGroupTitle = interactingGroupTitle != null;
        AnimatorListener groupTitleAnimListener = null;
        if (throughGroupTitle) {
            animators.add(
                    getReorderStripViewAnimatorForTabGroupIndicator(
                            mInteractingTab.getId(), curIndex, towardEnd, true));
            groupTitleAnimListener = getGroupTitleSlidingAnimatorListener();
        }

        // Add bottom indicator animation.
        animators.add(
                getBottomIndicatorAnimatorForMergeOrMoveOutOfGroup(
                        targetGroupTitle, isMovingOutOfGroup, throughGroupTitle));

        startAnimationList(animators, groupTitleAnimListener);
    }

    private Animator getBottomIndicatorAnimatorForMergeOrMoveOutOfGroup(
            StripLayoutGroupTitle groupTitle,
            boolean isMovingOutOfGroup,
            boolean throughGroupTitle) {
        // Calculate the initial width and the target width for the bottom indicator.
        float tabWidth = mCachedTabWidth - mTabOverlapWidth;
        float startWidth =
                calculateBottomIndicatorWidth(groupTitle, getNumOfTabsInGroup(groupTitle));
        float endWidth = isMovingOutOfGroup ? startWidth - tabWidth : startWidth + tabWidth;

        // Bottom indicator animation.
        int animDuration = throughGroupTitle ? ANIM_TAB_MOVE_MS : ANIM_TAB_SLIDE_OUT_MS;
        Animator animator =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        groupTitle,
                        StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                        startWidth,
                        endWidth,
                        animDuration);

        return animator;
    }

    private int updateHoveringOverGroup(float offset, int curIndex, boolean towardEnd) {
        boolean hoveringOverGroup = Math.abs(offset) > mTabMarginWidth - mTabOverlapWidth;

        // 1. Check if hover state has changed.
        if (mHoveringOverGroup != hoveringOverGroup) {
            // 1.a. Reset hover variables.
            mHoveringOverGroup = hoveringOverGroup;
            mHoverStartTime = INVALID_TIME;
            mHoverStartOffset = 0;

            // 1.b. Set tab group dim as necessary.
            int groupId =
                    getTabById(mStripTabs[curIndex + (towardEnd ? 1 : -1)].getId()).getRootId();
            setTabGroupBackgroundContainersVisible(groupId, mHoveringOverGroup);
        }

        // 2. If we are hovering, attempt to merge to the hovered group.
        if (mHoveringOverGroup) {
            return maybeMergeToGroup(offset, curIndex, towardEnd);
        }

        // 3. Default to not triggering a reorder.
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * This method determines the new index for the interacting tab, based on whether or not it has
     * met the conditions to be moved past a neighboring tab group.
     *
     * @param offset The distance the interacting tab has been dragged from its ideal x-position.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @return The new index for the interacting tab if it should be moved past the neighboring tab
     *         group and the INVALID_TAB_INDEX otherwise.
     */
    private int maybeMovePastGroup(float offset, int curIndex, boolean towardEnd) {
        int direction = towardEnd ? 1 : -1;
        int groupId = getTabById(mStripTabs[curIndex + direction].getId()).getRootId();
        int numTabsToSkip = mTabGroupModelFilter.getRelatedTabCountForRootId(groupId);
        float effectiveTabWidth = mCachedTabWidth - mTabOverlapWidth;
        float threshold = (numTabsToSkip * effectiveTabWidth) + mTabMarginWidth + mTabOverlapWidth;

        // If past threshold, un-dim hovered group and trigger reorder.
        if (Math.abs(offset) > threshold) {
            setTabGroupBackgroundContainersVisible(groupId, false);

            int destIndex = towardEnd ? curIndex + 1 + numTabsToSkip : curIndex - numTabsToSkip;
            return destIndex;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * This method determines the new index for the interacting tab, based on whether or not it has
     * met the conditions to be moved past a neighboring collapsed tab group.
     */
    private int maybeMovePastCollapsedGroup(
            StripLayoutGroupTitle groupTitle, float offset, int curIndex, boolean towardEnd) {
        int groupId = groupTitle.getRootId();
        int numTabsToSkip = mTabGroupModelFilter.getRelatedTabCountForRootId(groupId);
        float threshold = groupTitle.getWidth() * REORDER_OVERLAP_SWITCH_PERCENTAGE;

        // Animate group title moving to new position. mStripViews will be rebuilt when we receive
        // the #didMoveTab event from the TabModel.
        if (Math.abs(offset) > threshold) {
            int destIndex = towardEnd ? curIndex + 1 + numTabsToSkip : curIndex - numTabsToSkip;

            final float tabWidth = mCachedTabWidth - mTabOverlapWidth;
            final float startOffset =
                    MathUtils.flipSignIf(tabWidth, (!towardEnd) ^ LocalizationUtils.isLayoutRtl());
            // TODO(crbug.com/338130577): We intentionally start this outside of the
            //  "RunningAnimator" pattern so it doesn't finish early due to the subsequent
            //  #didMoveTab event. Fix this when we update #reorderTab to handle non-tab views.
            CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            groupTitle,
                            StripLayoutView.X_OFFSET,
                            startOffset,
                            0,
                            ANIM_TAB_MOVE_MS)
                    .start();

            return destIndex;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    private boolean notRelatedAndEitherTabInGroup(Tab curTab, Tab adjTab) {
        assert curTab != null && adjTab != null;
        return !(curTab.getRootId() == adjTab.getRootId())
                && (mTabGroupModelFilter.isTabInTabGroup(curTab)
                        || mTabGroupModelFilter.isTabInTabGroup(adjTab));
    }

    /**
     * @param id The id of the selected tab.
     * @return The outline color if the selected tab will show its Tab Group Indicator outline.
     *     {@code Color.TRANSPARENT} otherwise.
     */
    protected @ColorInt int getSelectedOutlineGroupTint(int id, boolean shouldShowOutline) {
        if (!shouldShowOutline) return Color.TRANSPARENT;

        Tab tab = getTabById(id);
        if (tab == null) return Color.TRANSPARENT;

        StripLayoutGroupTitle groupTitle = findGroupTitle(tab.getRootId());
        if (groupTitle == null) return Color.TRANSPARENT;

        return groupTitle.getTint();
    }

    /**
     * This method decides whether to show tab outline for Tab Group Indicators by checking whether
     * its the selected tab and tab container state.
     *
     * @param stripLayoutTab The current {@link StripLayoutTab}.
     * @return whether to show tab outline.
     */
    protected boolean shouldShowTabOutline(StripLayoutTab stripLayoutTab) {
        if (!ChromeFeatureList.sTabStripGroupIndicators.isEnabled()) {
            return false;
        }

        // Placeholder tabs on startup have invalid tab id, resulting in a null tab, if so, return
        // early.
        Tab tab = getTabById(stripLayoutTab.getId());
        if (tab == null
                || !mTabGroupModelFilter.isTabInTabGroup(tab)
                || tab.getRootId() == mTabGroupIdToHide) {
            return false;
        }

        // Show tab outline when tab is in group with folio attached and 1. tab is selected or 2.
        // tab is in foreground (e.g. the previously selected tab in destination strip).
        return stripLayoutTab.getFolioAttached()
                && (getSelectedStripTab() == stripLayoutTab
                        || stripLayoutTab.getContainerOpacity() == TAB_OPACITY_VISIBLE_FOREGROUND);
    }

    private void updateReorderPosition(float deltaX) {
        if (!mInReorderMode || mInteractingTab == null || mReorderingForTabDrop) return;

        int curIndex = findIndexForTab(mInteractingTab.getId());

        // Vivaldi
        if (curIndex == Tab.INVALID_TAB_ID) return;

        if (curIndex == TabModel.INVALID_TAB_INDEX) return;

        // 1. Compute drag position.
        final float tabWidth = mCachedTabWidth - mTabOverlapWidth;
        float offset = mInteractingTab.getOffsetX() + deltaX;
        boolean towardEnd = (offset >= 0) ^ LocalizationUtils.isLayoutRtl();

        // 2. Determine grouped state.
        Tab curTab = mModel.getTabAt(curIndex);
        Tab adjTab = mModel.getTabAt(curIndex + (towardEnd ? 1 : -1));
        boolean isInGroup = mTabGroupModelFilter.isTabInTabGroup(curTab);
        boolean mayDragInOrOutOfGroup =
                adjTab == null ? isInGroup : notRelatedAndEitherTabInGroup(curTab, adjTab);

        // Vivaldi
        boolean isPastRightThresholdAndNotRightMost = false;

        // 3. Check if we should swap tabs. Track the move threshold, destination index, and
        // interacting group title.
        final float moveThreshold;
        int destIndex = TabModel.INVALID_TAB_INDEX;
        StripLayoutGroupTitle interactingGroupTitle = null;

        if (mayDragInOrOutOfGroup) {
            if (ChromeFeatureList.sTabStripGroupIndicators.isEnabled()) {
                moveThreshold = calculateTabGroupThreshold(curIndex, isInGroup, towardEnd);
                if (isInGroup) {
                    // 3.a. Tab is in a group. Maybe drag out of group.
                    interactingGroupTitle = getInteractingGroupTitle(curIndex, towardEnd);
                    destIndex =
                            maybeMoveOutOfGroupForTabGroupIndicators(
                                    offset,
                                    curIndex,
                                    towardEnd,
                                    moveThreshold,
                                    interactingGroupTitle);
                } else {
                    StripLayoutGroupTitle adjTitle = findGroupTitle(adjTab.getRootId());
                    if (adjTitle != null && adjTitle.isCollapsed()) {
                        // 3.b. Tab is not in a group. Adjacent group is collapsed. Maybe reorder
                        // past the collapsed group.
                        interactingGroupTitle = adjTitle;
                        destIndex =
                                maybeMovePastCollapsedGroup(adjTitle, offset, curIndex, towardEnd);
                    } else {
                        // 3.c. Tab is not in a group. Adjacent group is not collapsed. Maybe merge
                        // to group.
                        interactingGroupTitle = getInteractingGroupTitle(curIndex, towardEnd);
                        destIndex =
                                maybeMergeToGroupForTabGroupIndicators(
                                        offset,
                                        curIndex,
                                        towardEnd,
                                        moveThreshold,
                                        interactingGroupTitle);
                    }
                }
            } else {
                if (isInGroup) {
                    // 3.d. Tab is in a group. Maybe drag out of group.
                    destIndex = maybeMoveOutOfGroup(offset, curIndex, towardEnd);
                } else {
                    // 3.e. Tab is not in a group. Maybe merge to tab group.
                    destIndex = updateHoveringOverGroup(offset, curIndex, towardEnd);

                    // 3.f. Tab is not in a group. Maybe drag past group.
                    if (destIndex == TabModel.INVALID_TAB_INDEX) {
                        destIndex = maybeMovePastGroup(offset, curIndex, towardEnd);
                    }
                }
            }
        } else {
            // 3.g. Tab is not interacting with tab groups. Reorder as normal.
            moveThreshold = REORDER_OVERLAP_SWITCH_PERCENTAGE * tabWidth;
            boolean pastLeftThreshold = offset < -moveThreshold;
            boolean pastRightThreshold = offset > moveThreshold;
            boolean isNotRightMost = curIndex < mStripTabs.length - 1;
            boolean isNotLeftMost = curIndex > 0;

            if (LocalizationUtils.isLayoutRtl()) {
                boolean oldLeft = pastLeftThreshold;
                pastLeftThreshold = pastRightThreshold;
                pastRightThreshold = oldLeft;
            }

            if (pastRightThreshold && isNotRightMost) {
                destIndex = curIndex + 2;
            } else if (pastLeftThreshold && isNotLeftMost) {
                destIndex = curIndex - 1;
            }
            // Vivaldi
            isPastRightThresholdAndNotRightMost = pastRightThreshold && isNotRightMost;
        }

        // 3. If we should swap tabs, make the swap.
        if (destIndex != TabModel.INVALID_TAB_INDEX) {
            // 3. a. Reset internal state.
            mHoveringOverGroup = false;

            // 3.b. Move the tab to its new position.

            // Note(david@vivaldi.com): Here we calculate the model index to where we move the
            // current selected tab and finally swap it. This only applies when tab group is
            // enabled as the model destination index is different to destination index of the
            // current (main or stack) tab strip.
            if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()) {
                int modelDestIndex = destIndex;
                // When moving right update modelDestIndex to point to the proper element.
                if (isPastRightThresholdAndNotRightMost) modelDestIndex--;
                int destinationTabId = mStripTabs[modelDestIndex].getId();
                if (mIsStackStrip) { // Stack strip
                    int newDestIndex = 0;
                    // Get the new destination index in the tab model.
                    for (int i = 0; i < mModel.getCount(); i++)
                        if (mModel.getTabAt(i).getId() == destinationTabId) newDestIndex = i;
                    // When moving right update destIndex to point to the proper element.
                    if (isPastRightThresholdAndNotRightMost) newDestIndex++;
                    // Swap the tabs.
                    reorderTab(mInteractingTab.getId(), curIndex, destIndex, true);
                    // Move the tab within the model.
                    mModel.moveTab(mInteractingTab.getId(), newDestIndex);
                } else { // Main strip
                    List<Tab> destTabGroup =
                            mTabGroupModelFilter.getRelatedTabList(destinationTabId);
                    int newDestIndex = mModel.indexOf(destTabGroup.get(0));
                    if (isPastRightThresholdAndNotRightMost)
                        newDestIndex =
                                mModel.indexOf(destTabGroup.get(destTabGroup.size() - 1)) + 1;
                    // Swap the tabs.
                    reorderTab(mInteractingTab.getId(), curIndex, destIndex, true);
                    // We move a group here, also when there is only one tab.
                    mTabGroupModelFilter.moveRelatedTabs(mInteractingTab.getId(), newDestIndex);
                }
            } else { // Vivaldi
            reorderTab(mInteractingTab.getId(), curIndex, destIndex, true);
            mModel.moveTab(mInteractingTab.getId(), destIndex);
            } //Vivaldi

            // 3.c. Re-compute tab group margins. Skip if group indicators are enabled instead.
            float oldIdealX = mInteractingTab.getIdealX();
            float oldOffset = mScrollOffset;
            if (!ChromeFeatureList.sTabStripGroupIndicators.isEnabled()) {
                if (!ChromeApplicationImpl.isVivaldi())
                computeAndUpdateTabGroupMargins(false, null);
            } else {
                // Update strip start and end margins to create more space for first tab or last tab
                // to drag out of group.
                if ((curIndex == 0 || curIndex >= mStripTabs.length - 2)
                        && mTabGroupModelFilter.isTabInTabGroup(
                                getTabById(mInteractingTab.getId()))) {
                    computeAndUpdateStartAndEndMargins(false, null);
                }
                // Manually reset last tab's trailing margin after the tab group is removed.
                if (mStripTabs.length > 1) {
                    mStripTabs[mStripTabs.length - 2].setTrailingMargin(0f);
                }
            }

            // 3.d. Since we just moved the tab we're dragging, adjust its offset so it stays in
            // the same apparent position.
            boolean shouldFlip = LocalizationUtils.isLayoutRtl() ^ towardEnd;
            if (mayDragInOrOutOfGroup) {
                // Account for group title offset.
                if (ChromeFeatureList.sTabStripGroupIndicators.isEnabled()
                        && interactingGroupTitle != null) {
                    float groupTitleWidth = interactingGroupTitle.getWidth();
                    offset += MathUtils.flipSignIf((groupTitleWidth), shouldFlip);
                } else {
                    offset -= (mInteractingTab.getIdealX() - oldIdealX);
                    // When the strip is scrolling, deltaX is already accounted for by idealX. This
                    // is because idealX uses the scroll offset which has already been adjusted by
                    // deltaX.
                    if (mLastReorderScrollTime != 0) offset -= deltaX;

                    // Tab group margins can affect minScrollOffset. When a dragged tab is near the
                    // strip's edge, the scrollOffset being clamped can affect the apparent
                    // position.
                    offset -=
                            MathUtils.flipSignIf(
                                    (mScrollOffset - oldOffset), LocalizationUtils.isLayoutRtl());
                }
            } else {
                offset += MathUtils.flipSignIf(tabWidth, shouldFlip);
            }

            // 3.e. Update our curIndex as we have just moved the tab.
            curIndex = destIndex > curIndex ? destIndex - 1 : destIndex;

            // 3.f. Update visual tab ordering.
            updateVisualTabOrdering();
        }

        // 4. Limit offset based on tab position.  First tab can't drag left, last tab can't drag
        // right. For Tab Group Indicators, since tab group margins were removed, we need to allow
        // dragging left by the drag threshold when first tab is in group, and the same applies for
        // dragging right when last tab is in group.
        // TODO(crbug.com/331854162): Refactor to set mStripStartMarginForReorder and the final
        //  tab's trailing margin.
        boolean indicatorsEnabled = ChromeFeatureList.sTabStripGroupIndicators.isEnabled();
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        float limit;
        if (curIndex == 0) {
            limit =
                    (indicatorsEnabled && mStripViews[0] instanceof StripLayoutGroupTitle)
                            ? calculateTabGroupThreshold(0, true, false)
                            : mStripStartMarginForReorder;
            offset = isRtl ? Math.min(limit, offset) : Math.max(-limit, offset);
        }
        if (curIndex == mStripTabs.length - 1) {
            offset =
                    LocalizationUtils.isLayoutRtl()
                            ? Math.max(-mStripTabs[curIndex].getTrailingMargin(), offset)
                            : Math.min(mStripTabs[curIndex].getTrailingMargin(), offset);
        }

        // 5. Set the new offset.
        mInteractingTab.setOffsetX(offset);
    }

    /**
     * This method prompts a confirmation dialog for deleting the tab group and handles the user
     * response.
     *
     * @param confirmationCallback The callback method to close the last tab or move the last tab
     *     out of the group when the user confirms the tab group deletion.
     * @param dragTabOffStrip Whether the tab is being dragged off tab strip.
     * @param closeTab Whether this method is triggered from tab closing.
     */
    private void showConfirmationDialogAndHandleResponse(
            Runnable confirmationCallback, boolean dragTabOffStrip, boolean closeTab) {
        // Clear any drag and drop in progress to display the dialog.
        if (!isTabRemoveDialogSkipped()) {
            if (mToolbarContainerView != null) {
                mToolbarContainerView.cancelDragAndDrop();
            }
        }

        // Do not run callback if the call is from tab drag and drop, tab group will be restored
        // if drop is not handled. If the tab drop is handled, the tab group will be deleted
        // when the tab is re-parented, so no action is needed here.
        boolean shouldRunIfImmediateContinue = closeTab || !dragTabOffStrip;

        // Show the delete group dialog for either removing or closing the last tab in the group.
        if (closeTab) {
            mActionConfirmationManager.processCloseTabAttempt(
                    (@ConfirmationResult Integer result) -> {
                        handleUserConfirmation(
                                result, confirmationCallback, shouldRunIfImmediateContinue);
                    });
        } else {
            mActionConfirmationManager.processRemoveTabAttempt(
                    (@ConfirmationResult Integer result) -> {
                        handleUserConfirmation(
                                result, confirmationCallback, shouldRunIfImmediateContinue);
                    });
        }
    }

    /**
     * This method handles the user response for the tab group delete dialog.
     *
     * @param result The integer value representing the user's response on whether to proceed with
     *     deleting the group.
     * @param confirmationCallback The callback method to close the last tab or move the last tab
     *     out of the group when the user confirms the tab group deletion.
     * @param shouldRunIfImmediateContinue Whether to run the callback method when dialog is
     *     skipped.
     */
    private void handleUserConfirmation(
            @ConfirmationResult Integer result,
            Runnable confirmationCallback,
            boolean shouldRunIfImmediateContinue) {
        mTabGroupIdToHide = Tab.INVALID_TAB_ID;
        if (result == ConfirmationResult.CONFIRMATION_NEGATIVE) {
            rebuildStripViews();
        } else if (result == ConfirmationResult.CONFIRMATION_POSITIVE) {
            confirmationCallback.run();
        } else {
            if (shouldRunIfImmediateContinue) {
                confirmationCallback.run();
            }
        }
    }

    /**
     * This method determines whether this tab drag is interacting with tab group title indicator.
     *
     * @param curIndexInStripTab The index of the interacting tab in mStripTabs.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @return Whether this tab drag is interacting with tab group title indicator.
     */
    private StripLayoutGroupTitle getInteractingGroupTitle(
            int curIndexInStripTab, boolean towardEnd) {
        int curIndexInStripView = findStripViewIndexForStripTab(curIndexInStripTab);
        if (curIndexInStripView == TabModel.INVALID_TAB_INDEX) {
            return null;
        }
        if (towardEnd) {
            if (curIndexInStripView == mStripViews.length - 1) {
                return null;
            }
            // The drag is interacting with group title when 1. curTab is not is group and 2. the
            // next view is a group title.
            return !isStripTabInTabGroup(mStripTabs[curIndexInStripTab])
                            && mStripViews[curIndexInStripView + 1]
                                    instanceof StripLayoutGroupTitle groupTitle
                    ? groupTitle
                    : null;
        } else {
            if (curIndexInStripView == 0) {
                return null;
            }
            // The drag is interacting with group title when the previous view is a group title.
            return mStripViews[curIndexInStripView - 1] instanceof StripLayoutGroupTitle groupTitle
                    ? groupTitle
                    : null;
        }
    }

    /**
     * This method determines the drag threshold for either merge into or move out of a tab group
     * for Tab Group Indicators.
     *
     * @param curIndexInStripTab The index of the interacting tab in mStripTabs.
     * @param isInGroup Whether the current tab is in a tab group.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @return The drag threshold float for either merge into or move out of a tab group for Tab
     *     Group Indicators.
     */
    @VisibleForTesting
    protected float calculateTabGroupThreshold(
            int curIndexInStripTab, boolean isInGroup, boolean towardEnd) {
        int curIndexInStripView = findStripViewIndexForStripTab(curIndexInStripTab);
        float dragOutThreshold = mHalfTabWidth * REORDER_OVERLAP_SWITCH_PERCENTAGE;
        float dragInThreshold = mHalfTabWidth;

        assert curIndexInStripView != TabModel.INVALID_TAB_INDEX;
        if (isInGroup) {
            if (curIndexInStripView > 0 && !towardEnd) {
                return dragOutThreshold + mStripViews[curIndexInStripView - 1].getWidth();
            } else {
                return dragOutThreshold;
            }
        } else {
            return dragInThreshold;
        }
    }

    private float adjustXForTabDrop(float x) {
        if (LocalizationUtils.isLayoutRtl()) {
            return x + (mCachedTabWidth / 2);
        } else {
            return x - (mCachedTabWidth / 2);
        }
    }

    void updateReorderPositionForTabDrop(float x) {
        if (mTabGroupMarginAnimRunning) return;

        // 1. Adjust by a half tab-width so that we target the nearest tab gap.
        x = adjustXForTabDrop(x);

        // 2. Clear previous "interacting" tab if inserting at the start of the strip.
        boolean inStartGap =
                LocalizationUtils.isLayoutRtl()
                        ? x > mStripTabs[0].getTouchTargetRight()
                        : x < mStripTabs[0].getTouchTargetLeft();
        if (inStartGap && mInteractingTab != null) {
            float delta = mTabMarginWidth - mStripStartMarginForReorder;
            mStripStartMarginForReorder = mTabMarginWidth;
            if (delta != 0.f) {
                mReorderExtraMinScrollOffset += delta;
                mScrollOffset -= delta;
            }

            finishAnimations();
            ArrayList<Animator> animationList = new ArrayList<>();
            setTrailingMarginForTab(mInteractingTab, mLastTrailingMargin, animationList);
            mInteractingTab = null;
            startAnimationList(animationList, getTabGroupMarginAnimatorListener(false));

            // 2.a. Early-out if we just entered the start gap.
            return;
        }

        // 3. Otherwise, update drop indicator if necessary.
        StripLayoutTab hoveredTab = getTabAtPosition(x);
        if (hoveredTab != null && hoveredTab != mInteractingTab) {
            finishAnimations();

            // 3.a. Reset the state for the previous "interacting" tab.
            ArrayList<Animator> animationList = new ArrayList<>();
            if (mInteractingTab != null) {
                setTrailingMarginForTab(mInteractingTab, mLastTrailingMargin, animationList);
                Tab tab = getTabById(mInteractingTab.getId());
                if (mTabGroupModelFilter.isTabInTabGroup(tab)
                        && !ChromeFeatureList.sTabStripGroupIndicators.isEnabled()) {
                    setTabGroupBackgroundContainersVisible(tab.getRootId(), false);
                }
            }

            // 3.b. Set state for the new "interacting" tab.
            mLastTrailingMargin = hoveredTab.getTrailingMargin();
            setTrailingMarginForTab(hoveredTab, mTabMarginWidth, animationList);
            Tab tab = getTabById(hoveredTab.getId());
            if (mTabGroupModelFilter.isTabInTabGroup(tab)
                    && !ChromeFeatureList.sTabStripGroupIndicators.isEnabled()) {
                setTabGroupBackgroundContainersVisible(tab.getRootId(), true);
            }
            mInteractingTab = hoveredTab;

            // 3.c. Animate.
            startAnimationList(animationList, getTabGroupMarginAnimatorListener(false));
        }
    }

    private void reorderTab(int id, int oldIndex, int newIndex, boolean animate) {
        StripLayoutTab tab = findTabById(id);
        if (tab == null || oldIndex == newIndex) return;

        // 1. If the tab is already at the right spot, don't do anything.
        int index = findIndexForTab(id);
        if (index == newIndex) return;

        // 2. Check if it's the tab we are dragging, but we have an old source index.  Ignore in
        // this case because we probably just already moved it.
        if (mInReorderMode && index != oldIndex && tab == mInteractingTab) return;

        // 3. Animate if necessary.
        if (animate && !mAnimationsDisabledForTesting) {
            final boolean towardEnd = oldIndex <= newIndex;
            final float flipWidth = mCachedTabWidth - mTabOverlapWidth;
            final int direction = towardEnd ? 1 : -1;
            final float animationLength =
                    MathUtils.flipSignIf(direction * flipWidth, LocalizationUtils.isLayoutRtl());

            finishAnimationsAndPushTabUpdates();
            ArrayList<Animator> slideAnimationList = new ArrayList<>();
            for (int i = oldIndex + direction; towardEnd == i < newIndex; i += direction) {
                StripLayoutTab slideTab = mStripTabs[i];
                CompositorAnimator animator =
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                slideTab,
                                StripLayoutView.X_OFFSET,
                                animationLength,
                                0f,
                                ANIM_TAB_MOVE_MS);
                slideAnimationList.add(animator);
                // When the reorder is triggered by an autoscroll, the first frame will not show the
                // sliding tabs with the correct offset. To fix this, we manually set the correct
                // starting offset. See https://crbug.com/1342811.
                slideTab.setOffsetX(animationLength);
            }
            startAnimationList(slideAnimationList, null);
        }

        // 4. Swap the tabs.
        moveElement(mStripTabs, index, newIndex);
        if (!mMovingGroup) {
            // When tab groups are moved, each tab is moved one-by-one. During this process, the
            // invariant that tab groups must be contiguous is temporarily broken, so we suppress
            // rebuilding until the entire group is moved. See https://crbug.com/329318567.
            // TODO(crbug.com/329335086): Investigate reordering (with #moveElement) instead of
            // rebuilding here.
            rebuildStripViews();
        }
    }

    /**
     * This method reorders the StripLayoutView when tab drag is interacting with group title for
     * Tab Group Indicators.
     *
     * @param id The tabId of the interacting tab.
     * @param oldIndex The starting index of the reorder.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @param animate Whether to animate the view swapping.
     */
    private Animator getReorderStripViewAnimatorForTabGroupIndicator(
            int id, int oldIndex, boolean towardEnd, boolean animate) {
        int direction = towardEnd ? 1 : -1;
        int oldIndexInStripView = findStripViewIndexForStripTab(oldIndex);
        assert oldIndexInStripView != TabModel.INVALID_TAB_INDEX;

        boolean isLeftMost = oldIndexInStripView == 0;
        boolean isRightMost = oldIndexInStripView >= mStripViews.length - 1;

        StripLayoutTab tab = findTabById(id);
        if (tab == null || (isLeftMost && !towardEnd) || (isRightMost && towardEnd)) return null;

        int newIndexInStripView = oldIndexInStripView + direction;

        // 1. If the view is already at the right spot, don't do anything.
        int index = findIndexForTab(id);
        int curIndexInStripView = findStripViewIndexForStripTab(index);
        assert curIndexInStripView != TabModel.INVALID_TAB_INDEX;

        if (curIndexInStripView == newIndexInStripView) return null;

        // 2. Check if it's the view we are dragging, but we have an old source index.  Ignore in
        // this case because we probably just already moved it.
        if (mInReorderMode
                && curIndexInStripView != oldIndexInStripView
                && tab == mInteractingTab) {
            return null;
        }

        CompositorAnimator animator = null;
        // 3. Animate if necessary.
        if (animate && !mAnimationsDisabledForTesting) {
            final float flipWidth = mCachedTabWidth - mTabOverlapWidth;
            final float animationLength =
                    MathUtils.flipSignIf(direction * flipWidth, !LocalizationUtils.isLayoutRtl());

            finishAnimationsAndPushTabUpdates();

            StripLayoutView slideView = mStripViews[newIndexInStripView];
            animator =
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            slideView,
                            StripLayoutView.DRAW_X,
                            slideView.getDrawX(),
                            slideView.getDrawX() + animationLength,
                            ANIM_TAB_MOVE_MS);
        }

        // 4. Swap the views.
        if (!isRightMost && towardEnd) {
            newIndexInStripView += 1;
        }
        moveElement(mStripViews, curIndexInStripView, newIndexInStripView);
        return animator;
    }

    private void handleReorderAutoScrolling(long time) {
        if (!mInReorderMode) return;

        // 1. Track the delta time since the last auto scroll.
        final float deltaSec =
                mLastReorderScrollTime == INVALID_TIME
                        ? 0.f
                        : (time - mLastReorderScrollTime) / 1000.f;
        mLastReorderScrollTime = time;

        // When we are reordering for tab drop, we are not offsetting the interacting tab. Instead,
        // we are adding a visual indicator (a gap between tabs) to indicate where the tab will be
        // added. As such, we need to base this on the most recent x-position of the drag, rather
        // than the interacting tab's drawX.
        final float x =
                mReorderingForTabDrop
                        ? adjustXForTabDrop(mLastReorderX)
                        : mInteractingTab.getDrawX();

        // 2. Calculate the gutters for accelerating the scroll speed.
        // Speed: MAX    MIN                  MIN    MAX
        // |-------|======|--------------------|======|-------|
        final float dragRange = REORDER_EDGE_SCROLL_START_MAX_DP - REORDER_EDGE_SCROLL_START_MIN_DP;
        final float leftMinX = REORDER_EDGE_SCROLL_START_MIN_DP + mLeftMargin;
        final float leftMaxX = REORDER_EDGE_SCROLL_START_MAX_DP + mLeftMargin;
        final float rightMinX =
                mWidth - mLeftMargin - mRightMargin - REORDER_EDGE_SCROLL_START_MIN_DP;
        final float rightMaxX =
                mWidth - mLeftMargin - mRightMargin - REORDER_EDGE_SCROLL_START_MAX_DP;

        // 3. See if the current draw position is in one of the gutters and figure out how far in.
        // Note that we only allow scrolling in each direction if the user has already manually
        // moved that way.
        float dragSpeedRatio = 0.f;
        if ((mReorderState & REORDER_SCROLL_LEFT) != 0 && x < leftMinX) {
            dragSpeedRatio = -(leftMinX - Math.max(x, leftMaxX)) / dragRange;
        } else if ((mReorderState & REORDER_SCROLL_RIGHT) != 0 && x + mCachedTabWidth > rightMinX) {
            dragSpeedRatio = (Math.min(x + mCachedTabWidth, rightMaxX) - rightMinX) / dragRange;
        }

        dragSpeedRatio = MathUtils.flipSignIf(dragSpeedRatio, LocalizationUtils.isLayoutRtl());

        if (dragSpeedRatio != 0.f) {
            // 4.a. We're in a gutter.  Update the scroll offset.
            float dragSpeed = REORDER_EDGE_SCROLL_MAX_SPEED_DP * dragSpeedRatio;
            updateScrollOffsetPosition(mScrollOffset + dragSpeed * deltaSec);

            mUpdateHost.requestUpdate();
        } else {
            // 4.b. We're not in a gutter.  Reset the scroll delta time tracker.
            mLastReorderScrollTime = INVALID_TIME;
        }
    }

    @VisibleForTesting
    Tab getTabById(int tabId) {
        return mModel.getTabById(tabId);
    }

    private int getSelectedTabId() {
        if (mModel == null) return Tab.INVALID_TAB_ID;

        int index = mModel.index();
        if (index == TabModel.INVALID_TAB_INDEX) return Tab.INVALID_TAB_ID;

        Tab tab = mModel.getTabAt(index);
        if (tab == null) return Tab.INVALID_TAB_ID;

        return tab.getId();
    }

    @VisibleForTesting
    int getSelectedStripTabIndex() {
        return mTabStateInitialized
                ? findIndexForTab(getSelectedTabId())
                : mActiveTabIndexOnStartup;
    }

    private StripLayoutTab getSelectedStripTab() {
        int index = getSelectedStripTabIndex();

        return index >= 0 && index < mStripTabs.length ? mStripTabs[index] : null;
    }

    private boolean isSelectedTab(int id) {
        return id != Tab.INVALID_TAB_ID && id == getSelectedTabId();
    }

    private void resetResizeTimeout(boolean postIfNotPresent) {
        final boolean present = mStripTabEventHandler.hasMessages(MESSAGE_RESIZE);

        if (present) mStripTabEventHandler.removeMessages(MESSAGE_RESIZE);

        if (present || postIfNotPresent) {
            mStripTabEventHandler.sendEmptyMessageAtTime(MESSAGE_RESIZE, RESIZE_DELAY_MS);
        }
    }

    protected void scrollTabToView(long time, boolean requestUpdate) {
        bringSelectedTabToVisibleArea(time, true);
        if (requestUpdate) mUpdateHost.requestUpdate();
    }

    @SuppressLint("HandlerLeak")
    private class StripTabEventHandler extends Handler {
        @Override
        public void handleMessage(Message m) {
            switch (m.what) {
                case MESSAGE_RESIZE:
                    computeAndUpdateTabWidth(true, false, null);
                    mUpdateHost.requestUpdate();
                    break;
                case MESSAGE_UPDATE_SPINNER:
                    mUpdateHost.requestUpdate();
                    break;
                default:
                    assert false : "StripTabEventHandler got unknown message " + m.what;
            }
        }
    }

    private class TabLoadTrackerCallbackImpl implements TabLoadTrackerCallback {
        @Override
        public void loadStateChanged(int id) {
            mUpdateHost.requestUpdate();
        }
    }

    private static <T> void moveElement(T[] array, int oldIndex, int newIndex) {
        if (oldIndex <= newIndex) {
            moveElementUp(array, oldIndex, newIndex);
        } else {
            moveElementDown(array, oldIndex, newIndex);
        }
    }

    private static <T> void moveElementUp(T[] array, int oldIndex, int newIndex) {
        assert oldIndex <= newIndex;
        if (oldIndex == newIndex || oldIndex + 1 == newIndex) return;

        T elem = array[oldIndex];
        for (int i = oldIndex; i < newIndex - 1; i++) {
            array[i] = array[i + 1];
        }
        array[newIndex - 1] = elem;
    }

    private static <T> void moveElementDown(T[] array, int oldIndex, int newIndex) {
        assert oldIndex >= newIndex;
        if (oldIndex == newIndex) return;

        T elem = array[oldIndex];
        for (int i = oldIndex - 1; i >= newIndex; i--) {
            array[i + 1] = array[i];
        }
        array[newIndex] = elem;
    }

    /**
     * Sets the current scroll offset of the TabStrip.
     * @param offset The offset to set the TabStrip's scroll state to.
     */
    public void setScrollOffsetForTesting(float offset) {
        mScrollOffset = offset;
        updateStrip();
    }

    float getMinTabWidthForTesting() {
        return mMinTabWidth;
    }

    /**
     * Displays the tab menu below the anchor tab.
     *
     * @param anchorTab The tab the menu will be anchored to
     */
    @VisibleForTesting
    void showTabMenu(StripLayoutTab anchorTab) {
        // 1. Bring the anchor tab to the foreground.
        int tabIndex = TabModelUtils.getTabIndexById(mModel, anchorTab.getId());
        TabModelUtils.setIndex(mModel, tabIndex);

        // 2. Anchor the popupMenu to the view associated with the tab
        if (mTabModelSelector.getCurrentTab() == null) return; // Vivaldi
        View tabView = TabModelUtils.getCurrentTab(mModel).getView();
        mTabMenu.setAnchorView(tabView);
        // 3. Set the vertical offset to align the tab menu with bottom of the tab strip
        int tabHeight = mManagerHost.getHeight();
        int verticalOffset =
                -(tabHeight - (int) mContext.getResources().getDimension(R.dimen.tab_strip_height));
        if (BuildConfig.IS_VIVALDI) {
            mTabMenu.setVerticalOffset(calculateTabMenuYOffset());
        } else {
            mTabMenu.setVerticalOffset(verticalOffset);
        }

        // 4. Set the horizontal offset to align the tab menu with the right side of the tab
        int horizontalOffset =
                Math.round(
                                (anchorTab.getDrawX() + anchorTab.getWidth())
                                        * mContext.getResources().getDisplayMetrics().density)
                        - mTabMenu.getWidth();
        // Cap the horizontal offset so that the tab menu doesn't get drawn off screen.
        horizontalOffset = Math.max(horizontalOffset, 0);
        mTabMenu.setHorizontalOffset(horizontalOffset);

        mTabMenu.show();
    }

    private void setScrollForScrollingTabStacker(float delta, boolean shouldAnimate, long time) {
        if (delta == 0.f) return;

        if (shouldAnimate && !mAnimationsDisabledForTesting) {
            mScroller.startScroll(
                    Math.round(mScrollOffset), 0, (int) delta, 0, time, getScrollDuration());
        } else {
            mScrollOffset = mScrollOffset + delta;
        }
    }

    /**
     * Scales the scroll duration based on the scroll offset.
     * @return the duration in ms.
     */
    private int getScrollDuration() {
        float scrollDistance = Math.abs(mScrollOffset);
        if (scrollDistance <= SCROLL_DISTANCE_SHORT) {
            return SCROLL_DURATION_MS;
        } else if (scrollDistance <= SCROLL_DISTANCE_MEDIUM) {
            return SCROLL_DURATION_MS_MEDIUM;
        } else {
            return SCROLL_DURATION_MS_LONG;
        }
    }

    /** Scrolls to the selected tab if it's not fully visible. */
    private void bringSelectedTabToVisibleArea(long time, boolean animate) {
        if (mWidth == 0) return;

        int index = getSelectedStripTabIndex();
        StripLayoutTab selectedLayoutTab =
                index >= 0 && index < mStripTabs.length ? mStripTabs[index] : null;
        if (selectedLayoutTab == null || isSelectedTabCompletelyVisible(selectedLayoutTab)) {
            return;
        }
        float delta = calculateDeltaToMakeIndexVisible(index);

        setScrollForScrollingTabStacker(delta, animate, time);
    }

    private boolean isSelectedTabCompletelyVisible(StripLayoutTab selectedTab) {
        // Note(david@vivaldi.com): We need to take the new mNewTabButtonWidth into account.
        if (ChromeApplicationImpl.isVivaldi())
            return selectedTab.isVisible() && selectedTab.getDrawX() >= 0
                    && selectedTab.getDrawX() + selectedTab.getWidth()
                    <= mWidth - mNewTabButtonWidth;

        return selectedTab.isVisible()
                && selectedTab.getDrawX() > getVisibleLeftBound() + mLeftFadeWidth
                && selectedTab.getDrawX() + selectedTab.getWidth()
                        < getVisibleRightBound() - mRightFadeWidth;
    }

    /**
     * Determines whether a drawn view is completely outside of the visible area of the tab strip.
     *
     * @param view The {@link StripLayoutView} whose visibility is determined.
     * @return {@code true} if the view is completely hidden, {@code false} otherwise.
     */
    @VisibleForTesting
    boolean isViewCompletelyHidden(StripLayoutView view) {
        return !view.isVisible() || isViewCompletelyHiddenAt(view.getDrawX(), view.getWidth());
    }

    /**
     * Determines whether a view will be completely outside of the visible area of the tab strip
     * once it reaches its ideal position.
     *
     * @param view The {@link StripLayoutView} whose visibility will be determined.
     * @return {@code true} if the view will be completely hidden, {@code false} otherwise.
     */
    private boolean willViewBeCompletelyHidden(StripLayoutView view) {
        return isViewCompletelyHiddenAt(view.getIdealX(), view.getWidth());
    }

    private boolean isViewCompletelyHiddenAt(float viewX, float viewWidth) {
        // Check if the tab is outside the visible bounds to the left...
        return viewX + viewWidth <= getVisibleLeftBound() + mLeftFadeWidth
                // ... or to the right.
                || viewX >= getVisibleRightBound() - mRightFadeWidth;
    }

    /**
     * To prevent accidental tab closures, when the close button of a tab is very close to the edge
     * of the tab strip, we hide the close button. The threshold for hiding is different based on
     * the length of the fade at the end of the strip.
     *
     * @param start Whether its the start of the tab strip.
     * @return The distance threshold from the edge of the tab strip to hide the close button.
     */
    private float getCloseBtnVisibilityThreshold(boolean start) {
        if (start) {
            // TODO(zheliooo): Add unit tests to cover start tab cases for testTabSelected in
            // StripLayoutHelperTest.
            return CLOSE_BTN_VISIBILITY_THRESHOLD_START;
        } else {
            return LocalizationUtils.isLayoutRtl() ? mLeftFadeWidth : mRightFadeWidth;
        }
    }

    /**
     * @return true if the tab menu is showing
     */
    public boolean isTabMenuShowingForTesting() {
        return mTabMenu.isShowing();
    }

    /**
     * @param menuItemId The id of the menu item to click
     */
    public void clickTabMenuItemForTesting(int menuItemId) {
        mTabMenu.performItemClick(menuItemId);
    }

    int getScrollDurationForTesting() {
        return getScrollDuration();
    }

    /**
     * @return The width of the tab strip.
     */
    float getWidthForTesting() {
        return mWidth;
    }

    /**
     * @return The width of a tab.
     */
    float getCachedTabWidthForTesting() {
        return mCachedTabWidth;
    }

    /**
     * @return The strip's minimum scroll offset.
     */
    float getMinimumScrollOffsetForTesting() {
        return mMinScrollOffset;
    }

    /**
     * @return The strip's additional minimum scroll offset for reorder mode.
     */
    float getReorderExtraMinScrollOffsetForTesting() {
        return mReorderExtraMinScrollOffset;
    }

    /**
     * @return The scroller.
     */
    StackScroller getScrollerForTesting() {
        return mScroller;
    }

    /**
     * @return An array containing the StripLayoutTabs.
     */
    StripLayoutTab[] getStripLayoutTabsForTesting() {
        return mStripTabs;
    }

    /** Set the value of mStripTabs for testing */
    void setStripLayoutTabsForTesting(StripLayoutTab[] stripTabs) {
        this.mStripTabs = stripTabs;
    }

    /**
     * @return An array containing the StripLayoutViews.
     */
    StripLayoutView[] getStripLayoutViewsForTesting() {
        return mStripViews;
    }

    /**
     * @return The amount tabs overlap.
     */
    float getTabOverlapWidthForTesting() {
        return mTabOverlapWidth;
    }

    /**
     * @return The currently interacting tab.
     */
    StripLayoutTab getInteractingTabForTesting() {
        return mInteractingTab;
    }

    /** Disables animations for testing purposes. */
    public void disableAnimationsForTesting() {
        mAnimationsDisabledForTesting = true;
    }

    Animator getRunningAnimatorForTesting() {
        return mRunningAnimator;
    }

    /**
     * @return Whether group title sliding animation is running.
     */
    boolean getGroupTitleSlidingForTesting() {
        return mGroupTitleSliding;
    }

    void setRunningAnimatorForTesting(Animator animator) {
        mRunningAnimator = animator;
    }

    protected boolean isMultiStepCloseAnimationsRunningForTesting() {
        return mMultiStepTabCloseAnimRunning;
    }

    protected float getLastReorderXForTesting() {
        return mLastReorderX;
    }

    protected void setInReorderModeForTesting(boolean inReorderMode) {
        mInReorderMode = inReorderMode;
    }

    private void setAccessibilityDescription(StripLayoutTab stripTab, Tab tab) {
        if (tab != null) setAccessibilityDescription(stripTab, tab.getTitle(), tab.isHidden());
    }

    /**
     * Set the accessibility description of a {@link StripLayoutTab}.
     *
     * @param stripTab The StripLayoutTab to set the accessibility description.
     * @param title The title of the tab.
     * @param isHidden Current visibility state of the Tab.
     */
    private void setAccessibilityDescription(
            StripLayoutTab stripTab, @Nullable String title, boolean isHidden) {
        if (stripTab == null) return;

        @StringRes int resId;
        if (mIncognito) {
            resId =
                    isHidden
                            ? R.string.accessibility_tabstrip_incognito_identifier
                            : R.string.accessibility_tabstrip_incognito_identifier_selected;
        } else {
            resId =
                    isHidden
                            ? R.string.accessibility_tabstrip_identifier
                            : R.string.accessibility_tabstrip_identifier_selected;
        }

        if (!stripTab.needsAccessibilityDescriptionUpdate(title, resId)) {
            // The resulting accessibility description would be the same as the current description,
            // so skip updating it to avoid having to read resources unnecessarily.
            return;
        }

        // Separator used to separate the different parts of the content description.
        // Not for sentence construction and hence not localized.
        final String contentDescriptionSeparator = ", ";
        final StringBuilder builder = new StringBuilder();
        if (!TextUtils.isEmpty(title)) {
            builder.append(title);
            builder.append(contentDescriptionSeparator);
        }
        builder.append(mContext.getResources().getString(resId));
        stripTab.setAccessibilityDescription(builder.toString(), title, resId);
    }

    private void performHapticFeedback(Tab tab) {
        View tabView = tab.getView();
        if (tabView == null) return;
        tabView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
    }

    protected void clearTabDragState() {
        // If the dragged tab was re-parented, it will have triggered a #computeAndUpdateTabOrders
        // call and will no longer be present in the list of tabs. If this is not the case, attempt
        // to return the dragged tab to its original position.
        StripLayoutTab selectedTab = getSelectedStripTab();
        if (selectedTab != null
                && findTabById(selectedTab.getId()) != null
                && selectedTab.isDraggedOffStrip()) {
            // Rebuild tab groups to unhide the interacting tab group as tab is restored back on tab
            // strip.
            if (ChromeFeatureList.sTabStripGroupIndicators.isEnabled()
                    && isTabRemoveDialogSkipped()
                    && isLastTabInGroup(selectedTab.getId())) {
                mTabGroupIdToHide = Tab.INVALID_TAB_ID;
                rebuildStripViews();
            }
            dragActiveClickedTabOntoStrip(LayoutManagerImpl.time(), 0.0f, false);
        }
        mLastOffsetX = 0.f;
        mActiveClickedTab = null;
    }

    StripLayoutTab getActiveClickedTabForTesting() {
        return mActiveClickedTab;
    }

    @VisibleForTesting
    boolean startDragAndDropTab(
            @NonNull StripLayoutTab clickedTab, @NonNull PointF dragStartPointF) {
        if (mTabDragSource == null || !mTabStateInitialized) return false;
        // In addition to reordering, one can drag and drop the tab beyond the strip layout view.
        Tab tabBeingDragged = getTabById(clickedTab.getId());
        boolean dragStarted = false;
        if (tabBeingDragged != null) {
            dragStarted =
                    mTabDragSource.startTabDragAction(
                            mToolbarContainerView,
                            tabBeingDragged,
                            dragStartPointF,
                            clickedTab.getDrawX(),
                            clickedTab.getWidth());
            if (dragStarted) {
                mActiveClickedTab = clickedTab;
                mLastOffsetX = 0.f;
            }
        }
        return dragStarted;
    }

    void setLastOffsetXForTesting(float lastOffsetX) {
        mLastOffsetX = lastOffsetX;
    }

    float getLastOffsetXForTesting() {
        return mLastOffsetX;
    }

    void prepareForTabDrop(
            long time,
            float currX,
            float lastX,
            boolean isSourceStrip,
            boolean draggedTabIncognito) {
        if (isSourceStrip) {
            dragActiveClickedTabOntoStrip(time, lastX, true);
        } else if (mIncognito == draggedTabIncognito) {
            updateStripForExternalTabDrop(currX);
        }
    }

    void clearForTabDrop(long time, boolean isSourceStrip, boolean draggedTabIncognito) {
        if (isSourceStrip) {
            dragActiveClickedTabOutOfStrip(time);
        } else if (mIncognito == draggedTabIncognito) {
            onUpOrCancel(time);
        }
    }

    private void dragActiveClickedTabOntoStrip(long time, float x, boolean startReorder) {
        StripLayoutTab draggedTab = getSelectedStripTab();
        assert draggedTab != null;

        finishAnimationsAndPushTabUpdates();
        draggedTab.setIsDraggedOffStrip(false);

        if (startReorder) {
            // If we're reordering, bring the tab to the correct position so we can begin reordering
            // immediately.
            draggedTab.setOffsetX(mLastOffsetX);
            draggedTab.setOffsetY(0);
            mLastOffsetX = 0.f;
            resizeTabStrip(false, false, false);
            startReorderTab(time, x, x);
        } else {
            // Else, animate the tab translating back up onto the tab strip.
            draggedTab.setWidth(0.f);
            List<Animator> animationList = resizeTabStrip(true, false, true);
            if (animationList != null) runTabAddedAnimator(animationList, draggedTab);
        }
    }

    /**
     * This method checks if the tab group delete dialog should be shown and temporarily hides the
     * tab group that may be deleted upon user confirmation.
     *
     * @param tabId The id of the interacting tab.
     * @param rootId The root id of the interacting tab.
     * @param draggingLastTabOffStrip Whether the last tab in group is being dragged off strip.
     * @param closeTab The tab being closed.
     * @param confirmationCallback The callback method to close the tab or move the tab out of group
     *     when user confirms the tab group deletion.
     */
    private void showDeleteGroupDialogAndProcessTabAction(
            int tabId,
            int rootId,
            boolean draggingLastTabOffStrip,
            boolean closeTab,
            Runnable confirmationCallback) {
        if (mTabGroupIdToHide == Tab.INVALID_TAB_ID) {
            // Hide the tab group and rebuild tab strip view.
            mTabGroupIdToHide = rootId;
            rebuildStripViews();

            // Show confirmation dialog and handle user response.
            showConfirmationDialogAndHandleResponse(
                    confirmationCallback, draggingLastTabOffStrip, closeTab);
        }
    }

    private void dragActiveClickedTabOutOfStrip(long time) {
        StripLayoutTab draggedTab = getSelectedStripTab();
        assert draggedTab != null;

        int tabId = draggedTab.getId();
        Tab tab = getTabById(tabId);

        // Show group delete dialog when the last tab in group is being dragged off tab strip.
        boolean draggingLastTabInGroup = isLastTabInGroup(tabId);
        if (draggingLastTabInGroup) {
            showDeleteGroupDialogAndProcessTabAction(
                    draggedTab.getId(),
                    tab.getRootId(),
                    /* draggingLastTabOffStrip= */ true,
                    /* closeTab= */ false,
                    () -> {
                        mTabGroupModelFilter.moveTabOutOfGroupInDirection(tabId, false);
                    });
        }

        // Store reorder state, then exit reorder mode.
        mLastOffsetX = draggedTab.getOffsetX();
        onUpOrCancel(time);
        finishAnimationsAndPushTabUpdates();

        // Skip hiding dragged tab container when tab group delete dialog is showing.
        if (!draggingLastTabInGroup || isTabRemoveDialogSkipped()) {

            // Immediately hide the dragged tab container, as if it were being translated off like a
            // closed tab.
            draggedTab.setIsDraggedOffStrip(true);
            draggedTab.setDrawX(draggedTab.getIdealX());
            draggedTab.setDrawY(mHeight);
            draggedTab.setOffsetY(mHeight);
            mMultiStepTabCloseAnimRunning = true;

            // Resize the tab strip accordingly.
            resizeStripOnTabClose(getTabById(draggedTab.getId()));
        }
    }

    void sendMoveWindowBroadcast(View view, float startXInView, float startYInView) {
        if (!TabUiFeatureUtilities.isTabDragAsWindowEnabled()) return;
        if (mWindowAndroid.getActivity().get() == null) return;

        // The start position is in the view coordinate system and related to the top left position
        // of the toolbar container view. Convert it to the screen coordinate system for window drag
        // start position.
        int[] topLeftLocation = new int[2];
        view.getLocationOnScreen(topLeftLocation);
        float startXInScreen = topLeftLocation[0] + startXInView;
        float startYInScreen = topLeftLocation[1] + startYInView;

        int taskId = ApplicationStatus.getTaskId(mWindowAndroid.getActivity().get());

        // Prepare the move window intent for the Android system to initiate move and take over the
        // user input events. The intent is ignored when not handled with no impact to existing
        // Android platforms.
        Intent intent = new Intent();
        intent.setPackage(view.getContext().getPackageName());
        intent.setAction("com.android.systemui.MOVE_WINDOW");
        intent.putExtra("MOVE_WINDOW_TASK_ID", taskId);
        intent.putExtra("MOVE_WINDOW_START_X", startXInScreen);
        intent.putExtra("MOVE_WINDOW_START_Y", startYInScreen);
        mWindowAndroid.sendBroadcast(intent);
    }

    /** Vivaldi **/
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {
        if (mOrientation != orientation) {
            mOrientation = orientation;
            scrollToSelectedTab(true);
        }
        onSizeChanged(width, height,
                mOrientation != orientation,SystemClock.uptimeMillis(),
                0, 0); // TODO(chr124) set leftPadding, rightPadding
    }

    /** Vivaldi **/
    public void scrollToSelectedTab(boolean shouldAnimate) {
        mScrollOffset = 0;
        if (mModel != null) {
            Tab tab = mModel.getTabAt(mModel.index());
            if (tab != null) {
                StripLayoutTab stripLayoutTab = findTabById(tab.getId());
                if (stripLayoutTab != null) {
                    updateStrip();
                    float delta =
                            calculateOffsetToMakeTabVisible(stripLayoutTab);
                    setScrollForScrollingTabStacker(
                            delta, shouldAnimate, SystemClock.uptimeMillis());
                }
            }
        }
    }

    /**
     * Vivaldi: Computes the tab order for main and tab stack strip.
     */
    private void computeAndUpdateTabOrders() {
        if (mIsRestoreInProgress) return;
        Tab selectedTab = mModel.getTabAt(mModel.index());
        if (mTabModelSelector != null)
            mTabGroupModelFilter =
                    (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                            .getCurrentTabModelFilter();
        if (mTabGroupModelFilter == null) return;

        List<Tab> stackedTabs = new ArrayList<>();
        List<Tab> mainTabs = new ArrayList<>();

        if (selectedTab == null) return;

        // Stacked tabs
        List<Tab> relatedTabs = mTabGroupModelFilter.getRelatedTabList(selectedTab.getId());
        if (relatedTabs.size() > 1) {
            for (int i = 0; i < relatedTabs.size(); i++) stackedTabs.add(relatedTabs.get(i));
        } else if (mIsStackStrip)
            return;

        List<Tab> ignoreList = new ArrayList<>();
        // Main tabs
        for (int i = 0; i < mModel.getCount(); i++) {
            final Tab tab = mModel.getTabAt(i);
            final List<Tab> relatedTabList = mTabGroupModelFilter.getRelatedTabList(tab.getId());
            final int lastShownTabId = mTabGroupModelFilter.getLastShownTabId(tab);
            boolean canIgnoreTab = false;
            // First check if we can ignore this tab.
            for (Tab ignoreTab : ignoreList) {
                if (ignoreTab.getId() == tab.getId()) canIgnoreTab = true;
            }
            if (canIgnoreTab) continue;
            // When tab is not a group add it.
            if (relatedTabList.size() == 0) {
                mainTabs.add(tab);
            } else {
                // Add the current tab to the appropriate list.
                for (Tab relatedTab : relatedTabList) {
                    if (relatedTab.getId() == selectedTab.getId()
                            || relatedTab.getId() == lastShownTabId)
                        mainTabs.add(relatedTab);
                    // Always add the related tab to the ignore list.
                    ignoreList.add(relatedTab);
                }
            }
        }

        // Populate all tabs which will be rendered.
        List<Tab> tabs = mIsStackStrip ? stackedTabs : mainTabs;

        List<StripLayoutTab> stripTabs = new ArrayList<>();
        for (Tab tab : tabs) {
            final int id = tab.getId();
            final StripLayoutTab oldTab = findTabById(id);
            stripTabs.add(oldTab != null ? oldTab : createStripTab(id));
            setAccessibilityDescription(stripTabs.get(stripTabs.size() - 1), tab);
        }

        int oldStripLength = mStripTabs.length;
        mStripTabs = stripTabs.toArray(new StripLayoutTab[0]);

        if (mStripTabs.length != oldStripLength) resizeTabStrip(false, false, false);

        rebuildStripViews();

        updateVisualTabOrdering();

        computeAndUpdateTabWidth(false, false, null);
    }

    /** Vivaldi **/
    public void setIsStackStrip(boolean isStackStrip) {
        mIsStackStrip = isStackStrip;
    }

    /** Vivaldi **/
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        ((VivaldiTabMenu) mTabMenu).setTabModel(mTabModelSelector);
    }

    /** Vivaldi **/
    private void setShowTabAsFavIcon() {
        mShowTabsAsFavIcon = getMinTabWidth() == TabWidthPreference.MIN_TAB_WIDTH_DP;
        mShowTabsAsFavIcon &= mCachedTabWidth <= TabWidthPreference.MIN_TAB_WIDTH_DP;
        mShowTabsAsFavIcon |= VivaldiPreferences.getSharedPreferencesManager().readBoolean(
                VivaldiPreferences.SHOW_TAB_AS_FAVICON, false);
    }

    /** Vivaldi **/
    private int getMinTabWidth() {
        return VivaldiPreferences.getSharedPreferencesManager().readInt(
                VivaldiPreferences.MIN_TAB_WIDTH, TabWidthPreference.MIN_TAB_WIDTH_DP);
    }

    /** Vivaldi **/
    public boolean showTabsAsFavIcon() {
        return mShowTabsAsFavIcon;
    }

    /** Vivaldi **/
    public float calculateOffsetToMakeTabVisible(StripLayoutTab tab) {
        int index = 0;
        int selIndex = 0;

        for (int i = 0; i < mStripTabs.length; i++) {
            if (mStripTabs[i].getId() == mTabModelSelector.getCurrentTabId()) selIndex = i;
            if (mStripTabs[i].getId() == tab.getId()) index = i;
        }

        float tabWidth = mCachedTabWidth - mTabOverlapWidth;
        int offset = 0;
        if (mShowTabsAsFavIcon && !showXButtonForBackgroundTabs())
            offset = (int) NO_X_BUTTON_OFFSET_DP;
        float optimalLeft = ((index * (tabWidth - offset)) - offset) * -1;
        if (index > selIndex) optimalLeft += tabWidth;
        return optimalLeft - mScrollOffset;
    }

    /** Vivaldi **/
    private boolean showXButtonForBackgroundTabs() {
        return VivaldiPreferences.getSharedPreferencesManager().readBoolean(
                VivaldiPreferences.SHOW_X_BUTTON_FOR_BACKGROUND_TABS, false);
    }

    /** Vivaldi **/
    public boolean shouldShowLoading() {
        mIsRestoreInProgress =
                ((TabModelSelectorImpl) mTabModelSelector).isSessionRestoreInProgress();
        return mIsRestoreInProgress;
    }

    /**
     * Vivaldi
     * Calculates the correct vertical offset for showTabMenu*
     */
    public int calculateTabMenuYOffset(){
        // Setting up the different boolean states for determining correct offset calculation.
        boolean hasRelatedTabs =
                VivaldiUtils.hasRelatedTabs(mTabModelSelector, mTabModelSelector.getCurrentTabId());
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        boolean isNative = TabModelUtils.getCurrentTab(mModel).isNativePage();

        // Setting up the constant sizes used for calculations.
        int toolBarHeight = (int) mContext.
                getResources().getDimension(R.dimen.bottom_toolbar_height);
        int tabStripHeight = (int) mContext.getResources().getDimension(R.dimen.tab_strip_height);
        int tabHeight = mManagerHost.getHeight();
        int addressFieldPadding = (int) mContext.
                getResources().getDimension(R.dimen.tab_long_press_padding);
        // Current Activity
        Activity mActivity = ((Activity) mContext);
        // Starting Offset calculation
        int verticalOffset = -(tabHeight - tabStripHeight);

        // Vivaldi - Note(nagamani@vivaldi.com): TabView height of native and other webpages are
        // different. Hence offset need to be adjusted accordingly.
        if (mIsStackStrip) verticalOffset += tabStripHeight;
        if (!mIsStackStrip && hasRelatedTabs) verticalOffset += tabStripHeight;
        // Specific case for a native page on !Tablet because sizes are different than normal pages.
        if (isNative && !isTablet) {
            verticalOffset += tabStripHeight + addressFieldPadding;
        }
        // Adjustments made for if the keyboard is showing
        if (KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                    mContext, mToolbarContainerView)
                && !isTablet && VivaldiUtils.isTopToolbarOn()) {
            // Keeps the shown TabMenu from changing position if keyboard is transitioning from/to
            // showing
            mActivity.getWindow().setSoftInputMode(
                    WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
            // Special considerations if this happens with a native page
            if (isNative) {
                verticalOffset -= toolBarHeight;
            }
            // Checks if this is the first longPress done, the tabHeight on first longPress differs
            // from succeeding presses. Adds missing keyboardHeight into the offset calculation to
            // balance this.
            if (mActivity.findViewById(R.id.sheet_container).getHeight() != tabHeight) {
                verticalOffset -=
                        (KeyboardVisibilityDelegate.getInstance().calculateTotalKeyboardHeight(
                                mToolbarContainerView));
            }
        }
        // Different calculations if AddressBar at bottom.
        if (!VivaldiUtils.isTopToolbarOn()) {
            // Native pages are smaller than normal pages and hasRelatedTabs/mIsStackStrip
            // indicate if the pressed tab is on the TabStackStrip or the main tab being pressed.
            if (isNative){
                verticalOffset = 0;
                verticalOffset -= tabStripHeight + addressFieldPadding + toolBarHeight;
                if (mIsStackStrip || hasRelatedTabs) {
                    verticalOffset -= toolBarHeight;
                }
            } else {
                // Offsets negative since we want to go from the bottom up
                verticalOffset = -((ChromeActivity) mContext)
                                          .getBrowserControlsManager()
                                          .getBottomControlsHeight()
                        - toolBarHeight;
                // Extra considerations if the longPress is on a TabStack / TabStrip
                if (mIsStackStrip || hasRelatedTabs) {
                    verticalOffset -= tabStripHeight + addressFieldPadding + toolBarHeight;
                } else {
                    verticalOffset -= toolBarHeight;
                }
            }
        }
        return verticalOffset;
    }

    /** Vivaldi reset Soft Input mode on dismiss longpress **/
    public void onDismiss() {
        if (((Activity) mContext).getWindow().getAttributes().softInputMode
                == WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING) {
            ((Activity) mContext).getWindow().setSoftInputMode(
                    WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN);
        }
    }
}
