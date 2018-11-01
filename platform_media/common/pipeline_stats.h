// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef MEDIA_BASE_PIPELINE_STATS_H_
#define MEDIA_BASE_PIPELINE_STATS_H_

#include <string>

#include "media/base/media_export.h"

namespace base {
class DictionaryValue;
}

namespace media {

class DemuxerStream;
enum class PlatformMediaDecodingMode;

// Use these functions to report about events concerning the whole media
// pipeline so that we can gather some statistics.
namespace pipeline_stats {

// A Demuxer was chosen to handle the content type, but could not be used due
// to missing system libraries.
MEDIA_EXPORT void ReportNoPlatformSupport();

// A Demuxer was chosen to handle the content type, but could not be used
// because there was no GPU process.
MEDIA_EXPORT void ReportNoGpuProcess();

// HW-accelerated decoders won't be available.
void ReportNoGpuProcessForDecoder();

// Demuxer initialization has completed.
void ReportStartResult(bool success,
                       PlatformMediaDecodingMode attempted_video_decoding_mode);

// AudioDecoder initialization has completed.
void ReportAudioDecoderInitResult(bool success);

// VideoDecoder initialization has completed.
void ReportVideoDecoderInitResult(bool success);

// Register a DemuxerStream to be used in ReportStreamError() later on.
void AddStream(const DemuxerStream* stream,
               PlatformMediaDecodingMode decoding_mode);

// Remove stream registration.
void RemoveStream(const DemuxerStream* stream);

// Register a decoder class name.  Only streams associated with decoders whose
// class names have been registered will actually be registered by
// AddStreamForDecoderClass().
void AddDecoderClass(const std::string& decoder_class_name);

// Register a DemuxerStream to be used in ReportStreamError() later on, but
// only if the class name of the associated decoder has been registered (see
// AddDecoderClass()).  This allows us to register streams from generic code
// and yet only collect data on streams associated with certain decoders.
void AddStreamForDecoderClass(const DemuxerStream* stream,
                              const std::string& decoder_class_name);

// Remove stream registration.
void RemoveStreamForDecoderClass(const DemuxerStream* stream,
                                 const std::string& decoder_class_name);

// There was an error related to |stream| after the pipeline had been
// initialized.
void ReportStreamError(const DemuxerStream* stream);

// Called in a child process to serialize the stats collected since startup or
// the last call to this function.
MEDIA_EXPORT void SerializeInto(base::DictionaryValue* dictionary);

// Called in the browser process to deserialize stats previously serialized
// using SerializeInto(), and report them to the metrics system.
MEDIA_EXPORT void DeserializeAndReport(const base::DictionaryValue& dictionary);

}  // namespace pipeline_stats
}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_STATS_H_
