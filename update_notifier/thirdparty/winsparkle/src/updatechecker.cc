/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2016 Vaclav Slavik
 *  Copyright (C) 2007 Andy Matuschak
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

#include "update_notifier/thirdparty/winsparkle/src/updatechecker.h"

#include "update_notifier/thirdparty/winsparkle/src/config.h"
#include "update_notifier/thirdparty/winsparkle/src/download.h"
#include "update_notifier/thirdparty/winsparkle/src/settings.h"

#include <inttypes.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <vector>

using namespace std;

namespace winsparkle {

/*--------------------------------------------------------------------------*
                              version comparison
 *--------------------------------------------------------------------------*/

// Note: This code is based on Sparkle's SUStandardVersionComparator by
//       Andy Matuschak.

namespace {

// String characters classification. Valid components of version numbers
// are numbers, period or string fragments ("beta" etc.).
enum class CharType { kNumber, kPeriod, kString };

CharType ClassifyChar(char c) {
  if (c == '.')
    return CharType::kPeriod;
  else if (c >= '0' && c <= '9')
    return CharType::kNumber;
  else
    return CharType::kString;
}

// Split version string into individual components. A component is continuous
// run of characters with the same classification. For example, "1.20rc3" would
// be split into ["1",".","20","rc","3"].
vector<string> SplitVersionString(const string& version) {
  vector<string> list;

  if (version.empty())
    return list;  // nothing to do here

  string s;
  const size_t len = version.length();

  s = version[0];
  CharType prevType = ClassifyChar(version[0]);

  for (size_t i = 1; i < len; i++) {
    const char c = version[i];
    const CharType newType = ClassifyChar(c);

    if (prevType != newType || prevType == CharType::kPeriod) {
      // We reached a new segment. Period gets special treatment,
      // because "." always delimiters components in version strings
      // (and so ".." means there's empty component value).
      list.push_back(s);
      s = c;
    } else {
      // Add character to current segment and continue.
      s += c;
    }

    prevType = newType;
  }

  // Don't forget to add the last part:
  list.push_back(s);

  return list;
}

bool CheckForInsecureURL(const GURL& url, const std::string& purpose) {
  if (!url.SchemeIs("https")) {
    LogError("----------------------------");
    LogError("*** USING INSECURE URL: " + purpose + " from " +
             url.possibly_invalid_spec() + " ***");
    LogError("----------------------------");
    return false;
  }
  return true;
}

}  // anonymous namespace

int CompareVersions(const string& verA, const string& verB) {
  const vector<string> partsA = SplitVersionString(verA);
  const vector<string> partsB = SplitVersionString(verB);

  // Compare common length of both version strings.
  const size_t n = min(partsA.size(), partsB.size());
  for (size_t i = 0; i < n; i++) {
    const string& a = partsA[i];
    const string& b = partsB[i];

    const CharType typeA = ClassifyChar(a[0]);
    const CharType typeB = ClassifyChar(b[0]);

    if (typeA == typeB) {
      if (typeA == CharType::kString) {
        int result = a.compare(b);
        if (result != 0)
          return result;
      } else if (typeA == CharType::kNumber) {
        const int intA = atoi(a.c_str());
        const int intB = atoi(b.c_str());
        if (intA > intB)
          return 1;
        else if (intA < intB)
          return -1;
      }
    } else  // components of different types
    {
      if (typeA != CharType::kString && typeB == CharType::kString) {
        // 1.2.0 > 1.2rc1
        return 1;
      } else if (typeA == CharType::kString && typeB != CharType::kString) {
        // 1.2rc1 < 1.2.0
        return -1;
      } else {
        // One is a number and the other is a period. The period
        // is invalid.
        return (typeA == CharType::kNumber) ? 1 : -1;
      }
    }
  }

  // The versions are equal up to the point where they both still have
  // parts. Lets check to see if one is larger than the other.
  if (partsA.size() == partsB.size())
    return 0;  // the two strings are identical

  // Lets get the next part of the larger version string
  // Note that 'n' already holds the index of the part we want.

  int shorterResult, longerResult;
  CharType missingPartType;  // ('missing' as in "missing in shorter version")

  if (partsA.size() > partsB.size()) {
    missingPartType = ClassifyChar(partsA[n][0]);
    shorterResult = -1;
    longerResult = 1;
  } else {
    missingPartType = ClassifyChar(partsB[n][0]);
    shorterResult = 1;
    longerResult = -1;
  }

  if (missingPartType == CharType::kString) {
    // 1.5 > 1.5b3
    return shorterResult;
  } else {
    // 1.5.1 > 1.5
    return longerResult;
  }
}

time_t GetLastUpdateCheckTime() {
  std::string last_check_str =
      Settings::ReadConfigValue(ConfigKey::kLastCheckTime);
  if (!last_check_str.empty()) {
    char* end;
    errno = 0;
    intmax_t last_check = strtoimax(last_check_str.c_str(), &end, 10);
    if (*end == '\0' && !errno && last_check > 0) {
      // Check overflow.
      time_t t = static_cast<time_t>(last_check);
      if (static_cast<intmax_t>(t) == last_check)
        return t;
    }
  }
  return 0;
}

std::unique_ptr<Appcast> CheckForUpdates(bool manual, Error& error) {
  if (error)
    return nullptr;

  const GURL& url = GetConfig().appcast_url;
  if (!url.is_valid()) {
    error.set(Error::kFormat, "Appcast URL not specified.");
    return nullptr;
  }
  CheckForInsecureURL(url, "appcast feed");

  int download_flags = 0;
  if (manual) {
    // Manual check should always connect to the server and bypass any
    // caching. This is good for finding updates that are too new to propagate
    // through caches yet.
    download_flags = Download_NoCached;
  }
  FileDownloader downloader(url, download_flags, error);
  std::string appcast_xml = downloader.FetchAll(error);
  if (error)
    return nullptr;
  if (appcast_xml.size() == 0) {
    error.set(Error::kFormat, "Appcast XML data incomplete.");
    return nullptr;
  }

  std::unique_ptr<Appcast> appcast = Appcast::Load(appcast_xml, error);
  if (!appcast)
    return nullptr;
  DCHECK(appcast->IsValid());
  if (!appcast->IsValid())
    return nullptr;

  if (appcast->ReleaseNotesURL.is_valid())
    CheckForInsecureURL(appcast->ReleaseNotesURL, "release notes");
  if (appcast->DownloadURL.is_valid())
    CheckForInsecureURL(appcast->DownloadURL, "update file");
  std::string time_seconds_str = std::to_string(time(nullptr));
  Settings::WriteConfigValue(ConfigKey::kLastCheckTime, time_seconds_str);

  const std::string currentVersion = base::UTF16ToUTF8(GetConfig().app_version);

  // Check if our version is out of date.
  if (CompareVersions(currentVersion, appcast->Version) >= 0) {
    // The same or newer version is already installed.
    return nullptr;
  }

  // If the user has previously chosen "Skip version", the following automated
  // update check should skip it. But a new manual check should still show
  // this version to allow the user to reconsider. This is the semantics in
  // Sparkle for Mac.
  if (!manual) {
    std::string toSkip = Settings::ReadConfigValue(ConfigKey::kSkipThisVersion);
    if (toSkip == appcast->Version)
      return nullptr;
  }

  return appcast;
}

}  // namespace winsparkle
