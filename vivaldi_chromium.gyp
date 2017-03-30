# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
{
  'targets': [
    {
      'target_name': 'vivaldi_browser',
      'type': 'static_library',
      'dependencies': [
        'app/vivaldi_resources.gyp:*',
        'chromium/base/base.gyp:base',
        'chromium/components/components.gyp:search_engines',
        'chromium/chrome/chrome_resources.gyp:chrome_resources',
        'chromium/chrome/chrome_resources.gyp:chrome_strings',
        'chromium/crypto/crypto.gyp:crypto',
        'chromium/chrome/chrome.gyp:browser_extensions',
        'chromium/components/components.gyp:os_crypt',
        'chromium/skia/skia.gyp:skia',
        'extensions/vivaldi_api_resources.gyp:*',
        'vivaldi_extensions',
        'vivaldi_preferences',
      ],
      'include_dirs': [
        '.',
        'chromium',
      ],
      'sources': [
        'app/vivaldi.rc',
        'app/vivaldi_commands.h',
        'app/vivaldi_command_controller.cpp',
        'app/vivaldi_command_controller.h',
        'browser/vivaldi_browser_finder.cc',
        'browser/vivaldi_browser_finder.h',
        'clientparts/vivaldi_content_browser_client_parts.cc',
        'clientparts/vivaldi_content_browser_client_parts.h',
        'extraparts/vivaldi_browser_main_extra_parts.cc',
        'extraparts/vivaldi_browser_main_extra_parts.h',
        'importer/imported_notes_entry.cpp',
        'importer/imported_notes_entry.h',
        'importer/imported_speeddial_entry.cpp',
        'importer/imported_speeddial_entry.h',
        'importer/viv_importer.cpp',
        'importer/viv_importer.h',
        'importer/chrome_importer_bookmark.cpp',
        'importer/chromium_importer.cpp',
        'importer/chromium_importer.h',
        'importer/chromium_profile_importer.h',
        'importer/chromium_profile_importer.cpp',
        'importer/chromium_profile_lock.cpp',
        'importer/chromium_profile_lock.h',
        'importer/viv_importer_bookmark.cpp',
        'importer/viv_importer_notes.cpp',
        'importer/viv_importer_utils.h',
        'importer/chrome_importer_utils.h',
        'importer/viv_importer_wand.cpp',
        'importer/viv_opera_reader.cpp',
        'importer/viv_opera_reader.h',
        'importer/chrome_bookmark_reader.cpp',
        'importer/chrome_bookmark_reader.h',
        'notes/notes_attachment.cpp',
        'notes/notes_attachment.h',
        'notes/notesnode.cpp',
        'notes/notesnode.h',
        'notes/notes_factory.cpp',
        'notes/notes_factory.h',
        'notes/notes_model.cpp',
        'notes/notes_model.h',
        'notes/notes_model_observer.h',
        'notes/notes_model_loaded_observer.cpp',
        'notes/notes_model_loaded_observer.h',
        'notes/notes_storage.cpp',
        'notes/notes_storage.h',
        'notes/notes_submenu_observer.cc',
        'notes/notes_submenu_observer.h',
        'notifications/notification_permission_context_extensions.cc',
        'notifications/notification_permission_context_extensions.h',
        'ui/webgui/notes_ui.cpp',
        'ui/webgui/notes_ui.h',
        'ui/webgui/vivaldi_web_ui_controller_factory.cpp',
        'ui/webgui/vivaldi_web_ui_controller_factory.h',
        'ui/views/vivaldi_pin_shortcut.cpp',
        'ui/views/vivaldi_pin_shortcut.h',
        'prefs/native_settings_observer.h',
        'prefs/native_settings_observer.cc',
        'prefs/native_settings_observer_delegate.h',
        'prefs/vivaldi_tab_zoom_pref.h',
        'prefs/vivaldi_tab_zoom_pref.cc',
        'ui/dragging/drag_tab_handler.cc',
        'ui/dragging/drag_tab_handler.h'
      ],
      # Disables warnings about size_t to int conversions when the types
      # have different sizes
      'msvs_disabled_warnings': [ 4267 ],
      'conditions': [
        ['OS=="linux"', {
          "sources":[
            'extraparts/vivaldi_browser_main_extra_parts_linux.cc',
            'importer/viv_importer_util_linux.cpp',
            'importer/chromium_importer_util_linux.cpp',
            'importer/chromium_profile_lock_posix.cpp',
            'prefs/native_settings_observer_linux.h',
            'prefs/native_settings_observer_linux.cc',
          ],
        }],
        ['OS=="win"', {
          "sources":[
            'browser/vivaldi_download_status.cpp',
            'browser/vivaldi_download_status.h',
            'extraparts/vivaldi_browser_main_extra_parts_win.cc',
            'importer/viv_importer_util_win.cpp',
            'importer/chrome_importer_util_win.cpp',
            'importer/chromium_profile_lock_win.cpp',
            'prefs/native_settings_observer_win.h',
            'prefs/native_settings_observer_win.cc',
          ],
        }],
        ['OS=="mac"', {
          "sources":[
            'browser/vivaldi_app_observer.h',
            'browser/vivaldi_app_observer.mm',
            'extraparts/vivaldi_browser_main_extra_parts_mac.mm',
            'importer/viv_importer_util_mac.mm',
            'importer/chromium_importer_util_mac.mm',
            'importer/chromium_profile_lock_mac.mm',
            'prefs/native_settings_observer_mac.h',
            'prefs/native_settings_observer_mac.mm',
            'ui/dragging/drag_tab_handler_helper_mac.mm',
            'ui/dragging/drag_tab_handler_helper_mac.h'
          ],
        }, { #'OS!="mac"
          "dependencies":[
            'chromium/chrome/chrome.gyp:utility',
          ],
        }],
      ],
    },
    {
      'target_name': 'vivaldi_extensions',
      'type': 'static_library',
      'dependencies': [
        'extensions/api/vivaldi_chrome_extensions.gyp:*',
        'extensions/schema/vivaldi_api.gyp:vivaldi_chrome_api',
      ],
      'sources': [
        'extensions/permissions/vivaldi_api_permissions.cpp',
        'extensions/permissions/vivaldi_api_permissions.h',
        'extensions/vivaldi_extensions_client.cpp',
        'extensions/vivaldi_extensions_client.h',
        'extensions/vivaldi_extensions_init.cpp',
        'extensions/vivaldi_extensions_init.h',
      ],
      'include_dirs': [
        '.',
        'chromium',
      ],
    },
    {
      'target_name': 'vivaldi_packaged_app',
      'type': 'none',
      'dependencies': [
        'vivapp/vivaldi_app.gypi:*',
      ],
    },
    {
      'target_name': 'vivaldi_helper_script',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)',
            'files': [
              'app/vivaldi_local_profile.bat',
            ],
          }],
        }],
        ['OS=="win" and target_arch == "ia32"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)',
            'files': [
              'third_party/_winsparkle_lib/WinSparkle.dll',
              'third_party/_winsparkle_lib/WinSparkle.lib',
            ],
          }],
        }],
        ['OS=="win" and target_arch == "x64"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)',
            'files': [
              'third_party/_winsparkle_lib/x64/WinSparkle.dll',
              'third_party/_winsparkle_lib/x64/WinSparkle.lib',
            ],
          }],
        }],
      ],
    },
    {
      'target_name': 'vivaldi_preferences',
      'type': 'static_library',
      'dependencies': [
        'chromium/base/base.gyp:base',
      ],
      'sources': [
        'prefs/vivaldi_pref_names.h',
        'prefs/vivaldi_pref_names.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          "sources":[
            'prefs/vivaldi_browser_prefs_linux.cc',
          ],
        }],
        ['OS=="win"', {
          "sources":[
            'prefs/vivaldi_browser_prefs_win.cc',
          ],
        }],
        ['OS=="mac"', {
          "sources":[
            'prefs/vivaldi_browser_prefs_mac.mm',
          ],
        }],
      ],
      'include_dirs': [
        '.',
        'chromium',
      ],
    },
  ],
}
