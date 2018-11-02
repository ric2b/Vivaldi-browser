// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_RENDERER_PIPELINE_PROTOCOL_SNIFFER_H_
#define PLATFORM_MEDIA_RENDERER_PIPELINE_PROTOCOL_SNIFFER_H_

#include "platform_media/common/feature_toggles.h"

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"

namespace media {

class DataSource;

// When media data is not transferred through HTTP we can't determine support
// by looking at the Content Type header, so we need to read the first few
// bytes and try to guess the actual media type.
class MEDIA_EXPORT ProtocolSniffer {
 public:
  // Called when sniffing is complete.  |mime_type| contains the media type
  // detected, or is empty on failure to detect.
  using Callback = base::Callback<void(const std::string& mime_type)>;

  ProtocolSniffer();
  ~ProtocolSniffer();

  static bool ShouldSniffProtocol(const std::string& content_type);

  void SniffProtocol(DataSource* data_source, const Callback& callback);

 private:
  void ReadDone(std::unique_ptr<uint8_t[]> data,
                const Callback& callback,
                int size_read);

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<ProtocolSniffer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolSniffer);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_PIPELINE_PROTOCOL_SNIFFER_H_
