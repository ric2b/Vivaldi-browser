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

#include "update_notifier/thirdparty/winsparkle/src/download.h"

#include "update_notifier/thirdparty/winsparkle/src/config.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/version_info/version_info_values.h"

#include <Windows.h>
#include <wininet.h>
#include <string>

namespace vivaldi_update_notifier {

/*--------------------------------------------------------------------------*
                                helpers
 *--------------------------------------------------------------------------*/

namespace {

void CloseInetHandle(HINTERNET& handle) {
  if (handle) {
    InternetCloseHandle(handle);
    handle = nullptr;
  }
}

std::wstring MakeUserAgent() {
  std::string userAgent = PRODUCT_NAME "/" VIVALDI_VERSION_STRING " WinSparkle/0.5.2";
#ifdef _WIN64
  userAgent += " (Win64)";
#endif
  return base::UTF8ToWide(userAgent);
}

bool GetHttpNumericHeader(HINTERNET handle, DWORD whatToGet, DWORD& output) {
  DWORD outputSize = sizeof(output);
  DWORD headerIndex = 0;
  whatToGet |= HTTP_QUERY_FLAG_NUMBER;
  return HttpQueryInfoA(handle, whatToGet, &output, &outputSize,
                        &headerIndex) == TRUE;
}

}  // anonymous namespace

FileDownloader::FileDownloader() = default;

void FileDownloader::SetHeader(std::string_view name, std::string_view value) {
  DCHECK(!inet_handle_) << "must not be called after Connect()";
  http_headers_.append(name.data(), name.length());
  http_headers_ += ": ";
  http_headers_.append(value.data(), value.length());
  http_headers_ += "\r\n";
}

void FileDownloader::Connect(const GURL& url, Error& error) {
  if (error)
    return;
  DCHECK(!inet_handle_);

  if (!url.is_valid()) {
    error.set(Error::kNetwork, "Invalid URL - " + url.possibly_invalid_spec());
    return;
  }
  if (!url.SchemeIsHTTPOrHTTPS()) {
    error.set(Error::kNetwork, "Unsupported URL scheme - " + url.spec());
    return;
  }

  INTERNET_SCHEME scheme =
      url.SchemeIs("https") ? INTERNET_SCHEME_HTTPS : INTERNET_SCHEME_HTTP;

  inet_handle_ =
      InternetOpen(MakeUserAgent().c_str(), INTERNET_OPEN_TYPE_PRECONFIG,
                   nullptr,  // lpszProxyName
                   nullptr,  // lpszProxyBypass
                   0);       // dwFlags

  if (!inet_handle_) {
    error.set(Error::kNetwork, LastWin32Error("InternetOpen"));
    return;
  }

  connection_handle_ =
      InternetConnectA(inet_handle_, url.host().c_str(), url.EffectiveIntPort(),
                       nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);

  if (!connection_handle_) {
    error.set(Error::kNetwork, LastWin32Error("InternetConnectA"));
    return;
  }

  DWORD dwFlags = INTERNET_FLAG_NO_UI;
  if (disable_caching_) {
    dwFlags |= INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD;
  }
  if (scheme == INTERNET_SCHEME_HTTPS) {
    dwFlags |= INTERNET_FLAG_SECURE;
  }

  request_handle_ =
      HttpOpenRequestA(connection_handle_, "GET", url.PathForRequest().c_str(),
                       nullptr, nullptr, nullptr, dwFlags, 0);

  if (!request_handle_) {
    error.set(Error::kNetwork, LastWin32Error("HttpOpenRequestA"));
    return;
  }

  if (!http_headers_.empty()) {
    if (!HttpAddRequestHeadersA(
            request_handle_, http_headers_.data(), http_headers_.size(),
            HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD)) {
      error.set(Error::kNetwork, LastWin32Error("HttpAddRequestHeadersA"));
      return;
    }
  }

  bool revocation_connection_error = false;

again:
  if (!HttpSendRequest(request_handle_, nullptr, 0, nullptr, 0)) {
    DWORD error_code = GetLastError();
    if (error_code == ERROR_INTERNET_SEC_CERT_REV_FAILED &&
        !revocation_connection_error) {
      // According to
      // https://docs.microsoft.com/en-us/windows/win32/wininet/wininet-errors
      // and various hints from the internet this is the error returned when the
      // certificate revocation server cannot be contacted due to network
      // problems. When the server was successfully contacted but the
      // certificate was revoked the error would be
      // ERROR_INTERNET_SEC_CERT_REVOKED. As the network errors can be caused by
      // transient problems or by firewalls blocking the revocation checks to
      // protect privacy, we try to connect again without revocation checks.
      // This will be similar how the browsers handles this.
      revocation_connection_error = true;
      DWORD flags = 0;
      DWORD flags_size = sizeof(flags);
      if (!InternetQueryOption(request_handle_, INTERNET_OPTION_SECURITY_FLAGS,
                               &flags, &flags_size)) {
        error.set(Error::kNetwork, LastWin32Error("InternetQueryOption"));
        return;
      }
      DCHECK_EQ(flags_size, sizeof(flags));
      flags |= SECURITY_FLAG_IGNORE_REVOCATION;
      if (!InternetSetOption(request_handle_, INTERNET_OPTION_SECURITY_FLAGS,
                             &flags, sizeof(flags))) {
        error.set(Error::kNetwork, LastWin32Error("InternetSetOption"));
        return;
      }
      goto again;
    }

    // Improve the error message for common connection errors.
    std::string message;
    switch (error_code) {
      case ERROR_INTERNET_NAME_NOT_RESOLVED:
        message = "Cannot resolve DNS name " + url.host();
        break;
      case ERROR_INTERNET_CANNOT_CONNECT:
        message = "Cannot connect to " + url.host() + ":" +
                  std::to_string(url.EffectiveIntPort());
        break;
      default:
        message = "Internet connection error " + std::to_string(error_code) +
                  " for " + url.spec();
        break;
    }
    error.set(Error::kNetwork, std::move(message));
    return;
  }

  DWORD http_status = 0;
  if (!GetHttpNumericHeader(request_handle_, HTTP_QUERY_STATUS_CODE,
                            http_status)) {
    error.set(Error::kNetwork, "HTTP reply without status code");
    return;
  }

  if (http_status == HTTP_STATUS_DENIED ||
      http_status == HTTP_STATUS_PROXY_AUTH_REQ) {
    DWORD dlg_flags = FLAGS_ERROR_UI_FILTER_FOR_ERRORS |
                      FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS |
                      FLAGS_ERROR_UI_FLAGS_GENERATE_DATA;
    DWORD dwError =
        InternetErrorDlg(GetDesktopWindow(), request_handle_,
                         ERROR_INTERNET_INCORRECT_PASSWORD, dlg_flags, nullptr);

    if (dwError == ERROR_INTERNET_FORCE_RETRY)
      goto again;
    if (dwError == ERROR_CANCELLED) {
      error.set(Error::kCancelled);
      return;
    }
  }

  if (http_status >= 400) {
    std::string message("DownloadFile: HTTP error status ");
    message += std::to_string(http_status);
    message += "\nURL: ";
    message += url.spec();
    error.set(Error::kNetwork, std::move(message));
    return;
  }

  // Get content length if possible:
  DWORD content_length;
  if (GetHttpNumericHeader(request_handle_, HTTP_QUERY_CONTENT_LENGTH,
                           content_length)) {
    if (content_length > static_cast<DWORD>(kMaxAllowedDownloadSize)) {
      error.set(Error::kNetwork, "Content-Length is too big - " +
                                     std::to_string(content_length));
    }
    content_length_ = static_cast<int>(content_length);
  }

  // Get filename fron Content-Disposition, if available
  char contentDisposition[256] = {};
  DWORD cdSize = sizeof(contentDisposition);
  if (HttpQueryInfoA(request_handle_, HTTP_QUERY_CONTENT_DISPOSITION,
                     contentDisposition, &cdSize, nullptr)) {
    char* ptr = strstr(contentDisposition, "filename=");
    if (ptr) {
      ptr += 9;
      while (*ptr == ' ')
        ptr++;

      bool quoted = false;
      if (*ptr == '"' || *ptr == '\'') {
        quoted = true;
        ptr++;
      }

      char* filename_start = ptr;
      while (*ptr != ';' && *ptr != 0)
        ptr++;

      size_t filename_length = ptr - filename_start;
      if (quoted && filename_length > 0) {
        filename_length--;
      }
      if (filename_length > 0) {
        file_name_ = std::string(filename_start, filename_length);
      }
    }
  }

  if (file_name_.empty()) {
    // Fallback to using the name from the URL path.
    file_name_ = url.ExtractFileName();
  }

  // Make sure that all characters in the file name are filesystem-safe.
  for (char& c : file_name_) {
    if (c < ' ' || strchr("\\/:?*|<>\x7F", c)) {
      c = 'X';
    }
  }
  if (file_name_.empty()) {
    file_name_ = "unknown.bin";
  }
}

FileDownloader::~FileDownloader() {
  CloseInetHandle(request_handle_);
  CloseInetHandle(connection_handle_);
  CloseInetHandle(inet_handle_);
}

bool FileDownloader::FetchData(Error& error) {
  if (error)
    return false;
  DCHECK(request_handle_);
  data_length_ = 0;

  if (buffer_.empty()) {
    buffer_.resize(10240);
  }

  DWORD read;
  if (!InternetReadFile(request_handle_, buffer_.data(), buffer_.size(),
                        &read)) {
    error.set(Error::kNetwork, LastWin32Error("InternetReadFile"));
    return false;
  }
  if (read == 0) {
    // Reading is done.
    if (content_length_ > 0 && content_length_ != total_read_length_) {
      error.set(Error::kNetwork,
                "Incomplete download (" + std::to_string(total_read_length_) +
                    " of " + std::to_string(content_length_) + " bytes)");
    }
    return false;
  }
  int limit = content_length_ > 0 ? content_length_ : kMaxAllowedDownloadSize;
  if (read > static_cast<DWORD>(limit - total_read_length_)) {
    error.set(Error::kNetwork, "the downloaded size exceeded the limit of " +
                                   std::to_string(limit) + " bytes");
    return false;
  }
  data_length_ = static_cast<int>(read);
  total_read_length_ += data_length_;
  return true;
}

std::string FileDownloader::FetchAll(Error& error) {
  std::string data;
  while (FetchData(error)) {
    data.append(GetData(), GetDataLength());
  }
  return data;
}

}  // namespace vivaldi_update_notifier
