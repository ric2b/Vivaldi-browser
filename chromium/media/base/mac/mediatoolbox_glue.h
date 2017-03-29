// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef MEDIA_BASE_MAC_MEDIATOOLBOX_GLUE_H_
#define MEDIA_BASE_MAC_MEDIATOOLBOX_GLUE_H_

#import <CoreFoundation/CoreFoundation.h>

#include <stdint.h>

#include "base/macros.h"
#include "media/base/mac/coremedia_glue.h"
#include "media/base/media_export.h"

struct AudioStreamBasicDescription;
struct AudioBufferList;

// Must use the same packing as the original structures defined in the OS X SDK
// to make sure data types are aligned the same way on different architectures.
#pragma pack(push, 4)

// MediaToolbox API is only introduced in Mac OS X 10.9, the (potential)
// linking with it has to happen in runtime. If it succeeds, subsequent clients
// can use MediaToolbox via the class declared in this file, where the original
// naming has been kept as much as possible.
class MEDIA_EXPORT MediaToolboxGlue {
 public:
  // Originally in CMBase.h
  typedef signed long CMItemCount;

  // Orinally in MTAudioProcessingTap.h
  typedef uint32_t MTAudioProcessingTapCreationFlags;
  typedef uint32_t MTAudioProcessingTapFlags;
  enum { kMTAudioProcessingTapCallbacksVersion_0 = 0 };
  enum {
    kMTAudioProcessingTapCreationFlag_PreEffects = 0x01,
    kMTAudioProcessingTapCreationFlag_PostEffects = 0x02
  };
  enum {
    kMTAudioProcessingTapFlag_StartOfStream = 0x100,
    kMTAudioProcessingTapFlag_EndOfStream = 0x200
  };
  typedef const struct opaqueMTAudioProcessingTap* MTAudioProcessingTapRef;
  typedef void (*MTAudioProcessingTapInitCallback)(MTAudioProcessingTapRef tap,
                                                   void* clientInfo,
                                                   void** tapStorageOut);
  typedef void (*MTAudioProcessingTapFinalizeCallback)(
      MTAudioProcessingTapRef tap);
  typedef void (*MTAudioProcessingTapPrepareCallback)(
      MTAudioProcessingTapRef tap,
      CMItemCount maxFrames,
      const AudioStreamBasicDescription* processingFormat);
  typedef void (*MTAudioProcessingTapUnprepareCallback)(
      MTAudioProcessingTapRef tap);
  typedef void (*MTAudioProcessingTapProcessCallback)(
      MTAudioProcessingTapRef tap,
      CMItemCount numberFrames,
      MTAudioProcessingTapFlags flags,
      AudioBufferList* bufferListInOut,
      CMItemCount* numberFramesOut,
      MTAudioProcessingTapFlags* flagsOut);

  typedef struct {
    int version;
    void* clientInfo;
    MTAudioProcessingTapInitCallback init;
    MTAudioProcessingTapFinalizeCallback finalize;
    MTAudioProcessingTapPrepareCallback prepare;
    MTAudioProcessingTapUnprepareCallback unprepare;
    MTAudioProcessingTapProcessCallback process;
  } MTAudioProcessingTapCallbacks;

  static OSStatus MTAudioProcessingTapCreate(
      CFAllocatorRef allocator,
      const MediaToolboxGlue::MTAudioProcessingTapCallbacks* callbacks,
      MediaToolboxGlue::MTAudioProcessingTapCreationFlags flags,
      MediaToolboxGlue::MTAudioProcessingTapRef* tapOut);

  static OSStatus MTAudioProcessingTapGetSourceAudio(
      MTAudioProcessingTapRef tap,
      CMItemCount numberFrames,
      AudioBufferList* bufferListInOut,
      MTAudioProcessingTapFlags* flagsOut,
      CoreMediaGlue::CMTimeRange* timeRangeOut,
      CMItemCount* numberFramesOut);

  static void* MTAudioProcessingTapGetStorage(
      MediaToolboxGlue::MTAudioProcessingTapRef tap);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaToolboxGlue);
};

#pragma pack(pop)

#endif  // MEDIA_BASE_MAC_MEDIATOOLBOX_GLUE_H_
