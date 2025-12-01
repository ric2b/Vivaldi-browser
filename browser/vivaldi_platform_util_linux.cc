// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include <fcntl.h>

#include <set>
#include <string>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

#include "browser/vivaldi_platform_util_linux.h"

std::string VivaldiFilterLDPreload() {
  // Basis for the new LD_PRELOAD variable is the current one.
  // We apply filtering based on the description in the header file.
  char* current_preload = getenv("LD_PRELOAD");

  if (!current_preload)
    return std::string{};

  // This will/would contain values that need to be filtered out, same format as
  // LD_PRELOAD
  char* filterable_preload = getenv("VIVALDI_PRELOADS");

  // We want to remove these values from the LD_PRELOAD.
  std::set<std::string> filterable;

  if (filterable_preload) {
    std::vector<std::string> filterables =
        base::SplitString(filterable_preload, ":", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    filterable = std::set<std::string>(filterables.begin(), filterables.end());
  }

  std::vector<std::string> preload_entries = base::SplitString(
      current_preload, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<std::string> filtered_entries;
  for (const auto& entry : preload_entries) {
    if (filterable.find(entry) != filterable.end())
      continue;

    if (!filterable_preload && entry.find("libffmpeg.so") != std::string::npos) {
      continue;
    }

    filtered_entries.push_back(entry);
  }

  if (!filtered_entries.empty()) {
    return base::JoinString(filtered_entries, ":");
  } else {
    return std::string{};
  }
}
