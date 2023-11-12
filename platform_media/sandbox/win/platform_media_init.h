// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_SANDBOX_WIN_PLATFORM_MEDIA_INIT_H_
#define PLATFORM_MEDIA_SANDBOX_WIN_PLATFORM_MEDIA_INIT_H_

#if !defined(USE_SYSTEM_PROPRIETARY_CODECS)
#error Should only be built with USE_SYSTEM_PROPRIETARY_CODECS
#endif

#include "base/base_export.h"
#include "base/win/windows_types.h"

namespace base {
class CommandLine;
}

namespace platform_media_init {

// To use Windows decoders for audio and video we must access the corresponding
// DLLs. But those can only be loaded before Chromium initializes the sandbox.
// This file provides helpers to do that. Then the code that needs a DLL handle
// calls one of the Get methods here.
//
// This uses BASE_EXPORT and is placed in the base component, not media, so the
// functions here can be used during early process initialization.

BASE_EXPORT void InitForGPUProcess();

BASE_EXPORT void InitForRendererProcess();

// Preload necessary media libraries for the utility process with the given
// command line so media works inside the sandbox.
BASE_EXPORT void InitForUtilityProcess(const base::CommandLine& command_line);

// Get WMF AAC library. Unless in unit tests this must be called after  calling
// one of |InitFor*()| functions.
BASE_EXPORT HMODULE GetWMFLibraryForAAC();

// Get WMF h264 library. Unless in unit tests this must be called after calling
// one of |InitFor*()| functions.
BASE_EXPORT HMODULE GetWMFLibraryForH264();

}  // namespace platform_media_init

#endif  // PLATFORM_MEDIA_SANDBOX_WIN_PLATFORM_MEDIA_INIT_H_
