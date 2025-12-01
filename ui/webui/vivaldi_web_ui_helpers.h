// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef UI_WEB_UI_VIVALDI_WEB_UI_HELPERS_H_
#define UI_WEB_UI_VIVALDI_WEB_UI_HELPERS_H_

#include <string>

#include "content/public/browser/web_ui_data_source.h"

namespace content {
  class WebUIConfigMap;
}

namespace vivaldi {
extern const char* kResourceWebUIBaseDir;

void SetVivaldiPathRequestFilter(::content::WebUIDataSource* source,
                                 const std::string& subdir_name);

void RegisterVivaldiWebIU(::content::WebUIConfigMap &);
} // namespace vivaldi


#endif // UI_WEB_UI_VIVALDI_WEB_UI_HELPERS_H_
