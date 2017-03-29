{
    'targets': [
      {
        'target_name': 'vivaldi_browser',
        'type': 'static_library',
        'dependencies': [
          'chromium/chrome/chrome_resources.gyp:chrome_resources',
          'chromium/chrome/chrome_resources.gyp:chrome_strings',
          'chromium/crypto/crypto.gyp:crypto',
          'chromium/components/components.gyp:os_crypt',
          'chromium/skia/skia.gyp:skia',
        ],
        'include_dirs': [
          '.',
          'chromium',
        ],
        'sources': [
          'app/vivaldi.rc',
          'app/vivaldi_commands.h',
          'app/vivaldi_command_controller.cpp',
          'clientparts/vivaldi_content_browser_client_parts.h',
          'clientparts/vivaldi_content_browser_client_parts.cc',
          'extraparts/vivaldi_browser_main_extra_parts.h',
          'extraparts/vivaldi_browser_main_extra_parts.cc',
          'importer/imported_notes_entry.cpp',
          'importer/imported_notes_entry.h',
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
          'notes/notes_base_model_observer.cpp',
          'notes/notes_base_model_observer.h',
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
          'ui/webgui/notes_ui.cpp',
          'ui/webgui/notes_ui.h',
          'ui/webgui/vivaldi_web_ui_controller_factory.cpp',
          'ui/webgui/vivaldi_web_ui_controller_factory.h',
        ],
    'conditions': [
      ['OS=="linux"', {
        "sources":[
          'importer/viv_importer_util_linux.cpp',
          'importer/chromium_importer_util_linux.cpp',
          'importer/chromium_profile_lock_posix.cpp',
          'extraparts/vivaldi_browser_main_extra_parts_linux.cc',
        ]
      }],
      ['OS=="win"', {
        "sources":[
          'importer/viv_importer_util_win.cpp',
          'importer/chrome_importer_util_win.cpp',
          'importer/chromium_profile_lock_win.cpp',
          'extraparts/vivaldi_browser_main_extra_parts_win.cc',
        ]
      }],
      ['OS=="mac"', {
        "sources":[
          'importer/viv_importer_util_mac.mm',
          'importer/chromium_importer_util_mac.mm',
          'importer/chromium_profile_lock_mac.mm',
          'extraparts/vivaldi_browser_main_extra_parts_mac.mm',
        ]
      }, { #'OS!="mac"
        "dependencies":[
          'chromium/chrome/chrome.gyp:utility',
        ]
      }],
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
  ],
}
