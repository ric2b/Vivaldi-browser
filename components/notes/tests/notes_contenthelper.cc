// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notes/tests/notes_contenthelper.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace notes_helper {

std::string IndexedURL(int i) {
  return base::StringPrintf("http://www.host.ext:1234/path/filename/%d", i);
}

std::string IndexedURLTitle(int i) {
  return base::StringPrintf("URL Title %d", i);
}

std::string IndexedFolderName(int i) {
  return base::StringPrintf("Folder Name %d", i);
}

std::string IndexedSubfolderName(int i) {
  return base::StringPrintf("Subfolder Name %d", i);
}

std::string IndexedSubsubfolderName(int i) {
  return base::StringPrintf("Subsubfolder Name %d", i);
}

static int content_index = 0;

std::string CreateAutoIndexedContent(int index) {
  if (!index)
    index = ++content_index;
  return base::StringPrintf("Note Auto Content #%d\nLine 1\nLine 2", index);
}

}  // namespace notes_helper
