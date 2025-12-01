// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#import "ios/reader_mode/vivaldi_reader_mode_java_script_feature.h"

#import <WebKit/WebKit.h>

#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"

namespace {
const char kScriptName[] = "reader_mode_controls";
const char kScriptMessageName[] = "ReaderModeMessage";

// Text zoom values (percentage)
const int kValidTextZoomLevels[] =
    {50, 66, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300};
const int kDefaultTextZoom = 100;
}  // namespace

#pragma mark - Static/Singleton Methods

// static
VivaldiReaderModeJavaScriptFeature*
      VivaldiReaderModeJavaScriptFeature::GetInstance() {
  static base::NoDestructor<VivaldiReaderModeJavaScriptFeature> instance;
  return instance.get();
}

#pragma mark - Constructor/Destructor

VivaldiReaderModeJavaScriptFeature::VivaldiReaderModeJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {
}

VivaldiReaderModeJavaScriptFeature::
      ~VivaldiReaderModeJavaScriptFeature() = default;

#pragma mark - Observer Management

void VivaldiReaderModeJavaScriptFeature::SetObserver(Observer* observer) {
  observer_ = observer;
}

#pragma mark - Reader Mode Control

bool VivaldiReaderModeJavaScriptFeature::
          ToggleReaderMode(web::WebState* web_state, bool was_enabled) {
  if (!web_state) {
    return false;
  }

  if (was_enabled) {
    DisableReaderMode(web_state);
    return false;
  } else {
    ApplyReaderMode(web_state);
    return true;
  }
}

void VivaldiReaderModeJavaScriptFeature::
        ApplyReaderMode(web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  auto frames = web_state->GetPageWorldWebFramesManager()->GetAllWebFrames();

  for (web::WebFrame* frame : frames) {
    ApplyReaderMode(frame);
  }
}

void VivaldiReaderModeJavaScriptFeature::
        DisableReaderMode(web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  auto frames = web_state->GetPageWorldWebFramesManager()->GetAllWebFrames();

  for (web::WebFrame* frame : frames) {
    DisableReaderMode(frame);
  }

  // Restore the original page zoom level
  FontSizeTabHelper* fontSizeTabHelper =
      FontSizeTabHelper::FromWebState(web_state);
  if (fontSizeTabHelper) {
    // This reapplies the current zoom level stored in preferences
    fontSizeTabHelper->SetPageZoomSize(fontSizeTabHelper->GetFontZoomSize());
  }
}

#pragma mark - Reader Mode Availability Checks

void VivaldiReaderModeJavaScriptFeature::CheckReaderModeAvailability(
                                                   web::WebState* web_state) {
  if (!web_state) {
    if (observer_) {
      observer_->OnReaderModeAvailabilityResult(web_state, false);
    }
    return;
  }

  // Target the main frame to ensure message arrives from main frame.
  web::WebFrame* mainFrame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!mainFrame) {
    if (observer_) {
      observer_->OnReaderModeAvailabilityResult(web_state, false);
    }
    return;
  }

  CallJavaScriptFunction(
      mainFrame, "vivaldiReaderMode.checkAvailability",
      base::Value::List()
  );
}

void VivaldiReaderModeJavaScriptFeature::CheckReaderModeEnabledState(
                                                  web::WebState* web_state) {
  if (!web_state) {
    if (observer_) {
      observer_->OnReaderModeEnabledStateResult(web_state, false);
    }
    return;
  }

  // Target the main frame to ensure message arrives from main frame.
  web::WebFrame* mainFrame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!mainFrame) {
    if (observer_) {
      observer_->OnReaderModeEnabledStateResult(web_state, false);
    }
    return;
  }

  CallJavaScriptFunction(
      mainFrame, "vivaldiReaderMode.checkEnabledState",
      base::Value::List()
  );
}

#pragma mark - Reader Mode Settings

bool VivaldiReaderModeJavaScriptFeature::
       SetFontSize(web::WebState* web_state, int size) {
  if (!web_state) {
    return false;
  }

  // Validate the zoom level is one of the allowed values
  bool validZoom = false;
  for (int zoomLevel : kValidTextZoomLevels) {
    if (size == zoomLevel) {
      validZoom = true;
      break;
    }
  }

  if (!validZoom) {
    size = kDefaultTextZoom;
  }

  // Use iOS native text zoom instead of JavaScript font size
  ios::provider::SetTextZoomForWebState(web_state, size);

  return true;
}

bool VivaldiReaderModeJavaScriptFeature::
       SetFontFamily(web::WebState* web_state, const std::string& family) {
  if (!web_state) {
    return false;
  }

  // Execute the command in all frames using CallJavaScriptFunction
  auto frames = web_state->GetPageWorldWebFramesManager()->GetAllWebFrames();
  if (frames.empty()) {
    return false;
  }

  for (web::WebFrame* frame : frames) {
    if (frame) {
      CallJavaScriptFunction(
          frame, "vivaldiReaderMode.setFontFamily",
          base::Value::List().Append(family)
      );
    }
  }
  return true;
}

bool VivaldiReaderModeJavaScriptFeature::
       SetTheme(web::WebState* web_state, const std::string& theme) {
  if (!web_state) {
    return false;
  }
  // Validate theme
  if (theme != "light" && theme != "dark" &&
      theme != "sepia" && theme != "black") {
    return false;
  }
  // Execute the command in all frames using CallJavaScriptFunction
  auto frames = web_state->GetPageWorldWebFramesManager()->GetAllWebFrames();
  if (frames.empty()) {
    return false;
  }

  for (web::WebFrame* frame : frames) {
    if (frame) {
      CallJavaScriptFunction(
          frame, "vivaldiReaderMode.setTheme",
          base::Value::List().Append(theme)
      );
    }
  }
  return true;
}

#pragma mark - Utility Methods

web::WebFramesManager* VivaldiReaderModeJavaScriptFeature::GetWebFramesManager(
    web::WebState* web_state) {
  if (!web_state) {
    return nullptr;
  }
  return web_state->GetPageWorldWebFramesManager();
}

#pragma mark - JavaScriptFeature Overrides

std::optional<std::string>
VivaldiReaderModeJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptMessageName;
}

void VivaldiReaderModeJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.is_main_frame() || !message.body() ||
      !message.body()->is_dict()) {
    return;
  }

  const base::Value::Dict& dict = message.body()->GetDict();
  const std::string* command = dict.FindString("command");

  if (!command) {
    return;
  }

  if (*command == "reader_mode_availability") {
    bool available = dict.FindBool("available").value_or(false);

    // Notify the C++ observer
    if (observer_) {
      observer_->OnReaderModeAvailabilityResult(web_state, available);
    }
  } else if (*command == "reader_mode_enabled_state") {
    bool enabled = dict.FindBool("enabled").value_or(false);

    // Notify the C++ observer
    if (observer_) {
      observer_->OnReaderModeEnabledStateResult(web_state, enabled);
    }
  }
}

#pragma mark - Private Helper Methods
void VivaldiReaderModeJavaScriptFeature::
       ApplyReaderMode(web::WebFrame* web_frame) {
  if (web_frame) {
    CallJavaScriptFunction(
        web_frame, "vivaldiReaderMode.enable",
        base::Value::List()
      );
  }
}

void VivaldiReaderModeJavaScriptFeature::
       DisableReaderMode(web::WebFrame* web_frame) {
  if (web_frame) {
    CallJavaScriptFunction(
        web_frame, "vivaldiReaderMode.disable",
        base::Value::List()
    );
  }
}
