// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

#include "chrome/grit/profile_picker_resources.h"
#include "base/memory/ref_counted_memory.h"
#include "components/datasource/resource_reader.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"

#include "ui/webui/vivaldi_profile_picker.h"
#include "ui/webui/vivaldi_profile_picker_handler.h"
#include "ui/webui/vivaldi_web_ui_helpers.h"

#include "components/strings/grit/components_strings.h"
#include "vivaldi/grit/vivaldi_native_unscaled.h"

const char* kVivaldiUIVivaldiProfilePickerUI = "profile-picker";

VivaldiProfilePickerUI::VivaldiProfilePickerUI(::content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  ::content::WebUIDataSource* source = ::content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kVivaldiUIVivaldiProfilePickerUI);

  static constexpr webui::LocalizedString localized_strings[] = {
      {"guestMode", IDS_PROFILE_PICKER_GUEST_MODE},
      {"onStartup", IDS_PROFILE_PICKER_SHOW_ON_STARTUP},
      {"introText", IDS_PROFILE_PICKER_INTRO_TEXT},
      {"whoIsUsing", IDS_PROFILE_PICKER_WHOIS_USING},
  };

  source->AddLocalizedStrings(localized_strings);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  vivaldi::SetVivaldiPathRequestFilter(source, "profile_picker");
  web_ui->AddMessageHandler(std::make_unique<VivaldiProfilePickerHandler>());
}
