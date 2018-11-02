// Copyright (c) 2015 Vivaldi Technologies.
// Defines constants used by Vivaldi.

#include "app/vivaldi_constants.h"

namespace vivaldi {

// All constants in alphabetical order. The constants should be documented

const char kExtDataKey[] = "extData";

// Bookmark error message
const char kNicknameExists[] = "Nickname exists.";

// The Application ID of the Vivaldi extension
const char kVivaldiAppId[] = VIVALDI_APP_ID;
const char kVivaldiAppIdHex[] = VIVALDI_APP_HEX_ID;

// Defines the prefix URL for the Vivaldi extension
const char kVivaldiAppURLDomain[] = "chrome-extension://" VIVALDI_APP_ID "/";
// Defines the URL that should be opened in an empty incognito window.
const char kVivaldiIncognitoURL[] = "chrome-extension://" VIVALDI_APP_ID
                                 "/components/"
                                 "private-intro/private-intro.html";
// Defines the URL that should be opened in an empty new tab or window.
const char kVivaldiNewTabURL[] = "chrome-extension://" VIVALDI_APP_ID
                                 "/components/"
                                 "startpage/startpage.html";

// Error message reported to extensions trying to use Vivaldi reserved APIs
const char kVivaldiReservedApiError[] =
    "Access denied while trying to set a reserved property.";

const char kVivaldiUIScheme[] = "vivaldi";

const char kVivaldiUIDataHost[] = "vivaldi-data";

const char kVivaldiUIDataURL[] = "chrome://vivaldi-data/";

const char kWindowExtDataKey[] = "extData";

const wchar_t kUpdateNotifierAutorunName[] = L"Vivaldi Update Notifier";

}  // namespace vivaldi
