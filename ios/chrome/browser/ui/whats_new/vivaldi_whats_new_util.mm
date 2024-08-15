// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/whats_new/vivaldi_whats_new_util.h"

#import "app/vivaldi_version_info.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"


namespace {

// Helper for version testing. It is assumed that each array hold at least two
// elements.
bool HasVersionChanged(std::vector<std::string> version,
                       std::vector<std::string> last_seen) {
  int last_seen_major, version_major, last_seen_minor, version_minor;
  if (base::StringToInt(version[0], &version_major) &&
      base::StringToInt(last_seen[0], &last_seen_major) &&
      base::StringToInt(version[1], &version_minor) &&
      base::StringToInt(last_seen[1], &last_seen_minor)) {
    return (version_major > last_seen_major) ||
           ((version_minor > last_seen_minor) &&
            (version_major >= last_seen_major));
  } else {
    return false;
  }
}

}  // namespace

NSString* vLastSeenVersionKey = @"vivaldiLastSeenVersion";

bool ShouldShowVivaldiWhatsNewPage () {
  if (::vivaldi::ReleaseKind() < ::vivaldi::Release::kBeta) {
    // Only show for Stable channel
    return false;
  }

  bool version_changed = false;
  std::string version = vivaldi::GetVivaldiVersionString();
  std::string last_seen_version = base::SysNSStringToUTF8(
      [[NSUserDefaults standardUserDefaults] stringForKey:vLastSeenVersionKey]);

  std::vector<std::string> version_array = base::SplitString(
      version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<std::string> last_seen_array =
      base::SplitString(last_seen_version, ".", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  if (version_array.size() != 4 || last_seen_array.size() != 4 ) {
    version_changed = true;
  } else {
    version_changed =
        HasVersionChanged(version_array, last_seen_array);
  }

  if (version_changed) {
    [[NSUserDefaults standardUserDefaults]
        setObject:base::SysUTF8ToNSString(version)
           forKey:vLastSeenVersionKey];
  }

  return version_changed;
}
