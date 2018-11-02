// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Custom bindings for the vivaldi.windowPrivate API.

var binding = require('binding').Binding.create('windowPrivate');

function VivaldiWindow(w) {
  this.id_ = w.id;
  this.state = w.state;
};

VivaldiWindow.prototype.isMinimized = function () {
  return Boolean(this.state === "minimized");
};

VivaldiWindow.prototype.isMaximized = function () {
  return Boolean(this.state === "maximized");
};

VivaldiWindow.prototype.isFullscreen = function () {
  return Boolean(this.state === "fullscreen");
};

Object.defineProperty(VivaldiWindow.prototype, 'id', {
  get: function () {
    return this.id_;
  }
});

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('current', function () {
    var win;
    var promise = new Promise(resolve => {
      chrome.windows.getCurrent(w => {
        resolve(w);
      });
    }).then(w => {
      win = new VivaldiWindow(w);
    });
    return win;
  });

  apiFunctions.setHandleRequest('get', (id) => {
    return new Promise(resolve => {
      chrome.windows.get(id, (w) => {
        resolve(w);
      });
    }).then(w => {
      return new VivaldiWindow(w);
    });
  });
});

exports.$set('binding', binding.generate());
exports.$set('VivaldiWindow', VivaldiWindow);
