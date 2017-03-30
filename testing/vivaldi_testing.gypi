{
  'type': 'none',
  'dependencies': [
    '<(DEPTH)/chrome/chrome.gyp:<(_target_name)',
  ],
  'target_conditions': [
    ['OS=="win"',  {
      # Allow running and debugging browser_tests from the testing directory of the MSVS solution view
      'product_name':'<(_target_name).exe',
    }],
  ],
}
