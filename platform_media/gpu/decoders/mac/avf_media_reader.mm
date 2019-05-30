// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/decoders/mac/avf_media_reader.h"

#import <AVFoundation/AVFoundation.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#import "platform_media/gpu/decoders/mac/data_source_loader.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/mac/framework_type_conversions.h"
#include "platform_media/common/mac/platform_media_pipeline_types_mac.h"

@interface AVURLAsset (MavericksSDK)
@property(nonatomic, readonly) AVAssetResourceLoader* resourceLoader;
@end

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
  return [tracks count] > 0u ? [tracks objectAtIndex:0] : nil;
}

scoped_refptr<DataBuffer> ReadAudioSample(
    AVAssetReaderOutput* audio_output) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer(
      reinterpret_cast<CMSampleBufferRef>(
          [audio_output copyNextSampleBuffer]));
  if (!sample_buffer)
    return nullptr;

  const CMTime timestamp = CoreMediaGlueCMTimeToCMTime(
      CMSampleBufferGetPresentationTimeStamp(sample_buffer));
  const CMTime duration = CoreMediaGlueCMTimeToCMTime(
      CMSampleBufferGetDuration(sample_buffer));
  if (!CMTIME_IS_VALID(timestamp) || !CMTIME_IS_VALID(duration)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Invalid timestamp and/or duration";
    return nullptr;
  }

  CMBlockBufferRef block_buffer =
      CMSampleBufferGetDataBuffer(sample_buffer);
  if (!block_buffer)
    return nullptr;

  const int data_size = base::saturated_cast<int>(
      CMBlockBufferGetDataLength(block_buffer));
  scoped_refptr<DataBuffer> audio_data_buffer =
      new DataBuffer(data_size);
  if (CMBlockBufferCopyDataBytes(
          block_buffer, 0, data_size, audio_data_buffer->writable_data()) !=
      kCMBlockBufferNoErr) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Failed to copy audio data";
    return nullptr;
  }

  audio_data_buffer->set_data_size(data_size);
  audio_data_buffer->set_timestamp(CMTimeToTimeDelta(timestamp));
  audio_data_buffer->set_duration(CMTimeToTimeDelta(duration));

  return audio_data_buffer;
}

scoped_refptr<DataBuffer> ReadVideoSample(
    AVAssetReaderOutput* video_output,
    const gfx::Size& coded_size) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer(
      reinterpret_cast<CMSampleBufferRef>(
          [video_output copyNextSampleBuffer]));
  if (!sample_buffer) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " No buffer available";
    return nullptr;
  }

  const CMTime timestamp = CoreMediaGlueCMTimeToCMTime(
      CMSampleBufferGetPresentationTimeStamp(sample_buffer));
  if (!CMTIME_IS_VALID(timestamp)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Invalid timestamp and/or duration";
    return nullptr;
  }

  CVImageBufferRef pixel_buffer =
      CMSampleBufferGetImageBuffer(sample_buffer);
  if (!pixel_buffer) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " No pixel_buffer available";
    return nullptr;
  }

  ScopedBufferLock auto_lock(pixel_buffer);

  size_t plane_sizes[3] = {0};
  const int video_frame_planes[] = {
      VideoFrame::kYPlane,
      VideoFrame::kUPlane,
      VideoFrame::kVPlane};

  // The planes in the pixel buffer are YUV, but PassThroughVideoDecoder
  // assumes YVU, so we switch the order of the last two planes.
  const int planes[] = {0, 2, 1};
  for (int i = 0; i < 3; ++i) {
    const int plane = planes[i];

    const int stride = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, plane);

    plane_sizes[plane] =
        stride *
        VideoFrame::PlaneSize(VideoPixelFormat::PIXEL_FORMAT_YV12,
                                     video_frame_planes[plane],
                                     coded_size).height();
  }

  // Copy all planes into contiguous memory.
  const int data_size = plane_sizes[0] + plane_sizes[1] + plane_sizes[2];
  scoped_refptr<DataBuffer> video_data_buffer =
      new DataBuffer(data_size);
  size_t data_offset = 0;
  for (int i = 0; i < 3; ++i) {
    const int plane = planes[i];

    memcpy(video_data_buffer->writable_data() + data_offset,
           CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, plane),
           plane_sizes[plane]);

    data_offset += plane_sizes[plane];
  }

  video_data_buffer->set_data_size(data_size);
  video_data_buffer->set_timestamp(CMTimeToTimeDelta(timestamp));

  return video_data_buffer;
}

}  // namespace

