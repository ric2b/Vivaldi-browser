// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread_restrictions.h"
#include "components/datasource/resource_reader.h"
#include "content/public/browser/webui_config_map.h"

#include "ui/webui/vivaldi_profile_picker.h"

namespace vivaldi {
const char* kResourceWebUIBaseDir = "resources/web_ui";

namespace {

bool IsPathSafe(const std::string& path) {
  for (size_t i = 0; i < path.size(); ++i) {
    // Prevent '..' in the url.
    if (i > 0 && path[i] == '.' && path[i-1] == '.')
      return false;

    // Prevent passing '.' character by %-notation
    if (path[i] == '%')
      return false;
  }
  return true;
}

} // namespace


void SetVivaldiPathRequestFilter(::content::WebUIDataSource* source,
                                 const std::string& subdir_name) {
  source->SetRequestFilter(
      base::BindRepeating([](const std::string& path) {
        // strings.js is defined by source->UseStringsJs()
        if (path == "strings.js") {
          return false;
        }
        return true;
      }),
      base::BindRepeating(
          [](std::string web_ui_subdir, const std::string& path_arg,
             content::WebUIDataSource::GotDataCallback callback) {
            base::VivaldiScopedAllowBlocking allow_blocking;

            std::string root_dir =
                std::string(kResourceWebUIBaseDir) + "/" + web_ui_subdir;
            std::string index = root_dir + "/index.html";

            std::string path = path_arg;
            if (!IsPathSafe(path)) {
              // Anything not safe reaches index.
              path = index;
            }

            std::string resource_path;
            if (path.empty()) {
              // Empty path reaches the index.
              resource_path = index;
            } else {
              // Concat path to the root dir.
              resource_path = std::string(root_dir) + "/" + path;
            }

            std::string content;

            {
              // Try to read the content.
              ResourceReader rr(resource_path);
              if (rr.IsValid()) {
                content = std::string(rr.as_string_view());
              } else {
                // The path does not exist, go to the index.
                resource_path = index;
              }
            }

            if (content.empty()) {
              // Retry.
              ResourceReader rr(resource_path);
              if (rr.IsValid()) {
                content = std::string(rr.as_string_view());
              } else {
                content = "Error: " + path + "; " + resource_path + ";";
              }
            }

            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(content));
          },
          subdir_name));
}

void RegisterVivaldiWebIU(::content::WebUIConfigMap &map) {
  map.AddWebUIConfig(std::make_unique<VivaldiProfilePickerUIConfig>());
}

} // namespace vivaldi
