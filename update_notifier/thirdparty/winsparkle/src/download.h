/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2016 Vaclav Slavik
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_DOWNLOAD_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_DOWNLOAD_H_

#include <string>
#include <string_view>
#include <vector>

#include "update_notifier/thirdparty/winsparkle/src/error.h"

#include "url/gurl.h"

namespace vivaldi_update_notifier {

// Maximum size of the allowed download.
constexpr int kMaxAllowedDownloadSize = 1024 * 1024 * 1024;

/**
 * Helper to Downloads a HTTP resource.
 */
class FileDownloader {
 public:
  FileDownloader();
  FileDownloader(const FileDownloader&) = delete;
  FileDownloader& operator=(const FileDownloader&) = delete;
  ~FileDownloader();

  // Don't get resources from cache, always contact the origin server
  void DisableCaching() {
    DCHECK(!inet_handle_);
    disable_caching_ = true;
  }

  void SetHeader(std::string_view name, std::string_view value);

  // Connect to the given URL.
  void Connect(const GURL& url, Error& error);

  /**
   * Get reported length of data to download. Return 0 if unknown.
   */
  int GetContentLength() const { return content_length_; }

  const std::string& GetFileName() const { return file_name_; }

  /**
   * Attempt to fetch more data. Return true if a new data is available in the
   * buffer. Return false on end-of-file or errors.
   */
  bool FetchData(Error& error);

  int GetDataLength() const { return data_length_; }

  int GetTotalReadLength() const { return total_read_length_; }

  const char* GetData() const { return buffer_.data(); }

  /**
   * Fetch all remaining data as a string.
   */
  std::string FetchAll(Error& error);

 private:
  void* inet_handle_ = nullptr;
  void* connection_handle_ = nullptr;
  void* request_handle_ = nullptr;
  std::string http_headers_;
  bool disable_caching_ = false;
  int content_length_ = 0;
  int data_length_ = 0;
  int total_read_length_ = 0;
  std::string file_name_;
  std::vector<char> buffer_;
};

void ReportWindowsError(const char* api_function, Error& error);

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_DOWNLOAD_H_