AVFMediaReader::StreamReader::StreamReader()
    : type(PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT),
      end_of_stream(false) {
}

AVFMediaReader::StreamReader::~StreamReader() = default;

AVFMediaReader::AVFMediaReader(dispatch_queue_t queue)
    : bitrate_(-1), queue_(queue) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  for (int i = 0; i < PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT; ++i)
    stream_readers_[i].type = static_cast<PlatformMediaDataType>(i);
}

AVFMediaReader::~AVFMediaReader() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(queue_ == dispatch_get_current_queue());

  [data_source_loader_ stop];
}

bool AVFMediaReader::Initialize(media::DataSource* data_source,
                                const std::string& mime_type) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " mimetype " << mime_type;
  DCHECK(queue_ == dispatch_get_current_queue());

  // The URL is completely arbitrary.  We use a "custom" URL scheme to force
  // AVURLAsset to ask our instance of AVAssetResourceLoaderDelegate for data.
  // This way, we make sure all data is fetched through Chrome's network stack
  // (via |data_source| rather than by AVURLAsset directly.

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Create Asset";
  NSString* url = [NSString stringWithFormat:@"opop:file.mp4"];
  asset_.reset([[AVURLAsset
      assetWithURL:[NSURL URLWithString:url]] retain]);

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Create DataSourceLoader";
  data_source_loader_.reset([[DataSourceLoader alloc]
      initWithDataSource:data_source
            withMIMEType:base::SysUTF8ToNSString(mime_type)]);

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Attach DataSourceLoader to Asset";
  [[asset_ resourceLoader] setDelegate:data_source_loader_
                                 queue:[data_source_loader_ dispatchQueue]];

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

  if (!CalculateBitrate()){
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Calculate bitrate failed";
    return false;
  }

  if (!has_video_track() && !has_audio_track()) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " No tracks to play";
    return false;
  }

  if (has_video_track()) {
    const CMVideoDimensions coded_size =
        CMVideoFormatDescriptionGetDimensions(
            video_stream_format());
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
  DCHECK(queue_ == dispatch_get_current_queue());
  DCHECK_GE(bitrate_, 0);
  return bitrate_;
}

base::TimeDelta AVFMediaReader::duration() const {
  DCHECK(queue_ == dispatch_get_current_queue());
  return CMTimeToTimeDelta([asset_ duration]);
}

base::TimeDelta AVFMediaReader::start_time() const {
  DCHECK(queue_ == dispatch_get_current_queue());

  return GetStartTimeFromTrack(
      GetTrack(has_audio_track() ? PlatformMediaDataType::PLATFORM_MEDIA_AUDIO
                                 : PlatformMediaDataType::PLATFORM_MEDIA_VIDEO));
}

AudioStreamBasicDescription AVFMediaReader::audio_stream_format() const {
  DCHECK(queue_ == dispatch_get_current_queue());
  DCHECK(has_audio_track());

  DCHECK_GE([[GetTrack(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO) formatDescriptions] count],
            1u);
  const CMAudioFormatDescriptionRef audio_format =
      reinterpret_cast<CMAudioFormatDescriptionRef>(
          [[GetTrack(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO) formatDescriptions]
              objectAtIndex:0]);

  return CMAudioFormatDescriptionGetRichestDecodableFormat(
             audio_format)
      ->mASBD;
}

