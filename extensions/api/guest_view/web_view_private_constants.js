// VIVALDI

var PRIVATE_CONSTANTS = {
  ATTRIBUTE_EXTENSIONHOST: 'extensionhost',
  ATTRIBUTE_INSPECT_TAB_ID: 'inspect_tab_id',
  ATTRIBUTE_TAB_ID: 'tab_id',
  ATTRIBUTE_USE_NORMAL_PROFILE: 'use_normal_profile',
  ATTRIBUTE_WASTYPED: 'wasTyped'
};

function addPrivateConstants(WebViewConstants) {
	for (var i in PRIVATE_CONSTANTS) {
		WebViewConstants[i] = PRIVATE_CONSTANTS[i];
	}
}

// Exports.
exports.$set('addPrivateConstants', addPrivateConstants);
