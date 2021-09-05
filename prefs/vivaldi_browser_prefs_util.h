// Copyright (c) 2020 Vivaldi Technologies

#ifndef PREFS_VIVALDI_BROWSERPREFS_UTIL_H_
#define PREFS_VIVALDI_BROWSERPREFS_UTIL_H_

#include "base/files/file_path.h"

namespace vivaldi {

bool GetDeveloperFilePath(const base::FilePath::StringPieceType& filename,
                          base::FilePath* file);

}  // vivaldi

#endif  // PREFS_VIVALDI_BROWSERPREFS_UTIL_H_
