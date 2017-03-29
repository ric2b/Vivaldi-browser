# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../skia/skia.gyp:skia',
    '../ui/gl/gl.gyp:gl',
    '../mojo/mojo_base.gyp:mojo_application_base',
  ],
  'sources': [
    'gpu/gpu_child_thread.cc',
    'gpu/gpu_child_thread.h',
    'gpu/gpu_main.cc',
    'gpu/gpu_process.cc',
    'gpu/gpu_process.h',
    'gpu/gpu_watchdog_thread.cc',
    'gpu/gpu_watchdog_thread.h',
    'gpu/in_process_gpu_thread.cc',
    'gpu/in_process_gpu_thread.h',
  ],
  'include_dirs': [
    '..',
  ],
  'conditions': [
    ['OS=="win"', {
      'include_dirs': [
        '<(DEPTH)/third_party/khronos',
        # ANGLE libs picked up from ui/gl
        '<(angle_path)/src',
        '<(DEPTH)/third_party/wtl/include',
      ],
      'link_settings': {
        'libraries': [
          '-lsetupapi.lib',
        ],
      },
    }],
    ['system_proprietary_codecs==1 and OS=="win"', {
      'dependencies': [
        '<(DEPTH)/net/net.gyp:net', # For GetMimeTypeFromFile
      ],
      'sources': [
        'common/gpu/media/wmf_byte_stream.cc',
        'common/gpu/media/wmf_byte_stream.h',
        'common/gpu/media/wmf_media_pipeline.cc',
        'common/gpu/media/wmf_media_pipeline.h',
      ],
    }],
    ['system_proprietary_codecs==1', {
      'sources': [
        'common/gpu/media/avf_data_buffer_queue.h',
        'common/gpu/media/avf_data_buffer_queue.mm',
        'common/gpu/media/avf_audio_tap.h',
        'common/gpu/media/avf_audio_tap.mm',
        'common/gpu/media/avf_media_decoder.h',
        'common/gpu/media/avf_media_decoder.mm',
        'common/gpu/media/avf_media_pipeline.h',
        'common/gpu/media/avf_media_pipeline.mm',
        'common/gpu/media/avf_media_reader.h',
        'common/gpu/media/avf_media_reader.mm',
        'common/gpu/media/avf_media_reader_runner.h',
        'common/gpu/media/avf_media_reader_runner.mm',
        'common/gpu/media/data_request_handler.h',
        'common/gpu/media/data_request_handler.mm',
        'common/gpu/media/data_source_loader.h',
        'common/gpu/media/data_source_loader.mm',
        'common/gpu/media/ipc_data_source.h',
        'common/gpu/media/ipc_data_source_impl.cc',
        'common/gpu/media/ipc_data_source_impl.h',
        'common/gpu/media/ipc_media_pipeline.cc',
        'common/gpu/media/ipc_media_pipeline.h',
        'common/gpu/media/platform_media_pipeline.h',
        'common/gpu/media/platform_media_pipeline_mac.cc',
        'common/gpu/media/platform_media_pipeline_win.cc',
      ],
      'dependencies': [
        '../media/media.gyp:media',
      ],
    }],
    ['target_arch!="arm" and chromeos == 1', {
      'include_dirs': [
        '<(DEPTH)/third_party/libva',
      ],
    }],
  ],
}
