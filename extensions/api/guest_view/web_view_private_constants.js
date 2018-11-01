// VIVALDI

var PRIVATE_CONSTANTS = {
  ATTRIBUTE_EXTENSIONHOST: 'extensionhost',
  ATTRIBUTE_INSPECT_TAB_ID: 'inspect_tab_id',
  ATTRIBUTE_TAB_ID: 'tab_id',
  ATTRIBUTE_WINDOW_ID: 'window_id',
  ATTRIBUTE_WASTYPED: 'wasTyped'
};

function addPrivateConstants(WebViewConstants) {
	for (var i in PRIVATE_CONSTANTS) {
		WebViewConstants[i] = PRIVATE_CONSTANTS[i];
	}
}

// Exports.
exports.$set('addPrivateConstants', addPrivateConstants);
