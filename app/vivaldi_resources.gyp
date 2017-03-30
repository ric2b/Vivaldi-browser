# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
{
  'includes': [
    '../chromium/build/chrome_settings.gypi',
  ],
  'variables': {
    'generate_untranslated%': 0,
  },
  'targets': [
    {
      'target_name': 'renderer_resources',
      'variables': {
        'main_resource_dir':  '<(DEPTH)/chrome/renderer/resources/',
        'merge_main_file': 'renderer_resources.grd',
        'vivaldi_resource_path':
            '<(VIVALDI)/app/resources/renderer/vivaldi_renderer_resources.grd',
        'have_strings': 0,
      },
      'includes': [ 'vivaldi_chrome_grit_action.gypi' ]
    },
    {
      'target_name': 'generated_resources',
      'variables': {
        'merge_main_file': 'generated_resources.grd',
        'vivaldi_resource_path':
          '<(VIVALDI)/app/resources/generated/vivaldi_generated_resources.grd',
      },
      'includes': [ 'vivaldi_chrome_grit_action.gypi' ]
    },
    {
      'target_name': 'vivaldi_chromium_strings',
      'variables': {
        'merge_main_file': 'chromium_strings.grd',
        'vivaldi_resource_path':
            '<(VIVALDI)/app/resources/strings/vivaldi_strings.grd',
      },
      'includes': [ 'vivaldi_chrome_grit_action.gypi' ]
    },
    {
      'target_name': 'vivaldi_strings',
      'variables': {
        'merge_main_file': 'google_chrome_strings.grd',
        'vivaldi_resource_path':
             '<(VIVALDI)/app/resources/strings/vivaldi_strings.grd',
        'target_file': 'vivaldi_strings.grd',
      },
      'includes': [ 'vivaldi_chrome_grit_action.gypi' ]
    },
    {
      'target_name': 'locale_resources',
      'variables': {
        'main_resource_dir':  '<(DEPTH)/chrome/app/resources',
        'merge_main_file': 'locale_settings.grd',
        'vivaldi_resource_path':
            '<(VIVALDI)/app/resources/locale/vivaldi_locale_strings.grd',
        'translation_dir': '',
      },
      'includes': [ 'vivaldi_chrome_grit_action.gypi' ]
    },
    {
      'target_name': 'theme_resources',
      'variables': {
        'main_resource_dir':  '<(DEPTH)/chrome/app/theme',
        'merge_main_file': 'theme_resources.grd',
        'vivaldi_resource_path':
             '<(VIVALDI)/app/resources/theme/vivaldi_theme.grd',
        'have_strings': 0,
      },
      'includes': [ 'vivaldi_chrome_grit_action.gypi' ]
    },
    {
      'target_name': 'unscaled_resources',
      'variables': {
        'main_resource_dir':  '<(DEPTH)/chrome/app/theme',
        'merge_main_file': 'chrome_unscaled_resources.grd',
        'vivaldi_resource_path':
            '<(VIVALDI)/app/resources/theme/vivaldi_unscaled_resources.grd',
        'have_strings': 0,
      },
      'includes': [ 'vivaldi_chrome_grit_action.gypi' ]
    },
    {
      'target_name': 'ui_resources',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/resources',
        'main_resource_dir':  '<(DEPTH)/ui/resources',
        'merge_main_file': 'ui_resources.grd',
        'vivaldi_resource_path':
            '<(VIVALDI)/app/resources/ui/vivaldi_ui_resources.grd',
        'have_strings': 0,
      },
      'includes': [ 'vivaldi_other_grit_action.gypi' ]
    },
    {
      'target_name': 'component_resources',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components/strings',
        'main_resource_dir':  '<(DEPTH)/components',
        'merge_main_file': 'components_strings.grd',
        'vivaldi_resource_path':
              '<(VIVALDI)/app/resources/components_strings/vivaldi_components_strings.grd',
        'translation_dir': 'strings',
      },
      'includes': [ 'vivaldi_other_grit_action.gypi' ]
    },
    {
      'target_name': 'component_chrome_resources',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components/strings',
        'main_resource_dir':  '<(DEPTH)/components',
        'merge_main_file': 'components_chromium_strings.grd',
        'vivaldi_resource_path':
               '<(VIVALDI)/app/resources/components/vivaldi_components.grd',
      },
      'includes': [ 'vivaldi_other_grit_action.gypi' ]
    },
    {
      'target_name': 'component_google_resources',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components/strings',
        'main_resource_dir':  '<(DEPTH)/components',
        'merge_main_file': 'components_google_chrome_strings.grd',
        'vivaldi_resource_path':
            '<(VIVALDI)/app/resources/components/vivaldi_components.grd',
        'translation_dir': 'strings',
        'target_file': 'components_vivaldi_strings.grd',
      },
      'includes': [ 'vivaldi_other_grit_action.gypi' ]
    },
    {
      'target_name': 'component_locale_resources',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components/strings',
        'main_resource_dir':  '<(DEPTH)/components',
        'merge_main_file': 'components_locale_settings.grd',
        'vivaldi_resource_path':
            '<(VIVALDI)/app/resources/components_locale_settings/vivaldi_components_locale_settings.grd',
        'translation_dir': 'strings',
        'target_file': 'components_locale_settings.grd',
      },
      'includes': [ 'vivaldi_other_grit_action.gypi' ]
    },
    {
      'target_name': 'component_scaled_resources',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components',
        'main_resource_dir':  '<(DEPTH)/components/resources',
        'merge_main_file': 'components_scaled_resources.grd',
        'vivaldi_resource_path':
            '<(VIVALDI)/app/resources/components_scaled/vivaldi_components_scaled.grd',
        'have_strings': 0,
      },
      'includes': [ 'vivaldi_other_grit_action.gypi' ]
    },
    {
      'target_name': 'ash_strings',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ash/strings',
        'grit_grd_file': '<(DEPTH)/ash/ash_strings.grd',
      },
      'includes': [ 'vivaldi_normal_grit_action.gypi' ]
    },
    {
      'target_name': 'bluetooth_strings',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/device/bluetooth/strings',
        'grit_grd_file': '<(DEPTH)/device/bluetooth/bluetooth_strings.grd',
      },
      'includes': [ 'vivaldi_normal_grit_action.gypi' ]
    },
    {
      'target_name': 'content_strings',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content/app/strings',
        'grit_grd_file': '<(DEPTH)/content/app/strings/content_strings.grd',
      },
      'includes': [ 'vivaldi_normal_grit_action.gypi' ]
    },
    {
      'target_name': 'libaddressinput_strings',
      'variables': {
        'grit_out_dir':
            '<(SHARED_INTERMEDIATE_DIR)/third_party/libaddressinput/',
        'grit_grd_file':
            '<(DEPTH)/chrome/app/address_input_strings.grd',
      },
      'includes': [ 'vivaldi_normal_grit_action.gypi' ]
    },
    {
      'target_name': 'policy_templates',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
        'grit_grd_file':
            '<(DEPTH)/components/policy/resources/policy_templates.grd',
      },
      'includes': [
        'vivaldi_normal_grit_action.gypi'
      ]
    },
    {
      'target_name': 'platform_locale_settings',
      'variables': {
        'grit_grd_file':
            '<(DEPTH)/chrome/app/resources/locale_settings_<(OS).grd',
      },
      'includes': [ 'vivaldi_normal_chrome_grit_action.gypi' ]
    },
    {
      'target_name': 'ui_strings',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/strings',
        'grit_grd_file': '<(DEPTH)/ui/strings/ui_strings.grd',
      },
      'includes': [ 'vivaldi_normal_grit_action.gypi' ]
    },
    {
      'target_name': 'extension_strings',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/extensions/strings',
        'grit_grd_file':  '<(DEPTH)/extensions/extensions_strings.grd',
      },
      'includes': [ 'vivaldi_normal_grit_action.gypi' ]
    },
    {
      'target_name': 'vivaldi_native_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/vivaldi',
        'grit_grd_file': 'native_resources/vivaldi_native_strings.grd',
        'grit_resource_ids': '<(VIVALDI)/app/gritsettings/resource_ids',
      },
      'actions': [
        {
          'action_name': 'Generate vivaldi_native_strings',
          'includes': [ '../chromium/build/grit_action.gypi' ],
        },
      ],
      'conditions': [
        ['generate_untranslated or buildtype == "Official"', {
          'includes': [
            'vivaldi_extract_not_translated.gypi',
          ],
        }],
      ],
      'includes': [
        '../chromium/build/grit_target.gypi'
      ],
    },
    {
      'target_name': 'vivaldi_native_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/vivaldi',
        'grit_resource_ids': '<(VIVALDI)/app/gritsettings/resource_ids',
      },
      'actions': [
        {
          'action_name': 'Generate vivaldi_native_resources',
          'variables': {
            'grit_grd_file': 'native_resources/vivaldi_native_resources.grd',
          },
          'includes': [ '../chromium/build/grit_action.gypi' ],
        },
        {
          'action_name': 'Generate vivaldi_native_unscaled',
          'variables': {
            'grit_grd_file': 'native_resources/vivaldi_native_unscaled.grd',
          },
          'includes': [ '../chromium/build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../chromium/build/grit_target.gypi' ],
    },
    {
      'target_name': 'vivaldi_other_resources',
      'type': 'none',
      'copies': [
        {
          'destination': '<(VIVALDI)/out/vivaldi_resources/other_resources/',
          'basepath': 'other_resources/<(VIVALDI_RELEASE_KIND)/',
          'files': [
            'other_resources/<(VIVALDI_RELEASE_KIND)/default_100_percent/product_icon_128.png',
            'other_resources/<(VIVALDI_RELEASE_KIND)/default_200_percent/product_icon_128.png',
            'other_resources/<(VIVALDI_RELEASE_KIND)/win8/Logo.png',
            'other_resources/<(VIVALDI_RELEASE_KIND)/win8/SecondaryTile.png',
            'other_resources/<(VIVALDI_RELEASE_KIND)/win8/SmallLogo.png',
          ],
        },
      ],
    },
    {
      'target_name': 'vivaldi_combined_component_resources',
      'type': 'none',
      'dependencies': [
        'component_resources',
        'component_chrome_resources',
        'component_google_resources',
      ]
    },
    {
      'target_name': 'vivaldi_combined_resources',
      'type': 'none',
      'dependencies': [
        'generated_resources',
        'vivaldi_strings',
        'vivaldi_chromium_strings',
        'renderer_resources',
        'locale_resources',
        'theme_resources',
        'unscaled_resources',
        'vivaldi_other_resources',
      ]
    },
  ],
}
