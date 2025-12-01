// Copyright (c) 2015 Vivaldi Technologies.
// Defines constants used by Vivaldi.

#include "app/vivaldi_constants.h"

namespace vivaldi {

// All constants in alphabetical order. The constants should be documented

const char kExtDataKey[] = "vivExtData";

// Bookmark error message
const char kNicknameExists[] = "Nickname exists.";

// The Application ID of the Vivaldi extension
const char kVivaldiAppId[] = VIVALDI_APP_ID;
const char kVivaldiAppIdHex[] = VIVALDI_APP_HEX_ID;

// Defines the prefix URL for the Vivaldi extension
const char kVivaldiAppURLDomain[] = "chrome-extension://" VIVALDI_APP_ID "/";
// Defines the URL of the bug report webpage
const char kVivaldiBugReportUrl[] = VIVALDI_BUGREPORT_URL;
// Defines the URL that should be used for the crash report upload
const char kVivaldiCrashReportUploadUrl[] = VIVALDI_CRASH_REPORT_UPLOAD_URL;
// Defines the URL that should be opened in an empty incognito window.
const char kVivaldiIncognitoURL[] = "chrome-extension://" VIVALDI_APP_ID
                                 "/components/"
                                 "private-intro/private-intro.html";
const char kVivaldiGuestSessionURL[] = "chrome-extension://" VIVALDI_APP_ID
                                       "/components/"
                                       "guest-intro/guest-intro.html";
const char kVivaldiMailURL[] = "chrome-extension://" VIVALDI_APP_ID
                               "/components/mail/mail.html";
// Defines the URL that should be opened in an empty new tab or window.
const char kVivaldiNewTabURL[] = "chrome://vivaldi-webui/startpage";

const char kVivaldiCrashStartPageURL[] = "chrome-extension://" VIVALDI_APP_ID
                               "/components/session-recovery/session-recovery.html";

// Error message reported to extensions trying to use Vivaldi reserved APIs
const char kVivaldiReservedApiError[] =
    "Access denied while trying to set a reserved property.";

const char kVivaldiGameHost[] = "game";

const char kVivaldiNativeScheme[] = "vivaldi-native";

const char kVivaldiUIDataHost[] = VIVALDI_DATA_URL_HOST;
const char kVivaldiWebUIHost[] = VIVALDI_WEBUI_URL_HOST;
const char kVivaldiThumbDataHost[] = VIVALDI_THUMB_URL_HOST;

const char kVivaldiUIDataURL[] =
    VIVALDI_DATA_URL_SCHEME "://" VIVALDI_DATA_URL_HOST "/";

const char kVivaldiWebUIURL[] =
    VIVALDI_DATA_URL_SCHEME "://" VIVALDI_WEBUI_URL_HOST "/";

const char kVivaldiThumbURL[] =
    VIVALDI_DATA_URL_SCHEME "://" VIVALDI_THUMB_URL_HOST "/";

const char kWindowExtDataKey[] = "vivExtData";

const char kSparkleAutoInstallSettingName[] = "SUAutomaticallyUpdate";
const char kSparkleLastCheckTimeSettingName[] = "SULastCheckTime";
}  // namespace vivaldi
