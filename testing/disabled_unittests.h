// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for all platforms  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

DISABLE(HistoryCounterTest, Synced)

DISABLE(InlineInstallPrivateApiTestApp, BackgroundInstall)

DISABLE_ALL(EncryptedMediaSupportedTypesWidevineTest)

// Assume these fails due to switches::kExtensionActionRedesign being disabled
//DISABLE_MULTI(ToolbarActionsBarUnitTest, ExtensionActionContextMenu)
//DISABLE_MULTI(ToolbarActionsBarUnitTest, ExtensionActionBlockedActions)
//DISABLE_MULTI(ToolbarActionsBarUnitTest, ExtensionActionWantsToRunAppearance)
//DISABLE_MULTI(ToolbarActionsBarUnitTest, TestNeedsOverflow)
//DISABLE_MULTI(ToolbarActionsBarUnitTest, TestActionFrameBounds)
//DISABLE_MULTI(ToolbarActionsBarUnitTest, TestStartAndEndIndexes)
//DISABLE(ToolbarActionViewInteractiveUITest, TestContextMenuOnOverflowedAction)
//DISABLE(ToolbarActionViewInteractiveUITest,
//        ActivateOverflowedToolbarActionWithKeyboard)
//DISABLE_MULTI(ShowPageActionWithoutPageActionTest, Test)
//DISABLE(ToolbarViewInteractiveUITest, TestAppMenuOpensOnDrag)
//DISABLE(ExtensionContextMenuModelTest, ExtensionContextMenuShowAndHide)

// DevTools related.
DISABLE(BrowserViewTest, DevToolsUpdatesBrowserWindow)
DISABLE(ChromeRenderProcessHostTest, DevToolsOnSelfInOwnProcess)
DISABLE(ChromeRenderProcessHostTest, DevToolsOnSelfInOwnProcessPPT)
DISABLE(DevToolsSanityTest, TestToolboxLoadedUndocked)
DISABLE(DevToolsTagTest, TagsManagerRecordsATag)
DISABLE(PolicyTest, DeveloperToolsDisabled)
DISABLE(TaskManagerBrowserTest, DevToolsNewUndockedWindow)
DISABLE(TaskManagerBrowserTest, DevToolsOldUndockedWindow)

// times out
DISABLE(WebViewTest, WebViewInBackgroundPage)

// Seems to have failed in v59
DISABLE(AutofillInteractiveTest, CrossSitePaymentForms)
//DISABLE(CommandsApiTest, PageAction)
//DISABLE(CommandsApiTest, PageActionKeyUpdated)
//DISABLE(ExtensionBrowserTest, PageAction)
//DISABLE(ExtensionBrowserTest, PageActionCrash25562)
//DISABLE(ExtensionBrowserTest, PageActionInPageNavigation)
//DISABLE(ExtensionBrowserTest, PageActionRefreshCrash)
//DISABLE(ExtensionBrowserTest, RSSMultiRelLink)
//DISABLE(ExtensionBrowserTest, TitleLocalizationPageAction)
//DISABLE(ExtensionBrowserTest, UnloadPageAction)
//DISABLE(LazyBackgroundPageApiTest, BroadcastEvent)
//DISABLE(PageActionApiTest, TestTriggerPageAction)
//DISABLE(MediaRouterUIBrowserTest, UpdateActionLocation)
DISABLE(SitePerProcessTextInputManagerTest, TrackSelectionBoundsForAllFrames)
DISABLE(SitePerProcessTextInputManagerTest, TrackTextSelectionForAllFrames)

// Seems to have failed in v60
DISABLE(SitePerProcessTextInputManagerTest, ResetStateAfterFrameDetached)
DISABLE(SitePerProcessTextInputManagerTest,
        ResetTextInputStateOnActiveWidgetChange)
DISABLE(SitePerProcessTextInputManagerTest, TrackCompositionRangeForAllFrames)
DISABLE(FFmpegGlueContainerTest, AAC)
//DISABLE(FFmpegGlueContainerTest, MOV)
//DISABLE(FFmpegGlueContainerTest, MP3)

// Flaky
DISABLE(SitePerProcessTextInputManagerTest, StopTrackingCrashedChildFrame)

// broke in v61
DISABLE(PasswordGenerationInteractiveTest, PopupShownAndPasswordSelected)

// Will be broken until we start using the TemplateURlService for searches
// because TemplateURLService::OnHistoryURLVisited is disabled until then.
DISABLE(TemplateURLServiceTest, GenerateVisitOnKeyword)

// Fails in raw chromium 64, too
//DISABLE(PasswordManagerBrowserTestBase, DeleteFrameBeforeSubmit)

