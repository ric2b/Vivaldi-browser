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

const callback = (environment, ...filters) => {
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
  const $$1 = Proxy;

  const {apply: a, bind: b, call: c} = Function;
  const apply$2 = c.bind(a);
  const bind = c.bind(b);
  const call = c.bind(c);

  const callerHandler = {
    get(target, name) {
      return bind(c, target[name]);
    }
  };

  const caller = target => new $$1(target, callerHandler);

  const proxy = (source, target) => new $$1(source, {
    apply: (_, self, args) => apply$2(target, self, args)
  });

  const handler$2 = {
    get(target, name) {
      return bind(target[name], target);
    }
  };

  const bound = target => new $$1(target, handler$2);

  const {
    assign: assign$1,
    defineProperties: defineProperties$1,
    freeze: freeze$1,
    getOwnPropertyDescriptor: getOwnPropertyDescriptor$3,
    getOwnPropertyDescriptors: getOwnPropertyDescriptors$1,
    getPrototypeOf
  } = bound(Object);

  const {hasOwnProperty} = caller({});

  const {species} = Symbol;

  const handler$1 = {
    get(target, name) {
      const Native = target[name];
      class Secure extends Native {}

      const proto = getOwnPropertyDescriptors$1(Native.prototype);
      delete proto.constructor;
      freeze$1(defineProperties$1(Secure.prototype, proto));

      const statics = getOwnPropertyDescriptors$1(Native);
      delete statics.length;
      delete statics.prototype;
      statics[species] = {value: Secure};
      return freeze$1(defineProperties$1(Secure, statics));
    }
  };

  const secure = target => new $$1(target, handler$1);

  const libEnvironment = typeof environment !== "undefined" ? environment :
                                                                     {};

  if (typeof globalThis === "undefined")
    window.globalThis = window;

  const {apply: apply$1, ownKeys} = bound(Reflect);

  const worldEnvDefined = "world" in libEnvironment;
  const isIsolatedWorld = worldEnvDefined && libEnvironment.world === "ISOLATED";
  const isMainWorld = worldEnvDefined && libEnvironment.world === "MAIN";
  const isChrome = typeof chrome === "object" && !!chrome.runtime;
  const isOtherThanChrome = typeof browser === "object" && !!browser.runtime;
  const isExtensionContext$2 = !isMainWorld &&
    (isIsolatedWorld || isChrome || isOtherThanChrome);
  const copyIfExtension = value => isExtensionContext$2 ?
    value :
    create(value, getOwnPropertyDescriptors(value));

  const {
    create,
    defineProperties,
    defineProperty,
    freeze,
    getOwnPropertyDescriptor: getOwnPropertyDescriptor$2,
    getOwnPropertyDescriptors
  } = bound(Object);

  const invokes = bound(globalThis);
  const classes = isExtensionContext$2 ? globalThis : secure(globalThis);
  const {Map: Map$8, RegExp: RegExp$1, Set: Set$2, WeakMap: WeakMap$4, WeakSet: WeakSet$3} = classes;

  const augment = (source, target, method = null) => {
    const known = ownKeys(target);
    for (const key of ownKeys(source)) {
      if (known.includes(key))
        continue;

      const descriptor = getOwnPropertyDescriptor$2(source, key);
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
      const result = apply$1(callback, this, arguments);
      return typeof result === type ? new Class(result) : result;
    };
    augment(Super, Class, method);
    augment(Super.prototype, Class.prototype, method);
    return Class;
  };

  const variables$3 = freeze({
    frozen: new WeakMap$4(),
    hidden: new WeakSet$3(),
    iframePropertiesToAbort: {
      read: new Set$2(),
      write: new Set$2()
    },
    abortedIframes: new WeakMap$4()
  });

  const startsCapitalized = new RegExp$1("^[A-Z]");

  var env = new Proxy(new Map$8([

    ["chrome", (
      isExtensionContext$2 && (
        (isChrome && chrome) ||
        (isOtherThanChrome && browser)
      )
    ) || void 0],
    ["isExtensionContext", isExtensionContext$2],
    ["variables", variables$3],

    ["console", copyIfExtension(console)],
    ["document", globalThis.document],
    ["performance", copyIfExtension(performance)],
    ["JSON", copyIfExtension(JSON)],
    ["Map", Map$8],
    ["Math", copyIfExtension(Math)],
    ["Number", isExtensionContext$2 ? Number : primitive("Number")],
    ["RegExp", RegExp$1],
    ["Set", Set$2],
    ["String", isExtensionContext$2 ? String : primitive("String")],
    ["WeakMap", WeakMap$4],
    ["WeakSet", WeakSet$3],

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

  class WeakValue {
    has() { return false; }
    set() {}
  }

  const helpers = {WeakSet, WeakMap, WeakValue};
  const {apply} = Reflect;

  function transformOnce (callback) {  const {WeakSet, WeakMap, WeakValue} = (this || helpers);
    const ws = new WeakSet;
    const wm = new WeakMap;
    const wv = new WeakValue;
    return function (any) {
      if (ws.has(any))
        return any;

      if (wm.has(any))
        return wm.get(any);

      if (wv.has(any))
        return wv.get(any);

      const value = apply(callback, this, arguments);
      ws.add(value);
      if (value !== any)
        (typeof any === 'object' && any ? wm : wv).set(any, value);
      return value;
    };
  }

  const {Map: Map$7, WeakMap: WeakMap$3, WeakSet: WeakSet$2, setTimeout} = env;

  let cleanup = true;
  let cleanUpCallback = map => {
    map.clear();
    cleanup = !cleanup;
  };

  var transformer = transformOnce.bind({
    WeakMap: WeakMap$3,
    WeakSet: WeakSet$2,

    WeakValue: class extends Map$7 {
      set(key, value) {
        if (cleanup) {
          cleanup = !cleanup;
          setTimeout(cleanUpCallback, 0, this);
        }
        return super.set(key, value);
      }
    }
  });

  const {concat, includes, join, reduce, unshift} = caller([]);

  const globals = secure(globalThis);

  const {
    Map: Map$6,
    WeakMap: WeakMap$2
  } = globals;

  const map = new Map$6;
  const descriptors = target => {
    const chain = [];
    let current = target;
    while (current) {
      if (map.has(current))
        unshift(chain, map.get(current));
      else {
        const descriptors = getOwnPropertyDescriptors$1(current);
        map.set(current, descriptors);
        unshift(chain, descriptors);
      }
      current = getPrototypeOf(current);
    }
    unshift(chain, {});
    return apply$2(assign$1, null, chain);
  };

  const chain = source => {
    const target = typeof source === 'function' ? source.prototype : source;
    const chained = descriptors(target);
    const handler = {
      get(target, key) {
        if (key in chained) {
          const {value, get} = chained[key];
          if (get)
            return call(get, target);
          if (typeof value === 'function')
            return bind(value, target);
        }
        return target[key];
      },
      set(target, key, value) {
        if (key in chained) {
          const {set} = chained[key];
          if (set) {
            call(set, target, value);
            return true;
          }
        }
        target[key] = value;
        return true;
      }
    };
    return target => new $$1(target, handler);
  };

  const {
    isExtensionContext: isExtensionContext$1,
    Array: Array$2,
    Number: Number$1,
    String: String$1,
    Object: Object$9
  } = env;

  const {isArray} = Array$2;
  const {getOwnPropertyDescriptor: getOwnPropertyDescriptor$1, setPrototypeOf: setPrototypeOf$1} = Object$9;

  const {toString: toString$1} = Object$9.prototype;
  const {slice} = String$1.prototype;
  const getBrand = value => call(slice, call(toString$1, value), 8, -1);

  const {get: nodeType} = getOwnPropertyDescriptor$1(Node.prototype, "nodeType");

  const chained = isExtensionContext$1 ? {} : {
    Attr: chain(Attr),
    CanvasRenderingContext2D: chain(CanvasRenderingContext2D),
    CSSStyleDeclaration: chain(CSSStyleDeclaration),
    Document: chain(Document),
    Element: chain(Element),
    HTMLCanvasElement: chain(HTMLCanvasElement),
    HTMLElement: chain(HTMLElement),
    HTMLImageElement: chain(HTMLImageElement),
    HTMLScriptElement: chain(HTMLScriptElement),
    MutationRecord: chain(MutationRecord),
    Node: chain(Node),
    ShadowRoot: chain(ShadowRoot),

    get CSS2Properties() {
      return chained.CSSStyleDeclaration;
    }
  };

  const upgrade = (value, hint) => {
    if (hint !== "Element" && hint in chained)
      return chained[hint](value);

    if (isArray(value))
      return setPrototypeOf$1(value, Array$2.prototype);

    const brand = getBrand(value);
    if (brand in chained)
      return chained[brand](value);

    if (brand in env)
      return setPrototypeOf$1(value, env[brand].prototype);

    if ("nodeType" in value) {
      switch (call(nodeType, value)) {
        case 1:
          if (!(hint in chained))
            throw new Error("unknown hint " + hint);
          return chained[hint](value);
        case 2:
          return chained.Attr(value);
        case 3:
          return chained.Node(value);
        case 9:
          return chained.Document(value);
      }
    }

    throw new Error("unknown brand " + brand);
  };

  var $ = isExtensionContext$1 ?
    value => (value === window || value === globalThis ? env : value) :
    transformer((value, hint = "Element") => {
      if (value === window || value === globalThis)
        return env;

      switch (typeof value) {
        case "object":
          return value && upgrade(value, hint);

        case "string":
          return new String$1(value);

        case "number":
          return new Number$1(value);

        default:
          throw new Error("unsupported value");
      }
    });

  const handler = {
    get(target, name) {
      const context = target;
      while (!hasOwnProperty(target, name))
        target = getPrototypeOf(target);
      const {get, set} = getOwnPropertyDescriptor$3(target, name);
      return function () {
        return arguments.length ?
                apply$2(set, context, arguments) :
                call(get, context);
      };
    }
  };

  const accessor = target => new $$1(target, handler);

  let debugging = false;

  function debug() {
    return debugging;
  }

  function setDebug() {
    debugging = true;
  }

  const {console: console$4} = $(window);

  const noop = () => {};

  function log(...args) {
    if (debug()) {
      const logArgs = ["%c DEBUG", "font-weight: bold;"];

      const isErrorIndex = args.indexOf("error");
      const isWarnIndex = args.indexOf("warn");
      const isSuccessIndex = args.indexOf("success");
      const isInfoIndex = args.indexOf("info");

      if (isErrorIndex !== -1) {
        logArgs[0] += " - ERROR";
        logArgs[1] += "color: red; border:2px solid red";
        $(args).splice(isErrorIndex, 1);
      }
      else if (isWarnIndex !== -1) {
        logArgs[0] += " - WARNING";
        logArgs[1] += "color: orange; border:2px solid orange ";
        $(args).splice(isWarnIndex, 1);
      }
      else if (isSuccessIndex !== -1) {
        logArgs[0] += " - SUCCESS";
        logArgs[1] += "color: green; border:2px solid green";
        $(args).splice(isSuccessIndex, 1);
      }
      else if (isInfoIndex !== -1) {
        logArgs[1] += "color: black;";
        $(args).splice(isInfoIndex, 1);
      }

      $(args).unshift(...logArgs);
    }
    console$4.log(...args);
  }

  function getDebugger(name) {
    return bind(debug() ? log : noop, null, name);
  }

  let {Math: Math$1, RegExp} = $(window);

  function regexEscape(string) {
    return $(string).replace(/[-/\\^$*+?.()|[\]{}]/g, "\\$&");
  }

  function toRegExp(pattern) {
    let {length} = pattern;

    if (length > 1 && pattern[0] === "/") {
      let isCaseSensitive = pattern[length - 1] === "/";

      if (isCaseSensitive || (length > 2 && $(pattern).endsWith("/i"))) {
        let args = [$(pattern).slice(1, isCaseSensitive ? -1 : -2)];
        if (!isCaseSensitive)
          args.push("i");

        return new RegExp(...args);
      }
    }

    return new RegExp(regexEscape(pattern));
  }

  function randomId() {

    return $(Math$1.floor(Math$1.random() * 2116316160 + 60466176)).toString(36);
  }

  let {
    parseFloat,
    variables: variables$2,
    Array: Array$1,
    Error: Error$7,
    Map: Map$5,
    Object: Object$8,
    ReferenceError: ReferenceError$2,
    Set: Set$1,
    WeakMap: WeakMap$1
  } = $(window);

  let {onerror} = accessor(window);

  let NodeProto$1 = Node.prototype;
  let ElementProto$2 = Element.prototype;

  let propertyAccessors = null;

  function wrapPropertyAccess(object, property, descriptor,
                                     setConfigurable = true) {
    let $property = $(property);
    let dotIndex = $property.indexOf(".");
    if (dotIndex == -1) {

      let currentDescriptor = Object$8.getOwnPropertyDescriptor(object, property);
      if (currentDescriptor && !currentDescriptor.configurable)
        return;

      let newDescriptor = Object$8.assign({}, descriptor, {
        configurable: setConfigurable
      });

      if (!currentDescriptor && !newDescriptor.get && newDescriptor.set) {
        let propertyValue = object[property];
        newDescriptor.get = () => propertyValue;
      }

      Object$8.defineProperty(object, property, newDescriptor);
      return;
    }

    let name = $property.slice(0, dotIndex).toString();
    property = $property.slice(dotIndex + 1).toString();
    let value = object[name];
    if (value && (typeof value == "object" || typeof value == "function"))
      wrapPropertyAccess(value, property, descriptor);

    let currentDescriptor = Object$8.getOwnPropertyDescriptor(object, name);
    if (currentDescriptor && !currentDescriptor.configurable)
      return;

    if (!propertyAccessors)
      propertyAccessors = new WeakMap$1();

    if (!propertyAccessors.has(object))
      propertyAccessors.set(object, new Map$5());

    let properties = propertyAccessors.get(object);
    if (properties.has(name)) {
      properties.get(name).set(property, descriptor);
      return;
    }

    let toBeWrapped = new Map$5([[property, descriptor]]);
    properties.set(name, toBeWrapped);
    Object$8.defineProperty(object, name, {
      get: () => value,
      set(newValue) {
        value = newValue;
        if (value && (typeof value == "object" || typeof value == "function")) {

          for (let [prop, desc] of toBeWrapped)
            wrapPropertyAccess(value, prop, desc);
        }
      },
      configurable: setConfigurable
    });
  }

  function overrideOnError(magic) {
    let prev = onerror();
    onerror((...args) => {
      let message = args.length && args[0];
      if (typeof message == "string" && $(message).includes(magic))
        return true;
      if (typeof prev == "function")
        return apply$2(prev, this, args);
    });
  }

  function abortOnRead(loggingPrefix, context, property,
                              setConfigurable = true) {
    let debugLog = getDebugger(loggingPrefix);

    if (!property) {
      debugLog("error", "no property to abort on read");
      return;
    }

    let rid = randomId();

    function abort() {
      debugLog("success", `${property} access aborted`);
      throw new ReferenceError$2(rid);
    }

    debugLog("info", `aborting on ${property} access`);

    wrapPropertyAccess(context,
                       property,
                       {get: abort, set() {}},
                       setConfigurable);
    overrideOnError(rid);
  }

  function abortOnWrite(loggingPrefix, context, property,
                               setConfigurable = true) {
    let debugLog = getDebugger(loggingPrefix);

    if (!property) {
      debugLog("error", "no property to abort on write");
      return;
    }

    let rid = randomId();

    function abort() {
      debugLog("success", `setting ${property} aborted`);
      throw new ReferenceError$2(rid);
    }

    debugLog("info", `aborting when setting ${property}`);

    wrapPropertyAccess(context, property, {set: abort}, setConfigurable);
    overrideOnError(rid);
  }

  function abortOnIframe(
    properties,
    abortRead = false,
    abortWrite = false
  ) {
    let abortedIframes = variables$2.abortedIframes;
    let iframePropertiesToAbort = variables$2.iframePropertiesToAbort;

    for (let frame of Array$1.from(window.frames)) {
      if (abortedIframes.has(frame)) {
        for (let property of properties) {
          if (abortRead)
            abortedIframes.get(frame).read.add(property);
          if (abortWrite)
            abortedIframes.get(frame).write.add(property);
        }
      }
    }

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
      for (let frame of Array$1.from(window.frames)) {

        if (!abortedIframes.has(frame)) {
          abortedIframes.set(frame, {
            read: new Set$1(iframePropertiesToAbort.read),
            write: new Set$1(iframePropertiesToAbort.write)
          });
        }

        let readProps = abortedIframes.get(frame).read;
        if (readProps.size > 0) {
          let props = Array$1.from(readProps);
          readProps.clear();
          for (let property of props)
            abortOnRead("abort-on-iframe-property-read", frame, property);
        }

        let writeProps = abortedIframes.get(frame).write;
        if (writeProps.size > 0) {
          let props = Array$1.from(writeProps);
          writeProps.clear();
          for (let property of props)
            abortOnWrite("abort-on-iframe-property-write", frame, property);
        }
      }
    }
  }

  function addHooksOnDomAdditions(endCallback) {
    let descriptor;

    wrapAccess(NodeProto$1, ["appendChild", "insertBefore", "replaceChild"]);
    wrapAccess(ElementProto$2, ["append", "prepend", "replaceWith", "after",
                              "before", "insertAdjacentElement",
                              "insertAdjacentHTML"]);

    descriptor = getInnerHTMLDescriptor(ElementProto$2, "innerHTML");
    wrapPropertyAccess(ElementProto$2, "innerHTML", descriptor);

    descriptor = getInnerHTMLDescriptor(ElementProto$2, "outerHTML");
    wrapPropertyAccess(ElementProto$2, "outerHTML", descriptor);

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
            result = apply$2(currentValue, this, args);
            endCallback && endCallback();
            return result;
          };
        }
      };
    }

    function getInnerHTMLDescriptor(target, property) {
      let desc = Object$8.getOwnPropertyDescriptor(target, property);
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
  function findOwner(root, path) {
    if (!(root instanceof NativeObject))
      return;

    let object = root;
    let chain = $(path).split(".");

    if (chain.length === 0)
      return;

    for (let i = 0; i < chain.length - 1; i++) {
      let prop = chain[i];

      if (!hasOwnProperty(object, prop))
        return;

      object = object[prop];

      if (!(object instanceof NativeObject))
        return;
    }

    let prop = chain[chain.length - 1];

    if (hasOwnProperty(object, prop))
      return [object, prop];
  }

  const decimals = $(/^\d+$/);

  function overrideValue(value) {
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

        throw new Error$7("[override-property-read snippet]: " +
                        `Value "${value}" is not valid.`);
    }
  }

  let {HTMLScriptElement: HTMLScriptElement$1, Object: Object$7, ReferenceError: ReferenceError$1} = $(window);
  let Script = Object$7.getPrototypeOf(HTMLScriptElement$1);

  function abortCurrentInlineScript(api, search = null) {
    const debugLog = getDebugger("abort-current-inline-script");
    const re = search ? toRegExp(search) : null;

    const rid = randomId();
    const us = $(document).currentScript;

    let object = window;
    const path = $(api).split(".");
    const name = $(path).pop();

    for (let node of $(path)) {
      object = object[node];
      if (
        !object || !(typeof object == "object" || typeof object == "function")) {
        debugLog("warn", path, " is not found");
        return;
      }
    }

    const {get: prevGetter, set: prevSetter} =
      Object$7.getOwnPropertyDescriptor(object, name) || {};

    let currentValue = object[name];
    if (typeof currentValue === "undefined")
      debugLog("warn", "The property", name, "doesn't exist yet. Check typos.");

    const abort = () => {
      const element = $(document).currentScript;
      if (element instanceof Script &&
          $(element, "HTMLScriptElement").src == "" &&
          element != us &&
          (!re || re.test($(element).textContent))) {
        debugLog("success", path, " is aborted \n", element);
        throw new ReferenceError$1(rid);
      }
    };

    const descriptor = {
      get() {
        abort();

        if (prevGetter)
          return call(prevGetter, this);

        return currentValue;
      },
      set(value) {
        abort();

        if (prevSetter)
          call(prevSetter, this, value);
        else
          currentValue = value;
      }
    };

    wrapPropertyAccess(object, name, descriptor);

    overrideOnError(rid);
  }

  function abortOnIframePropertyRead(...properties) {
    abortOnIframe(properties, true, false);
  }

  function abortOnIframePropertyWrite(...properties) {
    abortOnIframe(properties, false, true);
  }

  function abortOnPropertyRead(property, setConfigurable) {
    const configurableFlag = !(setConfigurable === "false");
    abortOnRead("abort-on-property-read", window, property, configurableFlag);
  }

  function abortOnPropertyWrite(property, setConfigurable) {
    const configurableFlag = !(setConfigurable === "false");
    abortOnWrite("abort-on-property-write", window, property, configurableFlag);
  }

  let {Error: Error$6, URL: URL$1} = $(window);
  let {cookie: documentCookies} = accessor(document);

  function cookieRemover(cookie, autoRemoveCookie = false) {
    if (!cookie)
      throw new Error$6("[cookie-remover snippet]: No cookie to remove.");

    let debugLog = getDebugger("cookie-remover");
    let re = toRegExp(cookie);

    if (!$(/^http|^about/).test(location.protocol)) {
      debugLog("warn", "Snippet only works for http or https and about.");
      return;
    }

    function getCookieMatches() {
      const arr = $(documentCookies()).split(";");
      return arr.filter(str => re.test($(str).split("=")[0]));
    }

    const mainLogic = () => {
      debugLog("info", "Parsing cookies for matches");
      for (const pair of $(getCookieMatches())) {
        let $hostname = $(location.hostname);

        if (!$hostname &&
          $(location.ancestorOrigins) && $(location.ancestorOrigins[0]))
          $hostname = new URL$1($(location.ancestorOrigins[0])).hostname;
        const name = $(pair).split("=")[0];
        const expires = "expires=Thu, 01 Jan 1970 00:00:00 GMT";
        const path = "path=/";
        const domainParts = $hostname.split(".");

        for (let numDomainParts = domainParts.length;
          numDomainParts > 0; numDomainParts--) {
          const domain =
            domainParts.slice(domainParts.length - numDomainParts).join(".");
          documentCookies(`${$(name).trim()}=;${expires};${path};domain=${domain}`);
          documentCookies(`${$(name).trim()}=;${expires};${path};domain=.${domain}`);
          debugLog("success", `Set expiration date on ${name}`);
        }
      }
    };

    mainLogic();

    if (autoRemoveCookie) {

      let lastCookie = getCookieMatches();
      setInterval(() => {
        let newCookie = getCookieMatches();
        if (newCookie !== lastCookie) {
          try {
            mainLogic();
          }
          finally {
            lastCookie = newCookie;
          }
        }
      }, 1000);
    }
  }

  let {
    console: console$3,
    document: document$1,
    getComputedStyle,
    isExtensionContext,
    variables: variables$1,
    Array,
    MutationObserver: MutationObserver$2,
    Object: Object$6,
    XPathEvaluator,
    XPathExpression,
    XPathResult
  } = $(window);

  const {querySelectorAll} = document$1;
  const document$$ = querySelectorAll && bind(querySelectorAll, document$1);

  function $openOrClosedShadowRoot(element, failSilently = false) {
    try {
      const shadowRoot = (navigator.userAgent.includes("Firefox")) ?
        element.openOrClosedShadowRoot :
        browser.dom.openOrClosedShadowRoot(element);
      if (shadowRoot === null && ((debug() && !failSilently)))
        console$3.log("Shadow root not found or not added in element yet", element);
      return shadowRoot;
    }
    catch (error) {
      if (debug() && !failSilently)
        console$3.log("Error while accessing shadow root", element, error);
      return null;
    }
  }

  function $$(selector, returnRoots = false) {

    return $$recursion(
      selector,
      document$$.bind(document$1),
      document$1,
      returnRoots
    );
  }

  function isArrayEmptyStrings(arr) {
    return !arr || arr.length === 0 || arr.every(item => item.trim() === "");
  }

  function executeSvgCommand(
    nestedCommands,
    rootParent,
    resultNodes,
    rootParents
  ) {
    const xlinkHref = rootParent.getAttribute("xlink:href") ||
            rootParent.getAttribute("href");
    if (xlinkHref) {
      const matchingElement = document$$(xlinkHref)[0];
      if (!matchingElement && debug()) {
        console$3.log("No elements found matching", xlinkHref);
        return false;
      }

      if (isArrayEmptyStrings(nestedCommands)) {
        const oldRootParents = rootParents.length > 0 ? rootParents : [];
        resultNodes.push({
          element: matchingElement,
          rootParents: [...oldRootParents, rootParent]
        });
        return false;
      }
      const next$$ = matchingElement.querySelectorAll.bind(matchingElement);
      return {
        nextBoundElement: matchingElement,
        nestedSelectorsString: nestedCommands.join("^^"),
        next$$
      };
    }
  }

  function executeShadowRootCommand(nestedCommands, rootParent) {
    const shadowRoot = $openOrClosedShadowRoot(rootParent);
    if (shadowRoot) {
      const {querySelectorAll: shadowRootQuerySelectorAll} = shadowRoot;
      const next$$ = shadowRootQuerySelectorAll &&
        bind(shadowRootQuerySelectorAll, shadowRoot).bind(shadowRoot);
      return {
        nextBoundElement: rootParent,
        nestedSelectorsString: ":host " + nestedCommands.join("^^"),
        next$$
      };
    }

    return false;
  }

  function $$recursion(
    selector,
    bound$$,
    boundElement,
    returnRoots,
    rootParents = []
  ) {
    if (selector.includes("^^")) {
      const [currentSelector, currentCommand, ...nestedCommands] =
        selector.split("^^");
      let newRootParents;

      let commandFn;
      switch (currentCommand) {
        case "svg": {
          commandFn = executeSvgCommand;
          break;
        }
        case "sh": {
          commandFn = executeShadowRootCommand;
          break;
        }
        default: {
          if (debug()) {
            console$3.log(
              currentCommand,
              " is not supported. Supported commands are: \n^^sh^^\n^^svg^^"
            );
          }
          return [];
        }
      }

      if (currentSelector.trim() === "")
        newRootParents = [boundElement];
      else
        newRootParents = bound$$(currentSelector);

      const resultNodes = [];

      for (const rootParent of newRootParents) {
        const res =
          commandFn(nestedCommands, rootParent, resultNodes, rootParents);
        if (!res)
          continue;
        const {next$$, nestedSelectorsString, nextBoundElement} = res;
        const nestedElements = $$recursion(
          nestedSelectorsString,
          next$$,
          nextBoundElement,
          returnRoots,
          [...rootParents, rootParent]
        );
        if (nestedElements)
          resultNodes.push(...nestedElements);
      }
      return resultNodes;
    }
    const foundElements = bound$$(selector);
    if (returnRoots) {
      return [...foundElements].map(element => (
        {element, rootParents: rootParents.length > 0 ? rootParents : []})
      );
    }
    return foundElements;
  }

  const {assign, setPrototypeOf} = Object$6;

  class $XPathExpression extends XPathExpression {
    evaluate(...args) {
      return setPrototypeOf(
        apply$2(super.evaluate, this, args),
        XPathResult.prototype
      );
    }
  }

  class $XPathEvaluator extends XPathEvaluator {
    createExpression(...args) {
      return setPrototypeOf(
        apply$2(super.createExpression, this, args),
        $XPathExpression.prototype
      );
    }
  }

  function hideElement(element) {
    if (variables$1.hidden.has(element))
      return false;

    notifyElementHidden(element);

    variables$1.hidden.add(element);

    let {style} = $(element);
    let $style = $(style, "CSSStyleDeclaration");
    let properties = $([]);
    let {debugCSSProperties} = libEnvironment;

    for (let [key, value] of (debugCSSProperties || [["display", "none"]])) {
      $style.setProperty(key, value, "important");
      properties.push([key, $style.getPropertyValue(key)]);
    }

    new MutationObserver$2(() => {
      for (let [key, value] of properties) {
        let propertyValue = $style.getPropertyValue(key);
        let propertyPriority = $style.getPropertyPriority(key);
        if (propertyValue != value || propertyPriority != "important")
          $style.setProperty(key, value, "important");
      }
    }).observe(element, {attributes: true,
                         attributeFilter: ["style"]});
    return true;
  }

  function notifyElementHidden(element) {
    if (isExtensionContext && typeof checkElement === "function")
      checkElement(element);
  }

  function initQueryAndApply(selector) {
    let $selector = selector;
    if ($selector.startsWith("xpath(") &&
        $selector.endsWith(")")) {
      let xpathQuery = $selector.slice(6, -1);
      let evaluator = new $XPathEvaluator();
      let expression = evaluator.createExpression(xpathQuery, null);

      let flag = XPathResult.ORDERED_NODE_SNAPSHOT_TYPE;

      return cb => {
        if (!cb)
          return;
        let result = expression.evaluate(document$1, flag, null);
        let {snapshotLength} = result;
        for (let i = 0; i < snapshotLength; i++)
          cb(result.snapshotItem(i));
      };
    }
    return cb => $$(selector).forEach(cb);
  }

  function initQueryAll(selector) {
    let $selector = selector;
    if ($selector.startsWith("xpath(") &&
        $selector.endsWith(")")) {
      let queryAndApply = initQueryAndApply(selector);
      return () => {
        let elements = $([]);
        queryAndApply(e => elements.push(e));
        return elements;
      };
    }
    return () => Array.from($$(selector));
  }

  let {ELEMENT_NODE, TEXT_NODE, prototype: NodeProto} = Node;
  let {prototype: ElementProto$1} = Element;
  let {prototype: HTMLElementProto} = HTMLElement;

  let {
    console: console$2,
    variables,
    DOMParser,
    Error: Error$5,
    MutationObserver: MutationObserver$1,
    Object: Object$5,
    ReferenceError
  } = $(window);

  let {getOwnPropertyDescriptor} = Object$5;

  function freezeElement(selector, options = "", ...exceptions) {
    let observer;
    let subtree = false;
    let shouldAbort = false;
    let exceptionSelectors = $(exceptions).filter(e => !isRegex(e));
    let regexExceptions = $(exceptions).filter(e => isRegex(e)).map(toRegExp);
    let rid = randomId();
    let targetNodes;
    let queryAll = initQueryAll(selector);

    checkOptions();
    let data = {
      selector,
      shouldAbort,
      rid,
      exceptionSelectors,
      regexExceptions,
      changeId: 0
    };
    if (!variables.frozen.has(document)) {
      variables.frozen.set(document, true);
      proxyNativeProperties();
    }
    observer = new MutationObserver$1(searchAndAttach);
    observer.observe(document, {childList: true, subtree: true});
    searchAndAttach();

    function isRegex(s) {
      return s.length >= 2 && s[0] == "/" && s[s.length - 1] == "/";
    }

    function checkOptions() {
      let optionsChunks = $(options).split("+");
      if (optionsChunks.length === 1 && optionsChunks[0] === "")
        optionsChunks = [];
      for (let chunk of optionsChunks) {
        switch (chunk) {
          case "subtree":
            subtree = true;
            break;
          case "abort":
            shouldAbort = true;
            break;
          default:
            throw new Error$5("[freeze] Unknown option passed to the snippet." +
                            " [selector]: " + selector +
                            " [option]: " + chunk);
        }
      }
    }

    function proxyNativeProperties() {
      let descriptor;

      descriptor = getAppendChildDescriptor(
        NodeProto, "appendChild", isFrozen, getSnippetData
      );
      wrapPropertyAccess(NodeProto, "appendChild", descriptor);

      descriptor = getAppendChildDescriptor(
        NodeProto, "insertBefore", isFrozen, getSnippetData
      );
      wrapPropertyAccess(NodeProto, "insertBefore", descriptor);

      descriptor = getAppendChildDescriptor(
        NodeProto, "replaceChild", isFrozen, getSnippetData
      );
      wrapPropertyAccess(NodeProto, "replaceChild", descriptor);

      descriptor = getAppendDescriptor(
        ElementProto$1, "append", isFrozen, getSnippetData
      );
      wrapPropertyAccess(ElementProto$1, "append", descriptor);

      descriptor = getAppendDescriptor(
        ElementProto$1, "prepend", isFrozen, getSnippetData
      );
      wrapPropertyAccess(ElementProto$1, "prepend", descriptor);

      descriptor = getAppendDescriptor(
        ElementProto$1,
        "replaceWith",
        isFrozenOrHasFrozenParent,
        getSnippetDataFromNodeOrParent
      );
      wrapPropertyAccess(ElementProto$1, "replaceWith", descriptor);

      descriptor = getAppendDescriptor(
        ElementProto$1,
        "after",
        isFrozenOrHasFrozenParent,
        getSnippetDataFromNodeOrParent
      );
      wrapPropertyAccess(ElementProto$1, "after", descriptor);

      descriptor = getAppendDescriptor(
        ElementProto$1,
        "before",
        isFrozenOrHasFrozenParent,
        getSnippetDataFromNodeOrParent
      );
      wrapPropertyAccess(ElementProto$1, "before", descriptor);

      descriptor = getInsertAdjacentDescriptor(
        ElementProto$1,
        "insertAdjacentElement",
        isFrozenAndInsideTarget,
        getSnippetDataBasedOnTarget
      );
      wrapPropertyAccess(ElementProto$1, "insertAdjacentElement", descriptor);

      descriptor = getInsertAdjacentDescriptor(
        ElementProto$1,
        "insertAdjacentHTML",
        isFrozenAndInsideTarget,
        getSnippetDataBasedOnTarget
      );
      wrapPropertyAccess(ElementProto$1, "insertAdjacentHTML", descriptor);

      descriptor = getInsertAdjacentDescriptor(
        ElementProto$1,
        "insertAdjacentText",
        isFrozenAndInsideTarget,
        getSnippetDataBasedOnTarget
      );
      wrapPropertyAccess(ElementProto$1, "insertAdjacentText", descriptor);

      descriptor = getInnerHTMLDescriptor(
        ElementProto$1, "innerHTML", isFrozen, getSnippetData
      );
      wrapPropertyAccess(ElementProto$1, "innerHTML", descriptor);

      descriptor = getInnerHTMLDescriptor(
        ElementProto$1,
        "outerHTML",
        isFrozenOrHasFrozenParent,
        getSnippetDataFromNodeOrParent
      );
      wrapPropertyAccess(ElementProto$1, "outerHTML", descriptor);

      descriptor = getTextContentDescriptor(
        NodeProto, "textContent", isFrozen, getSnippetData
      );
      wrapPropertyAccess(NodeProto, "textContent", descriptor);

      descriptor = getTextContentDescriptor(
        HTMLElementProto, "innerText", isFrozen, getSnippetData
      );
      wrapPropertyAccess(HTMLElementProto, "innerText", descriptor);

      descriptor = getTextContentDescriptor(
        NodeProto, "nodeValue", isFrozen, getSnippetData
      );
      wrapPropertyAccess(NodeProto, "nodeValue", descriptor);

      function isFrozen(node) {
        return node && variables.frozen.has(node);
      }

      function isFrozenOrHasFrozenParent(node) {
        try {
          return node &&
                 (variables.frozen.has(node) ||
                 variables.frozen.has($(node).parentNode));
        }
        catch (error) {
          return false;
        }
      }

      function isFrozenAndInsideTarget(node, isInsideTarget) {
        try {
          return node &&
                 (variables.frozen.has(node) && isInsideTarget ||
                  variables.frozen.has($(node).parentNode) &&
                  !isInsideTarget);
        }
        catch (error) {
          return false;
        }
      }

      function getSnippetData(node) {
        return variables.frozen.get(node);
      }

      function getSnippetDataFromNodeOrParent(node) {
        try {
          if (variables.frozen.has(node))
            return variables.frozen.get(node);
          let parent = $(node).parentNode;
          return variables.frozen.get(parent);
        }
        catch (error) {}
      }

      function getSnippetDataBasedOnTarget(node, isInsideTarget) {
        try {
          if (variables.frozen.has(node) && isInsideTarget)
            return variables.frozen.get(node);
          let parent = $(node).parentNode;
          return variables.frozen.get(parent);
        }
        catch (error) {}
      }
    }

    function searchAndAttach() {
      targetNodes = queryAll();
      markNodes(targetNodes, false);
    }

    function markNodes(nodes, isChild = true) {
      for (let node of nodes) {
        if (!variables.frozen.has(node)) {
          variables.frozen.set(node, data);
          if (!isChild && subtree) {
            new MutationObserver$1(mutationsList => {
              for (let mutation of $(mutationsList))
                markNodes($(mutation, "MutationRecord").addedNodes);
            }).observe(node, {childList: true, subtree: true});
          }
          if (subtree && $(node).nodeType === ELEMENT_NODE)
            markNodes($(node).childNodes);
        }
      }
    }

    function logPrefixed(id, ...args) {
      log(`[freeze][${id}] `, ...args);
    }

    function logChange(nodeOrDOMString, target, property, snippetData) {
      let targetSelector = snippetData.selector;
      let chgId = snippetData.changeId;
      let isDOMString = typeof nodeOrDOMString == "string";
      let action = snippetData.shouldAbort ? "aborting" : "watching";
      console$2.groupCollapsed(`[freeze][${chgId}] ${action}: ${targetSelector}`);
      switch (property) {
        case "appendChild":
        case "append":
        case "prepend":
        case "insertBefore":
        case "replaceChild":
        case "insertAdjacentElement":
        case "insertAdjacentHTML":
        case "insertAdjacentText":
        case "innerHTML":
        case "outerHTML":
          logPrefixed(chgId,
                      isDOMString ? "text: " : "node: ",
                      nodeOrDOMString);
          logPrefixed(chgId, "added to node: ", target);
          break;
        case "replaceWith":
        case "after":
        case "before":
          logPrefixed(chgId,
                      isDOMString ? "text: " : "node: ",
                      nodeOrDOMString);
          logPrefixed(chgId, "added to node: ", $(target).parentNode);
          break;
        case "textContent":
        case "innerText":
        case "nodeValue":
          logPrefixed(chgId, "content of node: ", target);
          logPrefixed(chgId, "changed to: ", nodeOrDOMString);
          break;
      }
      logPrefixed(chgId, `using the function "${property}"`);
      console$2.groupEnd();
      snippetData.changeId++;
    }

    function isExceptionNode(element, expSelectors) {
      if (expSelectors) {
        let $element = $(element);
        for (let exception of expSelectors) {
          if ($element.matches(exception))
            return true;
        }
      }
      return false;
    }

    function isExceptionText(string, regExceptions) {
      if (regExceptions) {
        for (let exception of regExceptions) {
          if (exception.test(string))
            return true;
        }
      }
      return false;
    }

    function abort(id) {
      throw new ReferenceError(id);
    }

    function checkHTML(htmlText, parent, property, snippetData) {
      let domparser = new DOMParser();
      let {body} = $(domparser.parseFromString(htmlText, "text/html"));
      let nodes = $(body).childNodes;
      let accepted = checkMultiple(nodes, parent, property, snippetData);
      let content = $(accepted).map(node => {
        switch ($(node).nodeType) {
          case ELEMENT_NODE:
            return $(node).outerHTML;
          case TEXT_NODE:
            return $(node).textContent;
          default:
            return "";
        }
      });
      return content.join("");
    }

    function checkMultiple(nodesOrDOMStrings, parent, property, snippetData) {
      let accepted = $([]);
      for (let nodeOrDOMString of nodesOrDOMStrings) {
        if (checkShouldInsert(nodeOrDOMString, parent, property, snippetData))
          accepted.push(nodeOrDOMString);
      }
      return accepted;
    }

    function checkShouldInsert(nodeOrDOMString, parent, property, snippetData) {
      let aborting = snippetData.shouldAbort;
      let regExceptions = snippetData.regexExceptions;
      let expSelectors = snippetData.exceptionSelectors;
      let id = snippetData.rid;
      if (typeof nodeOrDOMString == "string") {
        let domString = nodeOrDOMString;
        if (isExceptionText(domString, regExceptions))
          return true;
        if (debug())
          logChange(domString, parent, property, snippetData);
        if (aborting)
          abort(id);
        return debug();
      }

      let node = nodeOrDOMString;
      switch ($(node).nodeType) {
        case ELEMENT_NODE:
          if (isExceptionNode(node, expSelectors))
            return true;
          if (aborting) {
            if (debug())
              logChange(node, parent, property, snippetData);
            abort(id);
          }
          if (debug()) {
            hideElement(node);
            logChange(node, parent, property, snippetData);
            return true;
          }
          return false;
        case TEXT_NODE:
          if (isExceptionText($(node).textContent, regExceptions))
            return true;
          if (debug())
            logChange(node, parent, property, snippetData);
          if (aborting)
            abort(id);
          return false;
        default:
          return true;
      }
    }

    function getAppendChildDescriptor(target, property, shouldValidate,
                                      getSnippetData) {
      let desc = getOwnPropertyDescriptor(target, property) || {};
      let origin = desc.get && call(desc.get, target) || desc.value;
      if (!origin)
        return;

      return {
        get() {
          return function(...args) {
            if (shouldValidate(this)) {
              let snippetData = getSnippetData(this);
              if (snippetData) {
                let incomingNode = args[0];
                if (!checkShouldInsert(incomingNode, this, property, snippetData))
                  return incomingNode;
              }
            }
            return apply$2(origin, this, args);
          };
        }
      };
    }

    function getAppendDescriptor(
      target, property, shouldValidate, getSnippetData
    ) {
      let desc = getOwnPropertyDescriptor(target, property) || {};
      let origin = desc.get && call(desc.get, target) || desc.value;
      if (!origin)
        return;
      return {
        get() {
          return function(...nodesOrDOMStrings) {
            if (!shouldValidate(this))
              return apply$2(origin, this, nodesOrDOMStrings);

            let snippetData = getSnippetData(this);
            if (!snippetData)
              return apply$2(origin, this, nodesOrDOMStrings);

            let accepted = checkMultiple(
              nodesOrDOMStrings, this, property, snippetData
            );
            if (accepted.length > 0)
              return apply$2(origin, this, accepted);
          };
        }
      };
    }

    function getInsertAdjacentDescriptor(
      target, property, shouldValidate, getSnippetData
    ) {
      let desc = getOwnPropertyDescriptor(target, property) || {};
      let origin = desc.get && call(desc.get, target) || desc.value;
      if (!origin)
        return;

      return {
        get() {
          return function(...args) {
            let [position, value] = args;
            let isInsideTarget =
                position === "afterbegin" || position === "beforeend";
            if (shouldValidate(this, isInsideTarget)) {
              let snippetData = getSnippetData(this, isInsideTarget);
              if (snippetData) {
                let parent = isInsideTarget ?
                             this :
                             $(this).parentNode;
                let finalValue;
                switch (property) {
                  case "insertAdjacentElement":
                    if (!checkShouldInsert(value, parent, property, snippetData))
                      return value;
                    break;

                  case "insertAdjacentHTML":
                    finalValue = checkHTML(value, parent, property, snippetData);
                    if (finalValue)
                      return call(origin, this, position, finalValue);

                    return;

                  case "insertAdjacentText":
                    if (!checkShouldInsert(value, parent, property, snippetData))
                      return;
                    break;
                }
              }
            }
            return apply$2(origin, this, args);
          };
        }
      };
    }

    function getInnerHTMLDescriptor(
      target, property, shouldValidate, getSnippetData
    ) {
      let desc = getOwnPropertyDescriptor(target, property) || {};
      let {set: prevSetter} = desc;
      if (!prevSetter)
        return;

      return {
        set(htmlText) {
          if (!shouldValidate(this))
            return call(prevSetter, this, htmlText);

          let snippetData = getSnippetData(this);
          if (!snippetData)
            return call(prevSetter, this, htmlText);
          let finalValue = checkHTML(htmlText, this, property, snippetData);
          if (finalValue)
            return call(prevSetter, this, finalValue);
        }
      };
    }

    function getTextContentDescriptor(
      target, property, shouldValidate, getSnippetData
    ) {
      let desc = getOwnPropertyDescriptor(target, property) || {};
      let {set: prevSetter} = desc;
      if (!prevSetter)
        return;

      return {
        set(domString) {
          if (!shouldValidate(this))
            return call(prevSetter, this, domString);

          let snippetData = getSnippetData(this);
          if (!snippetData)
            return call(prevSetter, this, domString);
          if (checkShouldInsert(domString, this, property, snippetData))
            return call(prevSetter, this, domString);
        }
      };
    }
  }

  $(window);

  function raceWinner(name, lose) {

    return noop;
  }

  const {Map: Map$4, MutationObserver, Object: Object$4, Set, WeakSet: WeakSet$1} = $(window);

  let ElementProto = Element.prototype;
  let {attachShadow} = ElementProto;

  let hiddenShadowRoots = new WeakSet$1();
  let searches = new Map$4();
  let observer = null;

  function hideIfShadowContains(search, selector = "*") {

    let key = `${search}\\${selector}`;
    if (!searches.has(key)) {
      searches.set(key, [toRegExp(search), selector, raceWinner()
      ]);
    }

    const debugLog = getDebugger("hide-if-shadow-contain");

    if (!observer) {
      observer = new MutationObserver(records => {
        let visited = new Set();
        for (let {target} of $(records)) {

          let parent = $(target).parentNode;
          while (parent)
            [target, parent] = [parent, $(target).parentNode];

          if (hiddenShadowRoots.has(target))
            continue;

          if (visited.has(target))
            continue;

          visited.add(target);
          for (let [re, selfOrParent, win] of searches.values()) {
            if (re.test($(target).textContent)) {
              let closest = $(target.host).closest(selfOrParent);
              if (closest) {
                win();

                $(target).appendChild(
                  document.createElement("style")
                ).textContent = ":host {display: none !important}";

                hideElement(closest);

                hiddenShadowRoots.add(target);
                debugLog("success",
                         "Hiding: ",
                         closest,
                         " for params: ",
                         ...arguments);
              }
            }
          }
        }
      });

      Object$4.defineProperty(ElementProto, "attachShadow", {

        value: proxy(attachShadow, function() {

          let root = apply$2(attachShadow, this, arguments);
          debugLog("info", "attachShadow is called for: ", root);

          observer.observe(root, {
            childList: true,
            characterData: true,
            subtree: true
          });

          return root;
        })
      });
    }
  }

  const {Error: Error$4, JSON: JSON$2, Map: Map$3, Response: Response$1, Object: Object$3} = $(window);

  let paths$1 = null;

  function jsonOverride(rawOverridePaths, value,
                               rawNeedlePaths = "", filter = "") {
    if (!rawOverridePaths)
      throw new Error$4("[json-override snippet]: Missing paths to override.");

    if (typeof value == "undefined")
      throw new Error$4("[json-override snippet]: No value to override with.");

    if (!paths$1) {
      let debugLog = getDebugger("json-override");

      function overrideObject(obj, str) {
        for (let {prune, needle, filter: flt, value: val} of paths$1.values()) {
          if (flt && !flt.test(str))
            continue;

          if ($(needle).some(path => !findOwner(obj, path)))
            return obj;

          for (let path of prune) {
            let details = findOwner(obj, path);
            if (typeof details != "undefined") {
              debugLog("success", `Found ${path} replaced it with ${val}`);
              details[0][details[1]] = overrideValue(val);
            }
          }
        }
        return obj;
      }

      let {parse} = JSON$2;
      paths$1 = new Map$3();

      Object$3.defineProperty(window.JSON, "parse", {
        value: proxy(parse, function(str) {
          let result = apply$2(parse, this, arguments);
          return overrideObject(result, str);
        })
      });
      debugLog("info", "Wrapped JSON.parse for override");

      let {json} = Response$1.prototype;
      Object$3.defineProperty(window.Response.prototype, "json", {
        value: proxy(json, function(str) {
          let resultPromise = apply$2(json, this, arguments);
          return resultPromise.then(obj => overrideObject(obj, str));
        })
      });
      debugLog("info", "Wrapped Response.json for override");
    }

    paths$1.set(rawOverridePaths, {
      prune: $(rawOverridePaths).split(/ +/),
      needle: rawNeedlePaths.length ? $(rawNeedlePaths).split(/ +/) : [],
      filter: filter ? toRegExp(filter) : null,
      value
    });
  }

  let {Error: Error$3, JSON: JSON$1, Map: Map$2, Object: Object$2, Response} = $(window);

  let paths = null;

  function jsonPrune(rawPrunePaths, rawNeedlePaths = "") {
    if (!rawPrunePaths)
      throw new Error$3("Missing paths to prune");

    if (!paths) {
      let debugLog = getDebugger("json-prune");

      function pruneObject(obj) {
        for (let {prune, needle} of paths.values()) {
          if ($(needle).some(path => !findOwner(obj, path)))
            return obj;

          for (let path of prune) {
            let details = findOwner(obj, path);
            if (typeof details != "undefined") {
              debugLog("success", `Found ${path} and deleted`);
              delete details[0][details[1]];
            }
          }
        }
        return obj;
      }

      let {parse} = JSON$1;
      paths = new Map$2();

      Object$2.defineProperty(window.JSON, "parse", {
        value: proxy(parse, function() {
          let result = apply$2(parse, this, arguments);
          return pruneObject(result);
        })
      });
      debugLog("info", "Wrapped JSON.parse for prune");

      let {json} = Response.prototype;
      Object$2.defineProperty(window.Response.prototype, "json", {
        value: proxy(json, function() {
          let resultPromise = apply$2(json, this, arguments);
          return resultPromise.then(obj => pruneObject(obj));
        })
      });
      debugLog("info", "Wrapped Response.json for prune");
    }

    paths.set(rawPrunePaths, {
      prune: $(rawPrunePaths).split(/ +/),
      needle: rawNeedlePaths.length ? $(rawNeedlePaths).split(/ +/) : []
    });
  }

  let {Error: Error$2} = $(window);

  function overridePropertyRead(property, value, setConfigurable) {
    if (!property) {
      throw new Error$2("[override-property-read snippet]: " +
                       "No property to override.");
    }
    if (typeof value === "undefined") {
      throw new Error$2("[override-property-read snippet]: " +
                       "No value to override with.");
    }

    let debugLog = getDebugger("override-property-read");

    let cValue = overrideValue(value);

    let newGetter = () => {
      debugLog("success", `${property} override done.`);
      return cValue;
    };

    debugLog("info", `Overriding ${property}.`);

    const configurableFlag = !(setConfigurable === "false");

    wrapPropertyAccess(window,
                       property,
                       {get: newGetter, set() {}},
                       configurableFlag);
  }

  let {Error: Error$1, Map: Map$1, Object: Object$1, console: console$1} = $(window);

  let {toString} = Function.prototype;
  let EventTargetProto = EventTarget.prototype;
  let {addEventListener} = EventTargetProto;

  let events = null;

  function preventListener(event, eventHandler, selector) {
    if (!event)
      throw new Error$1("[prevent-listener snippet]: No event type.");

    if (!events) {
      events = new Map$1();

      let debugLog = getDebugger("[prevent]");

      Object$1.defineProperty(EventTargetProto, "addEventListener", {
        value: proxy(addEventListener, function(type, listener) {
          for (let {evt, handlers, selectors} of events.values()) {

            if (!evt.test(type))
              continue;

            let isElement = this instanceof Element;

            for (let i = 0; i < handlers.length; i++) {
              const handler = handlers[i];
              const sel = selectors[i];

              if (sel && !(isElement && $(this).matches(sel)))
                continue;

              if (handler) {
                const proxiedHandlerMatch = function() {
                  try {
                    const proxiedHandlerString = call(
                      toString,
                      typeof listener === "function" ?
                        listener : listener.handleEvent
                    );
                    return handler.test(proxiedHandlerString);
                  }
                  catch (e) {
                    debugLog("error",
                             "Error while trying to stringify listener: ",
                             e);
                    return false;
                  }
                };

                const actualHandlerMatch = function() {
                  try {
                    const actualHandlerString = String(
                      typeof listener === "function" ?
                        listener : listener.handleEvent
                    );
                    return handler.test(actualHandlerString);
                  }
                  catch (e) {
                    debugLog("error",
                             "Error while trying to stringify listener: ",
                             e);
                    return false;
                  }
                };

                if (!proxiedHandlerMatch() && !actualHandlerMatch())
                  continue;
              }

              if (debug()) {
                console$1.groupCollapsed("DEBUG [prevent] was successful");
                debugLog("success", `type: ${type} matching ${evt}`);
                debugLog("success", "handler:", listener);
                if (handler)
                  debugLog("success", `matching ${handler}`);
                if (sel)
                  debugLog("success", "on element: ", this, ` matching ${sel}`);
                debugLog("success", "was prevented from being added");
                console$1.groupEnd();
              }
              return;
            }
          }
          return apply$2(addEventListener, this, arguments);
        })
      });

      debugLog("info", "Wrapped addEventListener");
    }

    if (!events.has(event))
      events.set(event, {evt: toRegExp(event), handlers: [], selectors: []});

    let {handlers, selectors} = events.get(event);

    handlers.push(eventHandler ? toRegExp(eventHandler) : null);
    selectors.push(selector);
  }

  let {URL, fetch} = $(window);

  let {delete: deleteParam, has: hasParam} = caller(URLSearchParams.prototype);

  let parameters;

  function stripFetchQueryParameter(name, urlPattern = null) {
    const debugLog = getDebugger("strip-fetch-query-parameter");

    if (!parameters) {
      parameters = new Map();
      window.fetch = proxy(fetch, (...args) => {
        let [source] = args;
        if (typeof source === "string") {
          let url = new URL(source);
          for (let [key, reg] of parameters) {
            if (!reg || reg.test(source)) {
              if (hasParam(url.searchParams, key)) {
                debugLog("success", `${key} has been stripped from url ${source}`);
                deleteParam(url.searchParams, key);
                args[0] = url.href;
              }
            }
          }
        }
        return apply$2(fetch, self, args);
      });
    }
    parameters.set(name, urlPattern && toRegExp(urlPattern));
  }

  function trace(...args) {

    apply$2(log, null, args);
  }

  const snippets = {
    "abort-current-inline-script": abortCurrentInlineScript,
    "abort-on-iframe-property-read": abortOnIframePropertyRead,
    "abort-on-iframe-property-write": abortOnIframePropertyWrite,
    "abort-on-property-read": abortOnPropertyRead,
    "abort-on-property-write": abortOnPropertyWrite,
    "cookie-remover": cookieRemover,
    "debug": setDebug,
    "freeze-element": freezeElement,
    "hide-if-shadow-contains": hideIfShadowContains,
    "json-override": jsonOverride,
    "json-prune": jsonPrune,
    "override-property-read": overridePropertyRead,
    "prevent-listener": preventListener,
    "strip-fetch-query-parameter": stripFetchQueryParameter,
    "trace": trace
  };
  let context;
  for (const [name, ...args] of filters) {
    if (snippets.hasOwnProperty(name)) {
      try { context = snippets[name].apply(context, args); }
      catch (error) { console.error(error); }
    }
  }
  context = void 0;
};
const graph = new Map([["abort-current-inline-script",null],["abort-on-iframe-property-read",null],["abort-on-iframe-property-write",null],["abort-on-property-read",null],["abort-on-property-write",null],["cookie-remover",null],["debug",null],["freeze-element",null],["hide-if-shadow-contains",null],["json-override",null],["json-prune",null],["override-property-read",null],["prevent-listener",null],["strip-fetch-query-parameter",null],["trace",null]]);
callback.get = snippet => graph.get(snippet);
callback.has = snippet => graph.has(snippet);
export default callback;