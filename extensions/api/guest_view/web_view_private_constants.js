// VIVALDI

var PRIVATE_CONSTANTS = {
  ATTRIBUTE_INSPECT_TAB_ID: 'inspect_tab_id',
  ATTRIBUTE_TAB_ID: 'tab_id',
  ATTRIBUTE_PARENT_TAB_ID: 'parent_tab_id',
  ATTRIBUTE_VIEW_TYPE: 'vivaldi_view_type',
  ATTRIBUTE_WASTYPED: 'wasTyped',
  ATTRIBUTE_WINDOW_ID: 'windowId',
};

function addPrivateConstants(WebViewConstants) {
  for (var i in PRIVATE_CONSTANTS) {
    WebViewConstants[i] = PRIVATE_CONSTANTS[i];
  }
}

function addPrivateAttributeNames(attributes) {
  for (var i in PRIVATE_CONSTANTS) {
    attributes.push(PRIVATE_CONSTANTS[i]);
  }
}

// Exports.
exports.$set('addPrivateConstants', addPrivateConstants);
exports.$set('addPrivateAttributeNames', addPrivateAttributeNames);
