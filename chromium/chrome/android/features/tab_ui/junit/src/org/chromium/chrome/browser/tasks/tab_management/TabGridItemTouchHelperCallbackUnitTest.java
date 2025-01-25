// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

import android.graphics.Canvas;
import android.view.View;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Tests for {@link TabGridItemTouchHelperCallback}. */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final
        })
public class TabGridItemTouchHelperCallbackUnitTest {

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 357;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int POSITION4 = 3;
    private static final float THRESHOLD = 2f;
    private static final float MERGE_AREA_THRESHOLD = 0.5f;

    @Mock Canvas mCanvas;
    @Mock RecyclerView mRecyclerView;
    @Mock RecyclerView.Adapter mAdapter;
    @Mock TabModel mTabModel;
    @Mock TabListMediator.TabActionListener mTabClosedListener;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
    @Mock TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    @Mock Profile mProfile;
    @Mock Tracker mTracker;
    @Mock GridLayoutManager mGridLayoutManager;
    @Mock TabGroupCreationDialogManager mTabGroupCreationDialogManager;

    @Mock
    TabGridItemTouchHelperCallback.OnLongPressTabItemEventListener mOnLongPressTabItemEventListener;

    private final ObservableSupplierImpl<TabModelFilter> mTabModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private SimpleRecyclerViewAdapter.ViewHolder mMockViewHolder1;
    private SimpleRecyclerViewAdapter.ViewHolder mMockViewHolder2;
    private RecyclerView.ViewHolder mFakeViewHolder1;
    private RecyclerView.ViewHolder mFakeViewHolder2;
    private RecyclerView.ViewHolder mFakeViewHolder3;
    private RecyclerView.ViewHolder mFakeViewHolder4;
    private View mItemView1;
    private View mItemView2;
    private View mItemView3;
    private View mItemView4;
    private TabGridItemTouchHelperCallback mItemTouchHelperCallback;
    private TabListModel mModel;

    @Before
    public void setUp() {

        MockitoAnnotations.initMocks(this);

        Tab tab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        Tab tab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE);
        mMockViewHolder1 = prepareMockViewHolder(TAB1_ID, POSITION1);
        mMockViewHolder2 = prepareMockViewHolder(TAB2_ID, POSITION2);
        // Mock four cards in a grid layout. Each card is of width 4 and height 4. Both the side
        // gaps and top gaps between adjacent cards are 1.
        mItemView1 = prepareItemView(0, 0, 4, 4);
        mItemView2 = prepareItemView(5, 0, 9, 4);
        mItemView3 = prepareItemView(0, 5, 4, 9);
        mItemView4 = prepareItemView(5, 5, 9, 9);
        mFakeViewHolder1 = prepareFakeViewHolder(mItemView1);
        mFakeViewHolder2 = prepareFakeViewHolder(mItemView2);
        mFakeViewHolder3 = prepareFakeViewHolder(mItemView3);
        mFakeViewHolder4 = prepareFakeViewHolder(mItemView4);

