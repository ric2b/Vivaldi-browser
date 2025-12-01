// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Holds all the data necessary to create the views for the overflow menu.
@objcMembers public class OverflowMenuModel: NSObject, ObservableObject {
  /// Holds all the necessary data for the customization flow in the overflow
  /// menu.
  struct CustomizationModel: Equatable {
    let actions: ActionCustomizationModel

    #if VIVALDI_BUILD
    let vivaldiActions: ActionCustomizationModel
    #endif // Vivaldi

    let destinations: DestinationCustomizationModel
  }

  /// The destinations for the overflow menu.
  @Published public var destinations: [OverflowMenuDestination]

  /// The action groups for the overflow menu.
  @Published public var actionGroups: [OverflowMenuActionGroup]

  // Vivaldi
  /// The vivaldi action groups for the overflow menu.
  @Published public var vivaldiActionGroups: [OverflowMenuActionGroup]
  // End Vivaldi

  /// If present, indicates that the menu should show a customization flow using
  /// the provided data.
  @Published var customization: CustomizationModel?

  /// Whether or not customization is currently in progress.
  public var isCustomizationActive: Bool {
    return customization != nil
  }

  public init(
    destinations: [OverflowMenuDestination],
    actionGroups: [OverflowMenuActionGroup]
  ) {
    self.destinations = destinations
    self.actionGroups = actionGroups

    #if VIVALDI_BUILD
    self.vivaldiActionGroups = []
    #endif // End Vivaldi

  }

  public func startCustomization(
    actions: ActionCustomizationModel,
    destinations: DestinationCustomizationModel
  ) {

    #if VIVALDI_BUILD
    customization = CustomizationModel(
      actions: actions,
      vivaldiActions: ActionCustomizationModel(
        actions: []
      ),
      destinations: destinations
    )
    #else
    customization = CustomizationModel(actions: actions, destinations: destinations)
    #endif // End Vivaldi

  }

  public func endCustomization() {
    customization = nil
  }

  public func setDestinationsWithAnimation(_ destinations: [OverflowMenuDestination]) {
    withAnimation {
      self.destinations = destinations
    }
  }

  // MARK:  Vivaldi
  public init(
    destinations: [OverflowMenuDestination],
    vivaldiActionGroups: [OverflowMenuActionGroup],
    actionGroups: [OverflowMenuActionGroup]
  ) {
    self.destinations = destinations
    self.actionGroups = actionGroups
    self.vivaldiActionGroups = vivaldiActionGroups
  }

  public func startCustomization(
    actions: ActionCustomizationModel,
    vivaldiActions: ActionCustomizationModel,
    destinations: DestinationCustomizationModel
  ) {
    customization = CustomizationModel(
      actions: actions,
      vivaldiActions: vivaldiActions,
      destinations: destinations
    )
  }
  // End Vivaldi

}