CMFormatDescriptionRef AVFMediaReader::video_stream_format() const {
  DCHECK(queue_ == dispatch_get_current_queue());
  DCHECK(has_video_track());

  DCHECK_GE([[GetTrack(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO) formatDescriptions] count],
            1u);
  return reinterpret_cast<CMFormatDescriptionRef>(
      [[GetTrack(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO) formatDescriptions]
          objectAtIndex:0]);
}

CGAffineTransform AVFMediaReader::video_transform() const {
  DCHECK(queue_ == dispatch_get_current_queue());
  DCHECK(has_video_track());

  return [GetTrack(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO) preferredTransform];
}

scoped_refptr<DataBuffer> AVFMediaReader::GetNextAudioSample() {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(queue_ == dispatch_get_current_queue());

  return ProcessNextSample(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO);
}

scoped_refptr<DataBuffer> AVFMediaReader::GetNextVideoSample() {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(queue_ == dispatch_get_current_queue());

  return ProcessNextSample(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO);
}

bool AVFMediaReader::Seek(base::TimeDelta time) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Seeking to " << time.InMicroseconds() << " per pipeline request";
  DCHECK(queue_ == dispatch_get_current_queue());

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
  DCHECK(queue_ == dispatch_get_current_queue());

  float bitrate = 0;
  for (const StreamReader& stream_reader : stream_readers_)
    bitrate += [GetTrack(stream_reader.type) estimatedDataRate];
  if (!base::IsValueInRangeForNumericType<decltype(bitrate_)>(bitrate))
    return false;

  bitrate_ = bitrate;
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " bitrate = " << bitrate_;
  return true;
}

bool AVFMediaReader::ResetStreamReaders(base::TimeDelta start_time) {
  DCHECK(queue_ == dispatch_get_current_queue());
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " start_time " << start_time;

  for (StreamReader& stream_reader : stream_readers_) {
    if (GetTrack(stream_reader.type) != nil &&
        !ResetStreamReader(stream_reader.type, start_time))
      return false;
  }

  return true;
}

bool AVFMediaReader::ResetStreamReader(PlatformMediaDataType type,
                                       base::TimeDelta start_time) {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " start_time " << start_time;

  DCHECK(queue_ == dispatch_get_current_queue());
  DCHECK(GetTrack(type) != nil);

  [stream_readers_[type].asset_reader cancelReading];

  stream_readers_[type].expected_next_timestamp = start_time;
  stream_readers_[type].end_of_stream = false;

  NSError* reader_error = nil;
  stream_readers_[type].asset_reader.reset(
      [[AVAssetReader
          assetReaderWithAsset:asset_
                         error:&reader_error] retain]);
  if (reader_error != nil) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << base::SysNSStringToUTF8([reader_error localizedDescription]);
    return false;
  }

  const CMTimeRange time_range = CMTimeRangeMake(
      TimeDeltaToCoreMediaGlueCMTime(start_time),
      kCMTimePositiveInfinity);

  [stream_readers_[type].asset_reader
      setTimeRange:time_range];

  if (!InitializeOutput(type)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " InitializeOutput failed";
    return false;
  }

  if ([stream_readers_[type].asset_reader startReading] != YES) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to start reading with AVAssetReader for type "
               << LoggableMediaType(type) << " : "
               << base::SysNSStringToUTF8([[stream_readers_[type].asset_reader
                                              error] localizedDescription]);
    return false;
  }

  VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": Successful";
  return true;
}

bool AVFMediaReader::InitializeOutput(PlatformMediaDataType type) {
  DCHECK(queue_ == dispatch_get_current_queue());

  StreamReader& stream_reader = stream_readers_[type];

  stream_reader.output.reset([[AVAssetReaderTrackOutput
      assetReaderTrackOutputWithTrack:GetTrack(type)
                       outputSettings:GetOutputSettings(type)] retain]);
  [stream_reader.output setAlwaysCopiesSampleData:NO];

  if (![stream_reader.asset_reader canAddOutput:stream_reader.output]) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Cannot add output (" << type << ")";
    return false;
  }

  [stream_reader.asset_reader addOutput:stream_reader.output];
  return true;
}

