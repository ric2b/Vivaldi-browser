/*!
 * This file is part of eyeo's Anti-Circumvention Snippets module (@eyeo/snippets),
 * Copyright (C) 2006-present eyeo GmbH
 * 
 * @eyeo/snippets is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 * 
 * @eyeo/snippets is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with @eyeo/snippets.  If not, see <http://www.gnu.org/licenses/>.
 */

import $ from "../$.js";
import {accessor} from "proxy-pants/accessor";
import {apply, call} from "proxy-pants/function";
import {hasOwnProperty} from "proxy-pants/object";

import {getDebugger} from "../introspection/log.js";
import {randomId} from "./general.js";

let {
  parseFloat,
  variables,
  Array,
  Error,
  Map,
  Object,
  ReferenceError,
  Set,
  WeakMap
} = $(window);

let {onerror} = accessor(window);

let NodeProto = Node.prototype;
let ElementProto = Element.prototype;

let propertyAccessors = null;

export function wrapPropertyAccess(object, property, descriptor,
                                   setConfigurable = true) {
  let $property = $(property);
  let dotIndex = $property.indexOf(".");
  if (dotIndex == -1) {
    // simple property case.
    let currentDescriptor = Object.getOwnPropertyDescriptor(object, property);
    if (currentDescriptor && !currentDescriptor.configurable)
      return;

    // Keep it configurable because the same property can be wrapped via
    // multiple snippet filters (#7373).
    let newDescriptor = Object.assign({}, descriptor, {
      configurable: setConfigurable
    });

    if (!currentDescriptor && !newDescriptor.get && newDescriptor.set) {
      let propertyValue = object[property];
      newDescriptor.get = () => propertyValue;
    }

    Object.defineProperty(object, property, newDescriptor);
    return;
  }

  let name = $property.slice(0, dotIndex).toString();
  property = $property.slice(dotIndex + 1).toString();
  let value = object[name];
  if (value && (typeof value == "object" || typeof value == "function"))
    wrapPropertyAccess(value, property, descriptor);

  let currentDescriptor = Object.getOwnPropertyDescriptor(object, name);
  if (currentDescriptor && !currentDescriptor.configurable)
    return;

  // lazy initialization (reduced heap)
  if (!propertyAccessors)
    propertyAccessors = new WeakMap();

  // allow branched properties that might not exist yet
  if (!propertyAccessors.has(object))
    propertyAccessors.set(object, new Map());

  // if the name is already known, simply add the descriptor
  // to the sub-brnach for the property
  let properties = propertyAccessors.get(object);
  if (properties.has(name)) {
    properties.get(name).set(property, descriptor);
    return;
  }

  // in every other case just create the branch and set
  // the accessor only once for the very same name.
  let toBeWrapped = new Map([[property, descriptor]]);
  properties.set(name, toBeWrapped);
  Object.defineProperty(object, name, {
    get: () => value,
    set(newValue) {
      value = newValue;
      if (value && (typeof value == "object" || typeof value == "function")) {
        // loop through all branches to avoid loosing/overwriting previously
        // set ones
        for (let [prop, desc] of toBeWrapped)
          wrapPropertyAccess(value, prop, desc);
      }
    },
    configurable: setConfigurable
  });
}

/**
 * Overrides the `onerror` handler to discard tagged error messages from our
 * property wrapping.
 *
 * @param {string} magic The magic string that tags the error message.
 * @private
 */
export function overrideOnError(magic) {
  let prev = onerror();
  onerror((...args) => {
    let message = args.length && args[0];
    if (typeof message == "string" && $(message).includes(magic))
      return true;
    if (typeof prev == "function")
      return apply(prev, this, args);
  });
}

/**
 * Patches a property on the `context` object to abort execution when the
 * property is read.
 *
 * @param {string} loggingPrefix A string with which we prefix the logs.
 * @param {Window} context The window object whose property we patch.
 * @param {string} property The name of the property.
 * @param {boolean} setConfigurable Value of the configurable attribute.
 * @private
 */
