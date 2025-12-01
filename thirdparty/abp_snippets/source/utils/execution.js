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
import {formatArguments, randomId, toRegExp} from "./general.js";

let {
  parseFloat,
  variables,
  clearTimeout,
  fetch,
  setTimeout,
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
 * @param {string} formattedProperties pre-formatted logging helper
 * @param {boolean} setConfigurable Value of the configurable attribute.
 * @private
 */
export function abortOnRead(loggingPrefix, context,
                            property, formattedProperties = "",
                            setConfigurable = true) {
  let debugLog = getDebugger(loggingPrefix);

  if (!property) {
    debugLog("error", "no property to abort on read");
    return;
  }

  let rid = randomId();

  function abort() {
    debugLog("success", `${property} access aborted`, `\nFILTER: ${loggingPrefix} ${formattedProperties}`);
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
 * @param {string} formattedProperties pre-formatted logging helper
 * @param {boolean} setConfigurable Value of the configurable attribute.
 * @private
 */
export function abortOnWrite(loggingPrefix,
                             context, property,
                             formattedProperties = "",
                             setConfigurable = true) {
  let debugLog = getDebugger(loggingPrefix);

  if (!property) {
    debugLog("error", "no property to abort on write");
    return;
  }

  let rid = randomId();

  function abort() {
    debugLog("success", `setting ${property} aborted`, `\nFILTER: ${loggingPrefix} ${formattedProperties}`);
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
  // Map each properth to a string format for logging purposes
  const formattedPropertiesToLog = formatArguments(properties);

  // add new properties-to-abort to all aborted iframes' WeakMaps
  for (let frame of Array.from(window.frames)) {
    if (abortedIframes.has(frame)) {
      for (let property of properties) {
        if (abortRead)
          // eslint-disable-next-line max-len
          abortedIframes.get(frame).read.add({property, formattedProperties: formattedPropertiesToLog});
        if (abortWrite)
          // eslint-disable-next-line max-len
          abortedIframes.get(frame).write.add({property, formattedProperties: formattedPropertiesToLog});
      }
    }
  }

  // store properties-to-abort
  for (let property of properties) {
    if (abortRead)
      // eslint-disable-next-line max-len
      iframePropertiesToAbort.read.add({property, formattedProperties: formattedPropertiesToLog});
    if (abortWrite)
      // eslint-disable-next-line max-len
      iframePropertiesToAbort.write.add({property, formattedProperties: formattedPropertiesToLog});
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
        for (let {property, formattedProperties} of props) {
          abortOnRead("abort-on-iframe-property-read",
                      frame,
                      property,
                      formattedProperties);
        }
      }

      let writeProps = abortedIframes.get(frame).write;
      if (writeProps.size > 0) {
        let props = Array.from(writeProps);
        writeProps.clear();
        for (let {property, formattedProperties} of props) {
          abortOnWrite("abort-on-iframe-property-write",
                       frame,
                       property,
                       formattedProperties);
        }
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

/**
 * Checks if the current stack trace matches a given array of strings.
 * It captures the current stack trace by creating a new Error
 * and normalizes it by:
 * - Removing the "at" prefix from each line.
 * - Extracting the function name, URL, and line number from each stack
 * trace line.
 * - Replacing known patterns like inline or anonymous scripts with a
 * readable label.
 * - Ignoring lines that do not contain a resource name or line position.
 * - Prepending a `stackDepth` label that indicates the number of meaningful
 * lines in the stack.
 *
 * The stack trace is transformed into a single string where each relevant
 * line is in a new line (`\n`), in the following format:
 *   stackDepth:<number of lines> <functionName> <url>:<lineNumber>:1
 *
 * Each string in the `stackNeedle` array is converted to a regex and tested
 * against the normalized stack trace.
 *
 * @param {Array<string>} stackNeedle An array of strings to be converted
 *  to regex and checked in the normalized stack trace.
 * @param {Function} debugLog The debug logging function
 * @returns {boolean} True if any of the `stackNeedle` patterns match the
 *  normalized stack trace, otherwise false.
 */
export function matchesStackTrace(stackNeedle, debugLog) {
  if (!stackNeedle || !stackNeedle.length)
    return true; // If no stack needle specified, always match

  const token = randomId();
  const error = new Error(token);

  const locHref = new URL(self.location.href);
  locHref.hash = "";

  const lineRegex = /(.*?@)?(\S+)(:\d+):\d+\)?$/;
  const lines = [];
  for (let line of error.stack.split(/[\n\r]+/)) {
    if ($(line).includes(token))
      continue;

    line = $(line).trim();
    const match = $(lineRegex).exec(line);
    if (match === null)
      continue;

    let url = match[2];
    if ($(url).startsWith("("))
      url = $(url).slice(1);

    if (url === locHref.href)
      url = "inlineScript";
    else if ($(url).startsWith("<anonymous>"))
      url = "injectedScript";

    let functionName = match[1] ?
      $(match[1]).slice(0, -1) :
      $(line).slice(0, $(match).index).trim();

    if ($(functionName).startsWith("at"))
      functionName = $(functionName).slice(2).trim();

    let linePosition = match[3];
    $(lines).push(" " + `${functionName} ${url}${linePosition}:1`.trim());
  }

  lines[0] = `stackDepth:${lines.length - 1}`;
  const normalizedStack = $(lines).join("\n");

  for (let needle of stackNeedle) {
    const regex = toRegExp(needle);
    if (regex.test(normalizedStack)) {
      debugLog("info", `Found needle in stack trace: ${needle}`);
      return true;
    }
  }

  debugLog("info", `Stack trace does not match any needle. Stack trace: ${normalizedStack}`);
  return false;
}

/**
 * @typedef {object} FetchContentInfo
 * @property {function} remove
 * @property {Promise} result
 * @property {number} timer
 * @private
 */

/**
 * @type {Map.<string, FetchContentInfo>}
 * @private
 */
let fetchContentMap = new Map();


/**
 * Returns a potentially already resolved fetch auto cleaning, if not requested
 * again, after a certain amount of milliseconds.
 *
 * The resolved fetch is by default `arrayBuffer` but it can be any other kind
 * through the configuration object.
 *
 * @param {string} url The url to fetch
 * @param {object} [options] Optional configuration options.
 *                            By default is {as: "arrayBuffer", cleanup: 60000}
 * @param {string} [options.as] The fetch type: "arrayBuffer", "json", "text"..
 * @param {number} [options.cleanup] The cache auto-cleanup delay in ms: 60000
 *
 * @returns {Promise} The fetched result as Uint8Array|string.
 *
 * @example
 * fetchContent('https://any.url.com').then(arrayBuffer => { ... })
 * @example
 * fetchContent('https://a.com', {as: 'json'}).then(json => { ... })
 * @example
 * fetchContent('https://a.com', {as: 'text'}).then(text => { ... })
 * @private
 */
export function fetchContent(url, {as = "arrayBuffer", cleanup = 60000} = {}) {
  // make sure the fetch type is unique as the url fetching text or arrayBuffer
  // will fetch same url twice but it will resolve it as expected instead of
  // keeping the fetch potentially hanging forever.
  let uid = as + ":" + url;
  let details = fetchContentMap.get(uid) || {
    remove: () => fetchContentMap.delete(uid),
    result: null,
    timer: 0
  };
  clearTimeout(details.timer);
  details.timer = setTimeout(details.remove, cleanup);
  if (!details.result) {
    details.result = fetch(url).then(res => res[as]()).catch(details.remove);
    fetchContentMap.set(uid, details);
  }
  return details.result;
}
