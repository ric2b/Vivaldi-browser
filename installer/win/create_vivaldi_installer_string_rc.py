#!/usr/bin/env python

import os

chromium_installer_strings = os.path.join(
    "../../chromium/chrome/installer/util/prebuild",
    "create_installer_string_rc.py"
)

chromium_settings = dict();
exec(open(chromium_installer_strings).read(), globals(), chromium_settings)

STRING_IDS = chromium_settings['STRING_IDS'] + [
  'IDS_INSTALL_MODE_ADVANCED',
  'IDS_INSTALL_MODE_SIMPLE',
  'IDS_INSTALL_TXT_TOS_ACCEPT_AND_INSTALL',
  'IDS_INSTALL_TXT_TOS_ACCEPT_AND_UPDATE',
  'IDS_INSTALL_TOS_ACCEPT_AND_INSTALL',
  'IDS_INSTALL_TOS_ACCEPT_AND_UPDATE',
  'IDS_INSTALL_INSTALL_FOR_ALL_USERS',
  'IDS_INSTALL_INSTALL_PER_USER',
  'IDS_INSTALL_INSTALL_STANDALONE',
  'IDS_INSTALL_SELECT_A_FOLDER',
  'IDS_INSTALL_DEST_FOLDER_INVALID',
  'IDS_INSTALL_NOT_FINISHED_PROMPT',
  'IDS_INSTALL_INSTALLER_NAME',
  'IDS_INSTALL_INSTALL_CAPTION',
  'IDS_INSTALL_LANGUAGE',
  'IDS_INSTALL_INSTALLATION_TYPE',
  'IDS_INSTALL_DEST_FOLDER',
  'IDS_INSTALL_BROWSE',
  'IDS_INSTALL_SET_AS_DEFAULT',
  'IDS_INSTALL_MAKE_STANDALONE_AVAIL',
  'IDS_DISABLE_AUTOUPDATE',
  'IDS_INSTALL_RELAUNCH_WARNING',
  'IDS_INSTALL_CANCEL',
  'IDS_INSTALL_PROGRESS_CAPTION',
  'IDS_INSTALL_COPYRIGHT_AND_POLICY',
  'IDS_INSTALL_OUTDATED_WINDOWS_VERSION',
  'IDS_INSTALL_RUNNING_EMULATED_ON_ARM64'
]