export function abortOnRead(loggingPrefix, context, property,
                            setConfigurable = true) {
  let debugLog = getDebugger(loggingPrefix);

  if (!property) {
    debugLog("error", "no property to abort on read");
    return;
  }

  let rid = randomId();

  function abort() {
    debugLog("success", `${property} access aborted`);
    throw new ReferenceError(rid);
  }

  debugLog("info", `aborting on ${property} access`);

  wrapPropertyAccess(context,
                     property,
                     {get: abort, set() {}},
                     setConfigurable);
  overrideOnError(rid);
}

/**
 * Patches a property on the `context` object to abort execution when the
 * property is written.
 *
 * @param {string} loggingPrefix A string with which we prefix the logs.
 * @param {Window} context The window object whose property we patch.
 * @param {string} property The name of the property.
 * @param {boolean} setConfigurable Value of the configurable attribute.
 * @private
 */
export function abortOnWrite(loggingPrefix, context, property,
                             setConfigurable = true) {
  let debugLog = getDebugger(loggingPrefix);

  if (!property) {
    debugLog("error", "no property to abort on write");
    return;
  }

  let rid = randomId();

  function abort() {
    debugLog("success", `setting ${property} aborted`);
    throw new ReferenceError(rid);
  }

  debugLog("info", `aborting when setting ${property}`);

  wrapPropertyAccess(context, property, {set: abort}, setConfigurable);
  overrideOnError(rid);
}

/**
 * Patches a list of properties on the iframes' window object to abort execution
 * when the property is read/written.
 *
 * @param {...string} properties The list with the properties.
 * @param {boolean?} [abortRead=false] Should abort on read option.
 * @param {boolean?} [abortWrite=false] Should abort on write option.
 * @private
 */
export function abortOnIframe(
  properties,
  abortRead = false,
  abortWrite = false
) {
  let abortedIframes = variables.abortedIframes;
  let iframePropertiesToAbort = variables.iframePropertiesToAbort;

  // add new properties-to-abort to all aborted iframes' WeakMaps
  for (let frame of Array.from(window.frames)) {
    if (abortedIframes.has(frame)) {
      for (let property of properties) {
        if (abortRead)
          abortedIframes.get(frame).read.add(property);
        if (abortWrite)
          abortedIframes.get(frame).write.add(property);
      }
    }
  }

  // store properties-to-abort
  for (let property of properties) {
    if (abortRead)
      iframePropertiesToAbort.read.add(property);
    if (abortWrite)
      iframePropertiesToAbort.write.add(property);
  }

  queryAndProxyIframe();
  if (!abortedIframes.has(document)) {
    abortedIframes.set(document, true);
    addHooksOnDomAdditions(queryAndProxyIframe);
  }

  function queryAndProxyIframe() {
    for (let frame of Array.from(window.frames)) {
      // add WeakMap entry for every missing frame
      if (!abortedIframes.has(frame)) {
        abortedIframes.set(frame, {
          read: new Set(iframePropertiesToAbort.read),
          write: new Set(iframePropertiesToAbort.write)
        });
      }

      let readProps = abortedIframes.get(frame).read;
      if (readProps.size > 0) {
        let props = Array.from(readProps);
        readProps.clear();
        for (let property of props)
          abortOnRead("abort-on-iframe-property-read", frame, property);
      }

      let writeProps = abortedIframes.get(frame).write;
      if (writeProps.size > 0) {
        let props = Array.from(writeProps);
        writeProps.clear();
        for (let property of props)
          abortOnWrite("abort-on-iframe-property-write", frame, property);
      }
    }
  }
}

/**
 * Patches the native functions which are responsible with adding Nodes to DOM.
 * Adds a hook at right after the addition.
 *
 * @param {function} endCallback The list with the properties.
 * @private
 */