NSDictionary* AVFMediaReader::GetOutputSettings(
    PlatformMediaDataType type) {
  switch (type) {
    case PlatformMediaDataType::PLATFORM_MEDIA_AUDIO: {
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
          ChromeChannelLayoutToCoreAudioTag(
              chrome_default_channel_layout);

      return @{
        AVFormatIDKey : @(kAudioFormatLinearPCM),
        AVChannelLayoutKey :
            [NSData dataWithBytes:&channel_layout length:sizeof(channel_layout)]
      };
    }

    case PlatformMediaDataType::PLATFORM_MEDIA_VIDEO:
      return @{
        base::mac::CFToNSCast(kCVPixelBufferPixelFormatTypeKey) :
            @(kCVPixelFormatType_420YpCbCr8Planar)
            };

    default:
      NOTREACHED();
      return nil;
  }
}

AVAssetTrack* AVFMediaReader::GetTrack(
    PlatformMediaDataType type) const {
  DCHECK(queue_ == dispatch_get_current_queue());

  switch (type) {
    case PlatformMediaDataType::PLATFORM_MEDIA_AUDIO:
      return AssetTrackForType(asset_, AVMediaTypeAudio);

    case PlatformMediaDataType::PLATFORM_MEDIA_VIDEO:
      return AssetTrackForType(asset_, AVMediaTypeVideo);

    default:
      NOTREACHED();
      return nil;
  }
}

scoped_refptr<DataBuffer> AVFMediaReader::ProcessNextSample(
    PlatformMediaDataType type) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(queue_ == dispatch_get_current_queue());

  const scoped_refptr<DataBuffer> buffer = ReadNextSample(type);

  if (buffer && !buffer->end_of_stream()) {
    stream_readers_[type].expected_next_timestamp =
        buffer->timestamp() + buffer->duration();

    // If a duration is set on the audio buffer, it confuses the Chromium
    // pipeline.
    buffer->set_duration(base::TimeDelta());
  }

  return buffer;
}

scoped_refptr<DataBuffer> AVFMediaReader::ReadNextSample(
    PlatformMediaDataType type) {
  DCHECK(queue_ == dispatch_get_current_queue());

  if (stream_readers_[type].end_of_stream)
    return DataBuffer::CreateEOSBuffer();

  for (;;) {
    const scoped_refptr<DataBuffer> buffer =
        type == PlatformMediaDataType::PLATFORM_MEDIA_AUDIO
            ? ReadAudioSample(stream_readers_[type].output)
            : ReadVideoSample(stream_readers_[type].output, video_coded_size_);
    if (buffer != nullptr)
      return buffer;

    AVAssetReader* asset_reader = stream_readers_[type].asset_reader;

    // Failure to get a DataBuffer can mean a number of things: time-out caused
    // by shortage of raw media data, end of stream, or decoding error.

    if ([asset_reader status] == AVAssetReaderStatusFailed) {
      const bool timed_out =
          [[[asset_reader error] domain] isEqualToString:NSURLErrorDomain] &&
          [[asset_reader error] code] == NSURLErrorTimedOut;
      if (timed_out) {
        VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
                << " I think we've timed out, retrying";
        if (ResetStreamReader(type,
                              stream_readers_[type].expected_next_timestamp))
          continue;
      }

      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " Decode error(" << type
              << "): " << base::SysNSStringToUTF8(
                               [[asset_reader error] localizedDescription]);
      return nullptr;
    }

    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " EOS(" << type << ")";
    stream_readers_[type].end_of_stream = true;
    return DataBuffer::CreateEOSBuffer();
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace media
