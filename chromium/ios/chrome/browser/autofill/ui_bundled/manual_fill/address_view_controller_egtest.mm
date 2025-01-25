// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_constants.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using base::test::ios::kWaitForActionTimeout;
using chrome_test_util::ManualFallbackFormSuggestionViewMatcher;
using chrome_test_util::ManualFallbackKeyboardIconMatcher;
using chrome_test_util::ManualFallbackManageProfilesMatcher;
using chrome_test_util::ManualFallbackProfilesIconMatcher;
using chrome_test_util::ManualFallbackProfilesTableViewMatcher;
using chrome_test_util::ManualFallbackProfileTableViewWindowMatcher;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsProfileMatcher;
using chrome_test_util::TapWebElementWithId;

namespace {

constexpr char kFormElementAddress[] = "address";
constexpr char kFormElementCity[] = "city";
constexpr char kFormElementName[] = "name";
constexpr char kFormElementState[] = "state";
constexpr char kFormElementZip[] = "zip";

constexpr char kFormHTMLFile[] = "/profile_form.html";

// Waits for the keyboard to appear. Returns NO on timeout.
BOOL WaitForKeyboardToAppear() {
  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard"
                  block:^BOOL {
                    return [EarlGrey isKeyboardShownWithError:nil];
                  }];
  return [waitForKeyboard waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
}

// Opens the address manual fill view and verifies that the address view
// controller is visible afterwards.
void OpenAddressManualFillView() {
  id<GREYMatcher> button_to_tap;
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    button_to_tap = grey_accessibilityLabel(
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_AUTOFILL_DATA));
  } else {
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
    button_to_tap = ManualFallbackProfilesIconMatcher();
  }

  // Tap the button that'll open the address manual fill view.
  [[EarlGrey selectElementWithMatcher:button_to_tap] performAction:grey_tap()];

  // Verify the address table view controller is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Matcher for the page showing the details of an address.
id<GREYMatcher> AddressDetailsPage() {
  return grey_accessibilityID(kAutofillProfileEditTableViewId);
}

// Matcher for the expanded address manual fill view button.
id<GREYMatcher> AddressManualFillViewButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_AUTOFILL_ADDRESS_AUTOFILL_DATA)),
                    grey_ancestor(grey_accessibilityID(
                        kFormInputAccessoryViewAccessibilityID)),
                    nil);
}

// Matcher for the address tab in the manual fill view.
id<GREYMatcher> AddressManualFillViewTab() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_EXPANDED_MANUAL_FILL_ADDRESS_TAB_ACCESSIBILITY_LABEL)),
      grey_ancestor(
          grey_accessibilityID(manual_fill::kExpandedManualFillHeaderViewID)),
      nil);
}

// Matcher for the chip button with the given `title`.
id<GREYMatcher> ChipButton(std::u16string title) {
  NSString* accessibility_label =
      [AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]
          ? l10n_util::GetNSStringF(
                IDS_IOS_MANUAL_FALLBACK_CHIP_ACCESSIBILITY_LABEL, title)
          : base::SysUTF16ToNSString(title);
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(accessibility_label),
      grey_interactable(), nullptr);
}

// Matcher for the overflow menu button shown in the address cells.
id<GREYMatcher> OverflowMenuButton() {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_MANUAL_FALLBACK_THREE_DOT_MENU_BUTTON_ACCESSIBILITY_LABEL),
      grey_interactable(), nullptr);
}

// Matcher for the "Edit" action made available by the overflow menu button.
id<GREYMatcher> OverflowMenuEditAction() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_EDIT_ACTION_TITLE),
                    grey_interactable(), nullptr);
}

// Matcher for the "Autofill Form" button shown in the address cells.
id<GREYMatcher> AutofillFormButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_MANUAL_FALLBACK_AUTOFILL_FORM_BUTTON_TITLE),
                    grey_interactable(), nullptr);
}