function addHooksOnDomAdditions(endCallback) {
  let descriptor;

  wrapAccess(NodeProto, ["appendChild", "insertBefore", "replaceChild"]);
  wrapAccess(ElementProto, ["append", "prepend", "replaceWith", "after",
                            "before", "insertAdjacentElement",
                            "insertAdjacentHTML"]);

  descriptor = getInnerHTMLDescriptor(ElementProto, "innerHTML");
  wrapPropertyAccess(ElementProto, "innerHTML", descriptor);

  descriptor = getInnerHTMLDescriptor(ElementProto, "outerHTML");
  wrapPropertyAccess(ElementProto, "outerHTML", descriptor);

  function wrapAccess(prototype, names) {
    for (let name of names) {
      let desc = getAppendChildDescriptor(prototype, name);
      wrapPropertyAccess(prototype, name, desc);
    }
  }

  function getAppendChildDescriptor(target, property) {
    let currentValue = target[property];
    return {
      get() {
        return function(...args) {
          let result;
          result = apply(currentValue, this, args);
          endCallback && endCallback();
          return result;
        };
      }
    };
  }

  function getInnerHTMLDescriptor(target, property) {
    let desc = Object.getOwnPropertyDescriptor(target, property);
    let {set: prevSetter} = desc || {};
    return {
      set(val) {
        let result;
        result = call(prevSetter, this, val);
        endCallback && endCallback();
        return result;
      }
    };
  }
}

let {Object: NativeObject} = window;
export function findOwner(root, path) {
  if (!(root instanceof NativeObject))
    return;

  let object = root;
  let chain = $(path).split(".");

  if (chain.length === 0)
    return;

  for (let i = 0; i < chain.length - 1; i++) {
    let prop = chain[i];
    // eslint-disable-next-line no-prototype-builtins
    if (!hasOwnProperty(object, prop))
      return;

    object = object[prop];

    if (!(object instanceof NativeObject))
      return;
  }

  let prop = chain[chain.length - 1];
  // eslint-disable-next-line no-prototype-builtins
  if (hasOwnProperty(object, prop))
    return [object, prop];
}

// TBD: should this accept floating numbers too?
const decimals = $(/^\d+$/);

export function overrideValue(value) {
  switch (value) {
    case "false":
      return false;
    case "true":
      return true;
    case "null":
      return null;
    case "noopFunc":
      return () => {};
    case "trueFunc":
      return () => true;
    case "falseFunc":
      return () => false;
    case "emptyArray":
      return [];
    case "emptyObj":
      return {};
    case "undefined":
      return void 0;
    case "":
      return value;
    default:
      if (decimals.test(value))
        return parseFloat(value);

      throw new Error("[override-property-read snippet]: " +
                      `Value "${value}" is not valid.`);
  }
}

function getPromiseFromEvent(item, event) {
  return new Promise(
    resolve => {
      const listener = () => {
        item.removeEventListener(event, listener);
        resolve();
      };
      item.addEventListener(event, listener);
    }
  );
}

/**
 * Waits until the website is at the given state before running the
 * snippet main logic function.
 *
 * @param {function} debugLog debugLog function of the calling snippet
 * @param {function} mainLogic The function that will be run.
 * @param {string} waitUntil The event that will be used to delay the running
 * of mainLogic function. Could be one of the document states, window load
 * or any arbitrary event.
 * Accepts: ['interactive', 'ready', 'load', or any event on document ]
 * @private
 */
export function waitUntilEvent(
  debugLog,
  mainLogic,
  waitUntil) {
  if (waitUntil) {
    // waitUntil = load, wait until window.load
    if (waitUntil === "load") {
      debugLog("info", "Waiting until window.load");
      // If load is given wait for window.load
      window.addEventListener("load", () => {
        debugLog("info", "Window.load fired.");
        mainLogic();
      });
    }
    // waitUntil document readyStateChange
    else if (waitUntil === "loading" ||
            waitUntil === "interactive" ||
            waitUntil === "complete") {
      debugLog("info", "Waiting document state until :", waitUntil);
      // loading, interactive, complete
      document.addEventListener("readystatechange", () => {
        debugLog("info", "Document state changed:", document.readyState);
        if (document.readyState === waitUntil)
          mainLogic();
      });
    }
    // waitUntil is something else, assume it's an event
    else {
      debugLog("info",
               "Waiting until ",
               waitUntil,
               " event is triggered on document");
      getPromiseFromEvent(document, waitUntil).then(() => {
        debugLog("info",
                 waitUntil,
                 " is triggered on document, starting the snippet");
        mainLogic();
      }).catch(err => {
        debugLog("error",
                 "There was an error while waiting for the event.",
                 err);
      });
    }
  }
  else {
    // If waitUntil is not given, directly start the snippet.
    mainLogic();
  }
}
