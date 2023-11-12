// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/sandbox/win/platform_media_init.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/win/windows_version.h"

#include <windows.h>

namespace platform_media_init {

namespace {

// Type to distinguish failed-to-load library from not-yet-attempted-to-load.
using LibraryHandle = intptr_t;
constexpr LibraryHandle kNotLoadedLibrary = 0;
constexpr LibraryHandle kFailedLibraryLoad = -1;

static_assert(sizeof(LibraryHandle) == sizeof(HMODULE), "HMODULE is a pointer");

// Media Libraries to preload to enable media decoding and demultiplexing.
enum LibraryMF {
  kLibraryAAC,
  kLibraryH264,
};

static LibraryHandle g_mf_handles[kLibraryH264 + 1];

// This is a copy of switches::kUtilitySubType to avoid dependency on content
// components.
const char kUtilitySubTypeSwitch[] = "utility-sub-type";

LibraryHandle DoLoadLibrary(const wchar_t* library_name) {
  HMODULE library = ::LoadLibrary(library_name);
  if (library)
    return reinterpret_cast<LibraryHandle>(library);

  LOG(WARNING) << " PROPMEDIA(COMMON) : " << __FUNCTION__ << " Failed to load "
               << library_name
               << ". Some media features will not be available.";
  return kFailedLibraryLoad;
}

void LoadMFLibrary(LibraryMF library) {
  LibraryHandle& handle = g_mf_handles[library];
  if (handle != kNotLoadedLibrary)
    return;

  // Try to load a basic support first. If it is not available, do not bother
  // with the given library.
  static LibraryHandle mfplat_handle = kNotLoadedLibrary;
  if (mfplat_handle == kNotLoadedLibrary) {
    mfplat_handle = DoLoadLibrary(L"mfplat.dll");
  }
  if (mfplat_handle == kFailedLibraryLoad) {
    handle = kFailedLibraryLoad;
    return;
  }

  const wchar_t* name;
  switch (library) {
    case kLibraryAAC: {
      bool win8_or_later = base::win::GetVersion() >= base::win::Version::WIN8;
      name = win8_or_later ? L"msauddecmft.dll" : L"msmpeg2adec.dll";
      break;
    }
    case kLibraryH264:
      name = L"msmpeg2vdec.dll";
      break;
  }
  handle = DoLoadLibrary(name);
}

void LoadDecoders() {
  LoadMFLibrary(kLibraryAAC);
  LoadMFLibrary(kLibraryH264);
}

HMODULE GetLibrary(LibraryMF library) {
  LibraryHandle& handle = g_mf_handles[library];
  if (handle == kNotLoadedLibrary) {
    // This can happen only in unit tests as for normal browser runs or browser
    // tests the library should be preloaded before the sandbox. And in unit
    // tests the load should succeed as we do not run those on Windows-N. So
    // check for that to catch a potentially missing library preload before the
    // sandbox for a browser run as inside the sandbox |LoadLibrary()| fails.
    LoadMFLibrary(library);
#if !defined(OFFICIAL_BUILD)
    CHECK_NE(handle, kFailedLibraryLoad);
#endif
  }
  if (handle == kFailedLibraryLoad)
    return nullptr;
  return reinterpret_cast<HMODULE>(handle);
}

}  // namespace

HMODULE GetWMFLibraryForAAC() {
  return GetLibrary(kLibraryAAC);
}

HMODULE GetWMFLibraryForH264() {
  return GetLibrary(kLibraryH264);
}

void InitForGPUProcess() {
  LoadDecoders();
}

void InitForRendererProcess() {
  LoadDecoders();
}

void InitForUtilityProcess(const base::CommandLine& command_line) {
  // Preload audio decoder DLL for audio-related services including CDM or
  // Content-Decryption-Module.
  std::string subtype = command_line.GetSwitchValueASCII(kUtilitySubTypeSwitch);
  if (subtype == "audio.mojom.AudioService" ||
      subtype == "chrome.mojom.MediaParserFactory" ||
      subtype == "media.mojom.CdmServiceBroker") {
    LoadMFLibrary(kLibraryAAC);
  }
}

}  // namespace platform_media_init