        mTabModelFilterSupplier.set(mTabGroupModelFilter);
        doReturn(mProfile).when(mTabModel).getProfile();
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(tab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(tab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(POSITION3);
        doReturn(tab4).when(mTabModel).getTabAt(POSITION4);
        doReturn(4).when(mTabModel).getCount();
        doReturn(tab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(POSITION3);
        doReturn(tab4).when(mTabGroupModelFilter).getTabAt(POSITION4);
        doReturn(TAB1_ID).when(tab1).getRootId();
        doReturn(TAB2_ID).when(tab2).getRootId();
        doReturn(TAB3_ID).when(tab3).getRootId();
        doReturn(TAB4_ID).when(tab4).getRootId();
        setupRecyclerView();

        mModel = new TabListModel();
        setupItemTouchHelperCallback(false);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    private void setupRecyclerView() {
        doReturn(mAdapter).when(mRecyclerView).getAdapter();
        doReturn(mGridLayoutManager).when(mRecyclerView).getLayoutManager();
        doReturn(12).when(mRecyclerView).getBottom();
        doReturn(4).when(mRecyclerView).getChildCount();
        doReturn(4).when(mAdapter).getItemCount();
        when(mRecyclerView.getChildAt(POSITION1)).thenReturn(mItemView1);
        when(mRecyclerView.getChildAt(POSITION2)).thenReturn(mItemView2);
        when(mRecyclerView.getChildAt(POSITION3)).thenReturn(mItemView3);
        when(mRecyclerView.getChildAt(POSITION4)).thenReturn(mItemView4);
        doReturn(mRecyclerView).when(mItemView1).getParent();
        doReturn(mRecyclerView).when(mItemView2).getParent();
        doReturn(mRecyclerView).when(mItemView3).getParent();
        doReturn(mRecyclerView).when(mItemView4).getParent();
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION1))
                .thenReturn(mFakeViewHolder1);
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION2))
                .thenReturn(mFakeViewHolder2);
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION3))
                .thenReturn(mFakeViewHolder3);
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION4))
                .thenReturn(mFakeViewHolder4);
    }

    private void setupItemTouchHelperCallback(boolean isDialog) {
        mItemTouchHelperCallback =
                new TabGridItemTouchHelperCallback(
                        ContextUtils.getApplicationContext(),
                        mTabGroupCreationDialogManager,
                        mModel,
                        mTabModelFilterSupplier,
                        mTabClosedListener,
                        isDialog ? mTabGridDialogHandler : null,
                        "",
                        !isDialog,
                        TabListMode.GRID);
        mItemTouchHelperCallback.setOnLongPressTabItemEventListener(
                mOnLongPressTabItemEventListener);
        mItemTouchHelperCallback.setupCallback(THRESHOLD, MERGE_AREA_THRESHOLD, THRESHOLD);
        mItemTouchHelperCallback.getMovementFlags(mRecyclerView, mMockViewHolder1);
    }

    @Test
    public void initializesWithCurrentTabs() {
        initAndAssertAllProperties();
    }

    @Test
    public void onStartDraggingTab() {
        initAndAssertAllProperties();

        assertThat(
                mModel.get(0).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.CARD_RESTORE));
        assertThat(mModel.get(0).model.get(CARD_ALPHA), equalTo(1f));

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_DRAG);

        assertThat(
                mModel.get(0).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.SELECTED_CARD_ZOOM_IN));
        assertThat(mModel.get(0).model.get(CARD_ALPHA), equalTo(0.8f));
    }

    @Test
    public void onSwipeTab_Delete() {
        initAndAssertAllProperties();

        mItemTouchHelperCallback.onSwiped(mMockViewHolder1, POSITION1);

        verify(mTabClosedListener).run(null, TAB1_ID);
    }

    @Test
    public void onReleaseTab_NoMerge() {
        initAndAssertAllProperties();

        // Simulate the selection of card#1 in TabListModel.
        mModel.get(0)
                .model
                .set(
                        TabProperties.CARD_ANIMATION_STATUS,
                        TabGridView.AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(0).model.set(CARD_ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        assertThat(
                mModel.get(0).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.SELECTED_CARD_ZOOM_OUT));
        assertThat(mModel.get(0).model.get(CARD_ALPHA), equalTo(1f));
        assertThat(
                mModel.get(1).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.CARD_RESTORE));
        assertThat(mModel.get(1).model.get(CARD_ALPHA), equalTo(1f));
    }

    @Test
    public void onReleaseTab_MergeBackward() {
        initAndAssertAllProperties();

        // Simulate the selection of card#1 in TabListModel.
        mModel.get(0)
                .model
                .set(
                        TabProperties.CARD_ANIMATION_STATUS,
                        TabGridView.AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(0).model.set(CARD_ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Simulate hovering on card#2.
        mModel.get(1)
                .model
                .set(
                        TabProperties.CARD_ANIMATION_STATUS,
                        TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION2);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(TAB1_ID, TAB2_ID);
        verify(mGridLayoutManager).removeView(mItemView1);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
        assertThat(
                mModel.get(0).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_OUT));
    }

    @Test
    public void onReleaseTab_MergeForward() {
        initAndAssertAllProperties();

        // Simulate the selection of card#2 in TabListModel.
        mModel.get(1)
                .model
                .set(
                        TabProperties.CARD_ANIMATION_STATUS,
                        TabGridView.AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(1).model.set(CARD_ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);

        // Simulate hovering on card#1.
        mModel.get(0)
                .model
                .set(
                        TabProperties.CARD_ANIMATION_STATUS,
                        TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mGridLayoutManager).removeView(mItemView2);
        verify(mTabGroupModelFilter).mergeTabsToGroup(TAB2_ID, TAB1_ID);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
        assertThat(
                mModel.get(0).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_OUT));
    }

    @Test
    public void onReleaseTab_Merge_CleanOut() {
        initAndAssertAllProperties();

        // Simulate the selection of card#2 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);

        // Simulate hovering on card#1.
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);

        // Simulate that the recyclerView is cleaned out when the drop-to-merge happens.
        doReturn(null).when(mRecyclerView).findViewHolderForAdapterPosition(anyInt());

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mGridLayoutManager, never()).removeView(mItemView2);
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(TAB2_ID, TAB1_ID);
        verify(mTracker, never()).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
    }

    @Test
    public void onReleaseTab_Merge_Scrolling() {
        initAndAssertAllProperties();

        // Simulate the selection of card#2 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);

        // Simulate hovering on card#1.
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);

        // Simulate that the recyclerView is scrolling when the drop-to-merge happens.
        when(mRecyclerView.isComputingLayout()).thenReturn(true);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mGridLayoutManager, never()).removeView(mItemView2);
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(TAB2_ID, TAB1_ID);
        verify(mTracker, never()).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
    }

    @Test
    public void onReleaseTab_UngroupBar_Hide() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(true);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HIDE);
    }

    @Test
    public void onReleaseTab_Ungroup() {
        initAndAssertAllProperties();

        setupItemTouchHelperCallback(true);
        mItemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).moveTabOutOfGroup(TAB1_ID);
        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HIDE);
        verify(mGridLayoutManager).removeView(mItemView1);
    }

    @Test
    public void onReleaseTab_Ungroup_Scrolling() {
        initAndAssertAllProperties();

        setupItemTouchHelperCallback(true);
        mItemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);

        // Simulate that the recyclerView is scrolling when the drop-to-ungroup happens.
        when(mRecyclerView.isComputingLayout()).thenReturn(true);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter, never()).moveTabOutOfGroup(TAB1_ID);
        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HIDE);
        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void onReleaseTab_Ungroup_CleanOut() {
        initAndAssertAllProperties();

        setupItemTouchHelperCallback(true);
        mItemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);

        // Simulate that the recyclerView is cleaned out when the drop-to-ungroup happens.
        doReturn(null).when(mRecyclerView).findViewHolderForAdapterPosition(anyInt());

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter, never()).moveTabOutOfGroup(TAB1_ID);
        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HIDE);
        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void onDragTab_Hovered_GTS_Horizontal() {
        initAndAssertAllProperties();

        // Drag card#1 rightwards to hover on card#2.
        verifyDrag(
                mFakeViewHolder1,
                5,
                0,
                POSITION2,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#2 leftwards to hover on card#1.
        verifyDrag(
                mFakeViewHolder2,
                -5,
                0,
                POSITION1,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_Hovered_GTS_Vertical() {
        initAndAssertAllProperties();

        // Drag card#1 downwards to hover on card#3.
        verifyDrag(
                mFakeViewHolder1,
                0,
                5,
                POSITION3,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#3 upwards to hover on card#1.
        verifyDrag(
                mFakeViewHolder3,
                0,
                -5,
                POSITION1,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_Hovered_GTS_Diagonal() {
        initAndAssertAllProperties();

        // Drag card#1 diagonally to hover on card#4.
        verifyDrag(
                mFakeViewHolder1,
                5,
                5,
                POSITION4,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#4 diagonally to hover on card#1.
        verifyDrag(
                mFakeViewHolder4,
                -5,
                -5,
                POSITION1,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#2 diagonally to hover on card#3.
        verifyDrag(
                mFakeViewHolder2,
                -5,
                5,
                POSITION3,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#3 diagonally to hover on card#2.
        verifyDrag(
                mFakeViewHolder3,
                5,
                -5,
                POSITION2,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_NotHovered_GTS_Horizontal() {
        initAndAssertAllProperties();

        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mFakeViewHolder1, 3, 0, POSITION2, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder2, -3, 0, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);
        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mFakeViewHolder1, 7, 0, POSITION2, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder2, -7, 0, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_NotHovered_GTS_Vertical() {
        initAndAssertAllProperties();

        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mFakeViewHolder1, 0, 3, POSITION3, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder3, 0, -3, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);
        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mFakeViewHolder1, 0, 7, POSITION3, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder3, 0, -7, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_NotHovered_GTS_Diagonal() {
        initAndAssertAllProperties();

        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mFakeViewHolder1, 3, 4, POSITION4, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder1, 4, 3, POSITION4, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder4, -4, -3, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder4, -3, -4, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);
        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mFakeViewHolder1, 7, 6, POSITION4, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder1, 6, 7, POSITION4, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder4, -6, -7, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder4, -7, -6, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_Restore_Hovered_GTS() {
        initAndAssertAllProperties();

        // Simulate the process of hovering card#1 on card#2.
        verifyDrag(
                mFakeViewHolder1,
                5,
                0,
                POSITION2,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Continue to drag card#1 horizontally so that it is no longer hovering on card#2.
        verifyDrag(
                mFakeViewHolder1,
                10,
                0,
                POSITION2,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_OUT);

        // Simulate the process of hovering card#1 on card#3.
        verifyDrag(
                mFakeViewHolder1,
                0,
                5,
                POSITION3,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Continue to drag card#1 vertically so that it is no longer hovering on card#3.
        verifyDrag(
                mFakeViewHolder1,
                0,
                10,
                POSITION3,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_OUT);

        // Simulate the process of hovering card#1 on card#4.
        verifyDrag(
                mFakeViewHolder1,
                5,
                5,
                POSITION4,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Continue to drag card#1 diagonally so that it is no longer hovering on card#4.
        verifyDrag(
                mFakeViewHolder1,
                10,
                10,
                POSITION4,
                TabGridView.AnimationStatus.HOVERED_CARD_ZOOM_OUT);
    }

    @Test
    public void onDragTab_Hovered_NonGTS() {
        initAndAssertAllProperties();
        // Suppose drag happens in components other than GTS.
        mItemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(false);

        // Hovering shouldn't make any difference.
        verifyDrag(mFakeViewHolder1, 5, 0, POSITION2, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder2, -5, 0, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);

        verifyDrag(mFakeViewHolder1, 0, 5, POSITION3, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder3, 0, -5, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);

        verifyDrag(mFakeViewHolder1, 5, 5, POSITION4, TabGridView.AnimationStatus.CARD_RESTORE);
        verifyDrag(mFakeViewHolder4, -5, -5, POSITION1, TabGridView.AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_Ungroup() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(true);

        // Simulate dragging card#1 down to the ungroup bar.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mFakeViewHolder1,
                0,
                7,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HOVERED);

        // Simulate dragging card#3 down to the ungroup bar.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mFakeViewHolder1,
                0,
                2,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HOVERED);
    }

    @Test
    public void onDragTab_NotUngroup() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(true);

        // With recyclerview bottom equal to 12 and ungroup threshold equal to 2, any drag with
        // itemview.bottom + dY <= 10 should never trigger ungroup.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mFakeViewHolder1,
                0,
                6,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        // Simulate dragging card#3 down to the ungroup bar.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION3);
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mFakeViewHolder3,
                0,
                1,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        verify(mTabGridDialogHandler, times(2))
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.SHOW);

        verify(mTabGridDialogHandler, never())
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HOVERED);
    }

    @Test
    public void onDragTab_AfterRelease() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(true);

        // Simulate that drop is finished, but there are some extra onChildDraw calls.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(TabModel.INVALID_TAB_INDEX);

        // Simulate dragging the tab down to the ungroup bar.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mFakeViewHolder1,
                0,
                8,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        verify(mTabGridDialogHandler, never()).updateUngroupBarStatus(anyInt());
    }

    @Test
    public void onDraggingAnimationEnd_Stale() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(false);
        // Mock that when the dragging animation ends, the recyclerView is in an inconsistent state:
        // recyclerView should be cleaned out, yet the animated view is stale.
        mItemTouchHelperCallback.setCurrentActionStateForTesting(ItemTouchHelper.ACTION_STATE_DRAG);
        doReturn(1).when(mRecyclerView).getChildCount();
        doReturn(0).when(mAdapter).getItemCount();

        mItemTouchHelperCallback.clearView(mRecyclerView, mFakeViewHolder1);

        verify(mGridLayoutManager).removeView(mItemView1);
    }

    @Test
    public void onDraggingAnimationEnd_NonStale() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(false);
        // Mock that when the dragging animation ends, the recyclerView is in consistent state.
        mItemTouchHelperCallback.setCurrentActionStateForTesting(ItemTouchHelper.ACTION_STATE_DRAG);
        assertThat(mRecyclerView.getChildCount(), equalTo(mAdapter.getItemCount()));

        mItemTouchHelperCallback.clearView(mRecyclerView, mFakeViewHolder1);

        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void messageItemNotDraggable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.MESSAGE);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.hasDragFlagForTesting(mRecyclerView, mMockViewHolder1));

        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.LARGE_MESSAGE);

        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.hasDragFlagForTesting(mRecyclerView, mMockViewHolder1));

        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.CUSTOM_MESSAGE);
        mMockViewHolder1.model = Mockito.mock(PropertyModel.class);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.hasDragFlagForTesting(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void messageItemSwipeable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.MESSAGE);
        setupItemTouchHelperCallback(false);
        assertTrue(mItemTouchHelperCallback.hasSwipeFlag(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void messageItemSwipeable_archivedTabsMessageNotSwipable() {
        PropertyModel model = Mockito.mock(PropertyModel.class);
        when(model.get(MESSAGE_TYPE)).thenReturn(MessageType.ARCHIVED_TABS_MESSAGE);
        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.CUSTOM_MESSAGE);
        mMockViewHolder1.model = model;

        setupItemTouchHelperCallback(false);
        assertFalse(mItemTouchHelperCallback.hasSwipeFlag(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void messageItemNotDropable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.MESSAGE);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mMockViewHolder2, mMockViewHolder1));
    }

    @Test(expected = AssertionError.class)
    public void messageItemOnMoveFail() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.MESSAGE);
        setupItemTouchHelperCallback(false);
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder1, mMockViewHolder2);
    }

    @Test
    public void largeMessageItemNotDraggable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.LARGE_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.hasDragFlagForTesting(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void largeMessageItemSwipeable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.LARGE_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertTrue(mItemTouchHelperCallback.hasSwipeFlag(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void largeMessageItemNotDropable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.LARGE_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mMockViewHolder2, mMockViewHolder1));
    }

    @Test(expected = AssertionError.class)
    public void largeMessageItemOnMoveFail() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(TabProperties.UiType.LARGE_MESSAGE);
        setupItemTouchHelperCallback(false);
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder1, mMockViewHolder2);
    }

    @Test
    public void onLongPress_blockNextAction() {
        initAndAssertAllProperties();

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        assertTrue(mItemTouchHelperCallback.shouldBlockAction());
    }

    @Test
    public void onLongPressWithDrag_dontBlockNextAction() {
        initAndAssertAllProperties();

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Pretend a drag started.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mFakeViewHolder1,
                10,
                5,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        assertFalse(mItemTouchHelperCallback.shouldBlockAction());
    }

    @Test
    public void onLongPress_triggerTabListEditor() {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);

        initAndAssertAllProperties();

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mOnLongPressTabItemEventListener).onLongPressEvent(TAB1_ID);
        assertTrue(mItemTouchHelperCallback.shouldBlockAction());
    }

    @Test
    public void onLongPress_preventTriggerTabListEditor() {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);

        initAndAssertAllProperties();

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Simulate hovering on card#2.
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION2);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mOnLongPressTabItemEventListener, never()).onLongPressEvent(TAB1_ID);
        assertFalse(mItemTouchHelperCallback.shouldBlockAction());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID
    })
    public void onTabMergeToGroup_willMergingCreateNewGroup() {
        initAndAssertAllProperties();
        doReturn(true).when(mTabGroupModelFilter).willMergingCreateNewGroup(any());

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Simulate hovering on card#2.
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION2);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(TAB1_ID, TAB2_ID);
        verify(mTabGroupCreationDialogManager).showDialog(TAB2_ID, mTabGroupModelFilter);
    }

    private void verifyDrag(
            RecyclerView.ViewHolder viewHolder, float dX, float dY, int targetIndex, int status) {
        // Simulate the process of dragging one card to a position.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                viewHolder,
                dX,
                dY,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        // Verify the card in target index is in correct status.
        assertThat(
                mModel.get(targetIndex).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(status));
    }

    private void initAndAssertAllProperties() {
        for (int i = 0; i < mTabModel.getCount(); i++) {
            Tab tab = mTabModel.getTabAt(i);
            addTabInfoModel(tab);
        }

        assertThat(mModel.size(), equalTo(4));

        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(3).model.get(TabProperties.TAB_ID), equalTo(TAB4_ID));

        assertThat(
                mModel.get(0).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.CARD_RESTORE));
        assertThat(
                mModel.get(1).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.CARD_RESTORE));
        assertThat(
                mModel.get(2).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.CARD_RESTORE));
        assertThat(
                mModel.get(3).model.get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(TabGridView.AnimationStatus.CARD_RESTORE));

        assertThat(mModel.get(0).model.get(CARD_ALPHA), equalTo(1f));
        assertThat(mModel.get(1).model.get(CARD_ALPHA), equalTo(1f));
        assertThat(mModel.get(2).model.get(CARD_ALPHA), equalTo(1f));
        assertThat(mModel.get(3).model.get(CARD_ALPHA), equalTo(1f));
    }

    private void addTabInfoModel(Tab tab) {
        PropertyKey[] testKeysTabGrid =
                new PropertyKey[] {
                    TabProperties.TAB_ID, TabProperties.CARD_ANIMATION_STATUS, CARD_ALPHA, CARD_TYPE
                };
        PropertyModel tabInfo =
                new PropertyModel.Builder(testKeysTabGrid)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(
                                TabProperties.CARD_ANIMATION_STATUS,
                                TabGridView.AnimationStatus.CARD_RESTORE)
                        .with(CARD_ALPHA, 1f)
                        .with(CARD_TYPE, TAB)
                        .build();
        mModel.add(new MVCListAdapter.ListItem(0, tabInfo));
    }

    private Tab prepareTab(int id, String title) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        doReturn(title).when(tab).getTitle();
        return tab;
    }

    private SimpleRecyclerViewAdapter.ViewHolder prepareMockViewHolder(int id, int position) {
        SimpleRecyclerViewAdapter.ViewHolder viewHolder =
                mock(SimpleRecyclerViewAdapter.ViewHolder.class);
        viewHolder.model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, id)
                        .with(CARD_TYPE, TAB)
                        .build();
        return viewHolder;
    }

    private View prepareItemView(int left, int top, int right, int bottom) {
        View view = mock(View.class);
        doReturn(left).when(view).getLeft();
        doReturn(top).when(view).getTop();
        doReturn(right).when(view).getRight();
        doReturn(bottom).when(view).getBottom();
        doReturn(right - left).when(view).getWidth();
        doReturn(bottom - top).when(view).getHeight();
        return view;
    }

    private RecyclerView.ViewHolder prepareFakeViewHolder(View itemView) {
        return new RecyclerView.ViewHolder(itemView) {};
    }
}