// Checks that the chip button with `title` is sufficiently visible.
void CheckChipButtonVisibility(std::u16string title) {
  [[EarlGrey selectElementWithMatcher:ChipButton(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that the chip buttons of the example profile are all visible.
void CheckChipButtonsOfExampleProfile() {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  std::string locale = l10n_util::GetLocaleOverride();

  CheckChipButtonVisibility(profile.GetInfo(autofill::NAME_FIRST, locale));
  CheckChipButtonVisibility(profile.GetInfo(autofill::NAME_MIDDLE, locale));
  CheckChipButtonVisibility(profile.GetInfo(autofill::NAME_LAST, locale));
  CheckChipButtonVisibility(profile.GetInfo(autofill::COMPANY_NAME, locale));
  CheckChipButtonVisibility(
      profile.GetInfo(autofill::ADDRESS_HOME_LINE1, locale));
  CheckChipButtonVisibility(
      profile.GetInfo(autofill::ADDRESS_HOME_LINE2, locale));

  // Scroll down to show the remaining chips.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  CheckChipButtonVisibility(
      profile.GetInfo(autofill::ADDRESS_HOME_CITY, locale));
  CheckChipButtonVisibility(
      profile.GetInfo(autofill::ADDRESS_HOME_STATE, locale));
  CheckChipButtonVisibility(
      profile.GetInfo(autofill::ADDRESS_HOME_ZIP, locale));
  CheckChipButtonVisibility(
      profile.GetInfo(autofill::ADDRESS_HOME_COUNTRY, locale));
  CheckChipButtonVisibility(profile.GetInfo(autofill::EMAIL_ADDRESS, locale));
  CheckChipButtonVisibility(
      profile.GetInfo(autofill::ADDRESS_HOME_ZIP, locale));
}

// Opens the address manual fill view when there are no saved addresses and
// verifies that the address view controller is visible afterwards. Only useful
// when the `kIOSKeyboardAccessoryUpgrade` feature is enabled.
void OpenAddressManualFillViewWithNoSavedAddresses() {
  // Tap the button to open the expanded manual fill view.
  [[EarlGrey selectElementWithMatcher:AddressManualFillViewButton()]
      performAction:grey_tap()];

  // Tap the address tab from the segmented control.
  [[EarlGrey selectElementWithMatcher:AddressManualFillViewTab()]
      performAction:grey_tap()];

  // Verify the address table view controller is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

}  // namespace

// Integration Tests for Mannual Fallback Addresses View Controller.
@interface AddressViewControllerTestCase : ChromeTestCase
@end

@implementation AddressViewControllerTestCase

- (BOOL)shouldEnableKeyboardAccessoryUpgradeFeature {
  return YES;
}

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface saveExampleProfile];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];
}

- (void)tearDown {
  [AutofillAppInterface clearProfilesStore];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self shouldEnableKeyboardAccessoryUpgradeFeature]) {
    config.features_enabled.push_back(kIOSKeyboardAccessoryUpgrade);
  } else {
    config.features_disabled.push_back(kIOSKeyboardAccessoryUpgrade);
  }

  return config;
}

// Tests that the addresses view controller appears on screen.
// TODO(crbug.com/40711697): Flaky on ios simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testAddressesViewControllerIsPresented \
  DISABLED_testAddressesViewControllerIsPresented
#else
#define MAYBE_testAddressesViewControllerIsPresented \
  testAddressesViewControllerIsPresented
#endif
- (void)MAYBE_testAddressesViewControllerIsPresented {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view and verify that the address table view
  // controller is visible.
  OpenAddressManualFillView();
}

// Tests that the saved address chip buttons are all visible in the address
// table view controller, and that they have the right accessibility label.
- (void)testAddressChipButtonsAreAllVisible {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view and verify that the address table view
  // controller is visible.
  OpenAddressManualFillView();

  CheckChipButtonsOfExampleProfile();
}

// Tests that the "Manage Addresses..." action works.
- (void)testManageAddressesActionOpensAddressSettings {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillView();

  // Tap the "Manage Addresses..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackManageProfilesMatcher()]
      performAction:grey_tap()];

  // Verify the address settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsProfileMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that returning from "Manage Addresses..." leaves the icons and keyboard
