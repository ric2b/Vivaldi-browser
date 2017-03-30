# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # A hook that can be overridden in other repositories to add additional
    # compilation targets to 'All'.
    'app_targets%': [],
    # For Android-specific targets.
    'android_app_targets%': [],
  },
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'xcode_create_dependents_test_runner': 1,
      'dependencies': [
        'chromium/chrome/chrome.gyp:chrome',
        'testing/vivaldi_testing.gyp:*',
        'vivaldi.gyp:*',
      ],
      'conditions': [
        ['OS!="ios" and OS!="android"', {
          'dependencies': [
            'chromium/base/base.gyp:base_unittests',
            'chromium/chrome/chrome.gyp:browser_tests',
            'chromium/chrome/chrome.gyp:chromedriver_unittests',
            'chromium/chrome/chrome.gyp:interactive_ui_tests',
            'chromium/chrome/chrome.gyp:sync_integration_tests',
            'chromium/chrome/chrome.gyp:unit_tests',
            'chromium/cc/cc_tests.gyp:cc_unittests',
            'chromium/content/content_shell_and_tests.gyp:content_browsertests',
            'chromium/content/content_shell_and_tests.gyp:content_unittests',
            'chromium/content/content_shell_and_tests.gyp:content_shell',
            'chromium/google_apis/google_apis.gyp:google_apis_unittests',
            'chromium/gpu/gles2_conform_support/gles2_conform_support.gyp:gles2_conform_support',
            'chromium/gpu/gpu.gyp:gpu_unittests',
            'chromium/ipc/ipc.gyp:ipc_tests',
            'chromium/jingle/jingle.gyp:jingle_unittests',
            'chromium/media/cast/cast.gyp:cast_unittests',
            'chromium/media/media.gyp:media_unittests',
            'chromium/third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            'chromium/components/components_tests.gyp:components_unittests',
            'chromium/crypto/crypto.gyp:crypto_unittests',
            'chromium/net/net.gyp:net_unittests',
            'chromium/sql/sql.gyp:sql_unittests',
            'chromium/sync/sync.gyp:sync_unit_tests',
            'chromium/ui/gfx/gfx_tests.gyp:gfx_unittests',
            'chromium/url/url.gyp:url_unittests',
            'chromium/device/device_tests.gyp:device_unittests',
            'chromium/ppapi/ppapi_internal.gyp:ppapi_unittests',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'chromium/chrome/chrome.gyp:crash_service',
            'chromium/chrome/installer/mini_installer.gyp:mini_installer',
            'chromium/content/content_shell_and_tests.gyp:copy_test_netscape_plugin',
            'chromium/courgette/courgette.gyp:courgette_unittests',
            'chromium/sandbox/sandbox.gyp:sbox_integration_tests',
            'chromium/sandbox/sandbox.gyp:sbox_unittests',
            'chromium/sandbox/sandbox.gyp:sbox_validation_tests',
          ],
          'conditions': [
            # remoting_host_installation uses lots of non-trivial GYP that tend
            # to break because of differences between ninja and msbuild. Make
            # sure this target is built by the builders on the main waterfall.
            # See http://crbug.com/180600.
            ['wix_exists == "True" and sas_dll_exists == "True"', {
              'dependencies': [
                #'chromium/remoting/remoting.gyp:remoting_host_installation',
              ],
            }],
            ['syzyasan==1', {
              'variables': {
                # Disable incremental linking for all modules.
                # 0: inherit, 1: disabled, 2: enabled.
                'msvs_debug_link_incremental': '1',
                'msvs_large_module_debug_link_mode': '1',
                # Disable RTC. Syzygy explicitly doesn't support RTC
                # instrumented binaries for now.
                'win_debug_RuntimeChecks': '0',
              },
              'defines': [
                # Disable iterator debugging (huge speed boost).
                '_HAS_ITERATOR_DEBUGGING=0',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  # Enable profile information (necessary for SyzyAsan
                  # instrumentation). This is incompatible with incremental
                  # linking.
                  'Profile': 'true',
                },
              }
            }],
          ],
        }],
        ['OS=="ios"', {
          'dependencies': [
            'chromium/chrome/chrome.gyp:browser',
            'chromium/chrome/chrome.gyp:browser_ui',
            'chromium/ios/ios.gyp:*',
            # NOTE: This list of targets is present because
            # mojo_base.gyp:mojo_base cannot be built on iOS, as
            # javascript-related targets cause v8 to be built.
            'chromium/mojo/mojo_base.gyp:mojo_common_lib',
          ],
        }],
        ['use_openssl==0 and (OS=="mac" or OS=="ios" or OS=="win")', {
          'dependencies': [
            'chromium/third_party/nss/nss.gyp:*',
           ],
        }],
        ['OS=="win" or OS=="ios" or OS=="linux"', {
          'dependencies': [
            'chromium/breakpad/breakpad.gyp:*',
           ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            'chromium/chrome/chrome.gyp:build_app_dmg',
            'chromium/chrome/chrome.gyp:installer_packaging',
            'chromium/sandbox/sandbox.gyp:*',
            'chromium/third_party/crashpad/crashpad/crashpad.gyp:*',
            'chromium/third_party/ocmock/ocmock.gyp:*',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            'chromium/courgette/courgette.gyp:*',
            'chromium/sandbox/sandbox.gyp:*',
          ],
          'conditions': [
            ['enable_ipc_fuzzer==1', {
              'dependencies': [
                'chromium/tools/ipc_fuzzer/ipc_fuzzer.gyp:*',
              ],
            }],
            ['use_dbus==1', {
              'dependencies': [
                'chromium/dbus/dbus.gyp:*',
              ],
            }],
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            'chromium/tools/xdisplaycheck/xdisplaycheck.gyp:*',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            'chromium/ui/aura/aura.gyp:*',
            'chromium/ui/aura_extra/aura_extra.gyp:*',
          ],
        }],
        ['use_ash==1', {
          'dependencies': [
            'chromium/ash/ash.gyp:*',
          ],
        }],
        ['use_openssl==0', {
          'dependencies': [
            'chromium/net/third_party/nss/ssl.gyp:*',
          ],
        }],
        ['use_openssl==1', {
          'dependencies': [
            'chromium/third_party/boringssl/boringssl.gyp:*',
            #'chromium/third_party/boringssl/boringssl_tests.gyp:*',
          ],
        }],
        ['enable_basic_printing==1 or enable_print_preview==1', {
          'dependencies': [
            'chromium/printing/printing.gyp:printing_unittests',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            'chromium/ui/aura/aura.gyp:aura_unittests',
            'chromium/ui/compositor/compositor.gyp:compositor_unittests',
          ],
        }],
        ['use_aura==1 and chromecast==0', {
          'dependencies': [
            'chromium/ui/views/views.gyp:views_unittests',
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            'chromium/ui/app_list/app_list.gyp:app_list_unittests',
            'chromium/ui/message_center/message_center.gyp:message_center_unittests',
          ],
        }],
      ],
    }, # target_name: All
  ],
}
