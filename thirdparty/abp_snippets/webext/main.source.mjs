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

  const chromeObjAvailable = typeof chrome === "object" && !!chrome.runtime;
  const browserObjAvailable = typeof browser === "object" && !!browser.runtime;
  const isExtensionContext$2 = !isMainWorld &&
    (isIsolatedWorld || chromeObjAvailable || browserObjAvailable);
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
  const {Map: Map$d, RegExp: RegExp$4, Set: Set$3, WeakMap: WeakMap$6, WeakSet: WeakSet$4} = classes;

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
    frozen: new WeakMap$6(),
    hidden: new WeakSet$4(),
    iframePropertiesToAbort: {
      read: new Set$3(),
      write: new Set$3()
    },
    abortedIframes: new WeakMap$6()
  });

  const startsCapitalized = new RegExp$4("^[A-Z]");
  const extensionApi = (
    isExtensionContext$2 && (
      (chromeObjAvailable && chrome) ||
      (browserObjAvailable && browser)
    )
  ) || void 0;

  var env = new Proxy(new Map$d([

    ["chrome", extensionApi],
    ["browser", extensionApi],
    ["isExtensionContext", isExtensionContext$2],
    ["variables", variables$3],

    ["console", copyIfExtension(console)],
    ["document", globalThis.document],
    ["JSON", copyIfExtension(JSON)],
    ["Map", Map$d],
    ["Math", copyIfExtension(Math)],
    ["Number", isExtensionContext$2 ? Number : primitive("Number")],
    ["RegExp", RegExp$4],
    ["Set", Set$3],
    ["String", isExtensionContext$2 ? String : primitive("String")],
    ["WeakMap", WeakMap$6],
    ["WeakSet", WeakSet$4],

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

  const {Map: Map$c, WeakMap: WeakMap$5, WeakSet: WeakSet$3, setTimeout: setTimeout$1} = env;

  let cleanup = true;
  let cleanUpCallback = map => {
    map.clear();
    cleanup = !cleanup;
  };

  var transformer = transformOnce.bind({
    WeakMap: WeakMap$5,
    WeakSet: WeakSet$3,

    WeakValue: class extends Map$c {
      set(key, value) {
        if (cleanup) {
          cleanup = !cleanup;
          setTimeout$1(cleanUpCallback, 0, this);
        }
        return super.set(key, value);
      }
    }
  });

  const {concat, includes, join, reduce, unshift} = caller([]);

  const globals = secure(globalThis);

  const {
    Map: Map$b,
    WeakMap: WeakMap$4
  } = globals;

  const map = new Map$b;
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
    Array: Array$7,
    Number: Number$1,
    String: String$1,
    Object: Object$g
  } = env;

  const {isArray} = Array$7;
  const {getOwnPropertyDescriptor: getOwnPropertyDescriptor$1, setPrototypeOf: setPrototypeOf$1} = Object$g;

  const {toString: toString$1} = Object$g.prototype;
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
      return setPrototypeOf$1(value, Array$7.prototype);

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

  let {Math: Math$2, setInterval: setInterval$1, performance} = $(window);

  const noopProfile = {
    mark() {},
    end() {},
    toString() {
      return "{mark(){},end(){}}";
    }
  };

  let inactive = true;

  function setProfile() {
    inactive = false;
  }

  function profile(id, rate = 10) {
    if (inactive)
      return noopProfile;
    function processSamples() {
      let samples = $([]);

      for (let {name, duration} of performance.getEntriesByType("measure"))
        samples.push({name, duration});

      if (samples.length)
        performance.clearMeasures();
    }

    if (!profile[id]) {
      profile[id] = setInterval$1(processSamples,
                                Math$2.round(60000 / Math$2.min(60, rate)));
    }

    return {
      mark() {
        performance.mark(id);
      },
      end(clear = false) {
        performance.measure(id, id);
        const measures = performance.getEntriesByName(id, "measure");
        const measureObj = measures.length > 0 ?
                           measures[measures.length - 1] : null;
        console.log("PROFILER:", measureObj);
        performance.clearMarks(id);
        if (clear) {
          clearInterval(profile[id]);
          delete profile[id];
          processSamples();
        }
      }
    };
  }

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
    let {mark, end} = profile("log");
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
    mark();
    console$4.log(...args);
    end();
  }

  function getDebugger(name) {
    return bind(debug() ? log : noop, null, name);
  }

  let {Array: Array$6, Math: Math$1, RegExp: RegExp$3} = $(window);

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

        return new RegExp$3(...args);
      }
    }

    return new RegExp$3(regexEscape(pattern));
  }

  function randomId() {

    return $(Math$1.floor(Math$1.random() * 2116316160 + 60466176)).toString(36);
  }

  function formatArguments(args) {
    return $(Array$6.from(args)).map(arg => `'${arg}'`).join(" ");
  }

  let {
    parseFloat,
    variables: variables$2,
    clearTimeout,
    fetch: fetch$1,
    setTimeout,
    Array: Array$5,
    Error: Error$b,
    Map: Map$a,
    Object: Object$f,
    ReferenceError: ReferenceError$2,
    Set: Set$2,
    WeakMap: WeakMap$3
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

      let currentDescriptor = Object$f.getOwnPropertyDescriptor(object, property);
      if (currentDescriptor && !currentDescriptor.configurable)
        return;

      let newDescriptor = Object$f.assign({}, descriptor, {
        configurable: setConfigurable
      });

      if (!currentDescriptor && !newDescriptor.get && newDescriptor.set) {
        let propertyValue = object[property];
        newDescriptor.get = () => propertyValue;
      }

      Object$f.defineProperty(object, property, newDescriptor);
      return;
    }

    let name = $property.slice(0, dotIndex).toString();
    property = $property.slice(dotIndex + 1).toString();
    let value = object[name];
    if (value && (typeof value == "object" || typeof value == "function"))
      wrapPropertyAccess(value, property, descriptor);

    let currentDescriptor = Object$f.getOwnPropertyDescriptor(object, name);
    if (currentDescriptor && !currentDescriptor.configurable)
      return;

    if (!propertyAccessors)
      propertyAccessors = new WeakMap$3();

    if (!propertyAccessors.has(object))
      propertyAccessors.set(object, new Map$a());

    let properties = propertyAccessors.get(object);
    if (properties.has(name)) {
      properties.get(name).set(property, descriptor);
      return;
    }

    let toBeWrapped = new Map$a([[property, descriptor]]);
    properties.set(name, toBeWrapped);
    Object$f.defineProperty(object, name, {
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

  function abortOnRead(loggingPrefix, context,
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
      throw new ReferenceError$2(rid);
    }

    debugLog("info", `aborting on ${property} access`);

    wrapPropertyAccess(context,
                       property,
                       {get: abort, set() {}},
                       setConfigurable);
    overrideOnError(rid);
  }

  function abortOnWrite(loggingPrefix,
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

    const formattedPropertiesToLog = formatArguments(properties);

    for (let frame of Array$5.from(window.frames)) {
      if (abortedIframes.has(frame)) {
        for (let property of properties) {
          if (abortRead)

            abortedIframes.get(frame).read.add({property, formattedProperties: formattedPropertiesToLog});
          if (abortWrite)

            abortedIframes.get(frame).write.add({property, formattedProperties: formattedPropertiesToLog});
        }
      }
    }

    for (let property of properties) {
      if (abortRead)

        iframePropertiesToAbort.read.add({property, formattedProperties: formattedPropertiesToLog});
      if (abortWrite)

        iframePropertiesToAbort.write.add({property, formattedProperties: formattedPropertiesToLog});
    }

    queryAndProxyIframe();
    if (!abortedIframes.has(document)) {
      abortedIframes.set(document, true);
      addHooksOnDomAdditions(queryAndProxyIframe);
    }

    function queryAndProxyIframe() {
      for (let frame of Array$5.from(window.frames)) {

        if (!abortedIframes.has(frame)) {
          abortedIframes.set(frame, {
            read: new Set$2(iframePropertiesToAbort.read),
            write: new Set$2(iframePropertiesToAbort.write)
          });
        }

        let readProps = abortedIframes.get(frame).read;
        if (readProps.size > 0) {
          let props = Array$5.from(readProps);
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
          let props = Array$5.from(writeProps);
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
      let desc = Object$f.getOwnPropertyDescriptor(target, property);
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

        throw new Error$b("[override-property-read snippet]: " +
                        `Value "${value}" is not valid.`);
    }
  }

  function matchesStackTrace(stackNeedle, debugLog) {
    if (!stackNeedle || !stackNeedle.length)
      return true;

    const token = randomId();
    const error = new Error$b(token);

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

  new Map$a();

  let {HTMLScriptElement: HTMLScriptElement$1, Object: Object$e, ReferenceError: ReferenceError$1} = $(window);
  let Script = Object$e.getPrototypeOf(HTMLScriptElement$1);

  function abortCurrentInlineScript(api, search = null) {
    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("abort-current-inline-script");
    const {mark, end} = profile("abort-current-inline-script");
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
      Object$e.getOwnPropertyDescriptor(object, name) || {};

    let currentValue = object[name];
    if (typeof currentValue === "undefined")
      debugLog("warn", "The property", name, "doesn't exist yet. Check typos.");

    const abort = () => {
      const element = $(document).currentScript;
      if (element instanceof Script &&
          $(element, "HTMLScriptElement").src == "" &&
          element != us &&
          (!re || re.test($(element).textContent))) {
        debugLog("success",
                 path,
                 " is aborted \n",
                 element,
                 "\nFILTER: abort-current-inline-script",
                 formattedArguments);
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

    mark();
    wrapPropertyAccess(object,
                       name,
                       descriptor);
    end();

    overrideOnError(rid);
  }

  function abortOnIframePropertyRead(...properties) {
    const {mark, end} = profile("abort-on-iframe-property-read");
    mark();
    abortOnIframe(properties, true, false);
    end();
  }

  function abortOnIframePropertyWrite(...properties) {
    const {mark, end} = profile("abort-on-iframe-property-write");
    mark();
    abortOnIframe(properties, false, true);
    end();
  }

  function abortOnPropertyRead(property, setConfigurable) {
    const configurableFlag = !(setConfigurable === "false");
    const formattedArguments = formatArguments(arguments);
    const {mark, end} = profile("abort-on-property-read");
    mark();
    abortOnRead("abort-on-property-read",
                window,
                property,
                formattedArguments,
                configurableFlag);
    end();
  }

  function abortOnPropertyWrite(property, setConfigurable) {
    const formattedArguments = formatArguments(arguments);
    const {mark, end} = profile("abort-on-property-write");
    const configurableFlag = !(setConfigurable === "false");
    mark();
    abortOnWrite("abort-on-property-write",
                 window,
                 property,
                 formattedArguments,
                 configurableFlag);
    end();
  }

  const {Error: Error$a, Object: Object$d, Array: Array$4, Map: Map$9} = $(window);

  let arrayValues = null;

  function hasMatchingProperty(val, needle, pathSegments) {

    let current = val;
    for (const segment of pathSegments) {

      if (!current || !hasOwnProperty(current, segment))
        return false;
      current = current[segment];
    }

    if (typeof current === "string" || typeof current === "number"){
      const currStr = current.toString();
      return needle.test(currStr);
    }

    return false;
  }

  function arrayOverride(method, needle, returnValue = "false",
                                path, stack) {
    if (!method)
      throw new Error$a("[array-override snippet]: Missing method to override.");

    if (!needle)
      throw new Error$a("[array-override snippet]: Missing needle.");

    if (!arrayValues)
      arrayValues = new Map$9();

    let debugLog = getDebugger("array-override");
    const {mark, end} = profile("array-override");
    const formattedArgsToLog = formatArguments(arguments);

    if (method === "push" && !arrayValues.has("push")) {
      mark();
      const {push} = Array$4.prototype;
      arrayValues.set("push", $([]));

      Object$d.defineProperty(window.Array.prototype, "push", {
        value: proxy(push, function(val) {
          const overrideVals = arrayValues.get("push");
          for (const {needleRegex, pathSegments, stackNeedles} of overrideVals) {

            if (!pathSegments.length && (typeof val === "string" ||
                typeof val === "number")) {
              const valStr = val.toString();
              if (valStr.match && valStr.match(needleRegex) &&
                  matchesStackTrace(stackNeedles, debugLog)) {
                debugLog("success", `Array.push is ignored for needle: ${needleRegex}\nFILTER: array-override ${formattedArgsToLog}`);
                return;
              }
            }

            else if (pathSegments.length && typeof val === "object" &&
                     val !== null) {
              if (hasMatchingProperty(val, needleRegex, pathSegments) &&
                  matchesStackTrace(stackNeedles, debugLog)) {
                debugLog("success", `Array.push is ignored for object containing needle: ${needleRegex}\nFILTER: array-override ${formattedArgsToLog}`);
                return;
              }
            }
          }
          return apply$2(push, this, arguments);
        })
      });
      debugLog("info", "Wrapped Array.prototype.push");
      end();
    }

    else if (method === "includes" && !arrayValues.has("includes")) {
      mark();
      const {includes} = Array$4.prototype;
      arrayValues.set("includes", $([]));

      Object$d.defineProperty(window.Array.prototype, "includes", {
        value: proxy(includes, function(val) {
          const overrideVals = arrayValues.get("includes");
          for (const {
            needleRegex,
            retVal,
            pathSegments,
            stackNeedles
          } of overrideVals) {

            if (!pathSegments.length && (typeof val === "string" ||
                 typeof val === "number")) {
              if (val.toString().match &&
                  val.toString().match(needleRegex) &&
                  matchesStackTrace(stackNeedles, debugLog)) {
                debugLog("success", `Array.includes returned ${retVal} for ${needleRegex}\nFILTER: array-override ${formattedArgsToLog}`);
                return retVal;
              }
            }

            else if (pathSegments.length && typeof val === "object" &&
                     val !== null) {
              if (hasMatchingProperty(val, needleRegex, pathSegments) &&
                  matchesStackTrace(stackNeedles, debugLog)) {
                debugLog("success", `Array.includes returned ${retVal} for object containing ${needleRegex}\nFILTER: array-override ${formattedArgsToLog}`);
                return retVal;
              }
            }
          }
          return apply$2(includes, this, arguments);
        })
      });
      debugLog("info", "Wrapped Array.prototype.includes");
      end();
    }

    else if (method === "forEach" && !arrayValues.has("forEach")) {
      mark();
      const {forEach} = Array$4.prototype;
      arrayValues.set("forEach", $([]));

      Object$d.defineProperty(window.Array.prototype, "forEach", {
        value: proxy(forEach, function(callback, thisArg) {
          const overrideVals = arrayValues.get("forEach");

          const filteredCallback = function(item, index, array) {
            for (const {needleRegex, pathSegments, stackNeedles} of
              overrideVals) {

              if (!pathSegments.length && (typeof item === "string" ||
                  typeof item === "number")) {
                const itemStr = item.toString();
                if (itemStr.match &&
                    itemStr.match(needleRegex) &&
                    matchesStackTrace(stackNeedles, debugLog)) {
                  debugLog("success", `Array.forEach skipped callback for item matching needle: ${needleRegex}\nFILTER: array-override ${formattedArgsToLog}`);
                  return;
                }
              }

              else if (pathSegments.length && typeof item === "object" &&
                       item !== null) {
                if (hasMatchingProperty(item, needleRegex, pathSegments) &&
                    matchesStackTrace(stackNeedles, debugLog)) {
                  debugLog("success", `Array.forEach skipped callback for object containing needle: ${needleRegex}\nFILTER: array-override ${formattedArgsToLog}`);
                  return;
                }
              }
            }

            return apply$2(callback, thisArg || this, [item, index, array]);
          };
          return apply$2(forEach, this, [filteredCallback, thisArg]);
        })
      });
      debugLog("info", "Wrapped Array.prototype.forEach");
      end();
    }

    const needleRegex = toRegExp(needle);
    let pathSegments = [];
    if (path)
      pathSegments = path.split(".");

    let stackNeedles = [];
    if (stack)
      stackNeedles = stack.split(",").map(s => s.trim());

    const overrideVals = arrayValues.get(method);
    const retVal = returnValue === "true";
    overrideVals.push({needleRegex, retVal, pathSegments, stackNeedles});
    arrayValues.set(method, overrideVals);
  }

  const {Array: Array$3, Blob, Error: Error$9, Object: Object$c, Reflect: Reflect$2} = $(window);

  const blobRules = [];

  function blobOverride(search, replacement = "", needle = null) {
    if (!search) {
      throw new Error$9(
        "[blob-override snippet]: Missing parameter search."
      );
    }
    const debugLog = getDebugger("blob-override");
    const formattedArgsToLog = formatArguments(arguments);
    const {mark, end} = profile("blob-override");
    mark();

    blobRules.push({
      match: toRegExp(search),
      replaceWith: replacement,
      needle: needle ? toRegExp(needle) : null,
      formattedArgs: formattedArgsToLog
    });

    if (blobRules.length > 1)
      return;

    const OriginalBlob = Blob;
    function PatchedBlob(data, options = {}) {
      if (Array$3.isArray(data)) {
        let combinedData = $(data).join("");

        for (const rule of $(blobRules)) {
          if (
            (!rule.needle || rule.needle.test(combinedData)) &&
            rule.match.test(combinedData)
          ) {
            combinedData = combinedData.replace(rule.match, rule.replaceWith);
            debugLog("success", `Replaced: ${rule.match} → ${rule.replaceWith},\nFILTER: blob-override ${rule.formattedArgs}`);
          }
        }
        data = [combinedData];
      }

      const blob = Reflect$2.construct(OriginalBlob, [data, options]);
      Object$c.setPrototypeOf(blob, PatchedBlob.prototype);
      return blob;
    }

    PatchedBlob.prototype = OriginalBlob.prototype;
    Object$c.setPrototypeOf(PatchedBlob, OriginalBlob);
    window.Blob = PatchedBlob;
    debugLog("info", "Wrapped Blob constructor in context ");
    end();
  }

  let {Error: Error$8, URL: URL$1} = $(window);
  let {cookie: documentCookies} = accessor(document);

  function cookieRemover(cookie, autoRemoveCookie = false) {
    if (!cookie)
      throw new Error$8("[cookie-remover snippet]: No cookie to remove.");

    const formattedArguments = formatArguments(arguments);
    let debugLog = getDebugger("cookie-remover");
    const {mark, end} = profile("cookie-remover");
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
      mark();
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
          debugLog("success", `Set expiration date on ${name}`, "\nFILTER: cookie-remover", formattedArguments);
        }
      }
      end();
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

  const {Map: Map$8, Object: Object$b, Reflect: Reflect$1, WeakMap: WeakMap$2} = $(window);

  const originalAddEventListener = window.EventTarget.prototype.addEventListener;
  const originalRemoveEventListener = window.EventTarget.
                                      prototype.removeEventListener;

  const listenerMap = new WeakMap$2();
  let eventOverrides = [];

  function eventOverride(eventType,
                                mode,
                                needle = null) {
    const formattedArgs = formatArguments(arguments);
    const overrideConfig = {
      eventType,
      mode,
      needle: needle ? toRegExp(needle) : null,
      formattedArgs
    };

    if (!eventOverrides.includes(overrideConfig))
      eventOverrides.push(overrideConfig);

    if (eventOverrides.length > 1)
      return;

    let debugLog = getDebugger("[event-override]");
    const {mark, end} = profile("event-override");

    const addEventListenerDescriptor = Object$b.getOwnPropertyDescriptor(
      window.EventTarget.prototype,
      "addEventListener"
    );

    if (addEventListenerDescriptor.configurable) {
      Object$b.defineProperty(window.EventTarget.prototype, "addEventListener", {
        ...addEventListenerDescriptor,
        value: proxy(
          originalAddEventListener,
          function(type, listener, options) {
            mark();

            const filteredEvents = eventOverrides.filter(
              ev => ev.eventType === type
            );

            if (!filteredEvents.length || type !== filteredEvents[0].eventType) {
              end();
              return apply$2(originalAddEventListener, this, arguments);
            }

            const disabledEvent = filteredEvents.find(
              ev =>
                (ev.mode === "disable") &&
                (ev.needle ? ev.needle.test(listener.toString()) : true)
            );

            if (disabledEvent) {
              debugLog("success", `Disabling ${disabledEvent.eventType} event, \nFILTER: event-override ${disabledEvent.formattedArgs}`);
              end();
              return;
            }

            const changedEvents = filteredEvents.filter(
              ev =>
                (ev.mode === "trusted") &&
                (ev.needle ? ev.needle.test(listener.toString()) : true)
            );

            if (typeof listener !== "function" &&
                !(listener && typeof listener.handleEvent === "function") ||
                !changedEvents.length || type !== changedEvents[0].eventType) {
              end();
              return apply$2(originalAddEventListener, this, arguments);
            }

            const wrappedListener = function(originalEvent) {
              const customEvent = new Proxy(originalEvent, {
                get(target, prop) {
                  if (prop === "isTrusted") {
                    debugLog("success", `Providing trusted value for ${originalEvent.type} event`);
                    return true;
                  }

                  const val = Reflect$1.get(target, prop);

                  if (typeof val === "function") {
                    return function(...args) {
                      return apply$2(val, target, args);
                    };
                  }

                  return val;
                }
              });

              if (typeof listener === "function")
                return call(listener, this, customEvent);

              return call(listener.handleEvent, listener, customEvent);
            };

            wrappedListener.originalListener = listener;

            if (!listenerMap.has(listener))
              listenerMap.set(listener, new Map$8());

            listenerMap.get(listener).set(type, wrappedListener);
            debugLog("info", `\nWrapping event listener for ${type}`);

            end();
            return apply$2(
              originalAddEventListener,
              this,
              [type, wrappedListener, options]
            );
          })
      });
    }

    const removeEventListenerDescriptor = Object$b.getOwnPropertyDescriptor(
      window.EventTarget.prototype,
      "removeEventListener"
    );
    if (removeEventListenerDescriptor.configurable) {
      Object$b.defineProperty(window.EventTarget.prototype, "removeEventListener", {
        ...removeEventListenerDescriptor,
        value: proxy(
          originalRemoveEventListener,
          function(type, listener, options) {
            if (listener &&
              listenerMap.has(listener) && listenerMap.get(listener).has(type)) {
              const wrappedListener = listenerMap.get(listener).get(type);
              listenerMap.get(listener).delete(type);
              return apply$2(
                originalRemoveEventListener,
                this,
                [type, wrappedListener, options]
              );
            }

            return apply$2(originalRemoveEventListener, this, arguments);
          })
      });
    }

    debugLog("info", "Initialized event-override snippet");
  }

  let {
    console: console$3,
    document: document$2,
    getComputedStyle,
    isExtensionContext,
    variables: variables$1,
    Array: Array$2,
    MutationObserver: MutationObserver$3,
    Object: Object$a,
    XPathEvaluator,
    XPathExpression,
    XPathResult
  } = $(window);

  const {querySelectorAll} = document$2;
  const document$$ = querySelectorAll && bind(querySelectorAll, document$2);

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
      document$$.bind(document$2),
      document$2,
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

  const {assign, setPrototypeOf} = Object$a;

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

    new MutationObserver$3(() => {
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
        let result = expression.evaluate(document$2, flag, null);
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
    return () => Array$2.from($$(selector));
  }

  let {ELEMENT_NODE, TEXT_NODE, prototype: NodeProto} = Node;
  let {prototype: ElementProto$1} = Element;
  let {prototype: HTMLElementProto} = HTMLElement;

  let {
    console: console$2,
    variables,
    DOMParser,
    Error: Error$7,
    MutationObserver: MutationObserver$2,
    Object: Object$9,
    ReferenceError
  } = $(window);

  let {getOwnPropertyDescriptor} = Object$9;

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
    observer = new MutationObserver$2(searchAndAttach);
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
            throw new Error$7("[freeze] Unknown option passed to the snippet." +
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
            new MutationObserver$2(mutationsList => {
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

  const {CanvasRenderingContext2D: CanvasRenderingContext2D$1,
         document: document$1,
         Map: Map$7,
         MutationObserver: MutationObserver$1,
         Object: Object$8,
         Set: Set$1,
         WeakSet: WeakSet$2} = $(window);

  let canvasRules;
  let pendingHideCanvasElements = new Set$1();
  let hideCanvasSeenMap = new WeakSet$2();

  function hideIfCanvasContains(search, selector = "canvas") {
    const debugLog = getDebugger("hide-if-canvas-contains");
    const formattedArgsToLog = formatArguments(arguments);
    const {mark, end} = profile("hide-if-canvas-contains");

    if (!search) {
      debugLog("error", "The parameter 'search' is required");
      return;
    }

    if (!canvasRules) {
      mark();
      const CanvasProto = CanvasRenderingContext2D$1.prototype;
      debugLog("info", "CanvasRenderingContext2D proxied");

      function overrideFunctionInCanvas(functionName){
        const originalFunction = CanvasProto[functionName];

        Object$8.defineProperty(window.CanvasRenderingContext2D.prototype, functionName, {
          value: proxy(originalFunction, function(text, ...args) {
            for (const [searchRegex, rule] of canvasRules) {
              if (searchRegex.test(text)) {
                const canvasElement = this.canvas;
                let elementToHide = $(canvasElement).closest(rule.selector);

                if (elementToHide && !hideCanvasSeenMap.has(elementToHide)) {
                  hideElement(elementToHide);
                  hideCanvasSeenMap.add(elementToHide);
                  debugLog("success", "Matched: ", elementToHide, `\nFILTER: hide-if-canvas-contains ${rule.formattedArguments}`);
                }
                else {

                  scheduleElementToHide(canvasElement, rule, functionName, text);
                }
              }
            }
            return apply$2(originalFunction, this, [text, ...args]);
          })
        });
      }

      overrideFunctionInCanvas("fillText");
      overrideFunctionInCanvas("strokeText");
      canvasRules = new Map$7();

      const mo = new MutationObserver$1(mutationsList => {
        for (let mutation of $(mutationsList)) {
          if (mutation.type === "childList") {

            checkPendingElements();
          }
        }
      });

      mo.observe(document$1, {childList: true, subtree: true});
      end();
    }

    const searchRegex = toRegExp(search);

    canvasRules.set(searchRegex, {selector, formattedArguments: formattedArgsToLog});
  }

  function scheduleElementToHide(canvasElement, rule, functionName, text) {
    pendingHideCanvasElements.add({canvasElement, rule, functionName, text});
  }

  function checkPendingElements() {
    pendingHideCanvasElements.forEach(
      ({canvasElement, rule, functionName, text}) => {
        let elementToHide = $(canvasElement).closest(rule.selector);
        if (elementToHide && !hideCanvasSeenMap.has(elementToHide)) {
          hideElement(elementToHide);
          hideCanvasSeenMap.add(elementToHide);
          pendingHideCanvasElements.delete(
            {canvasElement, rule, functionName, text});
          getDebugger("hide-if-canvas-contains")("success", "Matched: ", elementToHide, `\nFILTER: hide-if-canvas-contains ${rule.formattedArguments}`);
        }
      });
  }

  $(window);

  function raceWinner(name, lose) {

    return noop;
  }

  const {Map: Map$6, MutationObserver, Object: Object$7, Set, WeakSet: WeakSet$1} = $(window);

  let ElementProto = Element.prototype;
  let {attachShadow} = ElementProto;

  let hiddenShadowRoots = new WeakSet$1();
  let searches = new Map$6();
  let observer = null;

  function hideIfShadowContains(search, selector = "*") {

    const formattedArgs = formatArguments(arguments);

    let key = `${search}\\${selector}`;
    if (!searches.has(key)) {
      searches.set(key, [toRegExp(search), selector, raceWinner()
      ], formattedArgs);
    }

    const debugLog = getDebugger("hide-if-shadow-contains");
    const {mark, end} = profile("hide-if-shadow-contains");

    if (!observer) {
      observer = new MutationObserver(records => {
        mark();
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
                         `\nFILTER: hide-if-shadow-contains ${formattedArgs}`);
              }
              end();
            }
          }
        }
      });

      Object$7.defineProperty(ElementProto, "attachShadow", {

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

  const {Array: Array$1, Error: Error$6, JSON: JSON$2, Map: Map$5, Object: Object$6, Response: Response$2} = $(window);

  let paths$1 = null;

  function jsonOverride(rawOverridePaths, value,
                               rawNeedlePaths = "", filter = "") {
    if (!rawOverridePaths)
      throw new Error$6("[json-override snippet]: Missing paths to override.");

    if (typeof value == "undefined")
      throw new Error$6("[json-override snippet]: No value to override with.");

    if (!paths$1) {
      let debugLog = getDebugger("json-override");
      const {mark, end} = profile("json-override");
      mark();

      function overrideObject(obj, str) {

        for (let {formattedArgs, prune, needle, filter: flt, value: val} of paths$1.values()) {
          if (flt && !flt.test(str))
            continue;

          if ($(needle).some(path => !findOwner(obj, path)))
            return obj;

          for (let path of prune) {
            if (path.includes("{}") || path.includes("[]"))
              overridePathWithPlaceholders(obj, path, val, formattedArgs);
            else
              overridePathSimple(obj, path, val, formattedArgs);
          }
        }
        return obj;
      }

      function overridePathWithPlaceholders(obj, path, newValue, formattedArgs) {
        let pathParts = $(path).split(".");
        let currentObj = obj;

        for (let i = 0; i < pathParts.length; i++) {
          let part = pathParts[i];

          if (part === "[]") {

            if (Array$1.isArray(currentObj)) {
              debugLog("info", `Iterating over array at: ${part}`);
              $(currentObj).forEach(item => {
                if (item !== null && typeof item !== "undefined") {
                  overridePathWithPlaceholders(item,
                                               pathParts.slice(i + 1).join("."),
                                               newValue,
                                               formattedArgs);
                }
              });
            }
            return;
          }
          else if (part === "{}") {

            if (currentObj && typeof currentObj === "object") {
              debugLog("info", `Iterating over object at: ${part}`);
              Object$6.keys(currentObj).forEach(key => {
                let nextItem = currentObj[key];
                if (nextItem !== null && typeof nextItem !== "undefined") {
                  overridePathWithPlaceholders(nextItem,
                                               pathParts.slice(i + 1).join("."),
                                               newValue,
                                               formattedArgs);
                }
              });
            }
            return;
          }
          else if (currentObj && typeof currentObj === "object" &&
            hasOwnProperty(currentObj, part)) {

            if (i === pathParts.length - 1) {
              debugLog("success", `Found ${path}, replaced it with ${newValue}`, `\nFILTER: json-override ${formattedArgs}`);
              currentObj[part] = overrideValue(newValue);
            }
            else {
              currentObj = currentObj[part];
            }
          }
          else {
            return;
          }
        }
      }

      function overridePathSimple(obj, path, newValue, formattedArgs) {
        let details = findOwner(obj, path);
        if (typeof details != "undefined") {
          debugLog("success", `Found ${path}, replaced it with ${newValue}`, `\nFILTER: json-override ${formattedArgs}`);
          details[0][details[1]] = overrideValue(newValue);
        }
      }

      let {parse} = JSON$2;
      paths$1 = new Map$5();

      Object$6.defineProperty(window.JSON, "parse", {
        value: proxy(parse, function(str) {
          let result = apply$2(parse, this, arguments);
          return overrideObject(result, str);
        })
      });
      debugLog("info", "Wrapped JSON.parse for override");

      let {json} = Response$2.prototype;
      Object$6.defineProperty(window.Response.prototype, "json", {
        value: proxy(json, function(str) {
          let resultPromise = apply$2(json, this, arguments);
          return resultPromise.then(obj => overrideObject(obj, str));
        })
      });
      debugLog("info", "Wrapped Response.json for override");
      end();
    }

    const formattedArgsToLog = formatArguments(arguments);

    paths$1.set(rawOverridePaths, {
      formattedArgs: formattedArgsToLog,
      prune: $(rawOverridePaths).split(/ +/),
      needle: rawNeedlePaths.length ? $(rawNeedlePaths).split(/ +/) : [],
      filter: filter ? toRegExp(filter) : null,
      value
    });
  }

  let {Array, Error: Error$5, JSON: JSON$1, Map: Map$4, Object: Object$5, Response: Response$1} = $(window);

  let paths = null;

  function jsonPrune(rawPrunePaths,
                            rawNeedlePaths = "",
                            rawNeedleStack = "") {
    if (!rawPrunePaths)
      throw new Error$5("Missing paths to prune");

    if (!paths) {
      let debugLog = getDebugger("json-prune");
      const {mark, end} = profile("json-prune");
      mark();

      function pruneObject(obj) {
        for (let {prune, needle, stackNeedle, formattedArgs} of paths.values()) {

          if ($(needle).length > 0 &&
            $(needle).some(path => !findOwner(obj, path)))
            return obj;

          if ($(stackNeedle) &&
              $(stackNeedle).length > 0 &&
              !matchesStackTrace(stackNeedle, debugLog))
            return obj;

          for (let path of prune) {
            if (path.includes("{}") || path.includes("[]"))
              prunePathWithPlaceholders(obj, path, formattedArgs);
            else
              prunePathSimple(obj, path, formattedArgs);
          }
        }
        return obj;
      }

      function prunePathWithPlaceholders(obj, path, formattedArgs) {
        let pathParts = $(path).split(".");
        let currentObj = obj;

        for (let i = 0; i < pathParts.length; i++) {
          let part = pathParts[i];

          if (part === "[]") {
            if (Array.isArray(currentObj)) {
              debugLog("info", `Iterating over array at: ${part}`);
              $(currentObj).forEach(item =>
                prunePathWithPlaceholders(item,
                                          pathParts.slice(i + 1).join("."),
                                          formattedArgs));
            }
            return;
          }
          else if (part === "{}") {
            if (typeof currentObj === "object" && currentObj !== null) {
              debugLog("info", `Iterating over object at: ${part}`);
              Object$5.keys(currentObj).forEach(key =>
                prunePathWithPlaceholders(currentObj[key],
                                          pathParts.slice(i + 1).join("."),
                                          formattedArgs));
            }
            return;
          }
          else if (currentObj && typeof currentObj === "object" &&
            hasOwnProperty(currentObj, part)) {
            if (i === pathParts.length - 1) {
              debugLog("success", `Found ${path} and deleted, \nFILTER: json-prune ${formattedArgs}`);
              delete currentObj[part];
            }
            else {
              currentObj = currentObj[part];
            }
          }
          else {
            return;
          }
        }
      }

      function prunePathSimple(obj, path, formattedArgs) {
        let details = findOwner(obj, path);
        if (typeof details != "undefined") {
          debugLog("success", `Found ${path} and deleted`, `\nFILTER: json-prune ${formattedArgs}`);
          delete details[0][details[1]];
        }
      }

      let {parse} = JSON$1;
      paths = new Map$4();

      Object$5.defineProperty(window.JSON, "parse", {
        value: proxy(parse, function() {
          let result = apply$2(parse, this, arguments);
          return pruneObject(result);
        })
      });
      debugLog("info", "Wrapped JSON.parse for prune");

      let {json} = Response$1.prototype;
      Object$5.defineProperty(window.Response.prototype, "json", {
        value: proxy(json, function() {
          let resultPromise = apply$2(json, this, arguments);
          return resultPromise.then(obj => pruneObject(obj));
        })
      });
      debugLog("info", "Wrapped Response.json for prune");
      end();
    }

    const formattedArgs = formatArguments(arguments);

    paths.set(rawPrunePaths, {
      formattedArgs,
      prune: $(rawPrunePaths).split(/ +/),
      needle: rawNeedlePaths.length ? $(rawNeedlePaths).split(/ +/) : [],
      stackNeedle: rawNeedleStack.length ? $(rawNeedleStack).split(/ +/) : []
    });
  }

  const {Error: Error$4, Object: Object$4, Map: Map$3} = $(window);

  let mapValues = null;

  function isMatchingValue(val, needle, pathSegments) {

    if (!pathSegments.length) {
      if (typeof val === "string" || typeof val === "number") {
        const valStr = val.toString();
        return needle.test(valStr);
      }
      return false;
    }

    let current = val;
    for (const segment of pathSegments) {

      if (!current || !hasOwnProperty(current, segment))
        return false;
      current = current[segment];
    }

    if (typeof current === "string" || typeof current === "number") {
      const currStr = current.toString();
      return needle.test(currStr);
    }

    return false;
  }

  function mapOverride(method, needle, returnValue = "", path,
                              stack) {
    if (!method)
      throw new Error$4("[map-override snippet]: Missing method to override.");

    if (!needle)
      throw new Error$4("[map-override snippet]: Missing needle.");

    if (!mapValues)
      mapValues = new Map$3();

    let debugLog = getDebugger("map-override");
    const {mark, end} = profile("map-override");
    const formattedArgsToLog = formatArguments(arguments);

    if (method === "set" && !mapValues.has("set")) {
      mark();
      const {set} = Map$3.prototype;
      mapValues.set("set", $([]));

      Object$4.defineProperty(window.Map.prototype, "set", {
        value: proxy(set, function(key, val) {
          const overrideVals = mapValues.get("set");
          for (const {needleRegex, pathSegments, stackNeedles} of overrideVals) {

            if (isMatchingValue(val, needleRegex, pathSegments) &&
                matchesStackTrace(stackNeedles, debugLog)) {
              debugLog("success", `Map.set is ignored for value matching needle: ${needleRegex}\nFILTER: map-override ${formattedArgsToLog}`);
              return this;
            }
          }
          return apply$2(set, this, arguments);
        })
      });
      debugLog("info", "Wrapped Map.prototype.set");
      end();
    }

    else if (method === "get" && !mapValues.has("get")) {
      mark();
      const {get} = Map$3.prototype;
      mapValues.set("get", $([]));

      Object$4.defineProperty(window.Map.prototype, "get", {
        value: proxy(get, function(key) {
          const overrideVals = mapValues.get("get");
          for (const {needleRegex, retVal, stackNeedles} of overrideVals) {

            if (typeof key === "string" || typeof key === "number") {
              const keyStr = key.toString();
              if (needleRegex.test(keyStr) &&
                  matchesStackTrace(stackNeedles, debugLog)) {
                debugLog("success", `Map.get returned ${retVal} for key: ${keyStr}\nFILTER: map-override ${formattedArgsToLog}`);
                return retVal;
              }
            }
          }
          return apply$2(get, this, arguments);
        })
      });
      debugLog("info", "Wrapped Map.prototype.get");
      end();
    }

    else if (method === "has" && !mapValues.has("has")) {
      mark();
      const {has} = Map$3.prototype;
      mapValues.set("has", $([]));

      Object$4.defineProperty(window.Map.prototype, "has", {
        value: proxy(has, function(key) {
          const overrideVals = mapValues.get("has");
          for (const {needleRegex, retVal, stackNeedles} of overrideVals) {

            if (typeof key === "string" || typeof key === "number") {
              const keyStr = key.toString();
              if (needleRegex.test(keyStr) &&
                  matchesStackTrace(stackNeedles, debugLog)) {
                debugLog("success", `Map.has returned ${retVal} for key: ${keyStr}\nFILTER: map-override ${formattedArgsToLog}`);
                return retVal;
              }
            }
          }
          return apply$2(has, this, arguments);
        })
      });
      debugLog("info", "Wrapped Map.prototype.has");
      end();
    }

    const needleRegex = toRegExp(needle);
    let pathSegments = [];
    if (path)
      pathSegments = path.split(".");

    let stackNeedles = [];
    if (stack)
      stackNeedles = stack.split(",").map(s => s.trim());

    const overrideVals = mapValues.get(method);

    let retVal;
    if (method === "get") {

      retVal = returnValue === "" ? void 0 : returnValue;
    }
    else if (method === "has") {

      retVal = returnValue === "true";
    }

    overrideVals.push({needleRegex, retVal, pathSegments, stackNeedles});
    mapValues.set(method, overrideVals);
  }

  let {Error: Error$3} = $(window);

  function overridePropertyRead(property, value, setConfigurable) {
    if (!property) {
      throw new Error$3("[override-property-read snippet]: " +
                       "No property to override.");
    }
    if (typeof value === "undefined") {
      throw new Error$3("[override-property-read snippet]: " +
                       "No value to override with.");
    }

    const formattedArguments = formatArguments(arguments);
    let debugLog = getDebugger("override-property-read");
    const {mark, end} = profile("override-property-read");

    let cValue = overrideValue(value);

    let newGetter = () => {
      debugLog("success", `${property} override done.`, "\nFILTER: override-property-read", formattedArguments);
      return cValue;
    };

    debugLog("info", `Overriding ${property}.`);

    const configurableFlag = !(setConfigurable === "false");
    mark();
    wrapPropertyAccess(window,
                       property,
                       {get: newGetter, set() {}},
                       configurableFlag);
    end();
  }

  let {Error: Error$2, Map: Map$2, Object: Object$3, console: console$1} = $(window);

  let {toString} = Function.prototype;
  let EventTargetProto = EventTarget.prototype;
  let {addEventListener} = EventTargetProto;

  let events = null;

  function preventListener(event, eventHandler, selector) {
    if (!event)
      throw new Error$2("[prevent-listener snippet]: No event type.");

    if (!events) {
      events = new Map$2();

      let debugLog = getDebugger("[prevent]");
      const {mark, end} = profile("prevent-listener");

      Object$3.defineProperty(EventTargetProto, "addEventListener", {
        value: proxy(addEventListener, function(type, listener) {
          mark();
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
                console$1.groupCollapsed("DEBUG [prevent] was successful", `\nFILTER: prevent-listener ${formattedArgs}`);
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
          end();
          return apply$2(addEventListener, this, arguments);
        })
      });

      debugLog("info", "Wrapped addEventListener");
    }

    const formattedArgsToLog = formatArguments(arguments);

    if (!events.has(event)) {
      events.set(event,
                 {evt: toRegExp(event),
                  handlers: [],
                  selectors: [],
                  formattedArgs: formattedArgsToLog});
    }

    let {handlers, selectors, formattedArgs} = events.get(event);

    handlers.push(eventHandler ? toRegExp(eventHandler) : null);
    selectors.push(selector);
  }

  let {fetch} = $(window);

  let hasFetchBeenProxied = false;

  const preFetchCallbacks = [];

  const postFetchCallbacks = [];

  const proxyFetch = () => {

    if (!hasFetchBeenProxied) {
      window.fetch = proxy(fetch, (...args) => {
        let [source] = args;
        if (preFetchCallbacks.length > 0 && typeof source === "string") {
          let url;
          try {
            url = new URL(source);
          }
          catch (e) {
            if (e instanceof TypeError)
              url = new URL(source, $(document).location);
            else
              throw e;
          }
          preFetchCallbacks.forEach(fn => fn(url));
          args[0] = url.href;
        }

        const promise = apply$2(fetch, self, args).then(origResponse => {
          let transformedResponse = origResponse;
          postFetchCallbacks.forEach(fn => {
            transformedResponse = fn(transformedResponse);
          });
          return transformedResponse;
        });
        return promise;
      });
      hasFetchBeenProxied = true;
    }
  };

  const addPreFetchCallback = callback => {
    preFetchCallbacks.push(callback);
    proxyFetch();
  };

  const addPostFetchCallback = callback => {
    postFetchCallbacks.push(callback);
    proxyFetch();
  };

  let {Map: Map$1, Object: Object$2, RegExp: RegExp$2, Response} = $(window);
  let fetchRules;

  function replaceFetchResponse(search, replacement = "", needle = null) {
    const formattedArgsToLog = formatArguments(arguments);
    const debugLog = getDebugger("replace-fetch-response");
    const {mark, end} = profile("replace-fetch-response");
    if (!search) {
      debugLog("error", "The parameter 'search' is required");
      return;
    }

    if (!fetchRules) {
      const mainLogic = origResponse => {
        mark();
        const clonedResponse = $(origResponse).clone();
        return clonedResponse.text().then(origText => {
          let replacedText = $(origText);

          for (const [thisSearch, {replacement: thisReplacement, needle: thisNeedle, formattedArgs}] of fetchRules) {
            if (thisNeedle) {
              const needleRegex = toRegExp(thisNeedle);

              if (needleRegex.test(replacedText)) {
                if (debug()) {
                  console.groupCollapsed(`DEBUG [replace-fetch-response] success: '${thisNeedle}' found in fetch response`);
                  debugLog("info", `${replacedText}`);
                  console.groupEnd();
                }
              }
              else {
                if (debug()) {
                  console.groupCollapsed(`DEBUG [replace-fetch-response] warn: '${thisNeedle}' not found in fetch response`);
                  debugLog("warn", `${replacedText}`);
                  console.groupEnd();
                }
                continue;
              }
            }
            replacedText = replacedText.replace(thisSearch, thisReplacement);
            if (debug() && replacedText.toString() !== origText.toString()) {
              console.groupCollapsed(`DEBUG [replace-fetch-response] success: '${thisSearch}' replaced with '${thisReplacement}' in fetch response`,
                `\nFILTER: replace-fetch-response ${formattedArgs}`
              );
              debugLog("success", `${replacedText}`);
              console.groupEnd();
            }
          }

          if (replacedText.toString() === origText.toString())
            return origResponse;

          const replacedResponse = new Response(replacedText.toString(), {
            status: origResponse.status,
            statusText: origResponse.statusText,
            headers: origResponse.headers
          });
          Object$2.defineProperties(replacedResponse, {
            ok: {value: origResponse.ok},
            redirected: {value: origResponse.redirected},
            type: {value: origResponse.type},
            url: {value: origResponse.url}
          });
          end();
          return replacedResponse;
        });
      };

      fetchRules = new Map$1();
      debugLog("info", "Network API proxied");
      addPostFetchCallback(mainLogic);
    }

    const regex = toRegExp(search);

    const globalisedRegEx = new RegExp$2(regex, "g");
    fetchRules.set(globalisedRegEx,
                   {replacement, needle, formattedArgs: formattedArgsToLog});
  }

  const {Error: Error$1, Object: Object$1, atob, btoa, RegExp: RegExp$1} = $(window);

  function replaceOutboundValue(methodPath, textToReplace = "",
                                       replacement = "", decodeMethod = "",
                                       path = "", stack = "") {
    if (!methodPath)
      throw new Error$1("[replace-outbound-value snippet]: Missing method path.");

    let debugLog = getDebugger("replace-outbound-value");
    const {mark, end} = profile("replace-outbound-value");
    formatArguments(arguments);

    function getPropertyInChain(base, propertyPath) {
      let object = base;
      let chain = $(propertyPath).split(".");

      for (let i = 0; i < chain.length - 1; i++) {
        let prop = chain[i];
        if (!object || typeof object !== "object") {
          return {
            base: object,
            prop,
            remainingPath: chain.slice(i).join("."),
            success: false
          };
        }
        object = object[prop];
      }

      let prop = chain[chain.length - 1];
      return {
        base: object,
        prop,
        success: true
      };
    }

    function isValidBase64(str) {
      try {
        if (str === "")
          return false;

        const decodedString = atob(str);
        const encodedString = btoa(decodedString);

        const stringWithoutPadding = $(str).replace(/=+$/, "").toString();
        const encodedStringWithoutPadding = $(encodedString).replace(/=+$/, "").toString();
        return encodedStringWithoutPadding === stringWithoutPadding;
      }
      catch (e) {
        return false;
      }
    }

    function decodeAndReplaceContent(content, pattern, textReplacement, decode) {
      switch (decode) {
        case "base64":
          try {
            const isBase64Encoded = isValidBase64(content);

            if (isBase64Encoded) {

              const decodedContent = atob(content);
              debugLog("info", `Decoded base64 content: ${decodedContent}`);

              const modifiedContent = pattern ?
                $(decodedContent).replace(pattern, textReplacement).toString() :
                decodedContent;

              const message = modifiedContent !== decodedContent ?
                `Modified decoded content: ${modifiedContent}` :
                "Decoded content was not modified";

              debugLog("info", message);

              const encodedContent = btoa(modifiedContent);
              debugLog("info", `Re-encoded to base64: ${encodedContent}`);
              return encodedContent;
            }

            debugLog("info", `Content is plain text: ${content}`);

            const modifiedContent = pattern ?
              $(content).replace(pattern, textReplacement).toString() :
              content;

            const message = modifiedContent !== content ?
              `Modified plain text content: ${modifiedContent}` :
              "Plain text content was not modified";

            debugLog("info", message);

            const encodedContent = btoa(modifiedContent);
            debugLog("info", `Encoded to base64: ${encodedContent}`);
            return encodedContent;
          }
          catch (e) {
            debugLog("info", `Error processing base64 content: ${e.message}`);
            return content;
          }
        default:
          return pattern ?
            $(content).replace(pattern, textReplacement).toString() :
            content;
      }
    }

    function replaceValueAtPath(obj, pathSegments, pattern, textReplacement,
                                decode) {
      if (!pathSegments.length)
        return obj;

      let current = obj;

      for (let i = 0; i < pathSegments.length - 1; i++) {
        if (!current || typeof current !== "object") {
          debugLog("info", `Cannot navigate to path: property '${pathSegments[i]}' not found`);
          return obj;
        }
        current = current[pathSegments[i]];
      }

      const finalProp = pathSegments[pathSegments.length - 1];
      if (!current || typeof current !== "object" || !(finalProp in current)) {
        debugLog("info", `Target property '${finalProp}' not found at path`);
        return obj;
      }

      const originalValue = current[finalProp];
      if (typeof originalValue !== "string") {
        debugLog("info", `Property at path is not a string: ${typeof originalValue}`);
        return obj;
      }

      const modifiedValue = decodeAndReplaceContent(
        originalValue, pattern, textReplacement, decode);

      if (modifiedValue !== originalValue) {

        const result = JSON.parse(JSON.stringify(obj));
        let resultCurrent = result;

        for (let i = 0; i < pathSegments.length - 1; i++)
          resultCurrent = resultCurrent[pathSegments[i]];
        resultCurrent[finalProp] = modifiedValue;

        debugLog("info", `Replaced value at path '${pathSegments.join(".")}': '${originalValue}' -> '${modifiedValue}'`);
        return result;
      }

      return obj;
    }

    function processReturnValue(returnValue, pathParts, textPattern, replaceWith,
                                decode, formattedArgs) {

      const patternRegexp = textPattern ? new RegExp$1(toRegExp(textPattern), "g") :
       null;
      if (pathParts.length && typeof returnValue === "object" &&
          returnValue !== null) {
        const modifiedObject = textPattern ?
          replaceValueAtPath(
            returnValue,
            pathParts,
            patternRegexp,
            replaceWith,
            decode
          ) :
          returnValue;

        if (modifiedObject !== returnValue) {
          debugLog("success",
                 `Replaced outbound value\nFILTER: replace-outbound-value ${formattedArgs}`);
        }

        return modifiedObject;
      }

      else if (typeof returnValue === "string") {
        if (!textPattern)
          debugLog("info", `Original text content: ${returnValue}`);

        const modifiedContent = textPattern ?
          decodeAndReplaceContent(
            returnValue,
            patternRegexp,
            replaceWith,
            decode
          ) :
          returnValue;

        if (modifiedContent !== returnValue) {
          debugLog("success",
                 `Replaced outbound value: ${modifiedContent} \nFILTER: replace-outbound-value ${formattedArgs}`);
        }

        return modifiedContent;
      }

      debugLog("info", pathParts.length ?
        "Content is not an object or path not specified" :
        "Content is not a string");
      return returnValue;
    }

    mark();

    const result = getPropertyInChain(window, methodPath);

    if (!result.success) {
      debugLog("error", `Could not reach the end of the prop chain: ${methodPath}. Remaining path: ${result.remainingPath}`);
      end();
      return;
    }

    const {base, prop} = result;
    const nativeMethod = base[prop];
    if (!nativeMethod || typeof nativeMethod !== "function") {
      debugLog("error", `Could not retrieve the method: ${methodPath}`);
      end();
      return;
    }

    let pathSegments = [];
    if (path)
      pathSegments = $(path).split(".");

    let stackNeedles = [];
    if (stack)
      stackNeedles = $(stack).split(",").map(s => s.trim());

    let isMatchingSuspended = false;

    Object$1.defineProperty(base, prop, {
      value: proxy(nativeMethod, function() {
        if (isMatchingSuspended)
          return apply$2(nativeMethod, this, arguments);

        isMatchingSuspended = true;
        const methodResult = apply$2(nativeMethod, this, arguments);

        if (stackNeedles.length && !matchesStackTrace(stackNeedles, debugLog)) {
          isMatchingSuspended = false;
          return methodResult;
        }

        if (methodResult && typeof methodResult.then === "function") {
          debugLog("info", "Method returned a Promise, modifying resolved value");

          isMatchingSuspended = false;
          return methodResult.then(resolvedValue => {
            const valueType = typeof resolvedValue === "object" ?
              JSON.stringify(resolvedValue) : resolvedValue;
            debugLog("info", `Promise resolved with value: ${valueType}`);

            return processReturnValue(
              resolvedValue,
              pathSegments,
              textToReplace,
              replacement,
              decodeMethod,
              path);
          }).catch(error => {
            debugLog("info", `Promise rejected: ${error.message}`);
            throw error;
          });
        }

        const processedResult = processReturnValue(
          methodResult,
          pathSegments,
          textToReplace,
          replacement,
          decodeMethod,
          path);
        isMatchingSuspended = false;
        return processedResult;
      })
    });

    debugLog("info", `Wrapped ${methodPath}`);
    end();
  }

  let {RegExp, XMLHttpRequest, WeakMap: WeakMap$1} = $(window);
  let xhrInFlightRequests;
  let xhrRules;

  function replaceXhrResponse(search, replacement = "", needle = null) {
    const formattedArgsToLog = formatArguments(arguments);
    const debugLog = getDebugger("replace-xhr-response");
    const {mark, end} = profile("replace-xhr-response");

    if (!search) {
      debugLog("error", "The parameter 'pattern' is required");
      return;
    }

    if (!xhrInFlightRequests) {
      xhrInFlightRequests = new WeakMap$1();
      xhrRules = new Map();
      debugLog("info", "XMLHttpRequest proxied");

      window.XMLHttpRequest = class extends XMLHttpRequest {
        open(method, url, ...args) {
          const originalXhr = this;
          const xhrData = {method, url};
          xhrInFlightRequests.set(originalXhr, xhrData);
          return super.open(method, url, ...args);
        }

        send(...args) {
          return super.send(...args);
        }
        get response() {
          const innerResponse = super.response;
          const xhrData = xhrInFlightRequests.get(this);
          if (typeof xhrData === "undefined")
            return innerResponse;
          mark();

          const responseLength = typeof innerResponse === "string" ?
            innerResponse.length : void 0;
          if (xhrData.lastResponseLength !== responseLength) {
            xhrData.response = void 0;
            xhrData.lastResponseLength = responseLength;
          }

          if (typeof xhrData.response !== "undefined")
            return xhrData.response;

          if (typeof innerResponse !== "string")
            return (xhrData.response = innerResponse);

          let replacedText = innerResponse;

          for (const [thisSearch, {replacement: thisReplacement, needle: thisNeedle, formattedArgs}] of xhrRules) {
            if (thisNeedle) {
              const needleRegex = toRegExp(thisNeedle);

              if (needleRegex.test(replacedText)) {
                if (debug()) {
                  console.groupCollapsed(`DEBUG [replace-xhr-response] success: '${thisNeedle}' found in XHR response`);
                  debugLog("info", replacedText);
                  console.groupEnd();
                }
              }
              else {
                if (debug()) {
                  console.groupCollapsed(`DEBUG [replace-xhr-response] warn: '${thisNeedle}' not found in XHR response`);
                  debugLog("warn", replacedText);
                  console.groupEnd();
                }
                continue;
              }
            }
            replacedText =
              $(replacedText).replace(thisSearch, thisReplacement).toString();
            if (debug() && innerResponse.toString() !== replacedText.toString()) {
              console.groupCollapsed(`DEBUG [replace-xhr-response] success: '${thisSearch}' replaced with '${thisReplacement}' in XHR response`,
                                      `\nFILTER: replace-xhr-response ${formattedArgs}`);
              debugLog("success", replacedText);
              console.groupEnd();
            }
          }
          end();
          return (xhrData.response = replacedText.toString());
        }
        get responseText() {
          const response = this.response;
          if (typeof response !== "string")
            return super.responseText;

          return response;
        }
      };
    }

    const regex = toRegExp(search);

    const globalisedRegEx = new RegExp(regex, "g");
    xhrRules.set(globalisedRegEx,
                 {replacement, needle, formattedArgs: formattedArgsToLog});
  }

  let {delete: deleteParam, has: hasParam} = caller(URLSearchParams.prototype);

  let parameters;

  function stripFetchQueryParameter(name, urlPattern = null) {
    const formattedArgs = formatArguments(arguments);
    const debugLog = getDebugger("strip-fetch-query-parameter");
    const {mark, end} = profile("strip-fetch-query-parameter");

    const stripFunction = url => {
      mark();
      for (let [key, value] of parameters.entries()) {
        const {reg, args} = value;
        if (!reg || reg.test(url)) {
          if (hasParam(url.searchParams, key)) {
            debugLog("success", `${key} has been stripped from url ${url}`, `\nFILTER: strip-fetch-query-parameter ${args}`);
            deleteParam(url.searchParams, key);
          }
        }
      }
      end();
    };

    if (!parameters) {
      parameters = new Map();
      addPreFetchCallback(stripFunction);
    }

    parameters.set(name,
                   {reg: urlPattern && toRegExp(urlPattern),
                    args: formattedArgs});
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
    "array-override": arrayOverride,
    "blob-override": blobOverride,
    "cookie-remover": cookieRemover,
    "profile": setProfile,
    "debug": setDebug,
    "event-override": eventOverride,
    "freeze-element": freezeElement,
    "hide-if-canvas-contains": hideIfCanvasContains,
    "hide-if-shadow-contains": hideIfShadowContains,
    "json-override": jsonOverride,
    "json-prune": jsonPrune,
    "map-override": mapOverride,
    "override-property-read": overridePropertyRead,
    "prevent-listener": preventListener,
    "replace-fetch-response": replaceFetchResponse,
    "replace-outbound-value": replaceOutboundValue,
    "replace-xhr-response": replaceXhrResponse,
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
const graph = new Map([["abort-current-inline-script",null],["abort-on-iframe-property-read",null],["abort-on-iframe-property-write",null],["abort-on-property-read",null],["abort-on-property-write",null],["array-override",null],["blob-override",null],["cookie-remover",null],["profile",null],["debug",null],["event-override",null],["freeze-element",null],["hide-if-canvas-contains",null],["hide-if-shadow-contains",null],["json-override",null],["json-prune",null],["map-override",null],["override-property-read",null],["prevent-listener",null],["replace-fetch-response",null],["replace-outbound-value",null],["replace-xhr-response",null],["strip-fetch-query-parameter",null],["trace",null]]);
callback.get = snippet => graph.get(snippet);
callback.has = snippet => graph.has(snippet);
export default callback;