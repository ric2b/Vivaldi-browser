// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_ANDROID_CONTENT_URI_TEST_UTILS_H_
#define BASE_TEST_ANDROID_CONTENT_URI_TEST_UTILS_H_

namespace base {
class FilePath;

namespace test::android {

// NativeTest app has a FileProvider org.chromium.native_test.fileprovider which
// includes files from its cache dir. If `file_name` is a path under the cache
// dir such as a path created under a ScopedTempDir, we can map it to a content
// URI.
//
// Returns true and sets `content_uri` from `file_name` if mapping succeeds,
// else returns false.
bool GetContentUriFromCacheDirFilePath(const FilePath& file_name,
                                       FilePath* content_uri);

}  // namespace test::android
}  // namespace base

#endif  // BASE_TEST_ANDROID_CONTENT_URI_TEST_UTILS_H_
