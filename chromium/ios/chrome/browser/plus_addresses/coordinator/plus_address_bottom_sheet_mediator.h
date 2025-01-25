// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_COORDINATOR_PLUS_ADDRESS_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_COORDINATOR_PLUS_ADDRESS_BOTTOM_SHEET_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/plus_addresses/plus_address_types.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_delegate.h"
#import "url/gurl.h"

namespace plus_addresses {
class PlusAddressService;
class PlusAddressSettingService;
}  // namespace plus_addresses

@protocol PlusAddressBottomSheetConsumer;
class UrlLoadingBrowserAgent;

// Mediator for the plus_addresses bottom sheet. It is responsible for service
// interactions underlying the UI.
@interface PlusAddressBottomSheetMediator
    : NSObject <PlusAddressBottomSheetDelegate>

// Designated initializer of the mediator, with `service` used to interface with
// the underlying data (and, transitively, the service that backs it).
// `mainFrameOrigin` is the origin any plus addresses will be scoped to.
- (instancetype)
    initWithPlusAddressService:(plus_addresses::PlusAddressService*)service
     plusAddressSettingService:
         (plus_addresses::PlusAddressSettingService*)plusAddressSettingService
                     activeUrl:(GURL)activeUrl
              autofillCallback:(plus_addresses::PlusAddressCallback)callback
                     urlLoader:(UrlLoadingBrowserAgent*)urlLoader
                     incognito:(BOOL)incognito NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The bottom sheet plus_address consumer, which will be notified as data
// becomes available or errors occur.
@property(nonatomic, strong) id<PlusAddressBottomSheetConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_COORDINATOR_PLUS_ADDRESS_BOTTOM_SHEET_MEDIATOR_H_
