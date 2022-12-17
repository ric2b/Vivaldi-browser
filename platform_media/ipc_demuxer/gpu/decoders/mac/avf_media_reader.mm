// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/ipc_demuxer/gpu/decoders/mac/avf_media_reader.h"

#import <AVFoundation/AVFoundation.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"

#include "platform_media/ipc_demuxer/mac/framework_type_conversions.h"
#include "platform_media/ipc_demuxer/gpu/pipeline/mac/media_utils_mac.h"
#include "platform_media/ipc_demuxer/platform_media_pipeline_types.h"
#import "platform_media/ipc_demuxer/gpu/decoders/mac/data_source_loader.h"

namespace media {

namespace {

class ScopedBufferLock {
 public:
  explicit ScopedBufferLock(CVPixelBufferRef buffer) : buffer_(buffer) {
    CVPixelBufferLockBaseAddress(buffer_, kCVPixelBufferLock_ReadOnly);
  }
  ~ScopedBufferLock() {
    CVPixelBufferUnlockBaseAddress(buffer_, kCVPixelBufferLock_ReadOnly);
  }

 private:
  const CVPixelBufferRef buffer_;
};

AVAssetTrack* AssetTrackForType(AVAsset* asset, NSString* track_type_name) {
  NSArray* tracks = [asset tracksWithMediaType:track_type_name];
  AVAssetTrack* track = [tracks count] > 0u ? [tracks objectAtIndex:0] : nil;

  if (track && track_type_name == AVMediaTypeVideo) {
    // NOTE(tomas@vivaldi.com): VB-45871
    // Return nil to avoid renderer crash
    if (track.formatDescriptions.count > 1) {
      return nil;
    }
    CMFormatDescriptionRef desc = reinterpret_cast<CMFormatDescriptionRef>(
        [[track formatDescriptions] objectAtIndex:0]);
    if ("jpeg" == FourCCToString(CMFormatDescriptionGetMediaSubType(desc))) {
      return nil;
    }
  }

  return track;
}

bool ReadAudioSample(AVAssetReaderOutput* audio_output,
                     IPCDecodingBuffer& ipc_buffer) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer(
      reinterpret_cast<CMSampleBufferRef>([audio_output copyNextSampleBuffer]));
  if (!sample_buffer) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " No buffer available";
    return false;
  }

  const CMTime timestamp = CoreMediaGlueCMTimeToCMTime(
      CMSampleBufferGetPresentationTimeStamp(sample_buffer));
  const CMTime duration =
      CoreMediaGlueCMTimeToCMTime(CMSampleBufferGetDuration(sample_buffer));
  if (!CMTIME_IS_VALID(timestamp) || !CMTIME_IS_VALID(duration)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Invalid timestamp and/or duration";
    return false;
  }

  CMBlockBufferRef block_buffer = CMSampleBufferGetDataBuffer(sample_buffer);
  if (!block_buffer)
    return false;

  size_t data_size = CMBlockBufferGetDataLength(block_buffer);
  if (!data_size)
    return false;
  uint8_t* data = ipc_buffer.PrepareMemory(data_size);
  if (!data)
    return false;
  OSStatus status =
      CMBlockBufferCopyDataBytes(block_buffer, 0, data_size, data);
  if (status != kCMBlockBufferNoErr) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " CMBlockBufferCopyDataBytes failed, status=" << status;
    return false;
  }

  ipc_buffer.set_timestamp(CMTimeToTimeDelta(timestamp));
  ipc_buffer.set_duration(CMTimeToTimeDelta(duration));
  VLOG(6) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " timestamp=" << ipc_buffer.timestamp()
          << " duration=" << ipc_buffer.duration();

  return true;
}

