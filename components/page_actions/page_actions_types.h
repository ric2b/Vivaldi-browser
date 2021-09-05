// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_TYPES_H_
#define COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_TYPES_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace page_actions {
struct MatchPattern {
  MatchPattern();
  ~MatchPattern();
  MatchPattern(MatchPattern&& other);
  MatchPattern& operator=(MatchPattern&& other);

  std::string scheme;
  std::string domain;
  std::string path;
};

struct ScriptFile {
  ScriptFile();
  ~ScriptFile();
  ScriptFile(ScriptFile&& other);
  ScriptFile& operator=(ScriptFile&& other);

  std::vector<MatchPattern> included_patterns;
  std::vector<MatchPattern> excluded_patterns;

  std::vector<std::string> included_glob;
  std::vector<std::string> excluded_glob;

  std::string content;

  bool active = false;
};

using ScriptFiles = std::map<base::FilePath, ScriptFile>;

struct ScriptDirectory {
  ScriptDirectory();
  ~ScriptDirectory();
  ScriptDirectory(ScriptDirectory&& other);
  ScriptDirectory& operator=(ScriptDirectory&& other);

  bool valid = false;
  ScriptFiles script_files;
};

}  // namespace page_actions

#endif  // COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_TYPES_H_
