// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_READER_MODE_READER_MODE_CONTROLLER_H_
#define IOS_READER_MODE_READER_MODE_CONTROLLER_H_

#import "base/check.h"
#import "ios/reader_mode/vivaldi_reader_mode_java_script_feature.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

class Browser;

// Objective-C protocol for reader mode controller observers
@protocol ReaderModeControllerObserver
- (void)onReaderModeAvailabilityChanged:(BOOL)available;
- (void)onReaderModeEnabledStateChanged:(BOOL)enabled;
@end

// Controller that manages reader mode for a specific WebState.
class ReaderModeController : public web::WebStateObserver,
                            public web::WebStateUserData<ReaderModeController> {
 public:
  ReaderModeController(const ReaderModeController&) = delete;
  ReaderModeController& operator=(const ReaderModeController&) = delete;

  ~ReaderModeController() override;

  // The Browser used to present UI for reader mode controls.
  static void SetBrowser(Browser* browser);
  Browser* GetBrowser() const { return browser_; }

  // Returns the controller for the given WebState, creating one if needed
  static ReaderModeController* FromWebState(web::WebState* web_state);

  // Global reader mode management
  static void DisableReaderModeGlobally();
  static void UpdateReaderModeAvailabilityGlobally();

  // Observer management
  void AddObserver(id<ReaderModeControllerObserver> observer);
  void RemoveObserver(id<ReaderModeControllerObserver> observer);

  // Reader mode availability
  bool IsReaderModeAvailable() const { return is_reader_mode_available_; }
  void CheckReaderModeAvailability();

  // Reader mode enabled state
  bool IsReaderModeEnabled() const;
  void CheckReaderModeEnabledState();

  // Reader mode state
  bool ToggleReaderMode();
  void ApplyReaderMode();
  void DisableReaderMode();

  // Reader mode settings
  bool SetFontSize(int size);
  bool SetFontFamily(const std::string& family);
  bool SetTheme(const std::string& theme);

  // Internal methods called by the observer bridge
  // (declared as public for access from the bridge)
  void OnReaderModeAvailabilityResultInternal(web::WebState* web_state,
                                              bool available);
  void OnReaderModeEnabledStateResultInternal(web::WebState* web_state,
                                              bool enabled);

 private:
  friend class web::WebStateUserData<ReaderModeController>;

  explicit ReaderModeController(web::WebState* web_state);

  // web::WebStateObserver implementation
  void WebStateDestroyed(web::WebState* web_state) override;
  void PageLoaded(web::WebState* web_state,
                  web::PageLoadCompletionStatus load_completion_status) override;

  // Notify all observers of state changes
  void NotifyAvailabilityObservers(bool available);
  void NotifyEnabledStateObservers(bool enabled);

  // Check if reader mode is enabled in user settings
  bool IsReaderModeEnabledInSettings() const;

  // Floating UI management (delegated to Objective-C bridge).
  void CreateFloatingUI();
  void RemoveFloatingUI();
  void PresentReaderModeUI(void* anchor_view_ptr);

  // The WebState this controller manages
  web::WebState* web_state_;
  // Browser to use for presenting UI.
  Browser* browser_ = nullptr;
  // Current reader mode states
  bool is_reader_mode_available_;
  bool is_reader_mode_enabled_;

  // Observers
  NSMutableSet<id<ReaderModeControllerObserver>>* observers_;

  // Opaque retained pointer to the Objective-C UI state
  // object (`VivaldiReaderModeFloatingUI`).
  // Managed via __bridge_retained/CFRelease
  // from C++ to avoid exposing Obj-C types
  // in this header. Owns the FAB button and presentation coordinator state.
  void* objc_ui_state_ = nullptr;
};

#endif  // IOS_READER_MODE_READER_MODE_CONTROLLER_H_
