// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_STATUS_H_
#define EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_STATUS_H_

enum class AutoUpdateStatus {
  kNoUpdate,
  kDidAbortWithError,
  kDidFindValidUpdate,
  kWillDownloadUpdate,
  kDidDownloadUpdate,
  kWillInstallUpdateOnQuit,
  kUpdaterDidRelaunchApplication
};

#endif  // EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_STATUS_H_