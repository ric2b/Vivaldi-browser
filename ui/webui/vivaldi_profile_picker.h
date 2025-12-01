// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef UI_PROFILE_PICKER_VIVALDI_PROFILE_PICKER_H_
#define UI_PROFILE_PICKER_VIVALDI_PROFILE_PICKER_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "content/public/browser/web_ui.h"

extern const char* kVivaldiUIVivaldiProfilePickerUI;

class VivaldiProfilePickerUI;

class VivaldiProfilePickerUIConfig
    : public content::DefaultWebUIConfig<VivaldiProfilePickerUI> {
 public:
  VivaldiProfilePickerUIConfig()
      : content::DefaultWebUIConfig<VivaldiProfilePickerUI>(
            content::kChromeUIScheme,
            kVivaldiUIVivaldiProfilePickerUI) {}
};

class VivaldiProfilePickerUI : public content::WebUIController {
public:
  explicit VivaldiProfilePickerUI(content::WebUI* web_ui);
};

#endif // UI_PROFILE_PICKER_VIVALDI_PROFILE_PICKER_H_
