// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "media/base/mac/mediatoolbox_glue.h"

#import <Foundation/Foundation.h>

#include <dlfcn.h>

#include "base/logging.h"
#include "base/lazy_instance.h"

namespace {

// This class is used to retrieve some MediaToolbox library functions. It must
// be used as a LazyInstance so that it is initialised once and in a thread-safe
// way. Normally no work is done in constructors: LazyInstance is an exception.
class MediaToolboxLibraryInternal {
 public:
  typedef OSStatus (*MTAudioProcessingTapCreateMethod)
      (CFAllocatorRef allocator,
      const MediaToolboxGlue::MTAudioProcessingTapCallbacks* callbacks,
      MediaToolboxGlue::MTAudioProcessingTapCreationFlags flags,
      MediaToolboxGlue::MTAudioProcessingTapRef* tapOut);

  typedef OSStatus (*MTAudioProcessingTapGetSourceAudioMethod)(
      MediaToolboxGlue::MTAudioProcessingTapRef tap,
      MediaToolboxGlue::CMItemCount numberFrames,
      AudioBufferList* bufferListInOut,
      MediaToolboxGlue::MTAudioProcessingTapFlags* flagsOut,
      CoreMediaGlue::CMTimeRange* timeRangeOut,
      MediaToolboxGlue::CMItemCount* numberFramesOut);

  typedef void* (*MTAudioProcessingTapGetStorageMethod)(
      MediaToolboxGlue::MTAudioProcessingTapRef tap);

  MediaToolboxLibraryInternal() {
    NSBundle* bundle = [NSBundle
        bundleWithPath:@"/System/Library/Frameworks/MediaToolbox.framework"];

    const char* path = [[bundle executablePath] fileSystemRepresentation];
    CHECK(path);
    void* library_handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    CHECK(library_handle) << dlerror();

    // Now extract the methods.
    mt_audio_processing_tap_create_method_ =
        reinterpret_cast<MTAudioProcessingTapCreateMethod>(
            dlsym(library_handle, "MTAudioProcessingTapCreate"));
    CHECK(mt_audio_processing_tap_create_method_) << dlerror();

    mt_audio_processing_tap_get_source_audio_method_ =
        reinterpret_cast<MTAudioProcessingTapGetSourceAudioMethod>(
            dlsym(library_handle, "MTAudioProcessingTapGetSourceAudio"));
    CHECK(mt_audio_processing_tap_get_source_audio_method_) << dlerror();

    mt_audio_processing_tap_get_storage_method_ =
        reinterpret_cast<MTAudioProcessingTapGetStorageMethod>(
            dlsym(library_handle, "MTAudioProcessingTapGetStorage"));
    CHECK(mt_audio_processing_tap_get_storage_method_) << dlerror();
  }

  const MTAudioProcessingTapCreateMethod&
      mt_audio_processing_tap_create_method() const {
    return mt_audio_processing_tap_create_method_;
  }

  const MTAudioProcessingTapGetSourceAudioMethod&
      mt_audio_processing_tap_get_source_audio_method() const {
    return mt_audio_processing_tap_get_source_audio_method_;
  }

  const MTAudioProcessingTapGetStorageMethod&
      mt_audio_processing_tap_get_storage_method() const {
    return mt_audio_processing_tap_get_storage_method_;
  }

 private:
  MTAudioProcessingTapCreateMethod mt_audio_processing_tap_create_method_;
  MTAudioProcessingTapGetSourceAudioMethod
      mt_audio_processing_tap_get_source_audio_method_;
  MTAudioProcessingTapGetStorageMethod
      mt_audio_processing_tap_get_storage_method_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolboxLibraryInternal);
};

}  // namespace

static base::LazyInstance<MediaToolboxLibraryInternal> g_mediatoolbox_handle =
    LAZY_INSTANCE_INITIALIZER;

// static
OSStatus MediaToolboxGlue::MTAudioProcessingTapCreate(
    CFAllocatorRef allocator,
    const MTAudioProcessingTapCallbacks* callbacks,
    MTAudioProcessingTapCreationFlags flags,
    MTAudioProcessingTapRef* tapOut) {
  return g_mediatoolbox_handle.Get().mt_audio_processing_tap_create_method()
      (allocator, callbacks, flags, tapOut);
}

// static
OSStatus MediaToolboxGlue::MTAudioProcessingTapGetSourceAudio(
    MTAudioProcessingTapRef tap,
    CMItemCount numberFrames,
    AudioBufferList* bufferListInOut,
    MTAudioProcessingTapFlags* flagsOut,
    CoreMediaGlue::CMTimeRange* timeRangeOut,
    CMItemCount* numberFramesOut) {
  return g_mediatoolbox_handle.Get()
      .mt_audio_processing_tap_get_source_audio_method()(tap,
                                                         numberFrames,
                                                         bufferListInOut,
                                                         flagsOut,
                                                         timeRangeOut,
                                                         numberFramesOut);
}

//static
void* MediaToolboxGlue::MTAudioProcessingTapGetStorage(
    MTAudioProcessingTapRef tap) {
  return g_mediatoolbox_handle.Get()
      .mt_audio_processing_tap_get_storage_method()(tap);
}
