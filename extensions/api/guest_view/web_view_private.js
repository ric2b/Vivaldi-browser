// VIVALDI

if (!apiBridge) {
  exports.$set('WebViewPrivate', require('binding').Binding.create('webViewPrivate').generate());
}
