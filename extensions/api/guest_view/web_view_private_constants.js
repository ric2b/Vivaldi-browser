// VIVALDI

var PRIVATE_CONSTANTS = {
  ATTRIBUTE_INSPECT_TAB_ID: 'inspect_tab_id',
  ATTRIBUTE_TAB_ID: 'tab_id',
  ATTRIBUTE_WASTYPED: 'wasTyped'
};

function addPrivateConstants(WebViewConstants) {
	for (var i in PRIVATE_CONSTANTS) {
		WebViewConstants[i] = PRIVATE_CONSTANTS[i];
	}
}

// Exports.
exports.$set('addPrivateConstants', addPrivateConstants);
