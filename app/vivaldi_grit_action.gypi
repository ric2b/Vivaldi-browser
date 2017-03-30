{
  'type': 'none',
  'hard_dependency': 1,
  'variables': {
    'variables': {
      'variables': {
        'main_resource_dir%': '<(DEPTH)/chrome/app',
        'combined_target_dir%': '<(VIVALDI)/out/vivaldi_resources/<(_target_name)',
        'extra_grit_defines%': [],
        'grit_additional_defines%':[],
        'translation_dir%':  'resources',
        'grit_out_dir%': '<(SHARED_INTERMEDIATE_DIR)/chrome',
        'target_file%': '<(merge_main_file)',
      },
      'main_resource_dir%': '<(main_resource_dir)',
      'combined_target_dir%': '<(combined_target_dir)',
      'target_file%': '<(target_file)',
      'have_strings%': 1,

      'main_resource_path': '<(main_resource_dir)/<(merge_main_file)',
      'merge_tool_path': 'resources/merge_resources.py',
      'mergeid_tool_path': 'resources/merge_idfile.py',
      'update_json_path': '<(VIVALDI)/app/resources/updates.json',
      'resource_id_file': '<(combined_target_dir)/resource_ids',
      'combined_target_file': '<(combined_target_dir)/<(target_file)',
      'translation_dir%':  '<(translation_dir)',
      'grit_out_dir%': '<(grit_out_dir)',
      'extra_grit_defines%': [ '<@(extra_grit_defines)' ],
      'grit_additional_defines%': [ '<@(grit_additional_defines)' ],
      'command_parameters': [
          '<@(grit_defines)',
          '<@(extra_grit_defines)',
          '<@(grit_additional_defines)',
          '--updatejson', '<(update_json_path)',
          '--root', '<(VIVALDI)',
          '--translation', '<(translation_dir)',
          '--output-file-name', '<(target_file)',
          '<(main_resource_path)',
          '<(vivaldi_resource_path)',
          '<(combined_target_dir)',
          ],
      # Hack to generate the merged GRD, JSON target list, and resource_ids files
      # before they are used by the other variables and actions.
      'vivaldi_migrated_source': [
          '<!@pymod_do_main(merge_idfile '
               '<(DEPTH)/tools/gritsettings/resource_ids <(resource_id_file) '
               '<(main_resource_path) <(combined_target_file))',
          '<!@pymod_do_main(merge_resources --build <@(command_parameters))',
      ],
    },
    'main_resource_dir%': '<(main_resource_dir)',
    'main_resource_path%': '<(main_resource_path)',
    'resource_id_file%': '<(resource_id_file)',
    'have_strings%': '<(have_strings)',
    'command_parameters%': '<(command_parameters)',
    'merge_tool_path%': '<(merge_tool_path)',
    'mergeid_tool_path%': '<(mergeid_tool_path)',
    'grit_out_dir%': '<(grit_out_dir)',
    'update_tool_path': 'resources/update_file.py',
    'update_json_path': '<(update_json_path)',
    'combined_target_dir%': '<(combined_target_dir)',
    'combined_target_file%': '<(combined_target_file)',
    'chrome_migrate_sources': [
       '<!@pymod_do_main(grit_info <@(grit_defines) <@(extra_grit_defines) '
                '<@(grit_additional_defines) --exclude-grit-source '
                '--inputs <(main_resource_path))',
    ],
    'vivaldi_migrate_sources': [
        '<!@pymod_do_main(merge_resources --list-secondary-resources '
            '<@(command_parameters))',
    ],
    'special_update_sources': [
         '<!@pymod_do_main(merge_resources --list-update-sources '
              '<@(command_parameters))'
    ],
    'special_update_targets': [
         '<!@pymod_do_main(merge_resources --list-update-targets '
              '<@(command_parameters))'
    ],
    'grit_grd_file': '<(combined_target_file)',
    'grit_resource_ids': '<(resource_id_file)',
  },
  'actions': [
    {
      'action_name': 'Generate merged <(_target_name) resource id',
      'inputs' : [
        '<(mergeid_tool_path)',
        '<(DEPTH)/tools/gritsettings/resource_ids',
      ],
      'outputs': [
        '<(resource_id_file)',
      ],
      'action': [
        'python', '-O',
        '<(mergeid_tool_path)',
        '<(DEPTH)/tools/gritsettings/resource_ids',
        '<(resource_id_file)',
        '<(main_resource_path)',
        '<(combined_target_file)',
      ],
    },
    {
      'action_name': 'Generate merged <(_target_name) resources',
      'inputs' : [
        '<(merge_tool_path)',
        '<(main_resource_path)',
        '<(vivaldi_resource_path)',
        '<!@pymod_do_main(merge_resources --list-main-sources '
              '<@(command_parameters))',
        '<!@pymod_do_main(merge_resources --list-secondary-sources '
              '<@(command_parameters))',
      ],
      'outputs': [
        '<(combined_target_file)',
        '<!@pymod_do_main(merge_resources --list-sources '
              '<@(command_parameters))',
      ],
      'action': [
        'python', '-O',
        '<(merge_tool_path)',
        '--build',
        '<@(command_parameters)',
      ],
    },
  ],
  'conditions': [
    ['special_update_sources', {
      'actions': [
        {
          'action_name': 'Copy and update special  <(_target_name) sources',
          'inputs': [
            '<(update_tool_path)',
            '<(update_json_path)',
            '<@(special_update_sources)',
          ],
          'outputs': ['<@(special_update_targets)',],
          'action': [
            'python', '-O',
            '<(update_tool_path)',
            '<(update_json_path)',
            '<(_target_name)',
            '<(combined_target_dir)',
            '<(VIVALDI)',
          ],
        }
      ],
    }],
    ['chrome_migrate_sources', {
      'copies': [
        {
          'destination': '<(combined_target_dir)',
          'basepath': '<(main_resource_dir)',
          'files': [
            '<@(chrome_migrate_sources)',
          ]
        },
      ],
    }],
    ['vivaldi_migrate_sources', {
      'copies': [
        {
          'destination': '<(combined_target_dir)',
          'basepath': '<(vivaldi_resource_path)',
          'files': [
            '<@(vivaldi_migrate_sources)',
          ]
        }
      ],
    }],
    ['have_strings and (generate_untranslated or buildtype == "Official")', {
      'includes': [
        'vivaldi_extract_not_translated.gypi',
      ],
    }],
  ],
  'includes': [
    '../chromium/build/grit_target.gypi'
  ],
}
