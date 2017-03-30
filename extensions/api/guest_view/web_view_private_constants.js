// VIVALDI

var PRIVATE_CONSTANTS = {
  ATTRIBUTE_TAB_ID: 'tab_id',
  ATTRIBUTE_WINDOW_ID: 'window_id',
  ATTRIBUTE_WASTYPED: 'wasTyped',
  ATTRIBUTE_EXTENSIONHOST: 'extensionhost',
};

function addPrivateConstants(WebViewConstants) {
	for (var i in PRIVATE_CONSTANTS) {
		WebViewConstants[i] = PRIVATE_CONSTANTS[i];
	}
}

// Exports.
exports.$set('addPrivateConstants', addPrivateConstants);
