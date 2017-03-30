{
  'targets': [
    {
      'target_name': 'vivaldi',
      'type': 'none',
      'dependencies': [
        'chromium/chrome/chrome.gyp:chrome',
      ],
      'conditions': [
        ['OS=="win"',  {
          # Allow running and debugging vivaldi from the top directory of the MSVS solution view
          'product_name':'vivaldi.exe',
          'dependencies': [
            'chromium/third_party/crashpad/crashpad/handler/handler.gyp:crashpad_handler',
          ],
        }]
      ]
    },
  ],
}
