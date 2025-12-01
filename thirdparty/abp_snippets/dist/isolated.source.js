(environment, ...filters) => {
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
    getOwnPropertyDescriptor: getOwnPropertyDescriptor$2,
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
    getOwnPropertyDescriptor: getOwnPropertyDescriptor$1,
    getOwnPropertyDescriptors
  } = bound(Object);

  const invokes = bound(globalThis);
  const classes = isExtensionContext$2 ? globalThis : secure(globalThis);
  const {Map: Map$7, RegExp: RegExp$1, Set: Set$1, WeakMap: WeakMap$4, WeakSet: WeakSet$a} = classes;

  const augment = (source, target, method = null) => {
    const known = ownKeys(target);
    for (const key of ownKeys(source)) {
      if (known.includes(key))
        continue;

      const descriptor = getOwnPropertyDescriptor$1(source, key);
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

  const variables$2 = freeze({
    frozen: new WeakMap$4(),
    hidden: new WeakSet$a(),
    iframePropertiesToAbort: {
      read: new Set$1(),
      write: new Set$1()
    },
    abortedIframes: new WeakMap$4()
  });

  const startsCapitalized = new RegExp$1("^[A-Z]");
  const extensionApi = (
    isExtensionContext$2 && (
      (chromeObjAvailable && chrome) ||
      (browserObjAvailable && browser)
    )
  ) || void 0;

  var env = new Proxy(new Map$7([

    ["chrome", extensionApi],
    ["browser", extensionApi],
    ["isExtensionContext", isExtensionContext$2],
    ["variables", variables$2],

    ["console", copyIfExtension(console)],
    ["document", globalThis.document],
    ["JSON", copyIfExtension(JSON)],
    ["Map", Map$7],
    ["Math", copyIfExtension(Math)],
    ["Number", isExtensionContext$2 ? Number : primitive("Number")],
    ["RegExp", RegExp$1],
    ["Set", Set$1],
    ["String", isExtensionContext$2 ? String : primitive("String")],
    ["WeakMap", WeakMap$4],
    ["WeakSet", WeakSet$a],

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

  const {Map: Map$6, WeakMap: WeakMap$3, WeakSet: WeakSet$9, setTimeout: setTimeout$3} = env;

  let cleanup = true;
  let cleanUpCallback = map => {
    map.clear();
    cleanup = !cleanup;
  };

  var transformer = transformOnce.bind({
    WeakMap: WeakMap$3,
    WeakSet: WeakSet$9,

    WeakValue: class extends Map$6 {
      set(key, value) {
        if (cleanup) {
          cleanup = !cleanup;
          setTimeout$3(cleanUpCallback, 0, this);
        }
        return super.set(key, value);
      }
    }
  });

  const {concat, includes, join, reduce, unshift} = caller([]);

  const globals = secure(globalThis);

  const {
    Map: Map$5,
    WeakMap: WeakMap$2
  } = globals;

  const map = new Map$5;
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
    Array: Array$4,
    Number: Number$1,
    String: String$1,
    Object: Object$3
  } = env;

  const {isArray} = Array$4;
  const {getOwnPropertyDescriptor, setPrototypeOf: setPrototypeOf$1} = Object$3;

  const {toString} = Object$3.prototype;
  const {slice} = String$1.prototype;
  const getBrand = value => call(slice, call(toString, value), 8, -1);

  const {get: nodeType} = getOwnPropertyDescriptor(Node.prototype, "nodeType");

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
      return setPrototypeOf$1(value, Array$4.prototype);

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

  let debugging = false;

  function debug() {
    return debugging;
  }

  function setDebug() {
    debugging = true;
  }

  let {
    console: console$2,
    document: document$1,
    getComputedStyle: getComputedStyle$6,
    isExtensionContext,
    variables: variables$1,
    Array: Array$3,
    MutationObserver: MutationObserver$a,
    Object: Object$2,
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
        console$2.log("Shadow root not found or not added in element yet", element);
      return shadowRoot;
    }
    catch (error) {
      if (debug() && !failSilently)
        console$2.log("Error while accessing shadow root", element, error);
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
        console$2.log("No elements found matching", xlinkHref);
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
            console$2.log(
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

  function $closest(element, selector, shadowRootParents = []) {
    if (selector.includes("^^svg^^"))
      selector = selector.split("^^svg^^")[0];

    if (selector.includes("^^sh^^")) {

      const splitSelector = selector.split("^^sh^^");
      const numShadowRootsToCross = splitSelector.length - 1;
      selector = `:host ${splitSelector[numShadowRootsToCross]}`;

      if (numShadowRootsToCross === shadowRootParents.length) {

        return element.closest(selector);
      }

      const shadowRootParent = shadowRootParents[numShadowRootsToCross];
      return shadowRootParent.closest(selector);
    }
    if (shadowRootParents[0])
      return shadowRootParents[0].closest(selector);
    return element.closest(selector);
  }

  function $childNodes(element, failSilently = true) {
    const shadowRoot = $openOrClosedShadowRoot(element, failSilently);
    if (shadowRoot)
      return shadowRoot.childNodes;

    return $(element).childNodes;
  }

  const {assign, setPrototypeOf} = Object$2;

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

    new MutationObserver$a(() => {
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
    return () => Array$3.from($$(selector));
  }

  function hideIfMatches(match, selector, searchSelector, onHideCallback) {
    if (searchSelector == null)
      searchSelector = selector;

    let won;
    const callback = () => {
      for (const {element, rootParents} of $$(searchSelector, true)) {
        const closest = $closest($(element), selector, rootParents);
        if (closest && match(element, closest, rootParents)) {
          won();
          if (hideElement(closest) && typeof onHideCallback === "function")
            onHideCallback(closest);
        }
      }
    };
    return assign(
      new MutationObserver$a(callback),
      {
        race(win) {
          won = win;
          this.observe(document$1, {childList: true,
                                  characterData: true,
                                  subtree: true});
          callback();
        }
      }
    );
  }

  function isVisible(element, style, closest, shadowRootParents) {
    let $style = $(style, "CSSStyleDeclaration");
    if ($style.getPropertyValue("display") == "none")
      return false;

    let visibility = $style.getPropertyValue("visibility");
    if (visibility == "hidden" || visibility == "collapse")
      return false;

    if (!closest || element == closest)
      return true;

    let parent = $(element).parentElement;
    if (!parent) {

      if (shadowRootParents && shadowRootParents.length) {
        parent = shadowRootParents[shadowRootParents.length - 1];
        shadowRootParents = shadowRootParents.slice(0, -1);
      }
      else {
        return true;
      }
    }

    return isVisible(
      parent, getComputedStyle$6(parent), closest, shadowRootParents
    );
  }

  function getComputedCSSText(element) {
    let style = getComputedStyle$6(element);
    let {cssText} = style;

    if (cssText)
      return cssText;

    for (let property of style)
      cssText += `${property}: ${style[property]}; `;

    return $(cssText).trim();
  }

  let {Array: Array$2, Math: Math$5, RegExp} = $(window);

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

  function formatArguments(args) {
    return $(Array$2.from(args)).map(arg => `'${arg}'`).join(" ");
  }

  function toHex(number, length = 2) {
    let hex = $(number).toString(16);

    if (hex.length < length)
      hex = $("0").repeat(length - hex.length) + hex;

    return hex;
  }

  function uint8ArrayToHex(uint8Array) {
    return uint8Array.reduce((hex, byte) => hex + toHex(byte), "");
  }

  let {Math: Math$4, setInterval: setInterval$1, performance} = $(window);

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
                                Math$4.round(60000 / Math$4.min(60, rate)));
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

  const {console: console$1} = $(window);

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
    console$1.log(...args);
    end();
  }

  function getDebugger(name) {
    return bind(debug() ? log : noop, null, name);
  }

  let {Array: Array$1, Error: Error$3, Map: Map$4, parseInt: parseInt$2} = $(window);

  let stack = null;
  let won = null;

  function race(action, winners = "1") {
    switch (action) {
      case "start":
        stack = {
          winners: parseInt$2(winners, 10) || 1,
          participants: new Map$4()
        };
        won = new Array$1();
        break;
      case "end":
      case "finish":
      case "stop":
        stack = null;
        for (let win of won)
          win();
        won = null;
        break;
      default:
        throw new Error$3(`Invalid action: ${action}`);
    }
  }

  function raceWinner(name, lose) {

    if (stack === null)
      return noop;

    let current = stack;
    let {participants} = current;
    participants.set(win, lose);

    return win;

    function win() {

      if (current.winners < 1)
        return;

      let debugLog = getDebugger("race");
      debugLog("success", `${name} won the race`);

      if (current === stack) {
        won.push(win);
      }
      else {
        participants.delete(win);
        if (--current.winners < 1) {
          for (let looser of participants.values())
            looser();

          participants.clear();
        }
      }
    }
  }

  function hideIfContains(search, selector = "*", searchSelector = null) {
    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("hide-if-contains");
    const {mark, end} = profile("hide-if-contains");
    const onHideCallback = node => {
      mark();
      debugLog("success",
               "Matched: ",
               node,
               "\nFILTER: hide-if-contains",
               formattedArguments);
      end();
    };
    let re = toRegExp(search);

    const mo = hideIfMatches(element => re.test($(element).textContent),
                             selector,
                             searchSelector,
                             onHideCallback);
    mo.race(raceWinner(
      "hide-if-contains",
      () => {
        mo.disconnect();
      }
    ));
  }

  const handler = {
    get(target, name) {
      const context = target;
      while (!hasOwnProperty(target, name))
        target = getPrototypeOf(target);
      const {get, set} = getOwnPropertyDescriptor$2(target, name);
      return function () {
        return arguments.length ?
                apply$2(set, context, arguments) :
                call(get, context);
      };
    }
  };

  const accessor = target => new $$1(target, handler);

  let {
    parseFloat: parseFloat$4,
    variables,
    clearTimeout,
    fetch,
    setTimeout: setTimeout$2,
    Array,
    Error: Error$2,
    Map: Map$3,
    Object: Object$1,
    ReferenceError,
    Set,
    WeakMap: WeakMap$1
  } = $(window);

  accessor(window);

  $(/^\d+$/);

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

  function waitUntilEvent(
    debugLog,
    mainLogic,
    waitUntil) {
    if (waitUntil) {

      if (waitUntil === "load") {
        debugLog("info", "Waiting until window.load");

        window.addEventListener("load", () => {
          debugLog("info", "Window.load fired.");
          mainLogic();
        });
      }

      else if (waitUntil === "loading" ||
              waitUntil === "interactive" ||
              waitUntil === "complete") {
        debugLog("info", "Waiting document state until :", waitUntil);

        document.addEventListener("readystatechange", () => {
          debugLog("info", "Document state changed:", document.readyState);
          if (document.readyState === waitUntil)
            mainLogic();
        });
      }

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

      mainLogic();
    }
  }

  let fetchContentMap = new Map$3();

  function fetchContent(url, {as = "arrayBuffer", cleanup = 60000} = {}) {

    let uid = as + ":" + url;
    let details = fetchContentMap.get(uid) || {
      remove: () => fetchContentMap.delete(uid),
      result: null,
      timer: 0
    };
    clearTimeout(details.timer);
    details.timer = setTimeout$2(details.remove, cleanup);
    if (!details.result) {
      details.result = fetch(url).then(res => res[as]()).catch(details.remove);
      fetchContentMap.set(uid, details);
    }
    return details.result;
  }

  let {MutationObserver: MutationObserver$9, WeakSet: WeakSet$8, getComputedStyle: getComputedStyle$5} = $(window);

  function hideIfContainsAndMatchesStyle(search,
                                                selector = "*",
                                                searchSelector = null,
                                                style = null,
                                                searchStyle = null,
                                                waitUntil,
                                                windowWidthMin = null,
                                                windowWidthMax = null
  ) {
    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("hide-if-contains-and-matches-style");
    const {mark, end} = profile("hide-if-contains-and-matches-style");
    const hiddenMap = new WeakSet$8();
    const logMap = debug() && new WeakSet$8();
    if (searchSelector == null)
      searchSelector = selector;

    const searchRegExp = toRegExp(search);

    const styleRegExp = style ? toRegExp(style) : null;
    const searchStyleRegExp = searchStyle ? toRegExp(searchStyle) : null;
    const mainLogic = () => {
      const callback = () => {
        mark();
        if ((windowWidthMin && window.innerWidth < windowWidthMin) ||
           (windowWidthMax && window.innerWidth > windowWidthMax)
        )
          return;
        for (const {element, rootParents} of $$(searchSelector, true)) {
          if (hiddenMap.has(element))
            continue;
          if (searchRegExp.test($(element).textContent)) {
            if (!searchStyleRegExp ||
              searchStyleRegExp.test(getComputedCSSText(element))) {
              const closest = $closest($(element), selector, rootParents);
              if (!closest)
                continue;
              if (!styleRegExp || styleRegExp.test(getComputedCSSText(closest))) {
                win();
                hideElement(closest);
                hiddenMap.add(element);
                debugLog("success",
                         "Matched: ",
                         closest,
                         "which contains: ",
                         element,
                         "\nFILTER: hide-if-contains-and-matches-style",
                         formattedArguments);
              }
              else {
                if (!logMap || logMap.has(closest))
                  continue;
                debugLog("info",
                         "In this element the searchStyle matched " +
                         "but style didn't:\n",
                         closest,
                         getComputedStyle$5(closest),
                         formattedArguments);
                logMap.add(closest);
              }
            }
            else {
              if (!logMap || logMap.has(element))
                continue;
              debugLog("info",
                       "In this element the searchStyle didn't match:\n",
                       element,
                       getComputedStyle$5(element),
                       formattedArguments);
              logMap.add(element);
            }
          }
        }
        end();
      };

      const mo = new MutationObserver$9(callback);
      const win = raceWinner(
        "hide-if-contains-and-matches-style",
        () => mo.disconnect()
      );
      mo.observe(document, {childList: true, characterData: true, subtree: true});
      callback();
    };
    waitUntilEvent(debugLog, mainLogic, waitUntil);
  }

  let {
    getComputedStyle: getComputedStyle$4,
    MutationObserver: MutationObserver$8,
    Uint8Array
  } = $(window);

  function hideIfContainsImage(search, selector, searchSelector) {
    if (searchSelector == null)
      searchSelector = selector;

    let searchRegExp = toRegExp(search);

    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("hide-if-contains-image");
    const {mark, end} = profile("hide-if-contains-image");

    let callback = () => {
      mark();
      for (const {element, rootParents} of $$(searchSelector, true)) {
        let style = getComputedStyle$4(element);
        let match = $(style["background-image"]).match(/^url\("(.*)"\)$/);
        if (match) {
          fetchContent(match[1]).then(content => {
            if (searchRegExp.test(uint8ArrayToHex(new Uint8Array(content)))) {
              let closest = $closest($(element), selector, rootParents);
              if (closest) {
                win();
                hideElement(closest);
                debugLog("success",
                         "Matched: ",
                         closest,
                         "\nFILTER: hide-if-contains-image",
                         formattedArguments);
              }
            }
          });
        }
      }
      end();
    };

    let mo = new MutationObserver$8(callback);
    let win = raceWinner(
      "hide-if-contains-image",
      () => mo.disconnect()
    );
    mo.observe(document, {childList: true, subtree: true});
    callback();
  }

  const {parseFloat: parseFloat$3, Math: Math$3, MutationObserver: MutationObserver$7, WeakSet: WeakSet$7} = $(window);
  const {min} = Math$3;

  const ld = (a, b) => {
    const len1 = a.length + 1;
    const len2 = b.length + 1;
    const d = [[0]];
    let i = 0;
    let I = 0;

    while (++i < len2)
      d[0][i] = i;

    i = 0;
    while (++i < len1) {
      const c = a[I];
      let j = 0;
      let J = 0;
      d[i] = [i];
      while (++j < len2) {
        d[i][j] = min(d[I][j] + 1, d[i][J] + 1, d[I][J] + (c != b[J]));
        ++J;
      }
      ++I;
    }
    return d[len1 - 1][len2 - 1];
  };

  function hideIfContainsSimilarText(
    search, selector,
    searchSelector = null,
    ignoreChars = 0,
    maxSearches = 0
  ) {
    const visitedNodes = new WeakSet$7();
    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("hide-if-contains-similar-text");
    const {mark, end} = profile("hide-if-contains-similar-text");
    const $search = $(search);
    const {length} = $search;
    const chars = length + parseFloat$3(ignoreChars) || 0;
    const find = $([...$search]).sort();
    const guard = parseFloat$3(maxSearches) || Infinity;

    if (searchSelector == null)
      searchSelector = selector;

    debugLog("info", "Looking for similar text: " + $search);

    const callback = () => {
      mark();
      for (const {element, rootParents} of $$(searchSelector, true)) {
        if (visitedNodes.has(element))
          continue;

        visitedNodes.add(element);
        const {innerText} = $(element);
        const loop = min(guard, innerText.length - chars + 1);
        for (let i = 0; i < loop; i++) {
          const str = $(innerText).substr(i, chars);
          const distance = ld(find, $([...str]).sort()) - ignoreChars;
          if (distance <= 0) {
            const closest = $closest($(element), selector, rootParents);
            debugLog("success",
                     "Found similar text: " + $search,
                     closest,
                     "\nFILTER: hide-if-contains-similar-text",
                     formattedArguments);
            if (closest) {
              win();
              hideElement(closest);
              break;
            }
          }
        }
      }
      end();
    };

    let mo = new MutationObserver$7(callback);
    let win = raceWinner(
      "hide-if-contains-similar-text",
      () => mo.disconnect()
    );
    mo.observe(document, {childList: true, characterData: true, subtree: true});
    callback();
  }

  let {getComputedStyle: getComputedStyle$3, Map: Map$2, WeakSet: WeakSet$6, parseFloat: parseFloat$2, DOMMatrix, Math: Math$2} = $(window);

  const {ELEMENT_NODE: ELEMENT_NODE$2, TEXT_NODE} = Node;

  function hideIfContainsVisibleText(search, selector,
                                            searchSelector = null,
                                            ...attributes) {
    const {mark, end} = profile("hide-if-contains-visible-text");
    const formattedArguments = formatArguments(arguments);
    let entries = $([]);
    const optionalParameters = new Map$2([
      ["-snippet-box-margin", "2"],
      ["-disable-bg-color-check", "false"],
      ["-check-is-contained", "false"],
      ["-pseudo-box-margin", "2"],
      ["-ignore-padding", "false"]
    ]);

    for (let attr of attributes) {
      attr = $(attr);
      let markerIndex = attr.indexOf(":");
      if (markerIndex < 0)
        continue;

      let key = attr.slice(0, markerIndex).trim().toString();
      let value = attr.slice(markerIndex + 1).trim().toString();

      if (key && value) {
        if (optionalParameters.has(key))
          optionalParameters.set(key, value);
        else
          entries.push([key, value]);
      }
    }

    let defaultEntries = $([
      ["opacity", "0"],
      ["font-size", "0px"],

      ["color", "rgba(0, 0, 0, 0)"]
    ]);

    let attributesMap = new Map$2(defaultEntries.concat(entries));

    function isTextVisible(element,
                           style,
                           {bgColorCheck = true, pseudoElemCheck = false} = {}) {
      if (!style)
        style = getComputedStyle$3(element);
      style = $(style);
      for (const [key, value] of attributesMap) {
        let valueAsRegex = toRegExp(value);
        if (valueAsRegex.test(style.getPropertyValue(key)))
          return false;
      }
      const color = style.getPropertyValue("color");
      if (bgColorCheck && style.getPropertyValue("background-color") === color)
        return false;

      if (!pseudoElemCheck) {
        const firstLineStyle = getComputedStyle$3(element, "::first-line");
        if (firstLineStyle) {
          return isTextVisible(element,
                               firstLineStyle,
                               {bgColorCheck, pseudoElemCheck: true});
        }
      }

      const textShadow = style.getPropertyValue("text-shadow");
      if (color.includes("rgba(0, 0, 0, 0)") &&
          (textShadow === "none" ||
          textShadow.includes("rgba(0, 0, 0, 0)"))
      )
        return false;
      return true;
    }

    function getTransformMatrix(element, pseudo = null) {
      const style = getComputedStyle$3(element, pseudo);
      let transform = style.transform;

      if (transform === "none")
        transform = "matrix(1, 0, 0, 1, 0, 0)";
      return new DOMMatrix(transform);
    }

    function getPseudoContent(element, pseudo, parentMatrix,
                              {bgColorCheck = true, translateThresh = 2} = {}) {
      let style = getComputedStyle$3(element, pseudo);

      if (!isVisible(element, style) ||
       !isTextVisible(element, style, {bgColorCheck}))
        return "";

      let {content} = $(style);
      if (content && content !== "none") {
        let strings = $([]);

        const domMatrix = getTransformMatrix(element, pseudo);
        const resultMatrix = parentMatrix.multiply(domMatrix);

        const angle = Math$2.atan2(resultMatrix.b, resultMatrix.a);
        const angleDegrees = angle * (180 / Math$2.PI);
        const rotated = Math$2.abs(angleDegrees) > 5;

        if (rotated)
          return "";

        const translated = Math$2.abs(resultMatrix.e) > translateThresh ||
                           Math$2.abs(resultMatrix.f) > translateThresh;
        if (translated)
          return "";

        content = $(content).trim().replace(
          /(["'])(?:(?=(\\?))\2.)*?\1/g,
          value => `\x01${strings.push($(value).slice(1, -1)) - 1}`
        );

        content = content.replace(
          /\s*attr\(\s*([^\s,)]+)[^)]*?\)\s*/g,
          (_, name) => $(element).getAttribute(name) || ""
        );

        return content.replace(
          /\x01(\d+)/g,
          (_, index) => strings[index]);
      }
      return "";
    }

    function isContained(childNode, parentNode, {
      boxMargin = 2,
      ignorePadding = false
    } = {}) {
      let child = $(childNode).getBoundingClientRect();
      if (ignorePadding) {
        const style = getComputedStyle$3(childNode);
        const paddingTop = parseFloat$2(style.paddingTop) || 0;
        const paddingRight = parseFloat$2(style.paddingRight) || 0;
        const paddingBottom = parseFloat$2(style.paddingBottom) || 0;
        const paddingLeft = parseFloat$2(style.paddingLeft) || 0;

        child = {
          left: child.left + paddingLeft,
          right: child.right - paddingRight,
          top: child.top + paddingTop,
          bottom: child.bottom - paddingBottom
        };
      }

      const parent = $(parentNode).getBoundingClientRect();
      const stretchedParent = {
        left: parent.left - boxMargin,
        right: parent.right + boxMargin,
        top: parent.top - boxMargin,
        bottom: parent.bottom + boxMargin
      };

      return (
        (stretchedParent.left <= child.left &&
           child.left <= stretchedParent.right &&
          stretchedParent.top <= child.top &&
           child.top <= stretchedParent.bottom) &&
        (stretchedParent.top <= child.bottom &&
           child.bottom <= stretchedParent.bottom &&
          stretchedParent.left <= child.right &&
           child.right <= stretchedParent.right)
      );
    }

    function getVisibleContent(element,
                               closest,
                               style,
                               parentOverflowNode,
                               originalElement,
                               shadowRootParents,
                               domMatrix,
                               {
                                 boxMargin = 2,
                                 bgColorCheck,
                                 checkIsContained,
                                 translateThresh
                               } = {}) {
      let checkClosest = !style;
      if (checkClosest)
        style = getComputedStyle$3(element);

      if (!isVisible(element, style, checkClosest && closest, shadowRootParents))
        return "";

      if (!parentOverflowNode &&
        (
          $(style).getPropertyValue("overflow-x") === "hidden" ||
          $(style).getPropertyValue("overflow-y") === "hidden"
        )
      )
        parentOverflowNode = element;

      if (!domMatrix)
        domMatrix = getTransformMatrix(element);

      else
        domMatrix = domMatrix.multiply(getTransformMatrix(element));

      let text = getPseudoContent(element,
                                  ":before",
                                  domMatrix,
                                  {bgColorCheck, translateThresh});
      for (let node of $childNodes($(element))) {
        switch ($(node).nodeType) {
          case ELEMENT_NODE$2:
            text += getVisibleContent(node,
                                      element,
                                      getComputedStyle$3(node),
                                      parentOverflowNode,
                                      originalElement,
                                      shadowRootParents,
                                      domMatrix,
                                      {
                                        boxMargin,
                                        bgColorCheck,
                                        checkIsContained,
                                        translateThresh
                                      }
            );
            break;
          case TEXT_NODE:

            if (parentOverflowNode) {
              if (isContained(element, parentOverflowNode, {
                boxMargin,
                ignorePadding
              }) && isTextVisible(element, style, {bgColorCheck}))
                text += $(node).nodeValue;
            }
            else if (isTextVisible(element, style, {bgColorCheck})) {
              if (checkIsContained && !isContained(element, originalElement, {
                boxMargin,
                ignorePadding
              }))
                continue;
              text += $(node).nodeValue;
            }
            break;
        }
      }
      text += getPseudoContent(element,
                               ":after",
                               domMatrix,
                               {bgColorCheck, translateThresh});
      return text;
    }

    const boxMarginStr = optionalParameters.get("-snippet-box-margin");
    const boxMargin = parseFloat$2(boxMarginStr) || 0;

    const bgColorCheckStr = optionalParameters.get("-disable-bg-color-check");
    const bgColorCheck = !(bgColorCheckStr === "true");

    const checkIsContainedStr = optionalParameters.get("-check-is-contained");
    const checkIsContained = (checkIsContainedStr === "true");

    const ignorePaddingStr = optionalParameters.get("-ignore-padding");
    const ignorePadding = (ignorePaddingStr === "true");

    const translateThreshStr = optionalParameters.get("-pseudo-box-margin");
    const translateThresh = parseFloat$2(translateThreshStr) || 0;

    let re = toRegExp(search);
    let seen = new WeakSet$6();

    const mo = hideIfMatches(
      (element, closest, rootParents) => {
        mark();
        if (seen.has(element))
          return false;

        seen.add(element);
        let text = getVisibleContent(
          element, closest, null, null, element, rootParents, null, {
            boxMargin,
            bgColorCheck,
            checkIsContained,
            translateThresh
          }
        );
        let result = re.test(text);
        if (debug() && text.length) {
          result ?

          log("success", result, re, text, "\nFILTER: hide-if-contains-visible-text", formattedArguments) :
          log("info", result, re, text);
        }
        end();
        return result;
      },
      selector,
      searchSelector
    );
    mo.race(raceWinner(
      "hide-if-contains-visible-text",
      () => {
        mo.disconnect();
      }
    ));
  }

  let {MutationObserver: MutationObserver$6, WeakSet: WeakSet$5, getComputedStyle: getComputedStyle$2} = $(window);

  function hideIfHasAndMatchesStyle(search,
                                           selector = "*",
                                           searchSelector = null,
                                           style = null,
                                           searchStyle = null,
                                           waitUntil = null,
                                           windowWidthMin = null,
                                           windowWidthMax = null
  ) {
    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("hide-if-has-and-matches-style");
    const {mark, end} = profile("hide-if-has-and-matches-style");
    const hiddenMap = new WeakSet$5();
    const logMap = debug() && new WeakSet$5();
    if (searchSelector == null)
      searchSelector = selector;

    const styleRegExp = style ? toRegExp(style) : null;
    const searchStyleRegExp = searchStyle ? toRegExp(searchStyle) : null;
    const mainLogic = () => {
      const callback = () => {
        mark();
        if ((windowWidthMin && window.innerWidth < windowWidthMin) ||
           (windowWidthMax && window.innerWidth > windowWidthMax)
        )
          return;
        for (const {element, rootParents} of $$(searchSelector, true)) {
          if (hiddenMap.has(element))
            continue;
          if ($(element).querySelector(search) &&
              (!searchStyleRegExp ||
              searchStyleRegExp.test(getComputedCSSText(element)))) {
            const closest = $closest($(element), selector, rootParents);
            if (closest && (!styleRegExp ||
                            styleRegExp.test(getComputedCSSText(closest)))) {
              win();
              hideElement(closest);
              hiddenMap.add(element);
              debugLog("success",
                       "Matched: ",
                       closest,
                       "which contains: ",
                       element,
                       "\nFILTER: hide-if-has-and-matches-style",
                       formattedArguments);
            }
            else {
              if (!logMap || logMap.has(closest))
                continue;
              debugLog("info",
                       "In this element the searchStyle matched" +
                       "but style didn't:\n",
                       closest,
                       getComputedStyle$2(closest),
                       ...arguments);
              logMap.add(closest);
            }
          }
          else {
            if (!logMap || logMap.has(element))
              continue;
            debugLog("info",
                     "In this element the searchStyle didn't match:\n",
                     element,
                     getComputedStyle$2(element),
                     ...arguments);
            logMap.add(element);
          }
        }
        end();
      };

      const mo = new MutationObserver$6(callback);
      const win = raceWinner(
        "hide-if-has-and-matches-style",
        () => mo.disconnect()
      );
      mo.observe(document, {childList: true, subtree: true});
      callback();
    };
    waitUntilEvent(debugLog, mainLogic, waitUntil);
  }

  let {getComputedStyle: getComputedStyle$1, MutationObserver: MutationObserver$5, WeakSet: WeakSet$4} = $(window);

  function hideIfLabelledBy(search, selector, searchSelector = null) {
    const {mark, end} = profile("hide-if-labelled-by");
    let sameSelector = searchSelector == null;

    let searchRegExp = toRegExp(search);

    let matched = new WeakSet$4();

    let callback = () => {
      mark();
      for (const {element, rootParents} of $$(selector, true)) {
        let closest = sameSelector ?
                      element :
                      $closest($(element), searchSelector, rootParents);
        if (!closest ||
            !isVisible(element, getComputedStyle$1(element), closest))
          continue;

        let attr = $(element).getAttribute("aria-labelledby");
        let fallback = () => {
          if (matched.has(closest))
            return;

          if (searchRegExp.test(
            $(element).getAttribute("aria-label") || ""
          )) {
            win();
            matched.add(closest);
            hideElement(closest);
          }
        };

        if (attr) {
          for (let label of $(attr).split(/\s+/)) {
            let target = $(document).getElementById(label);
            if (target) {
              if (!matched.has(target) && searchRegExp.test(target.innerText)) {
                win();
                matched.add(target);
                hideElement(closest);
              }
            }
            else {
              fallback();
            }
          }
        }
        else {
          fallback();
        }
      }
      end();
    };

    let mo = new MutationObserver$5(callback);
    let win = raceWinner(
      "hide-if-labelled-by",
      () => mo.disconnect()
    );
    mo.observe(document, {characterData: true, childList: true, subtree: true});
    callback();
  }

  let {MutationObserver: MutationObserver$4, WeakSet: WeakSet$3} = $(window);

  const {ELEMENT_NODE: ELEMENT_NODE$1} = Node;

  function hideIfMatchesXPath(query, scopeQuery, waitUntil) {
    const {mark, end} = profile("hide-if-matches-xpath");
    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("hide-if-matches-xpath");

    const mainLogic = () => {
      const startHidingMutationObserver = scopeNode => {
        const queryAndApply = initQueryAndApply(`xpath(${query})`);
        const seenMap = new WeakSet$3();
        const hideNode = node => {
          seenMap.add(node);
          win();

          if ($(node).nodeType === ELEMENT_NODE$1)
            hideElement(node);
          else
            $(node).textContent = "";
          debugLog("success",
                   "Matched: ",
                   node,
                   "\nFILTER: hide-if-matches-xpath",
                   formattedArguments);
        };

        const callback = () => {
          mark();
          queryAndApply(node => {
            if (seenMap.has(node))
              return false;

            if (scopeQuery) {
              const scopeQueryAndApply = initQueryAndApply(`xpath(${scopeQuery})`);
              scopeQueryAndApply(matchingScopeNode => {
                if (matchingScopeNode.contains(node)) {

                  hideNode(node);
                }
                else {

                  return false;
                }
              });
            }
            else {
              hideNode(node);
            }
          });
          end();
        };
        const mo = new MutationObserver$4(callback);
        const win = raceWinner(
          "hide-if-matches-xpath",
          () => mo.disconnect()
        );
        mo.observe(
          scopeNode, {characterData: true, childList: true, subtree: true});
        callback();
      };

      if (scopeQuery) {

        let count = 0;
        let scopeMutationObserver;
        const scopeQueryAndApply = initQueryAndApply(`xpath(${scopeQuery})`);
        const findMutationScopeNodes = () => {
          scopeQueryAndApply(scopeNode => {

            startHidingMutationObserver(scopeNode);
            count++;
          });
          if (count > 0)
            scopeMutationObserver.disconnect();
        };
        scopeMutationObserver = new MutationObserver$4(findMutationScopeNodes);
        scopeMutationObserver.observe(
          document, {characterData: true, childList: true, subtree: true}
        );
        findMutationScopeNodes();
      }
      else {

        startHidingMutationObserver(document);
      }
    };

    waitUntilEvent(debugLog, mainLogic, waitUntil);
  }

  let {MutationObserver: MutationObserver$3, WeakSet: WeakSet$2} = $(window);

  const {ELEMENT_NODE} = Node;

  function hideIfMatchesComputedXPath(query, searchQuery, searchRegex,
                                             waitUntil) {
    const {mark, end} = profile("hide-if-matches-computed-xpath");
    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("hide-if-matches-computed-xpath");

    if (!searchQuery || !query) {
      debugLog("error", "No query or searchQuery provided.");
      return;
    }

    const computeQuery = foundText => query.replace("{{}}", foundText);

    const startHidingMutationObserver = foundText => {
      const computedQuery = computeQuery(foundText);
      debugLog("info",
               "Starting hiding elements that match query: ",
               computedQuery);
      const queryAndApply = initQueryAndApply(`xpath(${computedQuery})`);
      const seenMap = new WeakSet$2();
      const callback = () => {
        mark();
        queryAndApply(node => {
          if (seenMap.has(node))
            return false;
          seenMap.add(node);
          win();
          if ($(node).nodeType === ELEMENT_NODE)
            hideElement(node);
          else
            $(node).textContent = "";
          debugLog("success",
                   "Matched: ",
                   node,
                   "\nFILTER: hide-if-matches-computed-xpath",
                   formattedArguments);
        });
        end();
      };
      const mo = new MutationObserver$3(callback);
      const win = raceWinner(
        "hide-if-matches-computed-xpath",
        () => mo.disconnect()
      );
      mo.observe(
        document, {characterData: true, childList: true, subtree: true});
      callback();
    };

    const re = toRegExp(searchRegex);

    const mainLogic = () => {
      if (searchQuery) {
        debugLog("info", "Started searching for: ", searchQuery);
        const seenMap = new WeakSet$2();
        let searchMO;
        const searchQueryAndApply = initQueryAndApply(`xpath(${searchQuery})`);
        const findMutationSearchNodes = () => {
          searchQueryAndApply(searchNode => {
            if (seenMap.has(searchNode))
              return false;
            seenMap.add(searchNode);
            debugLog("info", "Found node: ", searchNode);
            if (searchNode.innerHTML) {
              debugLog("info", "Searching in: ", searchNode.innerHTML);
              const foundTextArr = searchNode.innerHTML.match(re);
              if (foundTextArr && foundTextArr.length) {
                let foundText = "";

                foundTextArr[1] ? foundText = foundTextArr[1] :
                  foundText = foundTextArr[0];
                debugLog("info", "Matched search query: ", foundText);
                startHidingMutationObserver(foundText);
              }
            }
          });
        };

        searchMO = new MutationObserver$3(findMutationSearchNodes);
        searchMO.observe(
          document, {characterData: true, childList: true, subtree: true}
        );
        findMutationSearchNodes();
      }
    };

    waitUntilEvent(debugLog, mainLogic, waitUntil);
  }

  let {
    getComputedStyle,
    MutationObserver: MutationObserver$2,
    DOMParser,
    Math: Math$1,
    Node: Node$1,
    Map: Map$1
  } = $(window);

  function hideIfSvgContains(
    search,
    selector,
    searchSelector,
    ...attributes
  ) {
    if (searchSelector == null)
      searchSelector = selector;

    const textSearchRegExp = toRegExp(search);
    let entries = $([]);
    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("hide-if-svg-contains");
    const {mark, end} = profile("hide-if-svg-contains");
    const defaultOptionalParameters = new Map$1([
      ["-position-threshold", "500"],
      ["-disable-contained-check", "false"]
    ]);
    for (let attr of attributes) {
      attr = $(attr);
      let markerIndex = attr.indexOf(":");
      if (markerIndex < 0)
        continue;

      let key = attr.slice(0, markerIndex).trim().toString();
      let value = attr.slice(markerIndex + 1).trim().toString();

      if (key && value) {
        if (defaultOptionalParameters.has(key))
          defaultOptionalParameters.set(key, value);
        else
          entries.push([key, value]);
      }
    }
    let defaultCSSEntries = $([
      ["display", "none"],
      ["visibility", "hidden"],
      ["opacity", "0"],
      ["fill", "none"],
      ["color", "rgba(0, 0, 0, 0)"],
      ["font-size", "0"]
    ]);
    let attributesMap = new Map$1(defaultCSSEntries.concat(entries));

    let callback = async() => {
      mark();
      for (const {element, rootParents} of $$(searchSelector, true)) {
        let isMatchAndVisible = false;

        try {
          const backgroundImage = $(getComputedStyle(element).backgroundImage);
          const urlMatch = backgroundImage.match(/url\("?(.+?)"?\)/);
          if (!urlMatch)
            continue;
          const url = urlMatch[1];
          const svgContent = await fetchContent(url, {as: "text"});
          const parser = new DOMParser();
          const svgDoc = parser.parseFromString(svgContent, "image/svg+xml");
          if (svgDoc.querySelector("parsererror")) {
            debugLog(
              "warn", "Failed to parse SVG content for element:", element
            );
            continue;
          }
          const textElements = svgDoc.querySelectorAll("text, tspan");
          for (const textEl of textElements) {
            if (
              isElementVisibleAndTextMatchesInSvg(
                textEl,
                textSearchRegExp
              )) {
              isMatchAndVisible = true;
              debugLog(
                "Condition met: Text found visible in SVG of element", element
              );
              break;
            }
          }
        }
        catch (error) {
          debugLog(
            "warn", "An error occurred while processing element:", element, error
          );
          continue;
        }

        if (isMatchAndVisible) {
          const closestToHide = $closest($(element), selector, rootParents);
          if (closestToHide) {
            win();
            hideElement(closestToHide);
            debugLog("success",
                     "Matched: ",
                     closestToHide,
                     "\nFILTER: hide-if-svg-contains",
                     formattedArguments);
          }
        }
      }
      end();
    };

    let mo = new MutationObserver$2(callback);
    let win = raceWinner(
      "hide-if-svg-contains",
      () => mo.disconnect()
    );
    mo.observe(document, {childList: true, subtree: true});
    callback();

    function isElementVisibleAndTextMatchesInSvg(
      element,
      searchRegExp
    ) {
      if (!searchRegExp.test(element.textContent))
        return false;

      const {ELEMENT_NODE} = Node$1;
      const positionThresh =
        parseFloat(defaultOptionalParameters.get("-position-threshold")) || 0;
      const disableContainedCheck =
        (defaultOptionalParameters.get("-disable-contained-check") === "true");

      let currentElement = element;
      while (currentElement && currentElement.nodeType === ELEMENT_NODE) {
        let style = getComputedStyle(currentElement);
        style = $(style);
        for (const [key, value] of attributesMap) {
          if (value !== null) {
            const valueAsRegex = toRegExp(value);
            const styleValue = style.getPropertyValue(key) ||
              currentElement.getAttribute(key);
            if (valueAsRegex.test(styleValue))
              return false;
          }
        }
        if (!disableContainedCheck) {
          const x = parseFloat(currentElement.getAttribute("x")) || 0;
          const y = parseFloat(currentElement.getAttribute("y")) || 0;
          const dx = parseFloat(currentElement.getAttribute("dx")) || 0;
          const dy = parseFloat(currentElement.getAttribute("dy")) || 0;
          const transformAttr = currentElement.getAttribute("transform");
          let transformX = 0;
          let transformY = 0;
          if (transformAttr) {
            const transformRegex = /translate\((-?\d*\.?\d+)\s+(-?\d*\.?\d+)\)/;
            const transformMatches = transformAttr.match(transformRegex);

            if (transformMatches && transformMatches.length === 3) {
              transformX = parseFloat(transformMatches[1]);
              transformY = parseFloat(transformMatches[2]);
            }
          }
          const finalX = x + dx + transformX;
          const finalY = y + dy + transformY;

          if (Math$1.abs(finalX) > positionThresh ||
          Math$1.abs(finalY) > positionThresh)
            return false;
        }

        currentElement = currentElement.parentElement;
      }

      return true;
    }
  }

  let {
    parseInt: parseInt$1,
    setTimeout: setTimeout$1,
    Error: Error$1,
    MouseEvent: MouseEvent$1,
    MutationObserver: MutationObserver$1,
    WeakSet: WeakSet$1
  } = $(window);

  const VALID_TYPES = ["auxclick", "click", "dblclick",	"gotpointercapture",
                       "lostpointercapture", "mouseenter", "mousedown",
                       "mouseleave", "mousemove", "mouseout", "mouseover",
                       "mouseup",	"pointerdown", "pointerenter",
                       "pointermove", "pointerover", "pointerout",
                       "pointerup", "pointercancel", "pointerleave"];

  function simulateMouseEvent(...selectors) {
    const formattedArguments = formatArguments(arguments);
    const debugLog = getDebugger("simulate-mouse-event");
    const {mark, end} = profile("simulate-mouse-event");
    const MAX_ARGS = 7;
    if (selectors.length < 1)
      throw new Error$1("[simulate-mouse-event snippet]: No selector provided.");

    if (selectors.length > MAX_ARGS) {

      selectors = selectors.slice(0, MAX_ARGS);
    }
    function parseArg(theRule) {
      if (!theRule)
        return null;

      const result = {
        selector: "",
        continue: false,
        trigger: false,
        event: "click",
        delay: "500",
        clicked: false,
        found: false
      };
      const textArr = theRule.split("$");
      let options = [];
      if (textArr.length >= 2)
        options = textArr[1].toLowerCase().split(",");

      [result.selector] = textArr;

      for (const option of options) {
        if (option === "trigger") {
          result.trigger = true;
        }
        else if (option === "continue") {
          result.continue = true;
        }
        else if (option.startsWith("event")) {
          const event = option.toLowerCase().split("=");
          event[1] ? result.event = event[1] : result.event = "click";
        }
        else if (option.startsWith("delay")) {
          const delay = option.toLowerCase().split("=");
          delay[1] ? result.delay = delay[1] : result.delay = "500";
        }
      }
      if (!VALID_TYPES.includes(result.event)) {
        debugLog("warn",
                 result.event,
                 " might be misspelled, check for typos.\n",
                 "These are the supported events:",
                 VALID_TYPES);
      }
      return result;
    }

    const parsedArgs = $([]);

    $(selectors).forEach(rule => {
      const parsedRule = parseArg(rule);
      parsedArgs.push(parsedRule);
    });

    function checkIfAllSelectorsFound() {
      parsedArgs.forEach(arg => {
        if (!arg.found) {
          const queryAll = initQueryAll(arg.selector);
          const elems = queryAll();
          if (elems.length > 0)
            arg.found = true;
        }
      });
      return parsedArgs.every(arg => arg.found);
    }

    function resetAttributes() {
      parsedArgs.forEach(arg => {
        arg.found = false;
        arg.clicked = false;
      });
    }

    function triggerEvent(node, event, delay) {

      if (!node || !event)
        return;

      if (event === "click" && node.click) {
        node.click();
        debugLog("success",
                 "Clicked on this node:\n",
                 node,
                 "\nwith a delay of",
                 delay,
                 "ms",
                 `n\nFILTER: simulate-mouse-event ${formattedArguments}`
        );
      }
      else {
        node.dispatchEvent(
          new MouseEvent$1(event, {bubbles: true, cancelable: true})
        );
        debugLog("success",
                 "A",
                 event,
                 "event was dispatched with a delay of",
                 delay,
                 "ms on this node:\n",
                 node,
                 `n\nFILTER: simulate-mouse-event ${formattedArguments}`
        );
      }
    }
    let allFound = false;

    const [last] = parsedArgs.slice(-1);
    last.trigger = true;

    let dispatchedNodes = new WeakSet$1();

    let observer = new MutationObserver$1(findNodesAndDispatchEvents);
    observer.observe(document, {childList: true, subtree: true});
    findNodesAndDispatchEvents();

    function findNodesAndDispatchEvents() {
      mark();

      if (!allFound)
        allFound = checkIfAllSelectorsFound();
      if (allFound) {
        for (const parsedRule of parsedArgs) {
          const queryAndApply = initQueryAndApply(parsedRule.selector);
          const delayInMiliseconds = parseInt$1(parsedRule.delay, 10);
          if (parsedRule.trigger) {
            queryAndApply(node => {
              if (!dispatchedNodes.has(node)) {
                dispatchedNodes.add(node);
                if (parsedRule.continue) {
                  setInterval(() => {
                    resetAttributes();
                    const allIsStillFound = checkIfAllSelectorsFound();
                    if (allIsStillFound)
                      triggerEvent(node, parsedRule.event, parsedRule.delay);
                  }, delayInMiliseconds);
                }
                else {
                  setTimeout$1(() => {
                    triggerEvent(node, parsedRule.event, parsedRule.delay);
                  }, delayInMiliseconds);
                }
              }
            });
          }
        }
      }
      end();
    }
  }

  let {isNaN, MutationObserver, parseInt, parseFloat: parseFloat$1, setTimeout} = $(window);

  function skipVideo(playerSelector, xpathCondition, ...attributes) {
    const formattedArguments = formatArguments(arguments);
    const optionalParameters = new Map([
      ["-max-attempts", "10"],
      ["-retry-ms", "10"],
      ["-run-once", "false"],
      ["-wait-until", ""],
      ["-skip-to", "-0.1"],
      ["-stop-on-video-end", "false"],
      ["-start-from", "0"],
      ["-mute-video-when-skipping", "true"]
    ]);

    for (let attr of attributes) {
      attr = $(attr);
      let markerIndex = attr.indexOf(":");
      if (markerIndex < 0)
        continue;

      let key = attr.slice(0, markerIndex).trim().toString();
      let value = attr.slice(markerIndex + 1).trim().toString();

      if (key && value && optionalParameters.has(key))
        optionalParameters.set(key, value);
    }

    const maxAttemptsStr = optionalParameters.get("-max-attempts");
    const maxAttemptsNum = parseInt(maxAttemptsStr || 10, 10);

    const retryMsStr = optionalParameters.get("-retry-ms");
    const retryMsNum = parseInt(retryMsStr || 10, 10);

    const runOnceStr = optionalParameters.get("-run-once");
    const runOnceFlag = (runOnceStr === "true");

    const skipToStr = optionalParameters.get("-skip-to");
    const skipToNum = parseFloat$1(skipToStr || -0.1);

    const startFromStr = optionalParameters.get("-start-from");
    const startFrom = parseInt(startFromStr || 0, 10);

    const waitUntil = optionalParameters.get("-wait-until");

    const stopOnVideoEndStr = optionalParameters.get("-stop-on-video-end");
    const stopOnVideoEndFlag = (stopOnVideoEndStr === "true");

    const muteVideoStr = optionalParameters.get("-mute-video-when-skipping");
    const muteVideo = !(muteVideoStr === "false");

    const debugLog = getDebugger("skip-video");
    const {mark, end} = profile("skip-video");
    const queryAndApply = initQueryAndApply(`xpath(${xpathCondition})`);
    let skippedOnce = false;

    const mainLogic = () => {
      mark();
      const seenMap = new WeakSet();
      const callback = (retryCounter = 0) => {
        if (skippedOnce && runOnceFlag) {
          if (mo)
            mo.disconnect();
          return;
        }
        queryAndApply(node => {
          let nodeAlreadySeen = seenMap.has(node);
          let lastSkippedVideoDuration;
          if (!nodeAlreadySeen) {
            debugLog("info", "Matched:", node, " for selector: ", xpathCondition);
            debugLog("info", "Running video skipping logic.");
          }
          const videos = $$(playerSelector);
          let foundValidVideo = false;
          for (const video of videos) {
            if (!video || isNaN(video.duration) || isNaN(video.currentTime))
              continue;
            foundValidVideo = true;
            const videoNearEnd = (video.duration - video.currentTime) < 0.5;
            if ((video.duration > 0) && (video.currentTime < video.duration) &&
                !(stopOnVideoEndFlag && videoNearEnd)) {
              if (muteVideo) {
                video.muted = true;
                if (!nodeAlreadySeen)
                  debugLog("success", "Muted video...");
              }
              if (startFrom <= video.currentTime * 1000) {

                skipToNum <= 0 ?
                  video.currentTime = video.duration + skipToNum :
                  video.currentTime += skipToNum;
                if (lastSkippedVideoDuration !== video.duration) {
                  debugLog("success",
                           "Skipped video, currentTime: ",
                           video.currentTime,
                           "s.",
                           "\nFILTER: skip-video",
                           formattedArguments);
                  seenMap.add(node);
                  lastSkippedVideoDuration = video.duration;
                }
                video.paused && video.play();
                skippedOnce = true;
                win();
              }
            }
          }
          if (!foundValidVideo && retryCounter < maxAttemptsNum) {
            setTimeout(() => {
              const attempt = retryCounter + 1;
              debugLog("info",
                       "Running video skipping logic. Attempt: ",
                       attempt);
              callback(attempt);
            }, retryMsNum);
          }
        });
      };
      const mo = new MutationObserver(callback);
      const win = raceWinner(
        "skip-video",
        () => mo.disconnect()
      );
      mo.observe(
        document, {characterData: true, childList: true, subtree: true});
      callback();
      end();
    };

    waitUntilEvent(debugLog, mainLogic, waitUntil);
  }

  const snippets = {
    log,
    race,
    "debug": setDebug,
    "profile": setProfile,
    "hide-if-matches-xpath": hideIfMatchesXPath,
    "hide-if-matches-computed-xpath": hideIfMatchesComputedXPath,
    "hide-if-contains": hideIfContains,
    "hide-if-contains-similar-text": hideIfContainsSimilarText,
    "hide-if-contains-visible-text": hideIfContainsVisibleText,
    "hide-if-contains-and-matches-style": hideIfContainsAndMatchesStyle,
    "hide-if-has-and-matches-style": hideIfHasAndMatchesStyle,
    "hide-if-labelled-by": hideIfLabelledBy,
    "hide-if-contains-image": hideIfContainsImage,
    "hide-if-svg-contains": hideIfSvgContains,
    "simulate-mouse-event": simulateMouseEvent,
    "skip-video": skipVideo
  };
  let context;
  for (const [name, ...args] of filters) {
    if (snippets.hasOwnProperty(name)) {
      try { context = snippets[name].apply(context, args); }
      catch (error) { console.error(error); }
    }
  }
  context = void 0;
}