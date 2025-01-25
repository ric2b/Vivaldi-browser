// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionMediator.AccountChooserResult;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;

import java.util.Arrays;

/**
 * Integration tests for the Account Selection Button Mode component check that the calls to the
 * Account Selection API end up rendering a View. This class is parameterized to run all tests for
 * each RP mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountSelectionButtonModeIntegrationTest extends AccountSelectionIntegrationTestBase {
    @Before
    @Override
    public void setUp() throws InterruptedException {
        mRpMode = RpMode.BUTTON;
        super.setUp();
    }

    @Test
    @MediumTest
    public void testAddAccount() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));

        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                mAccountSelection.showAccounts(
                                        EXAMPLE_ETLD_PLUS_ONE,
                                        TEST_ETLD_PLUS_ONE_2,
                                        Arrays.asList(RETURNING_ANA),
                                        IDP_METADATA_WITH_ADD_ACCOUNT,
                                        mClientIdMetadata,
                                        /* isAutoReauthn= */ false,
                                        RpContext.SIGN_IN,
                                        /* requestPermission= */ true);
                                mAccountSelection.getMediator().setComponentShowTime(-1000);
                                return null;
                            }
                        })
                .when(mMockBridge)
                .onLoginToIdP(any(), any());

        // Click "Use a different account".
        runOnUiThreadBlocking(
                () -> {
                    contentView.findViewById(R.id.account_selection_add_account_btn).performClick();
                });

        // Because of how we implemented onLogInToIdP, we should be back to account chooser here.
        // Make sure that the Ana account is now displayed.
        onView(withText("Ana Doe")).check(matches(isDisplayed()));

        clickFirstAccountInAccountsList();

        // Because this is a returning account, we should immediately sign in now.
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testAccountChooserWithAddAccountForNewUser() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        // This should be the "multi-account chooser", so clicking an account should go
        // to the disclosure text screen.
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));

        clickFirstAccountInAccountsList();

        // Sheet should still be open.
        assertNotEquals(BottomSheetController.SheetState.HIDDEN, getBottomSheetState());
        onView(withId(R.id.account_selection_continue_btn))
                .check(matches(withText("Continue as Bob")));

        // Make sure we now show the disclosure text.
        TextView consent = contentView.findViewById(R.id.user_data_sharing_consent);
        if (consent == null) {
            throw new NoMatchingViewException.Builder()
                    .includeViewHierarchy(true)
                    .withRootView(contentView)
                    .build();
        }

        clickContinueButton();

        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testAccountChooserWithAddAccountReturningUser() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));

        clickFirstAccountInAccountsList();

        // Because this is a returning account, we should immediately sign in now.
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testAddAccountIsSecondaryButtonForSingleAccount() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Check that only one item is in the accounts list, and the item is an account.
        RecyclerView accountsList = contentView.findViewById(R.id.sheet_item_list);
        assertEquals(accountsList.getChildCount(), 1);
        assertEquals(
                accountsList.getAdapter().getItemViewType(0),
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT);

        // Check that secondary button is displayed, with the appropriate text.
        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));
    }

    @Test
    @MediumTest
    public void testAddAccountIsAccountRowForMultipleAccounts() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB, RETURNING_ANA),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Check that three items are in the accounts list, the first two items are accounts and the
        // third/last item is an add account button.
        RecyclerView accountsList = contentView.findViewById(R.id.sheet_item_list);
        assertEquals(accountsList.getChildCount(), 3);
        assertEquals(
                accountsList.getAdapter().getItemViewType(0),
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT);
        assertEquals(
                accountsList.getAdapter().getItemViewType(1),
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT);
        assertEquals(
                accountsList.getAdapter().getItemViewType(2),
                AccountSelectionProperties.ITEM_TYPE_ADD_ACCOUNT);

        // Check that secondary button is NOT displayed.
        onView(withId(R.id.account_selection_add_account_btn)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testLoadingDialogBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showLoadingDialog(
                            EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.BACK_PRESS);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testLoadingDialogSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showLoadingDialog(
                            EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testNewUserFlow() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Click the first account in the account chooser.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.SIGN_IN);
        clickFirstAccountInAccountsList();

        // Click continue in the request permission dialog.
        assertEquals(
                mAccountSelection.getMediator().getHeaderType(), HeaderType.REQUEST_PERMISSION);
        clickContinueButton();

        // User is now signed in and shown the verifying UI.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.VERIFY);
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testReturningUserFlow() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Click the first account account in the account chooser.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.SIGN_IN);
        clickFirstAccountInAccountsList();

        // Because this is a returning account, user is now signed in and shown the verifying UI.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.VERIFY);
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testRpInApprovedClientsFlow() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ false);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Click the first account account in the account chooser.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.SIGN_IN);
        clickFirstAccountInAccountsList();

        // Because requestPermission is false, user is now signed in and shown the verifying UI.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.VERIFY);
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testRequestPermissionDialogBackShowsAccountChooser() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Dialog is initially an account chooser.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.SIGN_IN);

        // Clicking an account should show the request permission dialog.
        clickFirstAccountInAccountsList();
        assertEquals(
                mAccountSelection.getMediator().getHeaderType(), HeaderType.REQUEST_PERMISSION);

        // Press back from the request permission dialog, returning to the account chooser.
        Espresso.pressBack();
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.SIGN_IN);
    }

    @Test
    @MediumTest
    public void testRequestPermissionDialogSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Dialog is initially an account chooser.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.SIGN_IN);

        // Clicking an account should show the request permission dialog.
        clickFirstAccountInAccountsList();
        assertEquals(
                mAccountSelection.getMediator().getHeaderType(), HeaderType.REQUEST_PERMISSION);

        // Swipe to dismiss on request permission dialog.
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testAccountChooserToVerifyingDialogHeaderReused() {
        doCallback(
                        /* index= */ 1,
                        (Callback<Bitmap> callback) -> {
                            callback.onResult(Bitmap.createBitmap(40, 40, Bitmap.Config.ARGB_8888));
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), any());

        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Dialog is initially an account chooser.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.SIGN_IN);
        String expectedTitle =
                ((TextView) contentView.findViewById(R.id.header_title)).getText().toString();
        String expectedSubtitle =
                ((TextView) contentView.findViewById(R.id.header_subtitle)).getText().toString();
        onView(withId(R.id.header_idp_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.header_rp_icon)).check(matches(not(isDisplayed())));
        onView(withId(R.id.arrow_range_icon)).check(matches(not(isDisplayed())));

        // Clicking an account should show the verifying dialog.
        clickFirstAccountInAccountsList();
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.VERIFY);
        assertEquals(
                expectedTitle,
                ((TextView) contentView.findViewById(R.id.header_title)).getText().toString());
        assertEquals(
                expectedSubtitle,
                ((TextView) contentView.findViewById(R.id.header_subtitle)).getText().toString());
        onView(withId(R.id.header_idp_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.header_rp_icon)).check(matches(not(isDisplayed())));
        onView(withId(R.id.arrow_range_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testRequestPermissionDialogToVerifyingDialogHeaderReused() {
        doCallback(
                        /* index= */ 1,
                        (Callback<Bitmap> callback) -> {
                            callback.onResult(Bitmap.createBitmap(40, 40, Bitmap.Config.ARGB_8888));
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), any());

        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Dialog is initially an account chooser.
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.SIGN_IN);

        // Clicking an account should show the request permission dialog.
        clickFirstAccountInAccountsList();
        assertEquals(
                mAccountSelection.getMediator().getHeaderType(), HeaderType.REQUEST_PERMISSION);
        String expectedTitle =
                ((TextView) contentView.findViewById(R.id.header_title)).getText().toString();
        String expectedSubtitle =
                ((TextView) contentView.findViewById(R.id.header_subtitle)).getText().toString();
        onView(withId(R.id.header_idp_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.header_rp_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.arrow_range_icon)).check(matches(isDisplayed()));

        // Clicking continue should show the verifying dialog.
        clickContinueButton();
        assertEquals(mAccountSelection.getMediator().getHeaderType(), HeaderType.VERIFY);
        assertEquals(
                expectedTitle,
                ((TextView) contentView.findViewById(R.id.header_title)).getText().toString());
        assertEquals(
                expectedSubtitle,
                ((TextView) contentView.findViewById(R.id.header_subtitle)).getText().toString());
        onView(withId(R.id.header_idp_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.header_rp_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.arrow_range_icon)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAccountSelectionRecordsAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ false);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult",
                        AccountChooserResult.ACCOUNT_ROW);

        clickFirstAccountInAccountsList();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testAddAccountRecordsAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult",
                        AccountChooserResult.USE_OTHER_ACCOUNT_BUTTON);

        // Click "Use a different account".
        runOnUiThreadBlocking(
                () -> {
                    contentView.findViewById(R.id.account_selection_add_account_btn).performClick();
                });

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testSwipeRecordsAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA, NEW_BOB),
                            IDP_METADATA,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult", AccountChooserResult.SWIPE);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testPressBackAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult", AccountChooserResult.BACK_PRESS);

        Espresso.pressBack();
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.BACK_PRESS);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testTapScrimAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult", AccountChooserResult.TAP_SCRIM);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.forceClickOutsideTheSheet();
                });

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.TAP_SCRIM);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testTabClosedAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(RETURNING_ANA),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            RpContext.SIGN_IN,
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult", AccountChooserResult.TAB_CLOSED);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.NAVIGATION);
                });

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);

        histogramWatcher.assertExpected();
    }

    private void clickFirstAccountInAccountsList() {
        runOnUiThreadBlocking(
                () -> {
                    ((RecyclerView)
                                    mBottomSheetController
                                            .getCurrentSheetContent()
                                            .getContentView()
                                            .findViewById(R.id.sheet_item_list))
                            .getChildAt(0)
                            .performClick();
                });
    }

    private void clickContinueButton() {
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController
                            .getCurrentSheetContent()
                            .getContentView()
                            .findViewById(R.id.account_selection_continue_btn)
                            .performClick();
                });
    }
}