// Flaky in v64
//DISABLE(PasswordGenerationInteractiveTest, AutoSavingGeneratedPassword)
//DISABLE(WebViewContextMenuInteractiveTest, ContextMenuParamCoordinates)
//DISABLE_MULTI(ParamaterizedFullscreenControllerInteractiveTest,
//              MouseLockSilentAfterTargetUnlock)
DISABLE(SitePerProcessTextInputManagerTest,
        TrackStateWhenSwitchingFocusedFrames)

// Require fieldtesting setting
DISABLE_MULTI(SSLUITestWithExtendedReporting, TestBrokenHTTPSReportingCloseTab)

// Timezone dependent
//DISABLE(InterventionsInternalsUITest, LogNewMessage)

// Broke in v65, can't reproduce locally
DISABLE_ALL(MediaEngagementPreloadedListTest)

// Disabled by chromium.
// https://bugs.chromium.org/p/chromium/issues/detail?id=806684
//DISABLE(ProcessManagerBrowserTest, NestedURLNavigationsToExtensionBlocked)

DISABLE(ExtensionApiTest, Bookmarks)

// Seems to require correct strings
DISABLE(SigninSyncConfirmationTest, DialogWithDice)
DISABLE(ExtensionApiTabTest, TabsOnUpdated)

//Broke in v66
DISABLE(BrowserFocusTest, FocusOnNavigate)
DISABLE_MULTI(ExtensionBindingsApiTest, UseAPIsAfterContextRemoval)
DISABLE(WebContentsImplBrowserTest, LoadProgressWithFrames)

// Broke in v67, unable to reproduce locally
DISABLE_ALL(MediaRouterIntegrationBrowserTest)
DISABLE(MediaRouterIntegrationIncognitoBrowserTest, Basic)
DISABLE(MediaRouterIntegrationOneUANoReceiverBrowserTest, ReconnectSessionSameTab)

// Removed on master
DISABLE(PolicyToolUIExportTest, ExportSessionPolicyToLinux)

// Disabled due to a US-only dependency
// See https://bugs.chromium.org/p/chromium/issues/detail?id=842460
DISABLE(PaymentRequestShippingAddressEditorTest, NoCountryNoState)
DISABLE(PaymentRequestShippingAddressEditorTest, NoCountryValidState_AsyncRegionLoad)
DISABLE(PaymentRequestShippingAddressEditorTest, SyncData)
DISABLE(PaymentRequestShippingAddressEditorTest, AddImpossiblePhoneNumber)
DISABLE(PaymentRequestShippingAddressEditorTest, AsyncData)
DISABLE(PaymentRequestShippingAddressEditorTest, DefaultRegion_RegionCode)
DISABLE(PaymentRequestShippingAddressEditorTest, NoCountryInvalidState)
DISABLE(PaymentRequestShippingAddressEditorTest, NoCountryValidState_SyncRegionLoad)
DISABLE(PaymentRequestShippingAddressEditorTest, SyncDataInIncognito)
DISABLE(PaymentRequestShippingAddressEditorTest, AddPossiblePhoneNumber)
DISABLE(PaymentRequestShippingAddressEditorTest, TimingOutRegionData)
DISABLE(PaymentRequestShippingAddressEditorTest, AddInternationalPhoneNumberFromOtherCountry)
DISABLE(PaymentRequestShippingAddressEditorTest, DefaultRegion_RegionName)
DISABLE(PaymentRequestShippingAddressEditorTest, FailToLoadRegionData)
DISABLE(PaymentRequestCreditCardEditorTest, CreateNewBillingAddress)

// Disabled because of rewrite of cookie migration SQL
DISABLE(SQLitePersistentCookieStoreTest, UpgradeToSchemaVersion10Corrupted)

// Disabled because we currently do not support site isolation
DISABLE(WebDriverSitePerProcessPolicyBrowserTest, Simple)

// Broken in v69, also breaks in pure chromium
DISABLE(PolicyPrefsTest,PolicyToPrefsMapping)
DISABLE_ALL(AutofillAddressPolicyHandlerTest)
DISABLE_ALL(AutofillCreditCardPolicyHandlerTest)


// Broke in v69
DISABLE(ProfileChooserViewExtensionsTest, DiceSigninPromoWithoutIllustration)
DISABLE(ProfileChooserViewExtensionsTest, IncrementDiceSigninPromoShowCounter)
DISABLE(SingleClientUserEventsSyncTest, NoUserEvents)
DISABLE_ALL(SigninSyncConfirmationTest)
DISABLE(PipelineIntegrationTest, BasicPlaybackHashed_M4A)
DISABLE(BrowserKeyEventsTest, AccessKeys)

// Broke in v69, seems to be for a disabled hardware media decryption feature
DISABLE_ALL(EncryptedMediaSupportedTypesWidevineHwSecureTest)

// Disabled since it fails when we disable access keys
DISABLE(BrowserKeyEventsTest, AccessKeys)

