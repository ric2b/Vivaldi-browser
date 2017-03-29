// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_COREMEDIA_GLUE_H_
#define MEDIA_BASE_MAC_COREMEDIA_GLUE_H_

#include <CoreVideo/CoreVideo.h>

#include "base/basictypes.h"
#include "media/base/media_export.h"

// Must use the same packing as the original structures defined in the OS X SDK
// to make sure data types are aligned the same way on different architectures.
#pragma pack(push, 4)

struct AudioStreamBasicDescription;

// CoreMedia API is only introduced in Mac OS X > 10.6, the (potential) linking
// with it has to happen in runtime. If it succeeds, subsequent clients can use
// CoreMedia via the class declared in this file, where the original naming has
// been kept as much as possible.
class MEDIA_EXPORT CoreMediaGlue {
 public:
  // Originally from CMTime.h
  typedef int64_t CMTimeValue;
  typedef int32_t CMTimeScale;
  typedef int64_t CMTimeEpoch;
  typedef uint32_t CMTimeFlags;
  typedef struct {
    CMTimeValue value;
    CMTimeScale timescale;
    CMTimeFlags flags;
    CMTimeEpoch epoch;
  } CMTime;

  static const CMTime kCMTimeZero;
  static const CMTime kCMTimePositiveInfinity;

  // Originally from CMTimeRange.h
  typedef struct {
    CMTime start;
    CMTime duration;
  } CMTimeRange;

  // Originally from CMBlockBuffer.h
  typedef uint32_t CMBlockBufferFlags;
  typedef struct OpaqueCMBlockBuffer* CMBlockBufferRef;
  typedef struct {
    uint32_t version;
    void* (*AllocateBlock)(void*, size_t);
    void (*FreeBlock)(void*, void*, size_t);
    void* refCon;
  } CMBlockBufferCustomBlockSource;

  // Originally from CMFormatDescription.h.
  typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
  typedef CMFormatDescriptionRef CMAudioFormatDescriptionRef;
  typedef CMFormatDescriptionRef CMVideoFormatDescriptionRef;
  typedef FourCharCode CMVideoCodecType;
  typedef struct {
    int32_t width;
    int32_t height;
  } CMVideoDimensions;
  enum {
    kCMPixelFormat_422YpCbCr8_yuvs = 'yuvs',
  };
  enum {
    kCMVideoCodecType_JPEG_OpenDML = 'dmb1',
    kCMVideoCodecType_H264 = 'avc1',
  };

  // Originally from CMFormatDescriptionBridge.h
  enum {
    kCMFormatDescriptionBridgeError_InvalidParameter = -12712,
  };

  static AudioStreamBasicDescription*
  CMAudioFormatDescriptionGetStreamBasicDescription(
      CMAudioFormatDescriptionRef desc);
  static CGRect CMVideoFormatDescriptionGetCleanAperture(
      CMVideoFormatDescriptionRef videoDesc,
      Boolean originIsAtTopLeft);
  static CGSize CMVideoFormatDescriptionGetPresentationDimensions(
      CMVideoFormatDescriptionRef videoDesc,
      Boolean usePixelAspectRatio,
      Boolean useCleanAperture);

  // Originally from CMSampleBuffer.h.
  typedef struct OpaqueCMSampleBuffer* CMSampleBufferRef;

  // Originally from CMTime.h.
  static CMTime CMTimeMake(int64_t value, int32_t timescale);
  static Float64 CMTimeGetSeconds(CMTime time);

  // Originally from CMTimeRange.h
  static CMTimeRange CMTimeRangeMake(CMTime start, CMTime duration);

  // Originally from CMBlockBuffer.h
  static OSStatus CMBlockBufferCopyDataBytes(CMBlockBufferRef theSourceBuffer,
                                             size_t offsetToData,
                                             size_t dataLength,
                                             void* destination);
  static OSStatus CMBlockBufferCreateContiguous(
      CFAllocatorRef structureAllocator,
      CMBlockBufferRef sourceBuffer,
      CFAllocatorRef blockAllocator,
      const CMBlockBufferCustomBlockSource* customBlockSource,
      size_t offsetToData,
      size_t dataLength,
      CMBlockBufferFlags flags,
      CMBlockBufferRef* newBBufOut);
  static size_t CMBlockBufferGetDataLength(CMBlockBufferRef theBuffer);
  static OSStatus CMBlockBufferGetDataPointer(CMBlockBufferRef theBuffer,
                                              size_t offset,
                                              size_t* lengthAtOffset,
                                              size_t* totalLength,
                                              char** dataPointer);
  static Boolean CMBlockBufferIsRangeContiguous(CMBlockBufferRef theBuffer,
                                                size_t offset,
                                                size_t length);

  // Originally from CMSampleBuffer.h.
  static CMBlockBufferRef CMSampleBufferGetDataBuffer(CMSampleBufferRef sbuf);
  static CMTime CMSampleBufferGetDuration(CMSampleBufferRef sbuf);
  static CMFormatDescriptionRef CMSampleBufferGetFormatDescription(
      CMSampleBufferRef sbuf);
  static CVImageBufferRef CMSampleBufferGetImageBuffer(
      CMSampleBufferRef buffer);
  static CMTime CMSampleBufferGetPresentationTimeStamp(CMSampleBufferRef sbuf);
  static CFArrayRef CMSampleBufferGetSampleAttachmentsArray(
      CMSampleBufferRef sbuf,
      Boolean createIfNecessary);
  static CFStringRef kCMSampleAttachmentKey_NotSync();

  // Originally from CMFormatDescription.h.
  static FourCharCode CMFormatDescriptionGetMediaSubType(
      CMFormatDescriptionRef desc);
  static CMVideoDimensions CMVideoFormatDescriptionGetDimensions(
      CMVideoFormatDescriptionRef videoDesc);
  static OSStatus CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
      CMFormatDescriptionRef videoDesc,
      size_t parameterSetIndex,
      const uint8_t** parameterSetPointerOut,
      size_t* parameterSetSizeOut,
      size_t* parameterSetCountOut,
      int* NALUnitHeaderLengthOut)
      /*__OSX_AVAILABLE_STARTING(__MAC_10_9,__IPHONE_7_0)*/;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CoreMediaGlue);
};

#pragma pack(pop)

#endif  // MEDIA_BASE_MAC_COREMEDIA_GLUE_H_
