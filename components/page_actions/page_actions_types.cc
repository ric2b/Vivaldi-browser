// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/page_actions/page_actions_types.h"

namespace page_actions {
MatchPattern::MatchPattern() = default;
MatchPattern::~MatchPattern() = default;
MatchPattern::MatchPattern(MatchPattern&& other) = default;
MatchPattern& MatchPattern::operator=(MatchPattern&& other) = default;

ScriptFile::ScriptFile() = default;
ScriptFile::~ScriptFile() = default;
ScriptFile::ScriptFile(ScriptFile&& other) = default;
ScriptFile& ScriptFile::operator=(ScriptFile&& other) = default;

ScriptDirectory::ScriptDirectory() = default;
ScriptDirectory::~ScriptDirectory() = default;
ScriptDirectory::ScriptDirectory(ScriptDirectory&& other) = default;
ScriptDirectory& ScriptDirectory::operator=(ScriptDirectory&& other) = default;
}  // namespace page_actions