// VIVALDI

var GuestViewAttributes = require('guestViewAttributes').GuestViewAttributes;
var WebViewConstants = require('webViewConstants').WebViewConstants;

function TabIdAttribute(view) {
  GuestViewAttributes.Attribute.call(this,
    WebViewConstants.ATTRIBUTE_TAB_ID, view);
}

TabIdAttribute.prototype.__proto__ =
  GuestViewAttributes.Attribute.prototype;

TabIdAttribute.prototype.handleMutation = function (oldValue, newValue) {
  // nothing to do here
};

function WindowIdAttribute(view) {
  GuestViewAttributes.Attribute.call(this,
    WebViewConstants.ATTRIBUTE_WINDOW_ID, view);
}

WindowIdAttribute.prototype.__proto__ =
  GuestViewAttributes.Attribute.prototype;

WindowIdAttribute.prototype.handleMutation = function (oldValue, newValue) {
  // nothing to do here
};

function WasTypedAttribute(view) {
  GuestViewAttributes.BooleanAttribute.call(this,
     WebViewConstants.ATTRIBUTE_WASTYPED, view);
  this.wasTyped = true;
}

WasTypedAttribute.prototype.__proto__ =
  GuestViewAttributes.BooleanAttribute.prototype;

WasTypedAttribute.prototype.handleMutation = function (oldValue, newValue) {
  if (!newValue && oldValue) {
    this.wasTyped = (newValue === 'true');
    this.setValueIgnoreMutation(newValue);
  }
};

function ExtensionHostAttribute(view) {
  GuestViewAttributes.Attribute.call(
      this, WebViewConstants.ATTRIBUTE_EXTENSIONHOST, view);
}

ExtensionHostAttribute.prototype.__proto__ =
  GuestViewAttributes.Attribute.prototype;

ExtensionHostAttribute.prototype.handleMutation = function(oldValue, newValue) {
  oldValue = oldValue || '';
  newValue = newValue || '';
  if (oldValue === newValue || !this.view.guest.getId())
    return;

  WebViewPrivate.setExtensionHost(this.view.guest.getId(), newValue);
};

function addPrivateAttributes(athis /* WebViewImpl */) {
  athis.attributes[WebViewConstants.ATTRIBUTE_TAB_ID] =
      new TabIdAttribute(athis);

  athis.attributes[WebViewConstants.ATTRIBUTE_WINDOW_ID] =
      new WindowIdAttribute(athis);

  athis.attributes[WebViewConstants.ATTRIBUTE_WASTYPED] =
      new WasTypedAttribute(athis);

  athis.attributes[WebViewConstants.ATTRIBUTE_EXTENSIONHOST] =
      new ExtensionHostAttribute(athis);
}

// Exports.
exports.$set('addPrivateAttributes', addPrivateAttributes);
