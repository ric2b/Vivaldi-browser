{
  'type': 'none',
  'variables': {
    'includes': [ '../chromium/chrome/chrome_features.gypi' ],
    'extra_grit_defines': ['<@(chrome_grit_defines)'],
    'grit_out_dir%': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'main_resource_dir%': '<(DEPTH)/chrome/app',
  },
  'includes': [ 'vivaldi_grit_action.gypi' ],
  'actions': [
    {
      'action_name': "Generate <(_target_name) resources",
      'includes': [ '../chromium/chrome/chrome_grit_action.gypi' ],
    },
  ],
}
