# Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

{
  'targets': [
    {
      'target_name': 'vivaldi_chrome_api',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../chromium/build/json_schema_bundle_compile.gypi',
        '../../chromium/build/json_schema_compile.gypi',
        'vivaldi_schemas.gypi',
      ],
      'dependencies': [
        '<@(schema_dependencies)',
      ],
    },
  ],
}
