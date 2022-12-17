// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_VIVALDI_INSTALL_MODES_H_
#define INSTALLER_VIVALDI_INSTALL_MODES_H_

#include "base/files/file_path.h"

namespace install_static {

// Hack to avoid enum type/namespace compile issues
#define VIVALDI_INDEX CHROMIUM_INDEX

}  // namespace install_static

namespace vivaldi {
  CLSID GetOrGenerateToastActivatorCLSID(const base::FilePath* = nullptr);
} // ns vivaldi
#endif  // INSTALLER_VIVALDI_INSTALL_MODES_H_
