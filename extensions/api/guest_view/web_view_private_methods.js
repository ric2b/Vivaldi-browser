
// An array of <webview>'s public-facing API methods. Methods without custom
// implementations will be given default implementations that call into the
// internal API method with the same name in |WebViewPrivate|. For example, a
// method called 'someApiMethod' would be given the following default
// implementation:
//
// WebViewImpl.prototype.someApiMethod = function(var_args) {
//   if (!this.guest.getId()) {
//     return false;
//   }
//   var args = $Array.concat([this.guest.getId()], $Array.slice(arguments));
//   $Function.apply(WebViewPrivate.someApiMethod, null, args);
//   return true;
// };
//
// These default implementations come from createDefaultApiMethod() in
// web_view_private_impl.js.
var VIVALDI_WEB_VIEW_API_METHODS = [
  // Vivaldi methods
  'allowBlockedInsecureContent',
  'discardPage',
  'getPageHistory',
  'getThumbnail',
  'setIsFullscreen',
  'showPageInfo',
  'sendRequest',
  'getPageSelection',
];

// Exports.
exports.$set('VIVALDI_WEB_VIEW_API_METHODS', VIVALDI_WEB_VIEW_API_METHODS);
