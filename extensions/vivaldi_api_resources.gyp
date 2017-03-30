# Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

{
  'variables': {
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/vivaldi',
    'grit_resource_ids': '<(VIVALDI)/app/gritsettings/resource_ids',
  },
  'targets': [
    {
      'target_name': 'vivaldi_extension_resources',
      'type': 'none',
      'dependencies': [
      ],
      'actions': [
        {
          'action_name': 'generate_vivaldi_extension_resources',
          'variables': {
            'grit_grd_file': 'vivaldi_extension_resources.grd',
          },
          'includes': [ '../chromium/build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../chromium/build/grit_target.gypi' ],
    },
  ],
}
