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
}

function ParentTabIdAttribute(view) {
  GuestViewAttributes.Attribute.call(this,
    WebViewConstants.ATTRIBUTE_PARENT_TAB_ID, view);
}

ParentTabIdAttribute.prototype.__proto__ =
  GuestViewAttributes.Attribute.prototype;

ParentTabIdAttribute.prototype.handleMutation = function (oldValue, newValue) {
  // nothing to do here
}

function InspectTabIdAttribute(view) {
  GuestViewAttributes.Attribute.call(this,
    WebViewConstants.ATTRIBUTE_INSPECT_TAB_ID, view);
}

InspectTabIdAttribute.prototype.__proto__ =
  GuestViewAttributes.Attribute.prototype;

InspectTabIdAttribute.prototype.handleMutation = function (oldValue, newValue) {
  // nothing to do here
}

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
}

function ViewtypeAttribute(view) {
  GuestViewAttributes.Attribute.call(this,
    WebViewConstants.ATTRIBUTE_VIEW_TYPE, view);
}

ViewtypeAttribute.prototype.__proto__ = GuestViewAttributes.Attribute.prototype;

function WindowIdAttribute(view) {
  GuestViewAttributes.Attribute.call(this,
    WebViewConstants.ATTRIBUTE_WINDOW_ID, view);
}

WindowIdAttribute.prototype.__proto__ =
  GuestViewAttributes.Attribute.prototype;

function addVivaldiWebViewAttributes(athis) {
  athis.InspectTabIdAttribute = InspectTabIdAttribute;
  athis.TabIdAttribute = TabIdAttribute;
  athis.WasTypedAttribute = WasTypedAttribute;
  athis.ViewtypeAttribute = ViewtypeAttribute;
  athis.WindowIdAttribute = WindowIdAttribute;
  athis.ParentTabIdAttribute = ParentTabIdAttribute;
}

// Exports.
exports.$set('addVivaldiWebViewAttributes', addVivaldiWebViewAttributes);
