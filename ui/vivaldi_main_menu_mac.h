//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIVALDI_MAIN_MENU_MAC_H_
#define UI_VIVALDI_MAIN_MENU_MAC_H_

#import <Cocoa/Cocoa.h>

#include <memory>
#include <string>

#include "base/task/cancelable_task_tracker.h"

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconImageResult;
}

class Profile;

namespace vivaldi {

class FaviconLoaderMac {
 public:
  explicit FaviconLoaderMac(Profile* profile);
  ~FaviconLoaderMac();

  void LoadFavicon(NSMenuItem* item, const std::string& url);
  void OnFaviconDataAvailable(
      NSMenuItem* item,
      const favicon_base::FaviconImageResult& image_result);
  void CancelPendingRequests();
  void UpdateProfile(Profile* profile);

 private:
  std::unique_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;
  favicon::FaviconService* favicon_service_;
  Profile* profile_;
};

}  // namespace vivaldi

#endif  // UI_VIVALDI_MAIN_MENU_MAC_H_
