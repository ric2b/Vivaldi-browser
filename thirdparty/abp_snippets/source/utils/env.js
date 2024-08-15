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

/* global chrome, browser, globalThis */

import {bound} from "proxy-pants/bound";
import {secure} from "proxy-pants/secure";
import {libEnvironment} from "../environment.js";

if (typeof globalThis === "undefined")
  window.globalThis = window;

const {apply, ownKeys} = bound(Reflect);

const worldEnvDefined = "world" in libEnvironment;
const isIsolatedWorld = worldEnvDefined && libEnvironment.world === "ISOLATED";
const isMainWorld = worldEnvDefined && libEnvironment.world === "MAIN";
const isChrome = typeof chrome === "object" && !!chrome.runtime;
const isOtherThanChrome = typeof browser === "object" && !!browser.runtime;
const isExtensionContext = !isMainWorld &&
  (isIsolatedWorld || isChrome || isOtherThanChrome);
const copyIfExtension = value => isExtensionContext ?
  value :
  create(value, getOwnPropertyDescriptors(value));

const {
  create,
  defineProperties,
  defineProperty,
  freeze,
  getOwnPropertyDescriptor,
  getOwnPropertyDescriptors
} = bound(Object);

const invokes = bound(globalThis);
const classes = isExtensionContext ? globalThis : secure(globalThis);
const {Map, RegExp, Set, WeakMap, WeakSet} = classes;

const augment = (source, target, method = null) => {
  const known = ownKeys(target);
  for (const key of ownKeys(source)) {
    if (known.includes(key))
      continue;

    const descriptor = getOwnPropertyDescriptor(source, key);
    if (method && "value" in descriptor) {
      const {value} = descriptor;
      if (typeof value === "function")
        descriptor.value = method(value);
    }
    defineProperty(target, key, descriptor);
  }
};

const primitive = name => {
  const Super = classes[name];
  class Class extends Super {}
  const {toString, valueOf} = Super.prototype;
  defineProperties(Class.prototype, {
    toString: {value: toString},
    valueOf: {value: valueOf}
  });
  const type = name.toLowerCase();
  const method = callback => function() {
    const result = apply(callback, this, arguments);
    return typeof result === type ? new Class(result) : result;
  };
  augment(Super, Class, method);
  augment(Super.prototype, Class.prototype, method);
  return Class;
};

const variables = freeze({
  frozen: new WeakMap(),
  hidden: new WeakSet(),
  iframePropertiesToAbort: {
    read: new Set(),
    write: new Set()
  },
  abortedIframes: new WeakMap()
});

const startsCapitalized = new RegExp("^[A-Z]");

// all default classes/namespaces that must be secured upfront when
// the environment is not executing in an isolated world
export default new Proxy(new Map([
  // custom environment variables
  ["chrome", (
    isExtensionContext && (
      (isChrome && chrome) ||
      (isOtherThanChrome && browser)
    )
  ) || void 0],
  ["isExtensionContext", isExtensionContext],
  ["variables", variables],
  // secured references and classes
  ["console", copyIfExtension(console)],
  ["document", globalThis.document],
  ["performance", copyIfExtension(performance)],
  ["JSON", copyIfExtension(JSON)],
  ["Map", Map],
  ["Math", copyIfExtension(Math)],
  ["Number", isExtensionContext ? Number : primitive("Number")],
  ["RegExp", RegExp],
  ["Set", Set],
  ["String", isExtensionContext ? String : primitive("String")],
  ["WeakMap", WeakMap],
  ["WeakSet", WeakSet],
  // no need to secure but it surely helps if we trust native references
  ["MouseEvent", MouseEvent]
]), {
  get(map, key) {
    if (map.has(key))
      return map.get(key);

    let value = globalThis[key];
    if (typeof value === "function")
      value = (startsCapitalized.test(key) ? classes : invokes)[key];

    map.set(key, value);
    return value;
  },
  has(map, key) {
    return map.has(key);
  }
});
