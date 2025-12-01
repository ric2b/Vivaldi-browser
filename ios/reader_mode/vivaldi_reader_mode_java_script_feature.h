// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_READER_MODE_READER_MODE_JAVA_SCRIPT_FEATURE_H_
#define IOS_READER_MODE_READER_MODE_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
class WebState;
class WebFramesManager;
class ScriptMessage;
}  // namespace web

// A class for handling reader mode JavaScript features in a web view.
class VivaldiReaderModeJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // MARK: - Static Methods
  // Singleton instance for the feature.
  static VivaldiReaderModeJavaScriptFeature* GetInstance();

  // MARK: - Observer Management
  // Observer for reader mode availability checks
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnReaderModeAvailabilityResult(web::WebState* web_state,
                                                 bool available) = 0;
    virtual void OnReaderModeEnabledStateResult(web::WebState* web_state,
                                                bool enabled) = 0;
  };

  // Sets the observer for availability check results
  void SetObserver(Observer* observer);

  // MARK: - Reader Mode Control
  // Toggles reader mode for the given web state.
  // Returns true if reader mode is enabled, false if disabled.
  bool ToggleReaderMode(web::WebState* web_state, bool was_enabled);

  // Applies the reader mode for the given web state.
  void ApplyReaderMode(web::WebState* web_state);

  // Disables reader mode for the given web state.
  void DisableReaderMode(web::WebState* web_state);

  // MARK: - Reader Mode Availability Checks
  // Checks if reader mode is available for the current page asynchronously.
  // Result will be delivered via Observer::OnReaderModeAvailabilityResult
  void CheckReaderModeAvailability(web::WebState* web_state);

  // Checks if reader mode is currently enabled for the current page asynchronously.
  // Result will be delivered via Observer::OnReaderModeEnabledStateResult
  void CheckReaderModeEnabledState(web::WebState* web_state);

  // MARK: - Reader Mode Settings
  // Sets the font size for reader mode. Takes a size in pixels (14-24).
  // Returns true if successful.
  bool SetFontSize(web::WebState* web_state, int size);

  // Sets the font family for reader mode. Takes either "serif" or "sans-serif".
  // Returns true if successful.
  bool SetFontFamily(web::WebState* web_state, const std::string& family);

  // Sets the theme for reader mode. Takes "light", "dark", or "sepia".
  // Returns true if successful.
  bool SetTheme(web::WebState* web_state, const std::string& theme);

  // MARK: - Utility Methods
  // Returns the WebFramesManager for the given web state.
  web::WebFramesManager* GetWebFramesManager(web::WebState* web_state);

 private:
  friend class base::NoDestructor<VivaldiReaderModeJavaScriptFeature>;

  // MARK: - Private Constructor/Destructor

  // Constructor and destructor.
  VivaldiReaderModeJavaScriptFeature();
  ~VivaldiReaderModeJavaScriptFeature() override;

  // MARK: - JavaScriptFeature Overrides

  // web::JavaScriptFeature overrides for message handling
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  // MARK: - Private Helper Methods

  // Applies the reader mode for the given web frame.
  void ApplyReaderMode(web::WebFrame* web_frame);

  // Disables reader mode for the given web frame.
  void DisableReaderMode(web::WebFrame* web_frame);



  // MARK: - Private Member Variables
  // Observer for availability check results
  Observer* observer_ = nullptr;

  // Disallow copy and assign.
  VivaldiReaderModeJavaScriptFeature(const
    VivaldiReaderModeJavaScriptFeature&) = delete;
  VivaldiReaderModeJavaScriptFeature&
    operator=(const VivaldiReaderModeJavaScriptFeature&) = delete;
};

#endif  // IOS_READER_MODE_READER_MODE_JAVA_SCRIPT_FEATURE_H_
