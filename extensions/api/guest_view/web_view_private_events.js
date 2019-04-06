// Event management for WebViewPrivate.

var CreateEvent = require('guestViewEvents').CreateEvent;

// A dictionary of <webview> extension events to be listened for. This
// dictionary augments |GuestViewEvents.EVENTS| in guest_view_events.js. See the
// documentation there for details.
var EVENTS_PRIVATE = {
  'fullscreen': {
    evt: CreateEvent('webViewPrivate.onFullscreen'),
    fields: ['enterFullscreen']
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
  'simpleAction': {
    evt: CreateEvent('webViewPrivate.onSimpleAction'),
    fields: ['command', 'text', 'url', 'modifiers']
  },
  'contentblocked': {
    evt: CreateEvent('webViewPrivate.onContentBlocked'),
    fields: ['blockedType']
  },
  'contentscreated': {
    evt: CreateEvent('webViewPrivate.onWebcontentsCreated')
  },
};


function addPrivateEvents(WebViewEvents) {
  for (var i in EVENTS_PRIVATE) {
    WebViewEvents.EVENTS[i] = EVENTS_PRIVATE[i];
  }
}

// Exports.
exports.$set('addPrivateEvents', addPrivateEvents);
