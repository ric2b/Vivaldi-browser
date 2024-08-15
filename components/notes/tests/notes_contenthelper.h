// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NOTES_TESTS_NOTES_CONTENTHELPER_H_
#define NOTES_TESTS_NOTES_CONTENTHELPER_H_

#include <string>

namespace notes_helper {

// Returns a URL identifiable by |i|.
std::string IndexedURL(int i);

// Returns a URL title identifiable by |i|.
std::string IndexedURLTitle(int i);

// Returns a folder name identifiable by |i|.
std::string IndexedFolderName(int i);

// Returns a subfolder name identifiable by |i|.
std::string IndexedSubfolderName(int i);

// Returns a subsubfolder name identifiable by |i|.
std::string IndexedSubsubfolderName(int i);

std::string CreateAutoIndexedContent(int index = 0);

}  // namespace notes_helper

#endif  // NOTES_TESTS_NOTES_CONTENTHELPER_H_
