// Copyright (c) 2017-2020 Vivaldi Technologies AS. All rights reserved.
//
// Custom bindings for the vivaldi.windowPrivate API.

var appWindowNatives = requireNative('app_window_natives');

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('create',
      function(name, request, callback, windowParams) {

    // |callback| is optional.
    let maybeCallback = callback || function() {};

    // When window creation fails, windowParams is undefined. Return undefined
    // to the caller.
    if (!windowParams || !windowParams.frameId) {
      maybeCallback(undefined);
      return;
    }

    // We could make our own based on this, but let's use it since it exists.
    let view = appWindowNatives.GetFrame(windowParams.frameId,
                                         true /* notifyBrowser */);

    let windowResult = view ? view : undefined;
    maybeCallback(windowParams.windowId, windowResult);

  });

});
