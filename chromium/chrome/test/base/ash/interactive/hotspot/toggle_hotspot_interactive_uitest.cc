// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/style/switch.h"
#include "chrome/test/base/ash/interactive/hotspot/hotspot_state_observer.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_service_util.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

// This string should match the prefix of
// IDS_SETTINGS_INTERNET_HOTSPOT_NO_MOBILE_DATA_SUBLABEL_WITH_LEARN_MORE_LINK
// without the "Learn more" link.
constexpr char kNoMobileDataLink[] = "Connect to mobile data to use hotspot.";

// This string should match the prefix of
// IDS_SETTINGS_INTERNET_HOTSPOT_MOBILE_DATA_NOT_SUPPORTED_SUBLABEL_WITH_LEARN_MORE_LINK
// without the "Learn more" link.
constexpr char kMobileDataNotSupportedLink[] =
    "Your mobile data may not support hotspot.";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(HotspotStateObserver, kHotspotStateService);

class ToggleHotspotInteractiveUITest : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();
  }

  void AddCellularService() {
    ShillServiceClient::Get()->GetTestInterface()->AddService(
        shill_service_info_.service_path(), shill_service_info_.service_guid(),
        shill_service_info_.service_name(), shill::kTypeCellular,
        shill::kStateOnline, /*visible=*/true);
  }

  void SetTetheringReadinessCheckSuccessResult(const std::string& result) {
    ShillManagerClient::Get()
        ->GetTestInterface()
        ->SetSimulateCheckTetheringReadinessResult(
            FakeShillSimulatedResult::kSuccess, result);
  }

 private:
  const ShillServiceInfo shill_service_info_ = ShillServiceInfo(/*id=*/0);
};

IN_PROC_BROWSER_TEST_F(ToggleHotspotInteractiveUITest,
                       HotspotToggleDisabledWhenNoCellularConnection) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Waiting for hotspot summary item and toggle to exist and disabled"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSummaryItem()),
      WaitForElementDisabled(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Waiting for localized link appeared"),

      WaitForElementTextContains(
          kOSSettingsId, settings::hotspot::HotspotSummarySubtitleLink(),
          /*text=*/kNoMobileDataLink),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ToggleHotspotInteractiveUITest,
                       HotspotToggleDisabledWhenCarrierNotSupported) {
  SetTetheringReadinessCheckSuccessResult(shill::kTetheringReadinessNotAllowed);
  AddCellularService();

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Waiting for hotspot summary item and toggle to exist and disabled"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSummaryItem()),
      WaitForElementDisabled(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Waiting for localized link appeared"),

      WaitForElementTextContains(
          kOSSettingsId, settings::hotspot::HotspotSummarySubtitleLink(),
          /*text=*/kMobileDataNotSupportedLink),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ToggleHotspotInteractiveUITest,
                       EnableHotspotFromSettingsAndQuickSettings) {
  SetTetheringReadinessCheckSuccessResult(shill::kTetheringReadinessReady);
  AddCellularService();
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateTetheringEnableResult(FakeShillSimulatedResult::kSuccess,
                                         shill::kTetheringEnableResultSuccess);

  RunTestSequence(
      Log("Open quick settings and make sure hotspot does not show"),

      OpenQuickSettings(),
      WaitForHide(ash::kHotspotFeatureTileDrillInArrowElementId));

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Waiting for hotspot summary item and toggle to exist and enabled"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSummaryItem()),
      WaitForElementEnabled(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Make sure hotspot is initially disabled"),

      ObserveState(kHotspotStateService,
                   std::make_unique<HotspotStateObserver>()),
      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kDisabled),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         false),

      Log("Waiting for hotspot toggle to be enabled then click it"),

      ClickElement(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Wait for the hotspot state to be enabled"),

      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kEnabled),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         true),

      Log("Click on toggle to disable it"),

      ClickElement(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Wait for the hotspot state to be disabled"),

      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kDisabled),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         false),

      Log("Turn on and off hotspot in OS Settings complete"));

  RunTestSequence(
      Log("Open quick settings and navigate to hotspot page"),

      OpenQuickSettings(), NavigateQuickSettingsToHotspotPage(),
      CheckViewProperty(ash::kHotspotDetailedViewToggleElementId,
                        &views::ToggleButton::GetIsOn, false),

      Log("Click on the toggle to turn on hotspot from Quick Settings"),

      MoveMouseTo(ash::kHotspotDetailedViewToggleElementId), ClickMouse(),

      Log("Hotspot is turned on from Quick Settings"),

      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kEnabled),
      CheckViewProperty(ash::kHotspotDetailedViewToggleElementId,
                        &views::ToggleButton::GetIsOn, true),

      Log("Click on the toggle to turn off hotspot from Quick Settings"),

      MoveMouseTo(ash::kHotspotDetailedViewToggleElementId), ClickMouse(),

      Log("Hotspot is turned off from Quick Settings"),

      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kDisabled),
      CheckViewProperty(ash::kHotspotDetailedViewToggleElementId,
                        &views::ToggleButton::GetIsOn, false),

      Log("Turn on and off hotspot from Quick Settings complete"));
}

IN_PROC_BROWSER_TEST_F(ToggleHotspotInteractiveUITest, AbortEnablingHotspot) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  SetTetheringReadinessCheckSuccessResult(shill::kTetheringReadinessReady);
  AddCellularService();

  // By setting the enable result to Busy, we are simulating the situation where
  // the enable operation is stuck.
  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateTetheringEnableResult(FakeShillSimulatedResult::kInProgress,
                                         shill::kTetheringEnableResultBusy);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Waiting for hotspot summary item and toggle to exist and enabled"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSummaryItem()),
      WaitForElementEnabled(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Make sure hotspot is initially disabled"),

      ObserveState(kHotspotStateService,
                   std::make_unique<HotspotStateObserver>()),
      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kDisabled),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         false),

      Log("Click the hotspot toggle then wait for it to be enabled"),

      ClickElement(kOSSettingsId, settings::hotspot::HotspotToggle()),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         true),
      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kEnabling),

      Log("Simulating enable result to success to successfully abort "
          "ongoing operations"),

      Do([&]() {
        ShillManagerClient::Get()
            ->GetTestInterface()
            ->SetSimulateTetheringEnableResult(
                FakeShillSimulatedResult::kSuccess,
                shill::kTetheringEnableResultSuccess);
      }),

      Log("Abort the enable operation by clicking on the toggle button"),

      ClickElement(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Wait for the hotspot state to be disabled"),

      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kDisabled),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         false),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