bool ReadVideoSample(AVAssetReaderOutput* video_output,
                     const gfx::Size& coded_size,
                     IPCDecodingBuffer& ipc_buffer) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer(
      reinterpret_cast<CMSampleBufferRef>([video_output copyNextSampleBuffer]));
  if (!sample_buffer) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " No buffer available";
    return false;
  }

  const CMTime timestamp = CoreMediaGlueCMTimeToCMTime(
      CMSampleBufferGetPresentationTimeStamp(sample_buffer));
  if (!CMTIME_IS_VALID(timestamp)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Invalid timestamp and/or duration";
    return false;
  }

  CVImageBufferRef pixel_buffer = CMSampleBufferGetImageBuffer(sample_buffer);
  if (!pixel_buffer) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " No pixel_buffer available";
    return false;
  }

  ScopedBufferLock auto_lock(pixel_buffer);

  size_t plane_sizes[3] = {0};
  const int video_frame_planes[] = {VideoFrame::kYPlane, VideoFrame::kUPlane,
                                    VideoFrame::kVPlane};

  // The planes in the pixel buffer are YUV, but PassThroughVideoDecoder
  // assumes YVU, so we switch the order of the last two planes.
  const int planes[] = {0, 2, 1};
  for (int i = 0; i < 3; ++i) {
    const int plane = planes[i];

    size_t stride = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, plane);
    if (stride == 0)
      return false;

    plane_sizes[plane] +=
        stride * VideoFrame::PlaneSize(VideoPixelFormat::PIXEL_FORMAT_YV12,
                                       video_frame_planes[plane], coded_size)
                     .height();
  }

  // Copy all planes into contiguous memory.
  size_t data_size = plane_sizes[0] + plane_sizes[1] + plane_sizes[2];
  uint8_t* data = ipc_buffer.PrepareMemory(data_size);
  if (!data)
    return false;
  size_t data_offset = 0;
  for (int i = 0; i < 3; ++i) {
    const int plane = planes[i];

    const void* pixels =
        CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, plane);
    if (!pixels)
      return false;
    memcpy(data + data_offset, pixels, plane_sizes[plane]);

    data_offset += plane_sizes[plane];
  }

  ipc_buffer.set_timestamp(CMTimeToTimeDelta(timestamp));
  VLOG(6) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " timestamp=" << ipc_buffer.timestamp();

  return true;
}

}  // namespace

AVFMediaReader::StreamReader::StreamReader() = default;

AVFMediaReader::StreamReader::~StreamReader() = default;

AVFMediaReader::AVFMediaReader(dispatch_queue_t queue) : queue_(queue) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
}

AVFMediaReader::~AVFMediaReader() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
}

bool AVFMediaReader::Initialize(base::scoped_nsobject<AVAsset> asset) {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  asset_ = std::move(asset);

  if (![asset_ isPlayable]) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Asset is not playable";
    return false;
  }

  if ([asset_ hasProtectedContent]) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Asset has protected content";
    return false;
  }

  if (!CalculateBitrate()) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Calculate bitrate failed";
    return false;
  }

  if (!has_video_track() && !has_audio_track()) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " No tracks to play";
    return false;
  }

  if (has_video_track()) {
    const CMVideoDimensions coded_size =
        CMVideoFormatDescriptionGetDimensions(video_stream_format());
    video_coded_size_ = gfx::Size(coded_size.width, coded_size.height);
  }

  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Resetting stream readers";
  if (!ResetStreamReaders(base::TimeDelta())) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Reset Stream Readers failed";
    return false;
  }

  return true;
}

int AVFMediaReader::bitrate() const {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  DCHECK_GE(bitrate_, 0);
  return bitrate_;
}

base::TimeDelta AVFMediaReader::duration() const {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  return CMTimeToTimeDelta([asset_ duration]);
}

base::TimeDelta AVFMediaReader::start_time() const {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));

  return GetStartTimeFromTrack(GetTrack(has_audio_track()
                                            ? PlatformStreamType::kAudio
                                            : PlatformStreamType::kVideo));
}

AudioStreamBasicDescription AVFMediaReader::audio_stream_format() const {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  DCHECK(has_audio_track());

  DCHECK_GE([[GetTrack(PlatformStreamType::kAudio) formatDescriptions] count],
            1u);
  const CMAudioFormatDescriptionRef audio_format =
      reinterpret_cast<CMAudioFormatDescriptionRef>([[GetTrack(
          PlatformStreamType::kAudio) formatDescriptions] objectAtIndex:0]);

  return CMAudioFormatDescriptionGetRichestDecodableFormat(audio_format)->mASBD;
}

CMFormatDescriptionRef AVFMediaReader::video_stream_format() const {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  DCHECK(has_video_track());

  DCHECK_GE([[GetTrack(PlatformStreamType::kVideo) formatDescriptions] count],
            1u);
  return reinterpret_cast<CMFormatDescriptionRef>([[GetTrack(
      PlatformStreamType::kVideo) formatDescriptions] objectAtIndex:0]);
}

