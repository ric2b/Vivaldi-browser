// Event management for WebViewPrivate.

var CreateEvent = require('guestViewEvents').CreateEvent;

// A dictionary of <webview> extension events to be listened for. This
// dictionary augments |GuestViewEvents.EVENTS| in guest_view_events.js. See the
// documentation there for details.
var EVENTS_PRIVATE = {
  'fullscreen': {
    evt: CreateEvent('webViewPrivate.onFullscreen'),
    fields: ['windowId', 'enterFullscreen']
  },
  'sslstatechanged': {
    evt: CreateEvent('webViewPrivate.onSSLStateChanged'),
    fields: ['SSLState', 'issuerstring']
  },
  'targeturlchanged': {
    evt: CreateEvent('webViewPrivate.onTargetURLChanged'),
    fields: ['newUrl']
  },
  'createsearch': {
    evt: CreateEvent('webViewPrivate.onCreateSearch'),
    fields: ['Name','Url']
  },
  'pasteandgo': {
    evt: CreateEvent('webViewPrivate.onPasteAndGo'),
    fields: ['clipBoardText', 'pasteTarget', 'modifiers']
  },
  'contentallowed': {
    evt: CreateEvent('webViewPrivate.onContentAllowed'),
    fields: ['allowedType']
  },
  'contentblocked': {
    evt: CreateEvent('webViewPrivate.onContentBlocked'),
    fields: ['blockedType']
  },
  'contentscreated': {
    evt: CreateEvent('webViewPrivate.onWebcontentsCreated')
  },
  'windowblocked': {
    evt: CreateEvent('webViewPrivate.onWindowBlocked'),
    fields: [
      'targetUrl',
      'name',
      'initialLeft',
      'initialTop',
      'initialWidth',
      'initialHeight']
  },
};


function addPrivateEvents(WebViewEvents) {
  for (var i in EVENTS_PRIVATE) {
    WebViewEvents.EVENTS[i] = EVENTS_PRIVATE[i];
    WebViewEvents.EVENTS[i].__proto__ = null;
  }
}

// Exports.
exports.$set('addPrivateEvents', addPrivateEvents);
