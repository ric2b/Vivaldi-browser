# Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

{
  'targets': [
    {
      'target_name': 'vivaldi_api_registration',
      'type': 'static_library',
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../chromium/build/json_schema_bundle_registration_compile.gypi',
        '../schema/vivaldi_schemas.gypi',
      ],
      'dependencies': [
        '../schema/vivaldi_api.gyp:vivaldi_chrome_api',

        # Different APIs include headers from these targets.
        "<(DEPTH)/content/content.gyp:content_browser",
      ],
    },
  ],
}
