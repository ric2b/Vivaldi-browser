# Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

{
  'sources': [
    '<@(schema_files)',
  ],
  'variables': {
    'vivaldi_schema_files': [
      'autoupdate.json',
      'bookmarks_private.json',
      'browser_action_utilities.json',
      'editcommand.json',
      'history_private.json',
      'import_data.json',
      'mousegestures.json',
      'notes.json',
      'runtime_private.json',
      'savedpasswords.json',
      'settings.json',
      'show_menu.json',
      'tabs_private.json',
      'thumbnails.json',
      'vivaldi_sessions.json',
      'vivaldi_utilities.json',
      'web_view_private.json',
      'zoom.json',
    ],
    'schema_files': [
      '<@(vivaldi_schema_files)',
    ],
    'non_compiled_schema_files': [
    ],
    'schema_include_rules': [
      '<(DEPTH)/../extensions/api:extensions::vivaldi::%(namespace)s',
      '<(DEPTH)/../schema:extensions::vivaldi::%(namespace)s',
    ],
    'schema_dependencies': [
    ],

    'chromium_code': 1,
    'actual_root': '<(VIVALDI)',
    'cc_dir': 'extensions/schema',
    'root_namespace': 'extensions::vivaldi::%(namespace)s',
    'bundle_name': 'Vivaldi',
    'impl_dir_': 'extensions/api/',
  },
}
