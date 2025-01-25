// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

// Defines all the command-line switches used by Vivaldi.

#ifndef APP_VIVALDI_CONSTANTS_H_
#define APP_VIVALDI_CONSTANTS_H_

#define VIVALDI_APP_ID "mpognobbkildjkofajifpdfhcoklimli"
#define VIVALDI_APP_ID_U u"mpognobbkildjkofajifpdfhcoklimli"
#define VIVALDI_APP_HEX_ID "A414F2F4543AC9C68DDF663D0A459E6053E3373C"

#define VIVALDI_DATA_URL_SCHEME "chrome"

#define VIVALDI_DATA_URL_HOST "vivaldi-data"
#define VIVALDI_WEBUI_URL_HOST "vivaldi-webui"
#define VIVALDI_THUMB_URL_HOST "thumb"

#define HOSTNAME_404 "return-404.vivaldi.com"
#define KNOWN_404(path) "https://" HOSTNAME_404 path

#define VIVALDI_HELP_SETTINGS_ABOUT "https://help.vivaldi.com/?pk_campaign=v-menu"
#define VIVALDI_HELP_PRIVACY_ABOUT_UNSAFE \
  "https://help.vivaldi.com/desktop/privacy/warnings-about-unsafe-websites/"
#define VIVALDI_CRASH_REPORT_UPLOAD_URL "https://crash.vivaldi.com/submit"
#define VIVALDI_BUGREPORT_URL "https://vivaldi.com/bugreport"

namespace vivaldi {

// All constants in alphabetical order. The constants should be documented
// alongside the definition of their values in the .cc file.

extern const char kExtDataKey[];

extern const char kNicknameExists[];
extern const char kVivaldiAppId[];
extern const char kVivaldiAppIdHex[];

extern const char kVivaldiAppURLDomain[];
extern const char kVivaldiBugReportUrl[];
extern const char kVivaldiCrashReportUploadUrl[];
extern const char kVivaldiNewTabURL[];
extern const char kVivaldiIncognitoURL[];
extern const char kVivaldiGuestSessionURL[];
extern const char kVivaldiMailURL[];
extern const char kVivaldiCrashStartPageURL[];

extern const char kVivaldiReservedApiError[];

inline constexpr char kVivaldiUIScheme[] = "vivaldi";
extern const char kVivaldiGameHost[];

extern const char kVivaldiNativeScheme[];

extern const char kVivaldiUIDataHost[];
extern const char kVivaldiWebUIHost[];
extern const char kVivaldiThumbDataHost[];
extern const char kVivaldiUIDataURL[];
extern const char kVivaldiWebUIURL[];
extern const char kVivaldiThumbURL[];

extern const char kWindowExtDataKey[];

extern const char kSparkleAutoInstallSettingName[];
extern const char kSparkleLastCheckTimeSettingName[];

inline constexpr int kVivaldiCrashLoopDetectionTimeoutInSeconds = 10;
}  // namespace vivaldi

#endif  // APP_VIVALDI_CONSTANTS_H_