// in the right state.
- (void)testAddressesStateAfterPresentingManageAddresses {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillView();

  // Icons are not present when the Keyboard Accessory Upgrade feature is
  // enabled.
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    // Verify the status of the icon.
    [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
        assertWithMatcher:grey_not(grey_userInteractionEnabled())];
  }

  // Tap the "Manage Addresses..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackManageProfilesMatcher()]
      performAction:grey_tap()];

  // Verify the address settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsProfileMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify the address settings closed.
  [[EarlGrey selectElementWithMatcher:SettingsProfileMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // TODO(crbug.com/332956674): Keyboard and keyboard accessory are not present
  // on iOS 17.4+, remove version check once fixed.
  if (@available(iOS 17.4, *)) {
    // Skip verifications.
  } else {
    // Icons are not present when the Keyboard Accessory Upgrade feature is
    // enabled.
    if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
      // Verify the status of the icons.
      [[EarlGrey
          selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
          performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
      [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
          assertWithMatcher:grey_sufficientlyVisible()];
      [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
          assertWithMatcher:grey_userInteractionEnabled()];
      [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
          assertWithMatcher:grey_not(grey_sufficientlyVisible())];
    }

    // Verify the keyboard is not covered by the profiles view.
    GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                   @"Keyboard should be shown");
  }

  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Address View Controller is dismissed when tapping the
// keyboard icon.
- (void)testKeyboardIconDismissAddressController {
  if ([ChromeEarlGrey isIPadIdiom] ||
      [AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"The keyboard icon is never present on iPads or when the Keyboard "
        @"Accessory Upgrade feature is enabled.");
  }

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillView();

  // Tap on the keyboard icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      performAction:grey_tap()];

  // Verify the address controller table view and the address icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Address View Controller is dismissed when tapping the outside
// the popover on iPad.
- (void)testIPadTappingOutsidePopOverDismissAddressController {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test is not applicable for iPhone");
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillView();

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackProfileTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the address controller table view and the address icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the address icon is hidden when no addresses are available.
- (void)testAddressIconIsNotVisibleWhenAddressStoreEmpty {
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is enabled.");
  }

  // Delete the profile that is added on `-setUp`.
  [AutofillAppInterface clearProfilesStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Wait for the keyboard to appear.
  WaitForKeyboardToAppear();

  // Assert the address icon is not visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Store one address.
  [AutofillAppInterface saveExampleProfile];

  // Tap another field to trigger form activity.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementCity)];

  // Assert the address icon is visible now.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that the the "no addresses found" message is visible when no address
// suggestions are available.
- (void)testNoAddressesFoundMessageIsVisibleWhenNoAddressSuggestions {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is disabled.");
  }

  [AutofillAppInterface clearProfilesStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillViewWithNoSavedAddresses();

  // Assert that the "no addresses found" message is visible.
  id<GREYMatcher> noAddressesFoundMessage = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_ADDRESSES));
  [[EarlGrey selectElementWithMatcher:noAddressesFoundMessage]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping the "Autofill Form" button fills the address form with
// the right data.
- (void)testAutofillFormButtonFillsForm {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Open the address manual fill view.
  OpenAddressManualFillView();

  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Autofill Form" button.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyAddressInfoHasBeenFilled:autofill::test::GetFullProfile()];
}

// Tests that the overflow menu button is only visible when the Keyboard
// Accessory Upgrade feature is enabled.
- (void)testOverflowMenuVisibility {
  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Open the address manual fill view.
  OpenAddressManualFillView();

  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests the "Edit" action of the overflow menu button displays the address's
// details in edit mode.
- (void)testEditAddressFromOverflowMenu {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  // Bring up the keyboard
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementName)];
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");

  // Open the address manual fill view.
  OpenAddressManualFillView();

  // Tap the overflow menu button and select the "Edit" action.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OverflowMenuEditAction()]
      performAction:grey_tap()];

  // Check that the address details page opened.
  [[EarlGrey selectElementWithMatcher:AddressDetailsPage()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Edit the city.
  [[EarlGrey selectElementWithMatcher:grey_text(@"Elysium")]
      performAction:grey_replaceText(@"Mountain View")];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Check that the address details page is no longer visible.
  [[EarlGrey selectElementWithMatcher:AddressDetailsPage()]
      assertWithMatcher:grey_notVisible()];

  // TODO(crbug.com/332956674): Check that the updated suggestion is visible.
}

#pragma mark - Private

// Verify that the address info has been filled.
- (void)verifyAddressInfoHasBeenFilled:(autofill::AutofillProfile)profile {
  std::string locale = l10n_util::GetLocaleOverride();

  // Full name.
  NSString* name =
      base::SysUTF16ToNSString(profile.GetInfo(autofill::NAME_FULL, locale));
  NSString* nameCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementName, name];

  // Address.
  NSString* address = base::SysUTF16ToNSString(
      profile.GetInfo(autofill::ADDRESS_HOME_LINE1, locale));
  NSString* addressCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementAddress, address];

  // City.
  NSString* city = base::SysUTF16ToNSString(
      profile.GetInfo(autofill::ADDRESS_HOME_CITY, locale));
  NSString* cityCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementCity, city];

  // State.
  NSString* state = base::SysUTF16ToNSString(
      profile.GetInfo(autofill::ADDRESS_HOME_STATE, locale));
  NSString* stateCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementState, state];

  // Zip code.
  NSString* zip = base::SysUTF16ToNSString(
      profile.GetInfo(autofill::ADDRESS_HOME_ZIP, locale));
  NSString* zipCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementZip, zip];

  NSString* condition =
      [NSString stringWithFormat:@"%@ && %@ && %@ && %@ && %@", nameCondition,
                                 addressCondition, cityCondition,
                                 stateCondition, zipCondition];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

@end

// Rerun all the tests in this file but with kIOSKeyboardAccessoryUpgrade
// disabled. This will be removed once that feature launches fully, but ensures
// regressions aren't introduced in the meantime.
@interface AddressViewControllerKeyboardAccessoryUpgradeDisabledTestCase
    : AddressViewControllerTestCase

@end

@implementation AddressViewControllerKeyboardAccessoryUpgradeDisabledTestCase

- (BOOL)shouldEnableKeyboardAccessoryUpgradeFeature {
  return NO;
}

// This causes the test case to actually be detected as a test case. The actual
// tests are all inherited from the parent class.
- (void)testEmpty {
}

@end
