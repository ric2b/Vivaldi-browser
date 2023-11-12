// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SANDBOX_VIVALDI_MODULE_NAME_H_
#define COMPONENTS_SANDBOX_VIVALDI_MODULE_NAME_H_

#include <windows.h>

namespace sandbox {

class InterceptionManager;

bool VivaldiSetupBasicInterceptions(InterceptionManager* manager);

}  // namespace sandbox

#endif  // COMPONENTS_SANDBOX_VIVALDI_MODULE_NAME_H_
