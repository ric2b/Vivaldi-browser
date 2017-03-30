{
  'type': 'none',
  'actions': [
    {
      'action_name': "Generate <(_target_name) resources",
      'includes': [ '../chromium/build/grit_action.gypi' ],
    },
  ],
  'include': [ 'vivaldi_extract_not_translated.gypi' ]
}
