// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.hamcrest.MockitoHamcrest.intThat;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.Window;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.base.WindowAndroid;

/**
 * Tests the EdgeToEdgeController code. Ideally this would include {@link EdgeToEdgeController},
 * {@link EdgeToEdgeControllerFactory}, along with {@link EdgeToEdgeControllerImpl}
 */
@DisableIf.Build(sdk_is_less_than = VERSION_CODES.R)
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        sdk = VERSION_CODES.R,
        manifest = Config.NONE,
        shadows = EdgeToEdgeControllerTest.ShadowEdgeToEdgeControllerFactory.class)
public class EdgeToEdgeControllerTest {

    private static final int TOP_INSET = 113;
    private static final int BOTTOM_INSET = 59;
    private static final Insets NAVIGATION_BAR_INSETS = Insets.of(0, 0, 0, BOTTOM_INSET);
    private static final Insets STATUS_BAR_INSETS = Insets.of(0, TOP_INSET, 0, 0);
    private static final Insets SYSTEM_INSETS = Insets.of(0, TOP_INSET, 0, BOTTOM_INSET);
    private static final Insets IME_INSETS = Insets.of(0, 0, 0, 0);

    private static final WindowInsetsCompat SYSTEM_BARS_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), NAVIGATION_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), SYSTEM_INSETS)
                    .setInsets(WindowInsetsCompat.Type.ime(), IME_INSETS)
                    .build();

    private Activity mActivity;
    private EdgeToEdgeControllerImpl mEdgeToEdgeControllerImpl;

    private ObservableSupplierImpl<Tab> mTabProvider;

    private UserDataHost mTabDataHost = new UserDataHost();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private InsetObserver mInsetObserver;
    @Mock private Tab mTab;

    @Mock private WebContents mWebContents;

    @Mock private EdgeToEdgeOSWrapper mOsWrapper;

    @Captor private ArgumentCaptor<WindowInsetsConsumer> mWindowInsetsListenerCaptor;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverArgumentCaptor;
    @Captor private ArgumentCaptor<Rect> mSafeAreaRectCaptor;

    @Mock private View mViewMock;

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private LayoutManager mLayoutManager;

    @Implements(EdgeToEdgeControllerFactory.class)
    static class ShadowEdgeToEdgeControllerFactory extends EdgeToEdgeControllerFactory {
        @Implementation
        protected static boolean isGestureNavigationMode(Window window) {
            return true;
        }
    }

    @Before
    public void setUp() {
        ChromeFeatureList.sDrawEdgeToEdge.setForTesting(true);
        ChromeFeatureList.sEdgeToEdgeBottomChin.setForTesting(true);
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(false);
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(false);

        MockitoAnnotations.openMocks(this);
        when(mWindowAndroid.getInsetObserver()).thenReturn(mInsetObserver);
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(SYSTEM_BARS_WINDOW_INSETS);

        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mTabProvider = new ObservableSupplierImpl<>();

        doNothing().when(mTab).addObserver(any());
        when(mTab.getUserDataHost()).thenReturn(mTabDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);

        doNothing().when(mOsWrapper).setDecorFitsSystemWindows(any(), anyBoolean());
        doNothing().when(mOsWrapper).setPadding(any(), anyInt(), anyInt(), anyInt(), anyInt());
        doNothing().when(mInsetObserver).addInsetsConsumer(mWindowInsetsListenerCaptor.capture());
        doAnswer(
                        invocationOnMock -> {
                            int bottomInset = invocationOnMock.getArgument(0);
                            mWebContents.setDisplayCutoutSafeArea(new Rect(0, 0, 0, bottomInset));
                            return null;
                        })
                .when(mInsetObserver)
                .updateBottomInsetForEdgeToEdge(anyInt());

        mEdgeToEdgeControllerImpl =
                new EdgeToEdgeControllerImpl(
                        mActivity,
                        mWindowAndroid,
                        mTabProvider,
                        mOsWrapper,
                        mBrowserControlsStateProvider,
                        mLayoutManager);
        assertNotNull(mEdgeToEdgeControllerImpl);
        verify(mOsWrapper, times(1)).setDecorFitsSystemWindows(any(), eq(false));
        verify(mOsWrapper, times(1))
                .setPadding(
                        any(),
                        eq(0),
                        intThat(Matchers.greaterThan(0)),
                        eq(0),
                        intThat(Matchers.greaterThan(0)));
        verify(mInsetObserver, times(1)).addInsetsConsumer(any());
        EdgeToEdgeControllerFactory.setHas3ButtonNavBar(false);
    }

    @After
    public void tearDown() {
        mEdgeToEdgeControllerImpl.destroy();
        mEdgeToEdgeControllerImpl = null;
        mTabProvider = null;
    }

    @Test
    public void drawEdgeToEdge_ToEdgeAndToNormal() {
        mEdgeToEdgeControllerImpl.drawToEdge(true);
        assertToEdgeExpectations();

        mEdgeToEdgeControllerImpl.drawToEdge(false);
        assertToNormalExpectations();
    }

    /** Test nothing is done when the Feature is not enabled. */
    @Test
    public void onObservingDifferentTab_default() {
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertNoChangeExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToNative() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(true);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertToEdgeExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToTabSwitcher() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        // For the Tab Switcher we need to switch from some non-null Tab to null.
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        Tab nullForTabSwitcher = null;
        mTabProvider.set(nullForTabSwitcher);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertToEdgeExpectations();
    }

    @Test
    @SuppressLint("NewApi")
    public void onObservingDifferentTab_changeToWebDisabled() {
        // First go ToEdge by invoking the changeToTabSwitcher test logic.
        mEdgeToEdgeControllerImpl.setIsOptedIntoEdgeToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);

        // Now test that a Web page causes a transition ToNormal (when Web forcing is disabled).
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertFalse(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertToNormalExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToWebEnabled() {
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);
    }

    @Test
    public void onObservingDifferentTab_changeToWebEnabled_SetsDecor() {
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);
    }

    @Test
    public void onObservingDifferentTab_viewportFitChanged() {
        // Start with web always-enabled.
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);
        // Enable the bottom chin.
        ChromeFeatureList.sEdgeToEdgeBottomChin.setForTesting(true);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);

        // Now switch the viewport-fit value of that page back and forth,
        // with web NOT always enabled. The page opt-in should switch with the viewport fit value,
        // but the page should still draw toEdge to properly show the bottom chin.
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(false);
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.AUTO);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());

        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());

        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.AUTO);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
    }

    /** Test the OSWrapper implementation without mocking it. Native ToNormal. */
    @Test
    public void onObservingDifferentTab_osWrapperImplToNormal() {
        ObservableSupplierImpl liveSupplier = new ObservableSupplierImpl();
        EdgeToEdgeControllerImpl liveController =
                (EdgeToEdgeControllerImpl)
                        EdgeToEdgeControllerFactory.create(
                                mActivity,
                                mWindowAndroid,
                                liveSupplier,
                                mBrowserControlsStateProvider,
                                mLayoutManager);
        assertNotNull(liveController);
        liveController.setIsOptedIntoEdgeToEdgeForTesting(true);
        liveController.setIsDrawingToEdgeForTesting(true);
        liveController.setSystemInsetsForTesting(SYSTEM_INSETS);
        when(mTab.isNativePage()).thenReturn(false);
        liveSupplier.set(mTab);
        verifyInteractions(mTab);
        assertFalse(liveController.isPageOptedIntoEdgeToEdge());
        // Check the Navigation Bar color, as an indicator that we really changed the window,
        // since we didn't use the OS Wrapper mock.
        assertNotEquals(Color.TRANSPARENT, mActivity.getWindow().getNavigationBarColor());
    }

    /** Test switching to the Tab Switcher, which uses a null Tab. */
    @Test
    public void onObservingDifferentTab_nullTabSwitcher() {
        mEdgeToEdgeControllerImpl.setIsOptedIntoEdgeToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.onTabSwitched(null);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        // Check the Navigation Bar color, as an indicator that we really changed the window.
        assertNotEquals(Color.TRANSPARENT, mActivity.getWindow().getNavigationBarColor());
        // Pad the top and the bottom to keep it all normal.
        verify(mOsWrapper, times(2))
                .setPadding(
                        any(),
                        eq(0),
                        intThat(Matchers.greaterThan(0)),
                        eq(0),
                        intThat(Matchers.greaterThan(0)));
    }

    /** Test that we update WebContentsObservers when a Tab changes WebContents. */
    @Test
    public void onTabSwitched_onWebContentsSwapped() {
        // Standard setup of a Web Tab ToEdge
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);

        // Grab the WebContentsObserver, and setup.
        WebContentsObserver initialWebContentsObserver =
                mEdgeToEdgeControllerImpl.getWebContentsObserver();
        when(mTab.getWebContents()).thenReturn(mWebContents);
        doNothing().when(mTab).addObserver(mTabObserverArgumentCaptor.capture());

        // When onTabSwitched is called, we capture the TabObserver created for the Tab.
        mEdgeToEdgeControllerImpl.onTabSwitched(mTab);
        // Simulate the tab getting new WebContents.
        mTabObserverArgumentCaptor.getValue().onWebContentsSwapped(mTab, true, true);
        assertNotNull(initialWebContentsObserver);
        assertNotNull(mEdgeToEdgeControllerImpl.getWebContentsObserver());
        assertNotEquals(
                "onWebContentsSwapped not handling WebContentsObservers correctly",
                initialWebContentsObserver,
                mEdgeToEdgeControllerImpl.getWebContentsObserver());
    }

    @Test
    public void onTabSwitched_onContentChanged() {
        // Start with a Tab with no WebContents
        when(mTab.getWebContents()).thenReturn(null);
        doNothing().when(mTab).addObserver(mTabObserverArgumentCaptor.capture());

        // When onTabSwitched is called, we capture the TabObserver created for the Tab.
        mEdgeToEdgeControllerImpl.onTabSwitched(mTab);
        TabObserver tabObserver = mTabObserverArgumentCaptor.getValue();
        assertNotNull(tabObserver);
        WebContentsObserver initialWebContentsObserver =
                mEdgeToEdgeControllerImpl.getWebContentsObserver();
        assertNull(initialWebContentsObserver);

        // Simulate the tab getting new WebContents.
        when(mTab.getWebContents()).thenReturn(mWebContents);
        tabObserver.onContentChanged(mTab);
        WebContentsObserver firstObserver = mEdgeToEdgeControllerImpl.getWebContentsObserver();
        assertNotNull(firstObserver);

        // Make sure we can still swap to another WebContents.
        tabObserver.onWebContentsSwapped(mTab, true, true);
        WebContentsObserver secondObserver = mEdgeToEdgeControllerImpl.getWebContentsObserver();
        assertNotNull(secondObserver);
        assertNotEquals(firstObserver, secondObserver);
    }

    @Test
    public void onObservingDifferentTab_simple() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        // For the Tab Switcher we need to switch from some non-null Tab to null.
        when(mTab.isNativePage()).thenReturn(true);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);
    }

    @Test
    public void isSupportedConfiguration_default() {
        assertTrue(
                "The default setup should be a supported configuration but it not!",
                EdgeToEdgeControllerFactory.isSupportedConfiguration(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    @Test
    @Config(qualifiers = "xlarge")
    public void disabledWhenNotPhone() {
        // Even these always-draw flags do not override the device abilities.
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);

        assertFalse(
                EdgeToEdgeControllerFactory.isSupportedConfiguration(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    @Test
    public void disabledWhenNotGestureEnabled() {
        // Even these always-draw flags do not override the device abilities.
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);

        EdgeToEdgeControllerFactory.setHas3ButtonNavBar(true);
        assertFalse(
                EdgeToEdgeControllerFactory.isSupportedConfiguration(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    // Regression test for https://crbug.com/329875254.
    @Test
    public void testViewportFitAfterListenerSet_ToNormal_BottomChinDisabled() {
        ChromeFeatureList.sEdgeToEdgeBottomChin.setForTesting(false);
        when(mTab.isNativePage()).thenReturn(false);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse("Shouldn't be toEdge.", mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());

        // Simulate a viewport fit change to kick off WindowInsetConsumer being hooked up.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        // Simulate another viewport fit change prior to #handleWindowInsets being called.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.CONTAIN);

        // Simulate insets being available.
        assertNotNull(mWindowInsetsListenerCaptor.getValue());
        mWindowInsetsListenerCaptor
                .getValue()
                .onApplyWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        assertFalse(
                "Shouldn't be opted into edge-to-edge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertFalse(
                "Shouldn't be drawing edge-to-edge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isDrawingToEdge());
        verify(mOsWrapper, atLeastOnce())
                .setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(BOTTOM_INSET));
    }

    @Test
    public void testViewportFitAfterListenerSet_ToNormal() {
        when(mTab.isNativePage()).thenReturn(false);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse(
                "Shouldn't be opted into edge-to-edge.",
                mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(
                "Should be drawing edge-to-edge for the bottom chin.",
                mEdgeToEdgeControllerImpl.isDrawingToEdge());

        // Simulate a viewport fit change to kick off WindowInsetConsumer being hooked up.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        // Simulate another viewport fit change prior to #handleWindowInsets being called.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.CONTAIN);

        // Simulate insets being available.
        assertNotNull(mWindowInsetsListenerCaptor.getValue());
        mWindowInsetsListenerCaptor
                .getValue()
                .onApplyWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        assertFalse(
                "Shouldn't be opted into edge-to-edge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(
                "Should still be drawing edge-to-edge after toggling viewport-fit to account for"
                        + " the bottom chin.",
                mEdgeToEdgeControllerImpl.isDrawingToEdge());
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(0));
    }

    @Test
    public void testViewportFitAfterListenerSet_ToEdge() {
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse("Shouldn't be toEdge.", mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());

        // Simulate a viewport fit change to kick off WindowInsetConsumer being hooked up.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        // Simulate another viewport fit change prior to #handleWindowInsets being called.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.CONTAIN);
        // Go back to edge.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);

        // Simulate insets being available.
        assertNotNull(mWindowInsetsListenerCaptor.getValue());
        mWindowInsetsListenerCaptor
                .getValue()
                .onApplyWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        assertTrue(
                "Should be opted into edge-to-edge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(
                "Should be drawing toEdge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isDrawingToEdge());
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(0));
    }

    @Test
    public void handleBrowserControls_properlyPadAdjusters() {
        int unused = -1;
        int browserControlsHeight = BOTTOM_INSET * 2;

        verify(mBrowserControlsStateProvider, atLeastOnce())
                .addObserver(eq(mEdgeToEdgeControllerImpl));

        mEdgeToEdgeControllerImpl.setIsOptedIntoEdgeToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.setKeyboardInsetsForTesting(null);

        // Register a new pad adjuster. Without the keyboard or browser controls visible, the insets
        // should just match the system bottom inset.
        MockPadAdjuster mockPadAdjuster = new MockPadAdjuster();
        mEdgeToEdgeControllerImpl.registerAdjuster(mockPadAdjuster);
        mockPadAdjuster.checkInsets(BOTTOM_INSET, BOTTOM_INSET);

        // Sometimes, the controls offset can change even when browser controls aren't visible. This
        // should be a no-op.
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused, unused, /* bottomOffset= */ browserControlsHeight, unused, false, false);
        mockPadAdjuster.checkInsets(BOTTOM_INSET, BOTTOM_INSET);

        // Show browser controls.
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(browserControlsHeight, unused);
        mockPadAdjuster.checkInsets(BOTTOM_INSET, 0);

        // Scroll off browser controls gradually.
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused,
                unused,
                /* bottomOffset= */ browserControlsHeight / 4,
                unused,
                false,
                false);
        mockPadAdjuster.checkInsets(BOTTOM_INSET, 0);
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused,
                unused,
                /* bottomOffset= */ browserControlsHeight / 2,
                unused,
                false,
                false);
        mockPadAdjuster.checkInsets(BOTTOM_INSET, 0);
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused, unused, /* bottomOffset= */ browserControlsHeight, unused, false, false);
        mockPadAdjuster.checkInsets(BOTTOM_INSET, BOTTOM_INSET);

        // Scroll the browser controls back up.
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused,
                unused,
                /* bottomOffset= */ browserControlsHeight / 2,
                unused,
                false,
                false);
        mockPadAdjuster.checkInsets(BOTTOM_INSET, 0);
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused, unused, /* bottomOffset= */ 0, unused, false, false);
        mockPadAdjuster.checkInsets(BOTTOM_INSET, 0);

        // Hide browser controls.
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(0, unused);
        mockPadAdjuster.checkInsets(BOTTOM_INSET, BOTTOM_INSET);

        mEdgeToEdgeControllerImpl.unregisterAdjuster(mockPadAdjuster);
    }

    void assertToEdgeExpectations() {
        assertNotNull(mWindowInsetsListenerCaptor.getValue());
        mWindowInsetsListenerCaptor
                .getValue()
                .onApplyWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        // Pad the top only, bottom is ToEdge.
        verify(mOsWrapper, atLeastOnce())
                .setPadding(any(), eq(0), intThat(Matchers.greaterThan(0)), eq(0), eq(0));
    }

    void assertToNormalExpectations() {
        // Pad the top and the bottom to keep it all normal.
        verify(mOsWrapper, atLeastOnce())
                .setPadding(
                        any(),
                        eq(0),
                        intThat(Matchers.greaterThan(0)),
                        eq(0),
                        intThat(Matchers.greaterThan(0)));
    }

    void assertNoChangeExpectations() {
        verifyNoMoreInteractions(mOsWrapper);
    }

    void verifyInteractions(Tab tab) {
        verify(tab, atLeastOnce()).addObserver(any());
        verify(tab, atLeastOnce()).getWebContents();
    }

    void assertBottomInsetForSafeArea(int bottomInset) {
        verify(mWebContents, atLeastOnce()).setDisplayCutoutSafeArea(mSafeAreaRectCaptor.capture());
        Rect safeAreaRect = mSafeAreaRectCaptor.getValue();
        assertEquals(
                "Bottom insets for safe area does not match.", bottomInset, safeAreaRect.bottom);
    }

    // TODO: Verify that the value of the updated insets returned from the
    //  OnApplyWindowInsetsListener is correct.

    private class MockPadAdjuster implements EdgeToEdgePadAdjuster {
        private int mDefaultInset;
        private int mInsetWithBrowserControls;

        MockPadAdjuster() {}

        @Override
        public void overrideBottomInset(int defaultInset, int insetWithBrowserControls) {
            mDefaultInset = defaultInset;
            mInsetWithBrowserControls = insetWithBrowserControls;
        }

        void checkInsets(int expectedDefaultInset, int expectedInsetWithBrowserControls) {
            assertEquals(
                    "The pad adjuster does not have the expected default inset.",
                    expectedDefaultInset,
                    mDefaultInset);
            assertEquals(
                    "The pad adjuster does not have the expected inset account for browser"
                            + " controls.",
                    expectedInsetWithBrowserControls,
                    mInsetWithBrowserControls);
        }
    }
}
