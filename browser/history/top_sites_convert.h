// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_HISTORY_TOP_SITES_CONVERT_H_
#define BROWSER_HISTORY_TOP_SITES_CONVERT_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"

using ConvertThumbnailDataCallback = base::RepeatingCallback<void(
    const base::FilePath& path,
    int64_t bookmark_id,
    scoped_refptr<base::RefCountedMemory> thumbnail)>;

namespace vivaldi {
void ConvertThumbnailDataOnUIThread(
  const base::FilePath& path,
  int64_t bookmark_id,
  scoped_refptr<base::RefCountedMemory> thumbnail);
}

#endif  // BROWSER_HISTORY_TOP_SITES_CONVERT_H_
