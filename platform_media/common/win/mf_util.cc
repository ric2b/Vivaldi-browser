// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/common/win/mf_util.h"

#include "base/logging.h"
#include "base/win/windows_version.h"

#include <windows.h>

namespace media {

namespace {

static HMODULE g_audio_decoder_dll = nullptr;
static HMODULE g_video_decoder_dll = nullptr;
static bool g_demuxer_support = false;

const char* GetMFAudioDecoderLibraryName() {
  const base::win::Version version = base::win::GetVersion();
  if (version >= base::win::Version::WIN8)
    return "msauddecmft.dll";
  return "msmpeg2adec.dll";
}

const char* GetMFVideoDecoderLibraryName() {
  return "msmpeg2vdec.dll";
}

HMODULE LoadMFLibrary(const char* library_name) {
  HMODULE library = ::LoadLibraryA(library_name);
  if (library)
    return library;

  LOG(WARNING) << " PROPMEDIA(COMMON) : " << __FUNCTION__ << " Failed to load "
               << library_name
               << ". Some media features will not be available.";
  return nullptr;
}

}  // namespace

void LoadMFDecodingLibraries(bool demuxer_support) {
  DCHECK(!g_audio_decoder_dll);
  DCHECK(!g_video_decoder_dll);
  DCHECK(!g_demuxer_support);

  if (!LoadMFLibrary("mfplat.dll")) {
    // Do not bother with other libraries if the basic support is not available.
    return;
  }
  g_audio_decoder_dll = LoadMFLibrary(GetMFAudioDecoderLibraryName());
  g_video_decoder_dll = LoadMFLibrary(GetMFVideoDecoderLibraryName());
  if (demuxer_support) {
    g_demuxer_support = !!LoadMFLibrary("mfreadwrite.dll");
  }
}

HMODULE GetMFAudioDecoderLibrary() {
  return g_audio_decoder_dll;
}

HMODULE GetMFVideoDecoderLibrary() {
  return g_video_decoder_dll;
}

bool HasMFDemuxerSupport() {
  return g_demuxer_support;
}

}  // namespace media