CGAffineTransform AVFMediaReader::video_transform() const {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  DCHECK(has_video_track());

  return [GetTrack(PlatformStreamType::kVideo) preferredTransform];
}

bool AVFMediaReader::Seek(base::TimeDelta time) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Seeking to "
          << time.InMicroseconds() << " per pipeline request";
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));

  if (time == duration()) {
    // AVAssetReader enters an error state if the time range starts where it
    // ends.
    for (StreamReader& stream_reader : stream_readers_)
      stream_reader.end_of_stream = true;

    return true;
  }

  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Resetting stream readers";
  if (!ResetStreamReaders(time))
    return false;

  return true;
}

bool AVFMediaReader::CalculateBitrate() {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));

  float bitrate = 0;
  for (PlatformStreamType stream_type : AllStreamTypes()) {
    bitrate += [GetTrack(stream_type) estimatedDataRate];
  }
  if (!base::IsValueInRangeForNumericType<decltype(bitrate_)>(bitrate) ||
      bitrate < 0.0)
    return false;

  bitrate_ = bitrate;
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " bitrate = " << bitrate_;
  return true;
}

bool AVFMediaReader::ResetStreamReaders(base::TimeDelta start_time) {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " start_time "
          << start_time;

  for (PlatformStreamType stream_type : AllStreamTypes()) {
    if (GetTrack(stream_type) != nil &&
        !ResetStreamReader(stream_type, start_time))
      return false;
  }

  return true;
}

bool AVFMediaReader::ResetStreamReader(PlatformStreamType stream_type,
                                       base::TimeDelta start_time) {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " start_time "
          << start_time;

  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  DCHECK(GetTrack(stream_type) != nil);

  StreamReader& stream_reader = GetElem(stream_readers_, stream_type);
  [stream_reader.asset_reader cancelReading];

  stream_reader.expected_next_timestamp = start_time;
  stream_reader.end_of_stream = false;

  NSError* reader_error = nil;
  stream_reader.asset_reader.reset(
      [[AVAssetReader assetReaderWithAsset:asset_ error:&reader_error] retain]);
  if (reader_error != nil) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << base::SysNSStringToUTF8([reader_error localizedDescription]);
    return false;
  }

  const CMTimeRange time_range = CMTimeRangeMake(
      TimeDeltaToCoreMediaGlueCMTime(start_time), kCMTimePositiveInfinity);

  [stream_reader.asset_reader setTimeRange:time_range];

  if (!InitializeOutput(stream_type)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " InitializeOutput failed";
    return false;
  }

  if ([stream_reader.asset_reader startReading] != YES) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to start reading with AVAssetReader for "
               << GetStreamTypeName(stream_type) << " : "
               << base::SysNSStringToUTF8([[stream_reader.asset_reader error]
                      localizedDescription]);
    return false;
  }

  VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": Successful";
  return true;
}

bool AVFMediaReader::InitializeOutput(PlatformStreamType stream_type) {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));

  StreamReader& stream_reader = GetElem(stream_readers_, stream_type);

  stream_reader.output.reset([[AVAssetReaderTrackOutput
      assetReaderTrackOutputWithTrack:GetTrack(stream_type)
                       outputSettings:GetOutputSettings(stream_type)] retain]);
  [stream_reader.output setAlwaysCopiesSampleData:NO];

  if (![stream_reader.asset_reader canAddOutput:stream_reader.output]) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Cannot add output ("
            << GetStreamTypeName(stream_type) << ")";
    return false;
  }

  [stream_reader.asset_reader addOutput:stream_reader.output];
  return true;
}

NSDictionary* AVFMediaReader::GetOutputSettings(
    PlatformStreamType stream_type) {
  switch (stream_type) {
    case PlatformStreamType::kAudio: {
      // Here we align the channel layouts on the Chrome and AVF sides in a way
      // that is intentionally lossy in some cases, for simplicity: First get
      // the default Chrome layout for a given number of channels, then map
      // that layout to a Core Audio layout.  This is good enough to avoid
      // major mixups like playing the center channel through the right speaker
      // only.  Time will tell if we need anything better than this.
      const ChannelLayout chrome_default_channel_layout =
          GuessChannelLayout(audio_stream_format().mChannelsPerFrame);

      AudioChannelLayout channel_layout = {0};
      channel_layout.mChannelLayoutTag =
          ChromeChannelLayoutToCoreAudioTag(chrome_default_channel_layout);

      return @{
        AVFormatIDKey : @(kAudioFormatLinearPCM),
        AVChannelLayoutKey : [NSData dataWithBytes:&channel_layout
                                            length:sizeof(channel_layout)]
      };
    }

    case PlatformStreamType::kVideo:
      return @{
        base::mac::CFToNSCast(kCVPixelBufferPixelFormatTypeKey) :
            @(kCVPixelFormatType_420YpCbCr8Planar)
      };
  }
}

