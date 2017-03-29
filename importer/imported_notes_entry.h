// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIVALDI_IMPORTER_IMPORTED_NOTES_ENTRY_H_
#define VIVALDI_IMPORTER_IMPORTED_NOTES_ENTRY_H_

#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

struct ImportedNotesEntry {
	ImportedNotesEntry();
	~ImportedNotesEntry();

	bool operator==(const ImportedNotesEntry& other) const;

	bool is_folder;
	GURL url;
	std::vector<base::string16> path;
	base::string16 title;
	base::string16 content;
	base::Time creation_time;
};

#endif  // VIVALDI_IMPORTER_IMPORTED_NOTES_ENTRY_H_
