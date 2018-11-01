// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef APP_VIVALDI_ICON_H_
#define APP_VIVALDI_ICON_H_

// Tricks to get the path name expansion to work
#define STR(X) #X
// Trick to get relative path to work, inlcusion with dirname causes expansion
// from ".." to "..."
#define PPSTR(X) STR(../../ ##X)
#define VIVALDI_RELEASE_ICONPATH(X) app/resources/theme/ ##X/vivaldi.ico
#define VIVALDI_RELEASE_ICON \
  PPSTR(VIVALDI_RELEASE_ICONPATH(VIVALDI_RELEASE_KIND))

#define PPPSTR(X) STR(../../../ ##X)
#define VIVALDI_INSTALLER_ICONPATH(X) installer/images/ ##X/win_installer.ico
#define VIVALDI_INSTALLER_ICON \
  PPPSTR(VIVALDI_INSTALLER_ICONPATH(VIVALDI_RELEASE_KIND))

#endif  // APP_VIVALDI_ICON_H_
