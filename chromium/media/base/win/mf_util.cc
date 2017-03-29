// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "media/base/win/mf_util.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/win/windows_version.h"

namespace media {

namespace {

bool LoadMFLibrary(const char* library_name) {
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    DLOG(WARNING) << "We don't support " << library_name
                  << " on this Windows version";
    return false;
  }

  if (::GetModuleHandleA(library_name) == NULL &&
      ::LoadLibraryA(library_name) == NULL) {
    DLOG(WARNING) << "Failed to load " << library_name
                  << ". Some media features will not be available.";
    return false;
  }

  return true;
}

// LazyInstance to have a lazily evaluated, cached result available to multiple
// threads in a safe manner.
class PrimaryLoader {
 public:
  PrimaryLoader()
      : media_foundation_available_(LoadMFLibrary("mfplat.dll")),
        audio_decoder_available_(
            LoadMFLibrary(GetMFAudioDecoderLibraryName().c_str())),
        video_decoder_available_(
            LoadMFLibrary(GetMFVideoDecoderLibraryName().c_str()) &&
            LoadMFLibrary("evr.dll")) {}

  bool is_media_foundation_available() const {
    return media_foundation_available_;
  }
  bool is_audio_decoder_available() const { return audio_decoder_available_; }
  bool is_video_decoder_available() const { return video_decoder_available_; }

 private:
  bool media_foundation_available_;
  bool audio_decoder_available_;
  bool video_decoder_available_;
};

class SecondaryLoader {
 public:
  SecondaryLoader()
      : source_reader_available_(LoadMFLibrary("mfreadwrite.dll") &&
                                 LoadMFLibrary("evr.dll")) {}

  bool is_source_reader_available() const { return source_reader_available_; }

 private:
  bool source_reader_available_;
};

// Provide two separate loaders, one for the common mfplat.dll library plus
// decoder libraries, and another one for mfreadwrite.dll.  The latter provides
// IMFSourceReader, which is only necessary when decoding _and_ demuxing using
// system libraries.
base::LazyInstance<PrimaryLoader> g_primary_loader = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<SecondaryLoader> g_secondary_loader =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool LoadMFCommonLibraries() {
  return g_primary_loader.Get().is_media_foundation_available();
}

bool LoadMFSourceReaderLibraries() {
  return g_secondary_loader.Get().is_source_reader_available();
}

bool LoadMFAudioDecoderLibraries() {
  return g_primary_loader.Get().is_audio_decoder_available();
}

bool LoadMFVideoDecoderLibraries() {
  return g_primary_loader.Get().is_video_decoder_available();
}

std::string GetMFAudioDecoderLibraryName() {
  std::string name;
  const base::win::Version version = base::win::GetVersion();
  if (version >= base::win::VERSION_WIN8)
    name = "msauddecmft.dll";
  else if (version == base::win::VERSION_WIN7)
    name = "msmpeg2adec.dll";
  else if (version == base::win::VERSION_VISTA)
    name = "mfheaacdec.dll";
  else
    NOTREACHED() << "Unexpected Windows version";

  return name;
}

std::string GetMFVideoDecoderLibraryName() {
  std::string name;
  const base::win::Version version = base::win::GetVersion();
  if (version >= base::win::VERSION_WIN7)
    name = "msmpeg2vdec.dll";
  else if (version == base::win::VERSION_VISTA)
    name = "mfh264dec.dll";
  else
    NOTREACHED() << "Unexpected Windows version";

  return name;
}

FARPROC GetFunctionFromLibrary(const char* function_name,
                               const char* library_name) {
  HMODULE library = ::GetModuleHandleA(library_name);
  if (!library)
    library = ::LoadLibraryA(library_name);
  if (!library)
    return nullptr;
  return ::GetProcAddress(library, function_name);
}

}  // namespace media
