{
  'type': 'none',
  'variables': {
    'includes': [ '../chromium/chrome/chrome_features.gypi' ],
    'extra_grit_defines': ['<@(chrome_grit_defines)'],
    'grit_out_dir%': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'have_strings%': 0,
  },
  'actions': [
    {
      'action_name': "Generate <(_target_name) resources",
      'includes': [ '../chromium/chrome/chrome_grit_action.gypi' ],
    },
  ],
  'conditions': [
    ['have_strings and (generate_untranslated or buildtype == "Official")', {
      'includes': [
        'vivaldi_extract_not_translated.gypi',
      ],
    }],
  ],
}
