// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/common/win/mf_util.h"

#include "platform_media/common/platform_mime_util.h"

#include <map>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/win/windows_version.h"

namespace media {

namespace {

// Used in UMA histograms.  Don't remove or reorder values!
enum MFStatus {
  MF_NOT_SUPPORTED = 0,
  MF_PLAT_AVAILABLE = 1,
  MF_PLAT_NOT_AVAILABLE = 2,
  MF_VIDEO_DECODER_AVAILABLE = 3,
  MF_VIDEO_DECODER_NOT_AVAILABLE = 4,
  MF_AAC_DECODER_AVAILABLE = 5,
  MF_AAC_DECODER_NOT_AVAILABLE = 6,
  MF_STATUS_COUNT
};

void ReportMFStatus(MFStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Opera.DSK.Media.MFStatus", status,
                            MF_STATUS_COUNT);
}

bool CheckOSVersion() {
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    LOG(WARNING) << " PROPMEDIA(COMMON) : " << __FUNCTION__
                 << " We don't support proprietary media codecs in this Windows version";
    return false;
  }

  return true;
}

bool LoadMFLibrary(const char* library_name) {
  if (!CheckOSVersion())
    return false;

  if (::GetModuleHandleA(library_name) == NULL &&
      ::LoadLibraryA(library_name) == NULL) {
    LOG(WARNING) << " PROPMEDIA(COMMON) : " << __FUNCTION__
                 << " Failed to load " << library_name
                 << ". Some media features will not be available.";
    return false;
  }

  return true;
}

// Provide two separate loaders, one for the common mfplat.dll library plus
// decoder libraries, and another one for mfreadwrite.dll.  The latter provides
// IMFSourceReader, which is only necessary when decoding _and_ demuxing using
// system libraries.
class PrimaryLoader {
 public:
  static PrimaryLoader * getInstance() {
    static PrimaryLoader * instance = new PrimaryLoader();
    return instance;
  }

  PrimaryLoader();

  bool is_media_foundation_available() const {
    return media_foundation_available_;
  }
  bool is_audio_decoder_available(AudioCodec codec) const {
    DCHECK_EQ(1u, audio_decoder_available_.count(codec));
    return audio_decoder_available_.find(codec)->second;
  }
  bool is_video_decoder_available() const { return video_decoder_available_; }

 private:
  void ReportLoadResults();

  bool media_foundation_available_;
  std::map<AudioCodec, bool> audio_decoder_available_;
  bool video_decoder_available_;
};

PrimaryLoader::PrimaryLoader()
    : media_foundation_available_(LoadMFLibrary("mfplat.dll")),
      video_decoder_available_(
          LoadMFLibrary(GetMFVideoDecoderLibraryName().c_str()) &&
          LoadMFLibrary("evr.dll")) {
  audio_decoder_available_[AudioCodec::kCodecAAC] =
      LoadMFLibrary(GetMFAudioDecoderLibraryName(AudioCodec::kCodecAAC).c_str());

  ReportLoadResults();
}

void PrimaryLoader::ReportLoadResults() {
  if (!CheckOSVersion()) {
    ReportMFStatus(MF_NOT_SUPPORTED);
    return;
  }

  ReportMFStatus(media_foundation_available_ ? MF_PLAT_AVAILABLE
                                             : MF_PLAT_NOT_AVAILABLE);
  ReportMFStatus(video_decoder_available_ ? MF_VIDEO_DECODER_AVAILABLE
                                          : MF_VIDEO_DECODER_NOT_AVAILABLE);
  ReportMFStatus(audio_decoder_available_[AudioCodec::kCodecAAC]
                     ? MF_AAC_DECODER_AVAILABLE
                     : MF_AAC_DECODER_NOT_AVAILABLE);
}

class SecondaryLoader {
 public:

  static SecondaryLoader * getInstance() {
    static SecondaryLoader * instance = new SecondaryLoader();
    return instance;
  }

  SecondaryLoader()
      : source_reader_available_(LoadMFLibrary("mfreadwrite.dll") &&
                                 LoadMFLibrary("evr.dll")) {}

  bool is_source_reader_available() const { return source_reader_available_; }

 private:
  bool source_reader_available_;
};

}  // namespace

bool LoadMFCommonLibraries() {
  return PrimaryLoader::getInstance()->is_media_foundation_available();
}

bool LoadMFSourceReaderLibraries() {
  return SecondaryLoader::getInstance()->is_source_reader_available();
}

void LoadMFAudioDecoderLibraries() {
  PrimaryLoader::getInstance();
}

bool LoadMFAudioDecoderLibrary(AudioCodec codec) {
  return PrimaryLoader::getInstance()->is_audio_decoder_available(codec);
}

bool LoadMFVideoDecoderLibraries() {
  return PrimaryLoader::getInstance()->is_video_decoder_available();
}

std::string GetMFAudioDecoderLibraryName(AudioCodec codec) {
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
