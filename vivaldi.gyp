{
  'targets': [
    {
      'target_name': 'vivaldi',
      'type': 'none',
      'dependencies': [
        'chromium/chrome/chrome.gyp:vivaldi',
      ],
      'conditions': [
        ['OS=="win"',  {
          # Allow running and debugging vivaldi from the top directory of the MSVS solution view
          'product_name':'vivaldi.exe',
        }]
      ]
    },
  ],
}
