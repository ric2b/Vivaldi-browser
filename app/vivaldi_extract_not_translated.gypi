# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
{
  'variables': {
    'grit_cmd': ['python', '<(DEPTH)/tools/grit/grit.py'],
    'grit_resource_ids%': '<(DEPTH)/tools/gritsettings/resource_ids',
    'grit_additional_defines%': [],
  },
  'actions': [
    {
      'action_name': "Generate list of missing translations for <(_target_name)",
      'inputs': [
        '<!@pymod_do_main(grit_info <@(grit_defines) <@(grit_additional_defines) '
            '--inputs <(grit_grd_file) '
            '-f "<(grit_resource_ids)")',
      ],
      'outputs': [
        '<!@(python list_lang_xmb_files.py '
            '<(PRODUCT_DIR)/translations/<(_target_name) '
            '<@(locales) <@(vivaldi_pending_locales)'
            ')',
      ],
      'action': ['<@(grit_cmd)',
                 '-i', '<(grit_grd_file)', 'xmb',
                 '-L',
                 '<@(grit_defines)',
                 '<@(grit_additional_defines)',
                 '<(PRODUCT_DIR)/translations/<(_target_name).xmb',
                 '<@(locales)',
                 '<@(vivaldi_pending_locales)'
                 ],
      'message': 'Generating resources from <(grit_grd_file)',
    },
    {
      'action_name': "Generate list of all strings for <(_target_name)",
      'inputs': [
        '<!@pymod_do_main(grit_info <@(grit_defines) <@(grit_additional_defines) '
            '--inputs <(grit_grd_file) '
            '-f "<(grit_resource_ids)")',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/translations/<(_target_name)_all.xmb',
      ],
      'action': ['<@(grit_cmd)',
                 '-i', '<(grit_grd_file)', 'xmb',
                 '<@(grit_defines)',
                 '<@(grit_additional_defines)',
                 '<(PRODUCT_DIR)/translations/<(_target_name)_all.xmb',
                 ],
      'message': 'Generating resources from <(grit_grd_file)',
    },
  ],
}
