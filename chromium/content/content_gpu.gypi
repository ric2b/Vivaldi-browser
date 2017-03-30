# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../gpu/gpu.gyp:command_buffer_traits',
    '../gpu/gpu.gyp:gpu',
    '../gpu/gpu.gyp:gpu_ipc_service',
    '../media/gpu/ipc/media_ipc.gyp:media_gpu_ipc_service',
    '../media/media.gyp:media_gpu',
    '../skia/skia.gyp:skia',
    '../ui/gl/gl.gyp:gl',
    '../ui/gl/init/gl_init.gyp:gl_init',
    'content_common_mojo_bindings.gyp:content_common_mojo_bindings',
  ],
  'sources': [
    'gpu/gpu_child_thread.cc',
    'gpu/gpu_child_thread.h',
    'gpu/gpu_main.cc',
    'gpu/gpu_process.cc',
    'gpu/gpu_process.h',
    'gpu/gpu_process_control_impl.cc',
    'gpu/gpu_process_control_impl.h',
    'gpu/gpu_watchdog_thread.cc',
    'gpu/gpu_watchdog_thread.h',
    'gpu/in_process_gpu_thread.cc',
    'gpu/in_process_gpu_thread.h',
    'public/gpu/content_gpu_client.cc',
    'public/gpu/content_gpu_client.h',
    'public/gpu/gpu_video_decode_accelerator_factory.cc',
    'public/gpu/gpu_video_decode_accelerator_factory.h',
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
          '-lPropsys.lib',
        ],
      },
    }],
    ['system_proprietary_codecs==1 and OS=="mac"', {
      'configurations': {
        'Release_Base': {
          'xcode_settings': {
            # VB-18038 audio doesn't work with -O2
            'GCC_OPTIMIZATION_LEVEL': '1',
           },
         },
      },
    }],
    ['system_proprietary_codecs==1 and OS=="win"', {
      'dependencies': [
        '<(DEPTH)/net/net.gyp:net', # For GetMimeTypeFromFile
        '<(DEPTH)/media/media.gyp:mf_initializer',
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
        'common/gpu/media/propmedia_gpu_channel.cc',
        'common/gpu/media/propmedia_gpu_channel.h',
        'common/gpu/media/propmedia_gpu_channel_manager.cc',
        'common/gpu/media/propmedia_gpu_channel_manager.h',
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
    ['OS=="android"', {
      'dependencies': [
        '<(DEPTH)/media/media.gyp:player_android',
      ],
    }],
  ],
}