AVAssetTrack* AVFMediaReader::GetTrack(PlatformStreamType stream_type) const {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));

  switch (stream_type) {
    case PlatformStreamType::kAudio:
      return AssetTrackForType(asset_, AVMediaTypeAudio);

    case PlatformStreamType::kVideo:
      return AssetTrackForType(asset_, AVMediaTypeVideo);
  }
}

media::Strides AVFMediaReader::GetStrides() {
  StreamReader& stream_reader =
      GetElem(stream_readers_, PlatformStreamType::kVideo);
  base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer(
      reinterpret_cast<CMSampleBufferRef>(
          [stream_reader.output copyNextSampleBuffer]));
  media::Strides strides;
  if (!sample_buffer) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " No buffer available";
    strides.stride_Y = 0;
    strides.stride_UV = 0;
    return strides;
  }

  CVImageBufferRef pixel_buffer = CMSampleBufferGetImageBuffer(sample_buffer);
  if (!pixel_buffer) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " No pixel_buffer available";
    strides.stride_Y = 0;
    strides.stride_UV = 0;
    return strides;
  }

  ScopedBufferLock auto_lock(pixel_buffer);
  strides.stride_Y = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);
  strides.stride_UV = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);
  return strides;
}

void AVFMediaReader::GetNextMediaSample(IPCDecodingBuffer& ipc_buffer) {
  DCHECK(dispatch_queue_get_label(queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));

  PlatformStreamType stream_type = ipc_buffer.stream_type();
  StreamReader& stream_reader = GetElem(stream_readers_, stream_type);
  for (;;) {
    if (stream_reader.end_of_stream) {
      ipc_buffer.set_status(MediaDataStatus::kEOS);
      return;
    }

    bool ok;
    switch (stream_type) {
      case PlatformStreamType::kAudio:
        ok = ReadAudioSample(stream_reader.output, ipc_buffer);
        break;
      case PlatformStreamType::kVideo:
        ok = ReadVideoSample(stream_reader.output, video_coded_size_,
                             ipc_buffer);
        break;
    }
    if (ok)
      break;

    // Failure can mean a number of things: time-out caused
    // by shortage of raw media data, end of stream, or decoding error.

    AVAssetReader* asset_reader = stream_reader.asset_reader;
    if ([asset_reader status] == AVAssetReaderStatusFailed) {
      NSError* error = [asset_reader error];
      NSErrorDomain domain = [error domain];
      NSInteger code = [error code];
      const bool timed_out = [domain isEqualToString:NSURLErrorDomain] &&
                             code == NSURLErrorTimedOut;
      if (timed_out) {
        VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
                << " I think we've timed out, retrying";
        if (ResetStreamReader(stream_type,
                              stream_reader.expected_next_timestamp))
          continue;
      }

      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Decode error("
              << GetStreamTypeName(stream_type)
              << "): domain=" << base::SysNSStringToUTF8(domain)
              << " code=" << code << " description="
              << base::SysNSStringToUTF8([error localizedDescription]);
      ipc_buffer.set_status(MediaDataStatus::kMediaError);
      return;
    }

    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " EOS type=" << GetStreamTypeName(stream_type)
            << " reader_status=" << [asset_reader status];
    stream_reader.end_of_stream = true;
  }

  stream_reader.expected_next_timestamp =
      ipc_buffer.timestamp() + ipc_buffer.duration();

  // If a duration is set on the audio buffer, it confuses the Chromium
  // pipeline.
  // TODO(igor@vivaldi.com): Figure out if this is true. WMF pipeline on
  // Windows set the duration and it works.
  if (stream_type == PlatformStreamType::kAudio) {
    ipc_buffer.set_duration(base::TimeDelta());
  } else {
    DCHECK(ipc_buffer.duration().is_zero());
  }

  ipc_buffer.set_status(MediaDataStatus::kOk);
}

}  // namespace media
