// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "media/filters/protocol_sniffer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/base/container_names.h"
#include "media/base/data_source.h"

namespace media {

namespace {

std::string DetermineContainer(const uint8_t* data, size_t data_size) {
  const container_names::MediaContainerName container =
      container_names::OperaDetermineContainer(data, data_size);

  switch (container) {
    case container_names::CONTAINER_AAC:
      return "audio/aac";

    case container_names::CONTAINER_MP3:
      return "audio/mp3";

    case container_names::CONTAINER_WAV:
      return "audio/wav";

    case container_names::CONTAINER_H264:
      return "video/mp4";

    default:
      break;
  }

  // Also check for Shoutcast, a popular live streaming protocol.
  if (data_size >= 3 && data[0] == 'I' && data[1] == 'C' && data[2] == 'Y')
    return "audio/mp3";

  // It might happen that we do not have enough data to correctly sniff mp3 with
  // OperaDetermineContainer, so we assume mp3 just by checking the header tag.
  if (data_size >= 3 && data[0] == 'I' && data[1] == 'D' && data[2] == '3')
    return "audio/mp3";

  return std::string();
}

}  // namespace

ProtocolSniffer::ProtocolSniffer() : weak_ptr_factory_(this) {
}

ProtocolSniffer::~ProtocolSniffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
bool ProtocolSniffer::ShouldSniffProtocol(const std::string& content_type) {
  return content_type == "application/octet-stream" ||
         content_type == "binary/octet-stream";
}

void ProtocolSniffer::SniffProtocol(DataSource* data_source,
                                    const Callback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_source);

  // We read the first 8192 bytes, same as FFmpeg.
  static const size_t kDataSize = 8192;
  scoped_ptr<uint8_t[]> data_holder(new uint8_t[kDataSize]);
  uint8_t* data = data_holder.get();

  data_source->Read(
      0, kDataSize, data,
      base::Bind(&ProtocolSniffer::ReadDone, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&data_holder), callback));
}

void ProtocolSniffer::ReadDone(scoped_ptr<uint8_t[]> data,
                               const Callback& callback,
                               int size_read) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string mime_type;

  if (size_read != DataSource::kReadError)
    mime_type = DetermineContainer(data.get(), size_read);

  callback.Run(mime_type);
}

}  // namespace media
