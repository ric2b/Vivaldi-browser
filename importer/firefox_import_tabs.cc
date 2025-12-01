// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include <cstdint>
#include <cstring>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/utility/importer/firefox_importer.h"
#include "components/tab_groups/tab_group_id.h"
#include "thirdparty/lz4/lz4.h"

#include "importer/imported_tab_entry.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chromium/content/public/browser/navigation_entry.h"

namespace {

bool DecompressMozLz4(const base::FilePath& input_path,
                      std::string& output_data) {
  std::string compressed_data;

  if (!base::ReadFileToString(input_path, &compressed_data)) {
    LOG(ERROR) << "FirefoxImport: Failed to read input file: " << input_path;
    return false;
  }

  // Check `mozLz4` header
  constexpr std::array<uint8_t, 8> kMozLz4Header = {'m', 'o', 'z', 'L',
                                                    'z', '4', '0', '\0'};

  if (compressed_data.size() < 12 ||
      !std::equal(kMozLz4Header.begin(), kMozLz4Header.end(),
                  compressed_data.begin())) {
    LOG(ERROR) << "FirefoxImport: Invalid mozLz4 header";
    return false;
  }

  // Read decompressed size (u32 after the header, stored in little-endian
  // format)
  const uint8_t* size_ptr = reinterpret_cast<const uint8_t*>(compressed_data.data() + 8);
  uint32_t decompressed_size = size_ptr[3] << 24 | size_ptr[2] << 16 | size_ptr[1] << 8 | size_ptr[0];

  if (decompressed_size == 0 ||
      decompressed_size > 100 * 1024 * 1024) {  // Limit to 100MB max
    LOG(ERROR) << "FirefoxImport: Invalid decompressed size: "
               << decompressed_size;
    return false;
  }

  // Allocate exact-sized string for decompression
  output_data.resize(decompressed_size);

  // Decompress
  int actual_size =
      LZ4_decompress_safe(compressed_data.data() + 12, output_data.data(),
                          compressed_data.size() - 12, decompressed_size);

  if (actual_size < 0 ||
      static_cast<uint32_t>(actual_size) != decompressed_size) {
    LOG(ERROR) << "FirefoxImport: LZ4 decompression failed or incorrect size";
    return false;
  }

  return true;
}

/// Helper that maps distinct firefox tab group IDs to TabGroupId values stored
/// as strings.
std::string mapGroup(std::map<std::string, std::string>& mappings,
                     const std::string firefox_id) {
  if (firefox_id.empty())
    return "";

  // the group is non-empty. If we already seen it, use the prev mapping,
  // otherwise create one
  auto it = mappings.find(firefox_id);

  if (it != mappings.end())
    return it->second;

  auto new_id = tab_groups::TabGroupId::GenerateNew().ToString();
  auto r = mappings.insert(std::pair{firefox_id, new_id});

  return r.first->second;
}

/// Takes the deserialized JSON and prepares Imported tab instances based on
/// that.
bool ExtractTabsFromSession(const base::Value::Dict& session_dict,
                            std::vector<ImportedTabEntry>& imported_tabs) {
  std::map<std::string, std::string> group_mapping;

  const base::Value::List* windows = session_dict.FindList("windows");
  if (!windows) {
    return false;
  }

  // We iterate all windows, but throw the result into just one.
  for (const auto& window : *windows) {
    if (!window.is_dict()) {
      return false;
    }
    const base::Value::List* tabs = window.GetDict().FindList("tabs");
    if (!tabs) {
      return false;
    }

    for (const auto& tab : *tabs) {
      if (!tab.is_dict()) {
        return false;
      }
      ImportedTabEntry imported_tab;
      imported_tab.pinned = tab.GetDict().FindBool("pinned").value_or(false);
      imported_tab.timestamp = base::Time::Now();
      auto group = tab.GetDict().FindString("groupId");
      if (group)
        imported_tab.group = mapGroup(group_mapping, *group);

      const base::Value::List* entries = tab.GetDict().FindList("entries");
      if (!entries || entries->empty()) {
        return false;
      }

      imported_tab.current_navigation_index =
          std::clamp(tab.GetDict().FindInt("index").value_or(0), 0,
                     static_cast<int>(entries->size()) - 1);

      for (const auto& entry : *entries) {
        if (!entry.is_dict()) {
          return false;
        }
        NavigationEntry nav_entry;
        const std::string* url = entry.GetDict().FindString("url");
        const std::string* title = entry.GetDict().FindString("title");
        const std::string* favicon_url = entry.GetDict().FindString("image");
        nav_entry.url = GURL(url ? *url : "about:blank");
        nav_entry.title = title ? base::UTF8ToUTF16(*title) : std::u16string();
        nav_entry.favicon_url = favicon_url ? GURL(*favicon_url) : GURL();
        imported_tab.navigations.push_back(std::move(nav_entry));
      }

      imported_tabs.push_back(std::move(imported_tab));
    }
  }

  return true;
}

}  // namespace

namespace vivaldi {

void ImportFirefoxTabs(FirefoxImporter* instance,
                       scoped_refptr<ImporterBridge>& bridge,
                       const base::FilePath& sessionstore_path) {
  std::string decompressed_data;

  if (!DecompressMozLz4(sessionstore_path, decompressed_data)) {
    LOG(ERROR) << "FirefoxImport: Failed to decompress sessionstore.jsonlz4";
    return;
  }

  std::optional<base::Value> json_value =
      base::JSONReader::Read(decompressed_data);

  if (!json_value || !json_value->is_dict()) {
    LOG(ERROR) << "FirefoxImport: Failed to parse sessionstore JSON";
    return;
  }

  base::Value::Dict* session_dict = json_value->GetIfDict();
  if (!session_dict) {
    LOG(ERROR) << "FirefoxImport: Session JSON is not a dictionary";
    return;
  }

  std::vector<ImportedTabEntry> imported_tabs;
  if (!ExtractTabsFromSession(*session_dict, imported_tabs)) {
    LOG(ERROR) << "FirefoxImport: Could not process the session store.";
    return;
  }

  bridge->AddOpenTabs(imported_tabs);
}

}  // namespace vivaldi
