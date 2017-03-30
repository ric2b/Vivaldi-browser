{
  'type': 'none',
  'variables': {
    'main_resource_dir%': '<(DEPTH)/chrome/app',
  },
  'includes': [ 'vivaldi_grit_action.gypi' ],
  'actions': [
    {
      'action_name': "Generate <(_target_name) resources",
      'includes': [ '../chromium/build/grit_action.gypi' ],
    },
  ],
}
