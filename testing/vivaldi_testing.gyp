{
  'targets': [
    {
      'target_name': 'browser_tests',
      'includes': [ 'vivaldi_testing.gypi', ],
    },
    {
      'target_name': 'chromedriver_unittests',
      'includes': [ 'vivaldi_testing.gypi', ],
    },
    {
      'target_name': 'interactive_ui_tests',
      'includes': [ 'vivaldi_testing.gypi', ],
    },
    {
      'target_name': 'sync_integration_tests',
      'includes': [ 'vivaldi_testing.gypi', ],
    },
    {
      'target_name': 'unit_tests',
      'includes': [ 'vivaldi_testing.gypi', ],
    },
    {
      'target_name': 'content_browsertests',
      'includes': [ 'vivaldi_testing_content.gypi', ],
    },
    {
      'target_name': 'content_unittests',
      'includes': [ 'vivaldi_testing_content.gypi', ],
    },
    {
      'target_name': 'cc_unittests',
      'dependencies': ['<(DEPTH)/cc/cc_tests.gyp:cc_unittests',],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'google_apis_unittests',
      'dependencies': [ '<(DEPTH)/google_apis/google_apis.gyp:google_apis_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'gpu_unittests',
      'dependencies': [ '<(DEPTH)/gpu/gpu.gyp:gpu_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'media_unittests',
      'dependencies': [ '<(DEPTH)/media/media.gyp:media_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'ipc_tests',
      'dependencies': [ '<(DEPTH)/ipc/ipc.gyp:ipc_tests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'cast_unittests',
      'dependencies': [ '<(DEPTH)/media/cast/cast.gyp:cast_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'jingle_unittests',
      'dependencies': [ '<(DEPTH)/jingle/jingle.gyp:jingle_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'cacheinvalidation_unittests',
      'dependencies': [ '<(DEPTH)/third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'components_unittests',
      'dependencies': [ '<(DEPTH)/components/components_tests.gyp:components_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'crypto_unittests',
      'dependencies': [ '<(DEPTH)/crypto/crypto.gyp:crypto_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'net_unittests',
      'dependencies': [ '<(DEPTH)/net/net.gyp:net_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'sql_unittests',
      'dependencies': [ '<(DEPTH)/sql/sql.gyp:sql_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'sync_unit_tests',
      'dependencies': [ '<(DEPTH)/sync/sync.gyp:sync_unit_tests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'gfx_unittests',
      'dependencies': [ '<(DEPTH)/ui/gfx/gfx_tests.gyp:gfx_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'url_unittests',
      'dependencies': [ '<(DEPTH)/url/url.gyp:url_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
    {
      'target_name': 'device_unittests',
      'dependencies': [ '<(DEPTH)/device/device_tests.gyp:device_unittests', ],
      'includes': [ 'vivaldi_testing_base.gypi', ],
    },
   # {
   #   'target_name': 'ppapi_unittest',
   #   'dependencies': [ '<(DEPTH)/ppapi/ppapi_internal.gyp:ppapi_unittest', ],
   #   'includes': [ 'vivaldi_testing_base.gypi', ],
   # },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'courgette_unittests',
          'dependencies': [ '<(DEPTH)/courgette/courgette.gyp:courgette_unittests', ],
          'includes': [ 'vivaldi_testing_base.gypi', ],
        },
        {
          'target_name': 'sbox_integration_tests',
          'dependencies': [ '<(DEPTH)/sandbox/sandbox.gyp:sbox_integration_tests', ],
          'includes': [ 'vivaldi_testing_base.gypi', ],
        },
        {
          'target_name': 'sbox_unittests',
          'dependencies': [ '<(DEPTH)/sandbox/sandbox.gyp:sbox_unittests', ],
          'includes': [ 'vivaldi_testing_base.gypi', ],
        },
        {
          'target_name': 'sbox_validation_tests',
          'dependencies': [ '<(DEPTH)/sandbox/sandbox.gyp:sbox_validation_tests', ],
          'includes': [ 'vivaldi_testing_base.gypi', ],
        },
      ],
    }],
  ]
}
