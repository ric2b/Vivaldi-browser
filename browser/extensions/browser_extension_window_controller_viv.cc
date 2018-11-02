// Copyright (c) 2018 Vivaldi Technologies

#include "chrome/browser/extensions/browser_extension_window_controller.h"

#include "app/vivaldi_constants.h"
#include "chrome/browser/ui/browser.h"


std::unique_ptr<base::DictionaryValue>
BrowserExtensionWindowController::CreateWindowValue() const {
  std::unique_ptr<base::DictionaryValue> result =
    extensions::WindowController::CreateWindowValue();
  result->SetString(vivaldi::kWindowExtDataKey, browser_->ext_data());
  return result;
}
