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
    getOwnPropertyDescriptor: getOwnPropertyDescriptor$1,
    getOwnPropertyDescriptors
  } = bound(Object);

  const invokes = bound(globalThis);
  const classes = isExtensionContext$2 ? globalThis : secure(globalThis);
  const {Map: Map$6, RegExp: RegExp$2, Set: Set$1, WeakMap: WeakMap$3, WeakSet: WeakSet$a} = classes;

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

  const variables$1 = freeze({
    frozen: new WeakMap$3(),
    hidden: new WeakSet$a(),
    iframePropertiesToAbort: {
      read: new Set$1(),
      write: new Set$1()
    },
    abortedIframes: new WeakMap$3()
  });

  const startsCapitalized = new RegExp$2("^[A-Z]");

  var env = new Proxy(new Map$6([

    ["chrome", (
      isExtensionContext$2 && (
        (isChrome && chrome) ||
        (isOtherThanChrome && browser)
      )
    ) || void 0],
    ["isExtensionContext", isExtensionContext$2],
    ["variables", variables$1],

    ["console", copyIfExtension(console)],
    ["document", globalThis.document],
    ["performance", copyIfExtension(performance)],
    ["JSON", copyIfExtension(JSON)],
    ["Map", Map$6],
    ["Math", copyIfExtension(Math)],
    ["Number", isExtensionContext$2 ? Number : primitive("Number")],
    ["RegExp", RegExp$2],
    ["Set", Set$1],
    ["String", isExtensionContext$2 ? String : primitive("String")],
    ["WeakMap", WeakMap$3],
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

  const {Map: Map$5, WeakMap: WeakMap$2, WeakSet: WeakSet$9, setTimeout: setTimeout$4} = env;

  let cleanup = true;
  let cleanUpCallback = map => {
    map.clear();
    cleanup = !cleanup;
  };

  var transformer = transformOnce.bind({
    WeakMap: WeakMap$2,
    WeakSet: WeakSet$9,

    WeakValue: class extends Map$5 {
      set(key, value) {
        if (cleanup) {
          cleanup = !cleanup;
          setTimeout$4(cleanUpCallback, 0, this);
        }
        return super.set(key, value);
      }
    }
  });

  const {concat, includes, join, reduce, unshift} = caller([]);

  const globals = secure(globalThis);

  const {
    Map: Map$4,
    WeakMap: WeakMap$1
  } = globals;

  const map = new Map$4;
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
    Array: Array$3,
    Number: Number$1,
    String: String$1,
    Object: Object$2
  } = env;

  const {isArray} = Array$3;
  const {getOwnPropertyDescriptor, setPrototypeOf: setPrototypeOf$1} = Object$2;

  const {toString} = Object$2.prototype;
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
      return setPrototypeOf$1(value, Array$3.prototype);

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
    variables,
    Array: Array$2,
    MutationObserver: MutationObserver$a,
    Object: Object$1,
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

  const {assign, setPrototypeOf} = Object$1;

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
    if (variables.hidden.has(element))
      return false;

    notifyElementHidden(element);

    variables.hidden.add(element);

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
    return () => Array$2.from($$(selector));
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

  let {Math: Math$2, RegExp: RegExp$1} = $(window);

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

        return new RegExp$1(...args);
      }
    }

    return new RegExp$1(regexEscape(pattern));
  }

  const {console: console$1} = $(window);

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
    console$1.log(...args);
  }

  function getDebugger(name) {
    return bind(debug() ? log : noop, null, name);
  }

  let {Array: Array$1, Error: Error$2, Map: Map$3, parseInt: parseInt$3} = $(window);

  let stack = null;
  let won = null;

  function race(action, winners = "1") {
    switch (action) {
      case "start":
        stack = {
          winners: parseInt$3(winners, 10) || 1,
          participants: new Map$3()
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
        throw new Error$2(`Invalid action: ${action}`);
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
    const debugLog = getDebugger("hide-if-contains");
    const onHideCallback = node => {
      debugLog("success",
               "Matched: ",
               node,
               " for selector: ",
               selector,
               searchSelector);
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

  $(window);

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
    const debugLog = getDebugger("hide-if-contains-and-matches-style");
    const hiddenMap = new WeakSet$8();
    const logMap = debug() && new WeakSet$8();
    if (searchSelector == null)
      searchSelector = selector;

    const searchRegExp = toRegExp(search);

    const styleRegExp = style ? toRegExp(style) : null;
    const searchStyleRegExp = searchStyle ? toRegExp(searchStyle) : null;
    const mainLogic = () => {
      const callback = () => {
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
                         " for params: ",
                         ...arguments);
              }
              else {
                if (!logMap || logMap.has(closest))
                  continue;
                debugLog("info",
                         "In this element the searchStyle matched " +
                         "but style didn't:\n",
                         closest,
                         getComputedStyle$5(closest),
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
                       getComputedStyle$5(element),
                       ...arguments);
              logMap.add(element);
            }
          }
        }
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
    clearTimeout: clearTimeout$1,
    fetch,
    getComputedStyle: getComputedStyle$4,
    setTimeout: setTimeout$3,
    Map: Map$2,
    MutationObserver: MutationObserver$8,
    Uint8Array: Uint8Array$1
  } = $(window);

  function hideIfContainsImage(search, selector, searchSelector) {
    if (searchSelector == null)
      searchSelector = selector;

    let searchRegExp = toRegExp(search);

    const debugLog = getDebugger("hide-if-contains-image");

    let callback = () => {
      for (const {element, rootParents} of $$(searchSelector, true)) {
        let style = getComputedStyle$4(element);
        let match = $(style["background-image"]).match(/^url\("(.*)"\)$/);
        if (match) {
          fetchContent(match[1]).then(content => {
            if (searchRegExp.test(uint8ArrayToHex(new Uint8Array$1(content)))) {
              let closest = $closest($(element), selector, rootParents);
              if (closest) {
                win();
                hideElement(closest);
                debugLog("success", "Matched: ", closest, " for:", ...arguments);
              }
            }
          });
        }
      }
    };

    let mo = new MutationObserver$8(callback);
    let win = raceWinner(
      "hide-if-contains-image",
      () => mo.disconnect()
    );
    mo.observe(document, {childList: true, subtree: true});
    callback();
  }

  let fetchContentMap = new Map$2();

  function fetchContent(url, {as = "arrayBuffer", cleanup = 60000} = {}) {

    let uid = as + ":" + url;
    let details = fetchContentMap.get(uid) || {
      remove: () => fetchContentMap.delete(uid),
      result: null,
      timer: 0
    };
    clearTimeout$1(details.timer);
    details.timer = setTimeout$3(details.remove, cleanup);
    if (!details.result) {
      details.result = fetch(url).then(res => res[as]()).catch(details.remove);
      fetchContentMap.set(uid, details);
    }
    return details.result;
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

  const {parseFloat: parseFloat$3, Math: Math$1, MutationObserver: MutationObserver$7, WeakSet: WeakSet$7} = $(window);
  const {min} = Math$1;

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
    const debugLog = getDebugger("hide-if-contains-similar-text");
    const $search = $(search);
    const {length} = $search;
    const chars = length + parseFloat$3(ignoreChars) || 0;
    const find = $([...$search]).sort();
    const guard = parseFloat$3(maxSearches) || Infinity;

    if (searchSelector == null)
      searchSelector = selector;

    debugLog("Looking for similar text: " + $search);

    const callback = () => {
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
            debugLog("success", "Found similar text: " + $search, closest);
            if (closest) {
              win();
              hideElement(closest);
              break;
            }
          }
        }
      }
    };

    let mo = new MutationObserver$7(callback);
    let win = raceWinner(
      "hide-if-contains-similar-text",
      () => mo.disconnect()
    );
    mo.observe(document, {childList: true, characterData: true, subtree: true});
    callback();
  }

  let {getComputedStyle: getComputedStyle$3, Map: Map$1, WeakSet: WeakSet$6, parseFloat: parseFloat$2} = $(window);

  const {ELEMENT_NODE: ELEMENT_NODE$3, TEXT_NODE} = Node;

  function hideIfContainsVisibleText(search, selector,
                                            searchSelector = null,
                                            ...attributes) {
    let entries = $([]);
    const optionalParameters = new Map$1([
      ["-snippet-box-margin", "2"],
      ["-disable-bg-color-check", "false"],
      ["-check-is-contained", "false"]
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

    let attributesMap = new Map$1(defaultEntries.concat(entries));

    function isTextVisible(element, style, {bgColorCheck = true} = {}) {
      if (!style)
        style = getComputedStyle$3(element);

      style = $(style);

      for (const [key, value] of attributesMap) {
        let valueAsRegex = toRegExp(value);
        if (valueAsRegex.test(style.getPropertyValue(key)))
          return false;
      }

      let color = style.getPropertyValue("color");
      if (bgColorCheck && style.getPropertyValue("background-color") == color)
        return false;

      return true;
    }

    function getPseudoContent(element, pseudo, {bgColorCheck = true} = {}) {
      let style = getComputedStyle$3(element, pseudo);
      if (!isVisible(element, style) ||
       !isTextVisible(element, style, {bgColorCheck}))
        return "";

      let {content} = $(style);
      if (content && content !== "none") {
        let strings = $([]);

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

    function isContained(childNode, parentNode, {boxMargin = 2} = {}) {
      const child = $(childNode).getBoundingClientRect();
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
                               {
                                 boxMargin = 2,
                                 bgColorCheck,
                                 checkIsContained
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

      let text = getPseudoContent(element, ":before", {bgColorCheck});
      for (let node of $childNodes($(element))) {
        switch ($(node).nodeType) {
          case ELEMENT_NODE$3:
            text += getVisibleContent(node,
                                      element,
                                      getComputedStyle$3(node),
                                      parentOverflowNode,
                                      originalElement,
                                      shadowRootParents,
                                      {
                                        boxMargin,
                                        bgColorCheck,
                                        checkIsContained
                                      }
            );
            break;
          case TEXT_NODE:

            if (parentOverflowNode) {
              if (isContained(element, parentOverflowNode, {boxMargin}) &&
                isTextVisible(element, style, {bgColorCheck}))
                text += $(node).nodeValue;
            }
            else if (isTextVisible(element, style, {bgColorCheck})) {
              if (checkIsContained &&
                 !isContained(element, originalElement, {boxMargin}))
                continue;
              text += $(node).nodeValue;
            }
            break;
        }
      }
      return text + getPseudoContent(element, ":after", {bgColorCheck});
    }

    const boxMarginStr = optionalParameters.get("-snippet-box-margin");
    const boxMargin = parseFloat$2(boxMarginStr) || 0;

    const bgColorCheckStr = optionalParameters.get("-disable-bg-color-check");
    const bgColorCheck = !(bgColorCheckStr === "true");

    const checkIsContainedStr = optionalParameters.get("-check-is-contained");
    const checkIsContained = (checkIsContainedStr === "true");

    let re = toRegExp(search);
    let seen = new WeakSet$6();

    const mo = hideIfMatches(
      (element, closest, rootParents) => {
        if (seen.has(element))
          return false;

        seen.add(element);
        let text = getVisibleContent(
          element, closest, null, null, element, rootParents, {
            boxMargin,
            bgColorCheck,
            checkIsContained
          }
        );
        let result = re.test(text);
        if (debug() && text.length) {
          result ? log("success", result, re, text) :
          log("info", result, re, text);
        }

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
    const debugLog = getDebugger("hide-if-has-and-matches-style");
    const hiddenMap = new WeakSet$5();
    const logMap = debug() && new WeakSet$5();
    if (searchSelector == null)
      searchSelector = selector;

    const styleRegExp = style ? toRegExp(style) : null;
    const searchStyleRegExp = searchStyle ? toRegExp(searchStyle) : null;
    const mainLogic = () => {
      const callback = () => {
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
                       " for params: ",
                       ...arguments);
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
    let sameSelector = searchSelector == null;

    let searchRegExp = toRegExp(search);

    let matched = new WeakSet$4();

    let callback = () => {
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
    };

    let mo = new MutationObserver$5(callback);
    let win = raceWinner(
      "hide-if-labelled-by",
      () => mo.disconnect()
    );
    mo.observe(document, {characterData: true, childList: true, subtree: true});
    callback();
  }

  $(window);

  const noopProfile = {
    mark() {},
    end() {},
    toString() {
      return "{mark(){},end(){}}";
    }
  };

  function profile(id, rate = 10) {
    return noopProfile;
  }

  let {MutationObserver: MutationObserver$4, WeakSet: WeakSet$3} = $(window);

  const {ELEMENT_NODE: ELEMENT_NODE$2} = Node;

  function hideIfMatchesXPath(query, scopeQuery) {
    const {mark, end} = profile();
    const debugLog = getDebugger("hide-if-matches-xpath");

    const startHidingMutationObserver = scopeNode => {
      const queryAndApply = initQueryAndApply(`xpath(${query})`);
      const seenMap = new WeakSet$3();
      const callback = () => {
        mark();
        queryAndApply(node => {
          if (seenMap.has(node))
            return false;
          seenMap.add(node);
          win();
          if ($(node).nodeType === ELEMENT_NODE$2)
            hideElement(node);
          else
            $(node).textContent = "";
          debugLog("success", "Matched: ", node, " for selector: ", query);
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
  }

  let {MutationObserver: MutationObserver$3, WeakSet: WeakSet$2} = $(window);

  const {ELEMENT_NODE: ELEMENT_NODE$1} = Node;

  function hideIfMatchesComputedXPath(query, searchQuery, searchRegex,
                                             waitUntil) {
    const {mark, end} = profile();
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
          if ($(node).nodeType === ELEMENT_NODE$1)
            hideElement(node);
          else
            $(node).textContent = "";
          debugLog("success", "Matched: ", node, " for selector: ", query);
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
    parseInt: parseInt$2,
    setTimeout: setTimeout$2,
    Error: Error$1,
    MouseEvent: MouseEvent$1,
    MutationObserver: MutationObserver$2,
    WeakSet: WeakSet$1
  } = $(window);

  const VALID_TYPES = ["auxclick", "click", "dblclick",	"gotpointercapture",
                       "lostpointercapture", "mouseenter", "mousedown",
                       "mouseleave", "mousemove", "mouseout", "mouseover",
                       "mouseup",	"pointerdown", "pointerenter",
                       "pointermove", "pointerover", "pointerout",
                       "pointerup", "pointercancel", "pointerleave"];

  function simulateMouseEvent(...selectors) {
    const debugLog = getDebugger("simulate-mouse-event");
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
                 "ms"
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
                 node
        );
      }
    }
    let allFound = false;

    const [last] = parsedArgs.slice(-1);
    last.trigger = true;

    let dispatchedNodes = new WeakSet$1();

    let observer = new MutationObserver$2(findNodesAndDispatchEvents);
    observer.observe(document, {childList: true, subtree: true});
    findNodesAndDispatchEvents();

    function findNodesAndDispatchEvents() {

      if (!allFound)
        allFound = checkIfAllSelectorsFound();
      if (allFound) {
        for (const parsedRule of parsedArgs) {
          const queryAndApply = initQueryAndApply(parsedRule.selector);
          const delayInMiliseconds = parseInt$2(parsedRule.delay, 10);
          if (parsedRule.trigger) {
            queryAndApply(node => {
              if (!dispatchedNodes.has(node)) {
                dispatchedNodes.add(node);
                if (parsedRule.continue) {
                  setInterval(() => {
                    triggerEvent(node, parsedRule.event, parsedRule.delay);
                  }, delayInMiliseconds);
                }
                else {
                  setTimeout$2(() => {
                    triggerEvent(node, parsedRule.event, parsedRule.delay);
                  }, delayInMiliseconds);
                }
              }
            });
          }
        }
      }
    }
  }

  let {isNaN: isNaN$1, MutationObserver: MutationObserver$1, parseInt: parseInt$1, parseFloat: parseFloat$1, setTimeout: setTimeout$1} = $(window);

  function skipVideo(playerSelector, xpathCondition, ...attributes) {
    const optionalParameters = new Map([
      ["-max-attempts", "10"],
      ["-retry-ms", "10"],
      ["-run-once", "false"],
      ["-wait-until", ""],
      ["-skip-to", "-0.1"],
      ["-stop-on-video-end", "false"]
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
    const maxAttemptsNum = parseInt$1(maxAttemptsStr || 10, 10);

    const retryMsStr = optionalParameters.get("-retry-ms");
    const retryMsNum = parseInt$1(retryMsStr || 10, 10);

    const runOnceStr = optionalParameters.get("-run-once");
    const runOnceFlag = (runOnceStr === "true");

    const skipToStr = optionalParameters.get("-skip-to");
    const skipToNum = parseFloat$1(skipToStr || -0.1);

    const waitUntil = optionalParameters.get("-wait-until");

    const stopOnVideoEndStr = optionalParameters.get("-stop-on-video-end");
    const stopOnVideoEndFlag = (stopOnVideoEndStr === "true");

    const debugLog = getDebugger("skip-video");
    const queryAndApply = initQueryAndApply(`xpath(${xpathCondition})`);
    let skippedOnce = false;

    const mainLogic = () => {
      const callback = (retryCounter = 0) => {
        if (skippedOnce && runOnceFlag) {
          if (mo)
            mo.disconnect();
          return;
        }
        queryAndApply(node => {
          debugLog("info", "Matched: ", node, " for selector: ", xpathCondition);
          debugLog("info", "Running video skipping logic.");
          const video = $$(playerSelector)[0];
          while (isNaN$1(video.duration) && retryCounter < maxAttemptsNum) {
            setTimeout$1(() => {
              const attempt = retryCounter + 1;
              debugLog("info",
                       "Running video skipping logic. Attempt: ",
                       attempt);
              callback(attempt);
            }, retryMsNum);
            return;
          }
          const videoNearEnd = (video.duration - video.currentTime) < 0.5;
          if (!isNaN$1(video.duration) && !(stopOnVideoEndFlag && videoNearEnd)) {
            video.muted = true;
            debugLog("success", "Muted video...");

            skipToNum <= 0 ?
              video.currentTime = video.duration + skipToNum :
              video.currentTime += skipToNum;
            debugLog("success", "Skipped duration...");
            video.paused && video.play();
            skippedOnce = true;
            win();
          }
        });
      };
      const mo = new MutationObserver$1(callback);
      const win = raceWinner(
        "skip-video",
        () => mo.disconnect()
      );
      mo.observe(
        document, {characterData: true, childList: true, subtree: true});
      callback();
    };

    waitUntilEvent(debugLog, mainLogic, waitUntil);
  }

  const snippets = {
    log,
    race,
    "debug": setDebug,
    "hide-if-matches-xpath": hideIfMatchesXPath,
    "hide-if-matches-computed-xpath": hideIfMatchesComputedXPath,
    "hide-if-contains": hideIfContains,
    "hide-if-contains-similar-text": hideIfContainsSimilarText,
    "hide-if-contains-visible-text": hideIfContainsVisibleText,
    "hide-if-contains-and-matches-style": hideIfContainsAndMatchesStyle,
    "hide-if-has-and-matches-style": hideIfHasAndMatchesStyle,
    "hide-if-labelled-by": hideIfLabelledBy,
    "hide-if-contains-image": hideIfContainsImage,
    "simulate-mouse-event": simulateMouseEvent,
    "skip-video": skipVideo
  };

  let {MutationObserver} = $(window);

  const {ELEMENT_NODE} = Node;

  function hideIfMatchesXPath3(query, scopeQuery) {
    let {mark, end} = profile();

    const namespaceResolver = prefix => {
      switch (prefix) {
        case "": return "http://www.w3.org/1999/xhtml";
        default: return false;
      }
    };
    function queryNodes(nodeQuery) {
      return fontoxpath.evaluateXPathToNodes(nodeQuery, document, null, null, {
        language: fontoxpath.evaluateXPath.XQUERY_3_1_LANGUAGE,
        namespaceResolver
      });
    }

    let debugLog = getDebugger("hide-if-matches-xpath3");

    const startHidingMutationObserver = scopeNode => {
      const seenMap = new WeakSet();
      const callback = () => {
        mark();

        const nodes = queryNodes(query);
        for (const node of $(nodes)) {
          if (seenMap.has(node))
            return false;
          seenMap.add(node);
          win();
          if ($(node).nodeType === ELEMENT_NODE)
            hideElement(node);
          else
            $(node).textContent = "";
          debugLog("success", "Matched: ", node, " for selector: ", query);
        }
        end();
      };

      const mo = new MutationObserver(callback);
      const win = raceWinner(
        "hide-if-matches-xpath3",
        () => mo.disconnect()
      );
      mo.observe(
        scopeNode, {characterData: true, childList: true, subtree: true});
      callback();
    };

    if (scopeQuery) {

      let count = 0;
      let scopeMutationObserver;
      const scopeNodes = queryNodes(scopeQuery);
      const findMutationScopeNodes = () => {
        for (const scopeNode of $(scopeNodes)) {

          startHidingMutationObserver(scopeNode);
          count++;
        }
        if (count > 0)
          scopeMutationObserver.disconnect();
      };

      scopeMutationObserver = new MutationObserver(findMutationScopeNodes);
      scopeMutationObserver.observe(
        document, {characterData: true, childList: true, subtree: true}
      );
      findMutationScopeNodes();
    }
    else {

      startHidingMutationObserver(document);
    }
  }

  const DEFAULT_GRAPH_CUTOFF=500;async function domToGraph(e,t,r=false){return new Promise(((o,i)=>{if(!e||!t)return i();let l=e.config;let n=l.cutoff||e.topology.graphml.nodes||DEFAULT_GRAPH_CUTOFF;l=l.filter((e=>e.include));for(let e of l)e.groupName=Object.keys(e)[2];let s=(e,t,r,o)=>{if(r==="attributes"&&typeof e.attributes[o]!=="undefined")t.attributes[o]=e.attributes[o].value;else if(r==="style"&&e.style[o])t.attributes.style[o]=e.style[o];else if(r==="css")t.cssSelectors=getComputedStyle(e).cssText||"";};let a=e=>{if(r&&!e.clientWidth&&!e.clientHeight)return;n-=1;if(n<0)return;let t={tag:e.tagName,width:e.clientWidth,height:e.clientHeight,attributes:{style:{}},children:[]};for(let r of l){for(let o of r[r.groupName].features){for(let[i,l]of Object.entries(o)){if("names"in l){for(let o of l.names){for(let i of o.split("^"))s(e,t,r.groupName,i);}}else {s(e,t,r.groupName,i);}}}}if(e.children){for(let r of e.children){let e=a(r);if(e)t.children.push(e);}}return t};let f=a(t);o(f);}))}function parseArgs(e){if(!e||!Array.isArray(e)||!e.length)return {};let t=[];let r={debug:false,frameonly:false,failsafe:false,denoise:false,silent:false,model:"",selector:"",subselector:""};for(let o of e){if(o&&o in r)r[o]=true;else t.push(o);}if(t.length<2)return {};r.model=t[0];t.splice(0,1);if(t.length>2||t.some((e=>e&&e.startsWith('"'))))t=t.join(" ").split('"').map((e=>e.trim())).filter((e=>e));r.selector=t[0]||"";r.subselector=t[1]||"";return r}function resolveSelectors(e,t){let r=[document];e=$(e).split("..");let o=[];$(e).forEach(((e,t)=>{if(t){r=$(r).reduce(((e,t)=>t&&$(t).parentElement?e.concat($(t).parentElement):e),$([]));}r=$(r).reduce(((t,r)=>{if(!e){t.push(r);}else {try{t=t.concat(...$(r).querySelectorAll(e));}catch(e){}}return t}),$([]));}));for(let e of r){let r=[e];if(t){try{r=$(e).querySelectorAll(t);}catch(e){}}o.push([e,r]);}return o}let{WeakSet:WeakSet$,MutationObserver:MutationObserver$}=(typeof $!=="undefined"?$:e=>e)(window);class Observer{constructor(){this.digestedElements=new WeakSet$;this.selector="";this.subselector="";}observe(e,t,r){this.selector=e;this.subselector=t;this.callback=r;this.elementObserver=new MutationObserver$(this.digest.bind(this));this.elementObserver.observe(document,{childList:true,subtree:true});this.digest();}stop(){if(this.elementObserver)this.elementObserver.disconnect();}digest(){let e=resolveSelectors(this.selector,this.subselector).filter((([e])=>!this.digestedElements.has(e)));this.callback(e);for(let[t]of e)this.digestedElements.add(t);}}let mode=false;function print$1(e,t=false,...r){if(mode){console.log("%cMLDBG ♥ %c| %s%c |","color:cyan",t?"color:red":"color:inherit",e,"color:inherit",...r);}}function toggle(e){mode=!!e;}let data={nodeCount:0,organicCount:0,adCount:0,aaCount:0,times:[]};function set(e=false,t=false,r){if(mode){if(!data.nodeCount){$(document).head.insertAdjacentHTML("beforeend",`\n        <style>body::before { display: block; position: fixed; pointer-events: none; top: 0; left: 0; z-index: 999999; background-color: rgba(0, 0, 0, 0.7); padding: 5px; content: "ad:\\9\\9 " var(--dbg-ad) "\\A nad:\\9\\9 " var(--dbg-nad) "\\A aa:\\9\\9 " var(--dbg-aa) "\\A time:\\9 " var(--dbg-t); font-size: 10px; white-space: pre-wrap; color: #fff; }</style>\n      `);}}data.nodeCount++;if(t)data.aaCount++;else if(e)data.adCount++;else data.organicCount++;if(r)data.times.push(r);}function print(){if(mode){$(document).body.style.setProperty("--dbg-ad",`"${data.adCount}"`);$(document).body.style.setProperty("--dbg-nad",`"${data.organicCount}"`);$(document).body.style.setProperty("--dbg-aa",`"${data.aaCount}"`);$(document).body.style.setProperty("--dbg-t",`"${data.times.reduce(((e,t,r)=>!r||r%3?e+=t+"ms ":e+="\\A\\9\\9 "+t+"ms "),"")}"`);}}const MESSAGE_PREFIX="ML:";const MESSAGE_PREPARE_SUFFIX="prepare";const MESSAGE_INFERENCE_SUFFIX="inference";const errors={UNKNOWN_REQUEST:1,MISSING_REQUEST_DATA:2,UNKNOWN_MODEL:3,MISSING_INFERENCE_DATA:4,INFERENCE_FAILED:5,MODEL_INSTANTIATION_FAILED:6,MISSING_ENVIRONMENTAL_SUPPORT:7};const IN_FRAME=window.self!==window.top;const SERVICE_WORKER_TIMEOUT=1e4;let{Map:Map$}=(typeof $!=="undefined"?$:e=>e)(window);let modelConfigs=new Map$;let globallyAllowlisted=false;function hideIfClassifies(...e){let{debug:t,frameonly:r,failsafe:o,denoise:i,silent:l,model:n,selector:s,subselector:a}=parseArgs(e||[]);toggle(t);if(typeof chrome==="undefined"||!chrome.runtime||!chrome.runtime.sendMessage)return print$1("environmental support",false);if(!n||!s)return print$1("Invalid filter",true);if(r&&!IN_FRAME)return;if(!IN_FRAME)print$1(`Filter hit for ${n}: ${s} ${a}`);let f=new Observer;let d=raceWinner("hide-if-graph-matches",(()=>f.stop()));let c=()=>{if(modelConfigs.has(n))return modelConfigs.get(n);print$1(`Preparing worker with model ${n}`);let e=new Promise(((e,t)=>{let r=setTimeout((()=>{t(`Worker preparation with ${n} failed: service worker timeout`);}),SERVICE_WORKER_TIMEOUT);p({type:MESSAGE_PREFIX+MESSAGE_PREPARE_SUFFIX,debug:mode,model:n},(o=>{clearTimeout(r);if(o&&"config"in o){print$1(`Received config for ${n}`,false,"config:",o.config);o.config.cutoff=o.cutoff;e(o.config);}else {t(`Worker preparation with ${n} failed`);}}));}));e.catch((e=>{}));modelConfigs.set(n,e);return e};let u=(e,t)=>{if(o&&data.nodeCount>=10&&data.adCount/data.nodeCount>=1){print$1("Ad to non-ad ratio is 100%/0%. Stopping inference.",true);return f.stop()}print$1(`Requesting inference with ${n}`,false,"graph:",e);p({type:MESSAGE_PREFIX+MESSAGE_INFERENCE_SUFFIX,debug:mode,model:n,graph:e},(r=>{if(r&&"prediction"in r&&typeof r.prediction==="boolean"){print$1(`Received ${r.prediction} inference results with ${n}`,false,"graph:",e);if(r.allowlisted&&!globallyAllowlisted)globallyAllowlisted=true;set(r.prediction,r.allowlisted,r.digestTime-r.startTime);if(globallyAllowlisted&&!mode)return f.stop();if(r.prediction&&!mode&&!l){d();hideElement(t);}else if(mode){if(r.allowlisted)$(t).style.border="3px solid #00ffff";else if(r.prediction)$(t).style.border="3px solid #ff0000";else if(!r.prediction)$(t).style.border="3px solid #00ff00";print();}}else {print$1(`Inference with ${n} failed`,true,"graph:",e,"response:",r);if("error"in r&&(r.error===errors.INFERENCE_FAILED||r.error===errors.MISSING_ENVIRONMENTAL_SUPPORT))f.stop();}}));};let p=(e,t)=>{if(!globallyAllowlisted)chrome.runtime.sendMessage(e,t);else if(mode)t({prediction:false,allowlisted:true});};let h=e=>{if((!e||!e.length)&&IN_FRAME)return c();c().then((t=>{if(!t)return Promise.reject(`Config file for ${n} corrupted`);for(let[r,o]of e){for(let e of o){print$1(`Preparing inference with ${n}`,false,"target:",r);domToGraph({config:t},e,i).then((r=>i&&!r?domToGraph({config:t},e,false):r)).then((e=>u(e,r))).catch((e=>print$1(`domToGraph failed with error "${e}"`,true,"target:",r)));}}})).catch((e=>{print$1(e,true);f.stop();}));};f.observe(s,a,h);}

  snippets["hide-if-matches-xpath3"] = hideIfMatchesXPath3;
  snippets["hide-if-classifies"] = hideIfClassifies;

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

  function hideIfMatchesXPath3Dependency() {
  	// whynot.js 5.0.0

  	// 	The MIT License (MIT)

  	// Copyright (c) 2017 Stef Busking

  	// Permission is hereby granted, free of charge, to any person obtaining a copy of
  	// this software and associated documentation files (the "Software"), to deal in
  	// the Software without restriction, including without limitation the rights to
  	// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
  	// the Software, and to permit persons to whom the Software is furnished to do so,
  	// subject to the following conditions:

  	// The above copyright notice and this permission notice shall be included in all
  	// copies or substantial portions of the Software.

  	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  	// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  	// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  	// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  	// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  	// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  	!function (t, s) { s((t = "undefined" != typeof globalThis ? globalThis : t || self).whynot = {}); }(this, (function (t) { function s(t, s, i, r) { const n = { op: s, func: i, data: r }; return t.push(n), n } function i(t, s) { return t } class r { constructor() { this.program = []; } test(t, i) { return s(this.program, 5, t, void 0 === i ? null : i) } jump(t) { return s(this.program, 3, null, t) } record(t, r) { return s(this.program, 4, void 0 === r ? i : r, t) } bad(t = 1) { return s(this.program, 1, null, t) } accept() { return s(this.program, 0, null, null) } fail(t) { return s(this.program, 2, t || null, null) } } class n { constructor(t, s, i) { this.programLength = t, this.maxFromByPc = s, this.maxSurvivorFromByPc = i; } static fromProgram(t) { const s = t.length, i = [], r = []; return t.forEach((t => { i.push(0), r.push(0); })), t.forEach(((t, n) => { switch (t.op) { case 2: if (null === t.func) return; if (n + 1 >= s) throw new Error("Invalid program: program could run past end"); i[n + 1] += 1; break; case 1: case 4: if (n + 1 >= s) throw new Error("Invalid program: program could run past end"); i[n + 1] += 1; break; case 3: t.data.forEach((t => { if (t < 0 || t >= s) throw new Error("Invalid program: program could run past end"); i[t] += 1; })); break; case 5: if (n + 1 >= s) throw new Error("Invalid program: program could run past end"); r[n + 1] += 1; break; case 0: r[n] += 1; } })), new n(s, i, r) } static createStub(t) { const s = [], i = []; for (let r = 0; r < t; ++r)s.push(t), i.push(t); return new n(t, s, i) } } class e { constructor(t) { this.acceptingTraces = t, this.success = t.length > 0; } } const h = 255; class l { constructor(t) { this.t = 0, this.i = 0, this.h = new Uint16Array(t), this.l = new Uint8Array(t); } getBadness(t) { return this.l[t] } add(t, s) { this.l[t] = s > h ? h : s; const i = function (t, s, i, r, n) { let e = r, h = n; for (; e < h;) { const r = e + h >>> 1; i < s[t[r]] ? h = r : e = r + 1; } return e }(this.h, this.l, s, this.i, this.t); this.h.copyWithin(i + 1, i, this.t), this.h[i] = t, this.t += 1; } reschedule(t, s) { const i = Math.max(this.l[t], s > h ? h : s); if (this.l[t] !== i) { const s = this.h.indexOf(t, this.i); if (s < 0 || s >= this.t) return void (this.l[t] = i); this.h.copyWithin(s, s + 1, this.t), this.t -= 1, this.add(t, i); } } getNextPc() { return this.i >= this.t ? null : this.h[this.i++] } reset() { this.t = 0, this.i = 0, this.l.fill(0); } } class o { constructor(t) { this.o = []; let s = t.length; t.forEach((t => { this.o.push(t > 0 ? s : -1), s += t; })), this.u = new Uint16Array(s); } clear() { this.u.fill(0, 0, this.o.length); } add(t, s) { const i = this.u[s], r = this.o[s]; this.u[s] += 1, this.u[r + i] = t; } has(t) { return this.u[t] > 0 } forEach(t, s) { const i = this.u[t], r = this.o[t]; for (let t = r; t < r + i; ++t)s(this.u[t]); } } function c(t, s, i = !1) { return null === t ? s : Array.isArray(t) ? (-1 === t.indexOf(s) && (i && (t = t.slice()), t.push(s)), t) : t === s ? t : [t, s] } class u { constructor(t, s) { this.prefixes = t, this.record = s; } } function a(t, s) { let i; if (null === s) { if (!Array.isArray(t)) return t; i = t; } else i = t === u.EMPTY ? [] : Array.isArray(t) ? t : [t]; return new u(i, s) } u.EMPTY = new u([], null); class f { constructor(t) { this.p = [], this.v = []; for (let s = 0; s < t; ++s)this.p.push(0), this.v.push(null); } mergeTraces(t, s, i, r, n, e) { let h = !1; return i.forEach(s, (s => { const i = this.trace(s, r, n, e); var l, o, u; o = i, u = h, t = null === (l = t) ? o : null === o ? l : Array.isArray(o) ? o.reduce(((t, s) => c(t, s, t === o)), l) : c(l, o, u), h = t === i; })), t } trace(t, s, i, r) { switch (this.p[t]) { case 2: return this.v[t]; case 1: return null }this.p[t] = 1; let n = null; const e = s[t]; if (null !== e) n = e; else if (!i.has(t)) throw new Error(`Trace without source at pc ${t}`); if (n = this.mergeTraces(n, t, i, s, i, r), null !== n) { const s = r[t]; null !== s && (n = a(n, s)); } return this.v[t] = n, this.p[t] = 2, n } buildSurvivorTraces(t, s, i, r, n) { for (let e = 0, h = t.length; e < h; ++e) { if (!i.has(e)) { s[e] = null; continue } this.v.fill(null), this.p.fill(0); const h = this.mergeTraces(null, e, i, t, r, n); if (null === h) throw new Error(`No non-cyclic paths found to survivor ${e}`); s[e] = a(h, null); } this.v.fill(null); } } class d { constructor(t) { this.g = [], this.k = [], this.m = [], this.A = new o(t.maxFromByPc), this.T = new o(t.maxSurvivorFromByPc), this.S = new f(t.programLength); for (let s = 0; s < t.programLength; ++s)this.g.push(null), this.k.push(null), this.m.push(null); this.k[0] = u.EMPTY; } reset(t) { this.A.clear(), this.T.clear(), this.g.fill(null), t && (this.k.fill(null), this.m.fill(null), this.k[0] = u.EMPTY); } record(t, s) { this.g[t] = s; } has(t) { return this.A.has(t) || null !== this.k[t] } add(t, s) { this.A.add(t, s); } hasSurvivor(t) { return this.T.has(t) } addSurvivor(t, s) { this.T.add(t, s); } buildSurvivorTraces() { const t = this.k; this.S.buildSurvivorTraces(t, this.m, this.T, this.A, this.g), this.k = this.m, this.m = t; } getTraces(t) { const s = t.reduce(((t, s) => c(t, this.k[s])), null); return null === s ? [] : Array.isArray(s) ? s : [s] } } class w { constructor(t) { this.I = [], this.M = new l(t.programLength), this.N = new l(t.programLength), this.j = new d(t); } reset() { this.M.reset(), this.M.add(0, 0), this.I.length = 0, this.j.reset(!0); } getNextThreadPc() { return this.M.getNextPc() } step(t, s, i) { const r = this.j.has(s); this.j.add(t, s); const n = this.M.getBadness(t) + i; r ? this.M.reschedule(s, n) : this.M.add(s, n); } stepToNextGeneration(t, s) { const i = this.j.hasSurvivor(s); this.j.addSurvivor(t, s); const r = this.M.getBadness(t); i ? this.N.reschedule(s, r) : this.N.add(s, r); } accept(t) { this.I.push(t), this.j.addSurvivor(t, t); } fail(t) { } record(t, s) { this.j.record(t, s); } nextGeneration() { this.j.buildSurvivorTraces(), this.j.reset(!1); const t = this.M; t.reset(), this.M = this.N, this.N = t; } getAcceptingTraces() { return this.j.getTraces(this.I) } } class p { constructor(t) { this.P = [], this.U = t, this.G = n.fromProgram(t), this.P.push(new w(this.G)); } execute(t, s) { const i = this.P.pop() || new w(this.G); i.reset(); const r = t.length; let n, h = -1; do { let e = i.getNextThreadPc(); if (null === e) break; for (++h, n = h >= r ? null : t[h]; null !== e;) { const t = this.U[e]; switch (t.op) { case 0: null === n ? i.accept(e) : i.fail(e); break; case 2: { const r = t.func; if (null === r || r(s)) { i.fail(e); break } i.step(e, e + 1, 0); break } case 1: i.step(e, e + 1, t.data); break; case 5: if (null === n) { i.fail(e); break } if (!(0, t.func)(n, t.data, s)) { i.fail(e); break } i.stepToNextGeneration(e, e + 1); break; case 3: { const s = t.data, r = s.length; if (0 === r) { i.fail(e); break } for (let t = 0; t < r; ++t)i.step(e, s[t], 0); break } case 4: { const r = (0, t.func)(t.data, h, s); null != r && i.record(e, r), i.step(e, e + 1, 0); break } }e = i.getNextThreadPc(); } i.nextGeneration(); } while (null !== n); const l = new e(i.getAcceptingTraces()); return i.reset(), this.P.push(i), l } } function b(t) { const s = new r; return t(s), new p(s.program) } var v = { Assembler: r, VM: p, compileVM: b }; t.Assembler = r, t.VM = p, t.compileVM = b, t.default = v, Object.defineProperty(t, "V", { value: !0 }); }));

  	// prsc 4.0.0

  	// The MIT License (MIT)

  	// Copyright (c) 2019 Stef Busking

  	// Permission is hereby granted, free of charge, to any person obtaining a copy of
  	// this software and associated documentation files (the "Software"), to deal in
  	// the Software without restriction, including without limitation the rights to
  	// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
  	// the Software, and to permit persons to whom the Software is furnished to do so,
  	// subject to the following conditions:

  	// The above copyright notice and this permission notice shall be included in all
  	// copies or substantial portions of the Software.

  	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  	// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  	// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  	// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  	// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  	// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  	!function (n, t) { t((n = "undefined" != typeof globalThis ? globalThis : n || self).prsc = {}); }(this, (function (n) { function t(n, t) { return { success: !0, offset: n, value: t } } function e(n) { return t(n, void 0) } function r(n, t, e = !1) { return { success: !1, offset: n, expected: t, fatal: e } } function o(n) { return n > 65535 ? 2 : 1 } function u(n, t) { return (u, c) => { const s = u.codePointAt(c); return void 0 !== s && n(s) ? e(c + o(s)) : r(c, t) } } function c(n, e) { return (r, o) => { const u = n(r, o); return u.success ? t(u.offset, e(u.value)) : u } } function s(n) { return (e, r) => { let o = [], u = r; for (; ;) { const t = n(e, u); if (!t.success) { if (t.fatal) return t; break } if (o.push(t.value), t.offset === u) break; u = t.offset; } return t(u, o) } } function f(n) { return (t, r) => { let o = r; for (; ;) { const e = n(t, o); if (!e.success) { if (e.fatal) return e; break } if (e.offset === o) break; o = e.offset; } return e(o) } } function i(n, e, r) { return (o, u) => { const c = n(o, u); if (!c.success) return c; const s = e(o, c.offset); return s.success ? t(s.offset, r(c.value, s.value)) : s } } function l(n, t) { return n } function d(n, t) { return t } function a(n, t) { return i(n, t, d) } function p(n, t) { return i(n, t, l) } function v(n, t) { return (o, u) => n(o, u).success ? r(u, t) : e(u) } function m(n) { return (t, e) => { const o = n(t, e); return o.success ? o : r(o.offset, o.expected, !0) } } const x = (n, t) => n.length === t ? e(t) : r(t, ["end of input"]); function g(n) { const t = []; let e = n.next(); for (; !e.done;)t.push(e.value), e = n.next(); return [t, e.value] } n.codepoint = u, n.codepoints = function (n, t) { return (o, u) => { const c = u; for (; ;) { const t = o.codePointAt(u); if (void 0 === t) break; if (!n(t)) break; u += t > 65535 ? 2 : 1; } return void 0 !== t && u === c ? r(u, t) : e(u) } }, n.collect = g, n.complete = function (n) { return i(n, x, l) }, n.consume = function (n) { return c(n, (() => { })) }, n.cut = m, n.delimited = function (n, t, e, r = !1) { return a(n, r ? m(p(t, e)) : p(t, e)) }, n.dispatch = function (n, t, e = 0, o = []) { return (u, c) => { const s = u.codePointAt(c + e); if (void 0 === s) return r(c, o); const f = n[s]; return void 0 === f ? void 0 === t ? r(c, o) : t(u, c) : f(u, c) } }, n.end = x, n.error = r, n.except = function (n, t, e) { return a(v(t, e), n) }, n.filter = function (n, t, e, o) { return (u, c) => { const s = n(u, c); return s.success ? t(s.value) ? s : r(c, e, o) : s } }, n.filterUndefined = function (n) { return c(n, (n => n.filter((n => void 0 !== n)))) }, n.first = l, n.followed = p, n.map = c, n.not = v, n.ok = e, n.okWithValue = t, n.optional = function (n) { return (e, r) => { const o = n(e, r); return o.success || o.fatal ? o : t(r, null) } }, n.or = function (n, t) { return (e, o) => { let u = null; for (const r of n) { const n = r(e, o); if (n.success) return n; if (null === u || n.offset > u.offset ? u = n : n.offset === u.offset && void 0 === t && (u.expected = u.expected.concat(n.expected)), n.fatal) return n } return t = t || (null == u ? void 0 : u.expected) || [], u && (u.expected = t), u || r(o, t) } }, n.peek = function (n) { return (e, r) => { const o = n(e, r); return o.success ? t(r, o.value) : o } }, n.plus = function (n) { return i(n, s(n), ((n, t) => [n].concat(t))) }, n.plusConsumed = function (n) { return i(n, f(n), d) }, n.preceded = a, n.range = function (n, t, e) { return u((e => n <= e && e <= t), e || [`${String.fromCodePoint(n)}-${String.fromCodePoint(t)}`]) }, n.recognize = function (n) { return (e, r) => { const o = n(e, r); return o.success ? t(o.offset, e.slice(r, o.offset)) : o } }, n.second = d, n.sequence = function (...n) { return (e, r) => { const o = []; for (const t of n) { const n = t(e, r); if (!n.success) return n; r = n.offset, o.push(n.value); } return t(r, o) } }, n.sequenceConsumed = function (...n) { return (t, r) => { for (const e of n) { const n = e(t, r); if (!n.success) return n; r = n.offset; } return e(r) } }, n.skipChars = function (n) { return (t, u) => { let c = n; for (; c > 0;) { const n = t.codePointAt(u); if (void 0 === n) return r(u, ["any character"]); u += o(n), c -= 1; } return e(u) } }, n.star = s, n.starConsumed = f, n.start = (n, t) => 0 === t ? e(t) : r(t, ["start of input"]), n.streaming = function (n) { return function* (t, e) { const r = n(t, e); return r.success && (yield r.value), r } }, n.streamingComplete = function (n) { return function* (t, e) { const r = yield* n(t, e); return r.success ? x(t, r.offset) : r } }, n.streamingFilterUndefined = function (n) { return function* (t, e) { const r = n(t, e); let o = r.next(); for (; !o.done;) { const n = o.value; void 0 !== n && (yield n), o = r.next(); } return o.value } }, n.streamingOptional = function (n) { return function* (t, r) { const [o, u] = g(n(t, r)); return u.success ? (yield* o, u) : u.fatal ? u : e(r) } }, n.streamingStar = function (n) { return function* (t, r) { for (; ;) { const [o, u] = g(n(t, r)); if (!u.success) return u.fatal ? u : e(r); if (yield* o, r === u.offset) return e(r); r = u.offset; } } }, n.streamingThen = function (n, t) { return function* (e, r) { const o = yield* n(e, r); return o.success ? yield* t(e, o.offset) : o } }, n.then = i, n.token = function (n) { return (e, o) => { const u = o + n.length; return e.slice(o, u) === n ? t(u, n) : r(o, [n]) } }, Object.defineProperty(n, "__esModule", { value: !0 }); }));

  	// xspattern 3.1.0

  	// 	The MIT License (MIT)

  	// Copyright (c) 2019 Stef Busking

  	// Permission is hereby granted, free of charge, to any person obtaining a copy of
  	// this software and associated documentation files (the "Software"), to deal in
  	// the Software without restriction, including without limitation the rights to
  	// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
  	// the Software, and to permit persons to whom the Software is furnished to do so,
  	// subject to the following conditions:

  	// The above copyright notice and this permission notice shall be included in all
  	// copies or substantial portions of the Software.

  	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  	// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  	// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  	// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  	// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  	// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  	!function (A, B) { B((A = "undefined" != typeof globalThis ? globalThis : A || self).xspattern = {}, A.whynot); }(this, (function (A, B) { function a(A) { return B => B === A } function n(A, B) { if (null === A || null === B) throw new Error("unescaped hyphen may not be used as a range endpoint"); if (B < A) throw new Error("character range is in the wrong order"); return a => A <= a && a <= B } function e(A) { return !0 } function t() { return !1 } function G(A, B) { return a => A(a) || B(a) } const i = -1, r = -2; function o(A, B) { switch (B.kind) { case "predicate": return void A.test(B.value); case "regexp": return void H(A, B.value, !1) } } function l(A, B) { B.forEach((B => { !function (A, B) { const [a, { min: n, max: e }] = B; if (null !== e) { for (let B = 0; B < n; ++B)o(A, a); for (let B = n; B < e; ++B) { const B = A.jump([]); B.data.push(A.program.length), o(A, a), B.data.push(A.program.length); } } else if (n > 0) { for (let B = 0; B < n - 1; ++B)o(A, a); const B = A.program.length; o(A, a), A.jump([B]).data.push(A.program.length); } else { const B = A.program.length, n = A.jump([]); n.data.push(A.program.length), o(A, a), A.jump([B]), n.data.push(A.program.length); } }(A, B); })); } function H(A, B, a) { const n = A.program.length, e = A.jump([]); a && (e.data.push(A.program.length), A.test((() => !0)), A.jump([n])); const t = []; if (B.forEach((B => { e.data.push(A.program.length), l(A, B), t.push(A.jump([])); })), t.forEach((B => { B.data.push(A.program.length); })), a) { const B = A.program.length, a = A.jump([]); a.data.push(A.program.length), A.test((() => !0)), A.jump([B]), a.data.push(A.program.length); } } function u(A, B) { return { success: !0, offset: A, value: B } } function C(A) { return u(A, void 0) } function s(A, B, a = !1) { return { success: !1, offset: A, expected: B, fatal: a } } function c(A) { return (B, a) => { const n = a + A.length; return B.slice(a, n) === A ? u(n, A) : s(a, [A]) } } function D(A, B) { return (a, n) => { const e = A(a, n); return e.success ? u(e.offset, B(e.value)) : e } } function m(A, B, a, n) { return (e, t) => { const G = A(e, t); return G.success ? B(G.value) ? G : s(t, a, n) : G } } function d(A, B) { return (a, n) => { let e = null; for (const t of A) { const A = t(a, n); if (A.success) return A; if (null === e || A.offset > e.offset ? e = A : A.offset === e.offset && void 0 === B && (e.expected = e.expected.concat(A.expected)), A.fatal) return A } return B = B || (null == e ? void 0 : e.expected) || [], e && (e.expected = B), e || s(n, B) } } function I(A) { return (B, a) => { const n = A(B, a); return n.success || n.fatal ? n : u(a, null) } } function h(A) { return (B, a) => { let n = [], e = a; for (; ;) { const a = A(B, e); if (!a.success) { if (a.fatal) return a; break } if (n.push(a.value), a.offset === e) break; e = a.offset; } return u(e, n) } } function p(A, B, a) { return (n, e) => { const t = A(n, e); if (!t.success) return t; const G = B(n, t.offset); return G.success ? u(G.offset, a(t.value, G.value)) : G } } function T(A) { return p(A, h(A), ((A, B) => [A].concat(B))) } function f(A, B) { return A } function F(A, B) { return B } function E(A, B) { return p(A, B, F) } function g(A, B) { return p(A, B, f) } function M(A, B, a, n = !1) { return E(A, n ? J(g(B, a)) : g(B, a)) } function P(A, B) { return (a, n) => A(a, n).success ? s(n, B) : C(n) } function J(A) { return (B, a) => { const n = A(B, a); return n.success ? n : s(n.offset, n.expected, !0) } } const S = (A, B) => A.length === B ? C(B) : s(B, ["end of input"]); const K = ["Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me", "Nd", "Nl", "No", "Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po", "Zs", "Zl", "Zp", "Sm", "Sc", "Sk", "So", "Cc", "Cf", "Co", "Cn"]; const y = {}; function b(A) { return A.codePointAt(0) } "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/".split("").forEach(((A, B) => { y[A] = B; })); const x = A => A === i || A === r; function Q(A) { return B => !x(B) && !A(B) } function L(A, B) { return null === B ? A : a => A(a) && !B(a) } const X = function (A, B) { const a = new Map; let e = 0; return A.forEach(((A, t) => { const i = B[t]; null !== A && A.split("|").forEach((A => { const B = a.get(A), t = n(e, e + i - 1); a.set(A, B ? G(B, t) : t); })), e += i; })), a }(["BasicLatin", "Latin-1Supplement", "LatinExtended-A", "LatinExtended-B", "IPAExtensions", "SpacingModifierLetters", "CombiningDiacriticalMarks", "GreekandCoptic|Greek", "Cyrillic", "CyrillicSupplement", "Armenian", "Hebrew", "Arabic", "Syriac", "ArabicSupplement", "Thaana", "NKo", "Samaritan", "Mandaic", "SyriacSupplement", "ArabicExtended-B", "ArabicExtended-A", "Devanagari", "Bengali", "Gurmukhi", "Gujarati", "Oriya", "Tamil", "Telugu", "Kannada", "Malayalam", "Sinhala", "Thai", "Lao", "Tibetan", "Myanmar", "Georgian", "HangulJamo", "Ethiopic", "EthiopicSupplement", "Cherokee", "UnifiedCanadianAboriginalSyllabics", "Ogham", "Runic", "Tagalog", "Hanunoo", "Buhid", "Tagbanwa", "Khmer", "Mongolian", "UnifiedCanadianAboriginalSyllabicsExtended", "Limbu", "TaiLe", "NewTaiLue", "KhmerSymbols", "Buginese", "TaiTham", "CombiningDiacriticalMarksExtended", "Balinese", "Sundanese", "Batak", "Lepcha", "OlChiki", "CyrillicExtended-C", "GeorgianExtended", "SundaneseSupplement", "VedicExtensions", "PhoneticExtensions", "PhoneticExtensionsSupplement", "CombiningDiacriticalMarksSupplement", "LatinExtendedAdditional", "GreekExtended", "GeneralPunctuation", "SuperscriptsandSubscripts", "CurrencySymbols", "CombiningDiacriticalMarksforSymbols|CombiningMarksforSymbols", "LetterlikeSymbols", "NumberForms", "Arrows", "MathematicalOperators", "MiscellaneousTechnical", "ControlPictures", "OpticalCharacterRecognition", "EnclosedAlphanumerics", "BoxDrawing", "BlockElements", "GeometricShapes", "MiscellaneousSymbols", "Dingbats", "MiscellaneousMathematicalSymbols-A", "SupplementalArrows-A", "BraillePatterns", "SupplementalArrows-B", "MiscellaneousMathematicalSymbols-B", "SupplementalMathematicalOperators", "MiscellaneousSymbolsandArrows", "Glagolitic", "LatinExtended-C", "Coptic", "GeorgianSupplement", "Tifinagh", "EthiopicExtended", "CyrillicExtended-A", "SupplementalPunctuation", "CJKRadicalsSupplement", "KangxiRadicals", null, "IdeographicDescriptionCharacters", "CJKSymbolsandPunctuation", "Hiragana", "Katakana", "Bopomofo", "HangulCompatibilityJamo", "Kanbun", "BopomofoExtended", "CJKStrokes", "KatakanaPhoneticExtensions", "EnclosedCJKLettersandMonths", "CJKCompatibility", "CJKUnifiedIdeographsExtensionA", "YijingHexagramSymbols", "CJKUnifiedIdeographs", "YiSyllables", "YiRadicals", "Lisu", "Vai", "CyrillicExtended-B", "Bamum", "ModifierToneLetters", "LatinExtended-D", "SylotiNagri", "CommonIndicNumberForms", "Phags-pa", "Saurashtra", "DevanagariExtended", "KayahLi", "Rejang", "HangulJamoExtended-A", "Javanese", "MyanmarExtended-B", "Cham", "MyanmarExtended-A", "TaiViet", "MeeteiMayekExtensions", "EthiopicExtended-A", "LatinExtended-E", "CherokeeSupplement", "MeeteiMayek", "HangulSyllables", "HangulJamoExtended-B", "HighSurrogates", "HighPrivateUseSurrogates", "LowSurrogates", "PrivateUseArea|PrivateUse", "CJKCompatibilityIdeographs", "AlphabeticPresentationForms", "ArabicPresentationForms-A", "VariationSelectors", "VerticalForms", "CombiningHalfMarks", "CJKCompatibilityForms", "SmallFormVariants", "ArabicPresentationForms-B", "HalfwidthandFullwidthForms", "Specials", "LinearBSyllabary", "LinearBIdeograms", "AegeanNumbers", "AncientGreekNumbers", "AncientSymbols", "PhaistosDisc", null, "Lycian", "Carian", "CopticEpactNumbers", "OldItalic", "Gothic", "OldPermic", "Ugaritic", "OldPersian", null, "Deseret", "Shavian", "Osmanya", "Osage", "Elbasan", "CaucasianAlbanian", "Vithkuqi", null, "LinearA", "LatinExtended-F", null, "CypriotSyllabary", "ImperialAramaic", "Palmyrene", "Nabataean", null, "Hatran", "Phoenician", "Lydian", null, "MeroiticHieroglyphs", "MeroiticCursive", "Kharoshthi", "OldSouthArabian", "OldNorthArabian", null, "Manichaean", "Avestan", "InscriptionalParthian", "InscriptionalPahlavi", "PsalterPahlavi", null, "OldTurkic", null, "OldHungarian", "HanifiRohingya", null, "RumiNumeralSymbols", "Yezidi", "ArabicExtended-C", "OldSogdian", "Sogdian", "OldUyghur", "Chorasmian", "Elymaic", "Brahmi", "Kaithi", "SoraSompeng", "Chakma", "Mahajani", "Sharada", "SinhalaArchaicNumbers", "Khojki", null, "Multani", "Khudawadi", "Grantha", null, "Newa", "Tirhuta", null, "Siddham", "Modi", "MongolianSupplement", "Takri", null, "Ahom", null, "Dogra", null, "WarangCiti", "DivesAkuru", null, "Nandinagari", "ZanabazarSquare", "Soyombo", "UnifiedCanadianAboriginalSyllabicsExtended-A", "PauCinHau", "DevanagariExtended-A", null, "Bhaiksuki", "Marchen", null, "MasaramGondi", "GunjalaGondi", null, "Makasar", "Kawi", null, "LisuSupplement", "TamilSupplement", "Cuneiform", "CuneiformNumbersandPunctuation", "EarlyDynasticCuneiform", null, "Cypro-Minoan", "EgyptianHieroglyphs", "EgyptianHieroglyphFormatControls", null, "AnatolianHieroglyphs", null, "BamumSupplement", "Mro", "Tangsa", "BassaVah", "PahawhHmong", null, "Medefaidrin", null, "Miao", null, "IdeographicSymbolsandPunctuation", "Tangut", "TangutComponents", "KhitanSmallScript", "TangutSupplement", null, "KanaExtended-B", "KanaSupplement", "KanaExtended-A", "SmallKanaExtension", "Nushu", null, "Duployan", "ShorthandFormatControls", null, "ZnamennyMusicalNotation", null, "ByzantineMusicalSymbols", "MusicalSymbols", "AncientGreekMusicalNotation", null, "KaktovikNumerals", "MayanNumerals", "TaiXuanJingSymbols", "CountingRodNumerals", null, "MathematicalAlphanumericSymbols", "SuttonSignWriting", null, "LatinExtended-G", "GlagoliticSupplement", "CyrillicExtended-D", null, "NyiakengPuachueHmong", null, "Toto", "Wancho", null, "NagMundari", null, "EthiopicExtended-B", "MendeKikakui", null, "Adlam", null, "IndicSiyaqNumbers", null, "OttomanSiyaqNumbers", null, "ArabicMathematicalAlphabeticSymbols", null, "MahjongTiles", "DominoTiles", "PlayingCards", "EnclosedAlphanumericSupplement", "EnclosedIdeographicSupplement", "MiscellaneousSymbolsandPictographs", "Emoticons", "OrnamentalDingbats", "TransportandMapSymbols", "AlchemicalSymbols", "GeometricShapesExtended", "SupplementalArrows-C", "SupplementalSymbolsandPictographs", "ChessSymbols", "SymbolsandPictographsExtended-A", "SymbolsforLegacyComputing", null, "CJKUnifiedIdeographsExtensionB", null, "CJKUnifiedIdeographsExtensionC", "CJKUnifiedIdeographsExtensionD", "CJKUnifiedIdeographsExtensionE", "CJKUnifiedIdeographsExtensionF", null, "CJKCompatibilityIdeographsSupplement", null, "CJKUnifiedIdeographsExtensionG", "CJKUnifiedIdeographsExtensionH", null, "Tags", null, "VariationSelectorsSupplement", null, "SupplementaryPrivateUseArea-A|PrivateUse", "SupplementaryPrivateUseArea-B|PrivateUse"], [128, 128, 128, 208, 96, 80, 112, 144, 256, 48, 96, 112, 256, 80, 48, 64, 64, 64, 32, 16, 48, 96, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 256, 160, 96, 256, 384, 32, 96, 640, 32, 96, 32, 32, 32, 32, 128, 176, 80, 80, 48, 96, 32, 32, 144, 80, 128, 64, 64, 80, 48, 16, 48, 16, 48, 128, 64, 64, 256, 256, 112, 48, 48, 48, 80, 64, 112, 256, 256, 64, 32, 160, 128, 32, 96, 256, 192, 48, 16, 256, 128, 128, 256, 256, 96, 32, 128, 48, 80, 96, 32, 128, 128, 224, 16, 16, 64, 96, 96, 48, 96, 16, 32, 48, 16, 256, 256, 6592, 64, 20992, 1168, 64, 48, 320, 96, 96, 32, 224, 48, 16, 64, 96, 32, 48, 48, 32, 96, 32, 96, 32, 96, 32, 48, 64, 80, 64, 11184, 80, 896, 128, 1024, 6400, 512, 80, 688, 16, 16, 16, 32, 32, 144, 240, 16, 128, 128, 64, 80, 64, 48, 128, 32, 64, 32, 48, 32, 48, 32, 64, 32, 80, 48, 48, 80, 48, 64, 80, 64, 384, 64, 64, 64, 32, 32, 48, 48, 32, 32, 32, 64, 32, 96, 96, 32, 32, 32, 64, 64, 32, 32, 48, 80, 80, 48, 128, 64, 288, 32, 64, 64, 48, 64, 64, 48, 32, 128, 80, 48, 80, 48, 96, 32, 80, 48, 48, 80, 128, 128, 128, 96, 160, 128, 96, 32, 80, 48, 80, 176, 80, 80, 96, 96, 64, 96, 80, 96, 16, 64, 96, 160, 112, 80, 64, 96, 80, 304, 32, 96, 80, 16, 64, 1024, 128, 208, 2624, 112, 1072, 48, 4e3, 640, 8576, 576, 48, 96, 48, 144, 688, 96, 96, 160, 64, 32, 6144, 768, 512, 128, 8816, 16, 256, 48, 64, 400, 2304, 160, 16, 4688, 208, 48, 256, 256, 80, 112, 32, 32, 96, 32, 128, 1024, 688, 1104, 256, 48, 96, 112, 80, 320, 48, 64, 464, 48, 736, 32, 224, 32, 96, 784, 80, 64, 80, 176, 256, 256, 48, 112, 96, 256, 256, 768, 80, 48, 128, 128, 128, 256, 256, 112, 144, 256, 1024, 42720, 32, 4160, 224, 5776, 7488, 3088, 544, 1504, 4944, 4192, 711760, 128, 128, 240, 65040, 65536, 65536]), Z = function (A) { const B = new Map, e = A.split(""), i = K.map((() => [])); let r = 0, o = 0; for (; o < e.length;) { const A = y[e[o]], B = (31 & A) - 2; let t = 1 + y[e[o + 1]]; switch (32 & A ? (t += y[e[o + 2]] << 6, t += y[e[o + 3]] << 12, t += y[e[o + 4]] << 18, o += 5) : o += 2, B) { case -2: { let A = 0; for (let B = r; B < r + t; ++B) { i[A].push(a(B)), A = (A + 1) % 2; } break } case -1: break; default: { const A = i[B]; 1 === t ? A.push(a(r)) : A.push(n(r, r + t - 1)); break } }r += t; } const l = new Map; return K.forEach(((A, a) => { const n = i[a].reduce(G, t); B.set(A, n); const e = A.charAt(0), r = l.get(e) || []; l.set(e, r), r.push(n); })), l.forEach(((A, a) => { B.set(a, A.reduce(G, t)); })), B }("bfUATCYATCPAQATAXATAOATBKJTBXCTBCZPATAQAZANAZADZPAXAQAXAbgUATAYDaATAZAaAGARAXAcAaAZAaAXAMBZADATBZAMAGASAMCTACWXACGDXXADHA3DAAPDAAtCAAFDBCAADCAABCCDBCCABCAABCCDCCAABCAAFCAADDAABCAABCBADCBDBGACADCGDCAEADACAEADACAEADAAPDAARDACAEADAABCBA7DFCAABCBDBABCCAJjDBAAGADaFRZDFLZNFEZGFAZAFAZQnvBAAADFAZACADABBFADCTACABDZBCATACCBACABACAABCQBACIDiCADBCCDCAXDDCADAXAABCBDBCyDvAhaAHEJBA1CAANDAgfBAABAClBBFATFDoTAOABBaBYABAHsOAHATAHBTAHBTAHABHGaBDGDTBBKcFXCTBYATBaBHKTAcATCGfFAGJHUKJTDGBHAmiBAATAGAHGcAaAHFFBHBaAHDGBKJGCaBGATNBAcAGAHAGdHaBBmYBAAHKGABNKJGgHIFBaATCFABBHAYBGVHDFAHIFAHCFAHEBBTOBAGYHCBBTABAGKBEGXZAGFBAcBBFHHGoFAHXcAHfIAG1HAIAHAGAICHHIDHAIBGAHGGJHBTBKJTAFAGOHAIBBAGHBBGBBBGVBAGGBAGABCGDBBHAGAICHDBBIBBBIBHAGABHIABDGBBAGCHBBBKJGBYBMFaAYAGATAHABBHBIABAGFBDGBBBGVBAGGBAGBBAGBBAGBBBHABAICHBBDHBBBHCBCHABGGDBAGABGKJHBGCHATABJHBIABAGIBAGCBAGVBAGGBAGBBAGEBBHAGAICHEBAHBIABAIBHABBGABOGBHBBBKJTAYABGGAHFBAHAIBBAGHBBGBBBGVBAGGBAGBBAGEBBHAGAIAHAIAHDBBIBBBIBHABGHBIABDGBBAGCHBBBKJaAGAMFBJHAGABAGFBCGCBAGDBCGBBAGABAGBBCGBBCGCBCGLBDIBHAIBBCICBAICHABBGABFIABNKJMCaFYAaABEHAICHAGHBAGCBAGWBAGPBBHAGAHCIDBAHCBAHDBGHBBAGCBBGABBGBHBBBKJBGTAMGaAGAHAIBTAGHBAGCBAGWBAGJBAGEBBHAGAIAHAIEBAHAIBBAIBHBBGIBBFGBBAGBHBBBKJBAGBIABLHBIBGIBAGCBAGoHBGAICHDBAICBAICHAGAaABDGCIAMGGCHBBBKJMIaAGFBAHAIBBAGRBCGXBAGIBAGABBGGBCHABDICHCBAHABAIHBFKJBBIBTABLGvHAGBHGBDYAGFFAHHTAKJTBBkGBBAGABAGEBAGXBAGABAGJHAGBHIGABBGEBAFABAHGBAKJBBGDBfGAaCTOaATAaCHBaFKJMJaAHAaAHAaAHAPAQAPAQAIBGHBAGjBDHNIAHETAHBGEHKBAHjBAaHHAaFBAaBTEaDTBBkGqIBHDIAHFIAHBIBHBGAKJTFGFIBHBGDHCGAICGBIGGCHDGMHAIBHBIFHAGAIAKJICHAaBClBACABECABBDqTAFADCmIFAABAGDBBGGBAGABAGDBBGoBAGDBBGgBAGDBBGGBAGABAGDBBGOBAG4BAGDBBmCBAABBHCTIMTBCGPaJBFiVBAABBDFBBOAmrJAAaATAGQUAGZPAQABCmKBAATCLCGHBGGRHCIABIGSHBIATBBIGRHBBLGMBAGCBAHBBLGzHBIAHGIHHAIBHKTCFATCYAGAHABBKJBFMJBFTFOATDHCcAHAKJBFGiFAG0BGGEHBGhHAGABEmFBAABJGeBAHCIDHBICBDIBHAIFHCBDaABCTBKJGdBBGEBKGrBDGZBFKJMABCahGWHBIBHABBTBG0IAHAIAHGBAHAIAHAIBHHIFHJBBHAKJBFKJBFTGFATFBBHNJAHPBwHDIAGuHAIAHEIAHAIEHAIBGHBCKJTGaJHIaITBBAHBIAGdIAHDIBHBIAHCGBKJGrHAIAHBICHAIAHCIBBHTDGjIHHHIBHBBCTEKJBCGCKJGdFFTBDIBGCqBBCCTHBHHCTAHMIAHGGDHAGFHAGBIAHBGABEDrF+DMFADhFkH/gVCAADHghBAADHCHDFBBCFBBDHCHDHCHDFBBCFBBDHBACABACABACABACADHCHDNBBDHEHDHEHDHEHDEBADBCDEAZADAZCDCBADBCDEAZCDDBBDBCDBAZCDHCEZCBBDCBADBCDEAZBBAUKcEOFTBRASAPARBSAPARATHVAWAcEUATIRASATDNBTCXAPAQATKXATANATJUAcEBAcJMAFABBMFXCPAQAFAMJXCPAQABAFMBCYgBOHMJDHAJCHLBOaBCAaDCAaBDACCDBCCDAaACAaBXACEaFCAaACAaACAaACDaADACDDAGDDAaBDBCBXECADDaAXAaBDAaAMPLiCADALDMAaBBDXEaEXBaDXAaBXAaBXAaGXAaeXBaBXAaAXAae3LEAAaHPAQAPAQAaTXBaGPAQA6QBAAXAadXYanXF6EBAABYaKBUM76NBAAMV62CAAXAaIXAa1XH6uBAAXA63DAAPAQAPAQAPAQAPAQAPAQAPAQAPAQAMdarXEPAQAXePAQAPAQAPAQAPAQAPAQAXP6/DAA3CCAAPAQAPAQAPAQAPAQAPAQAPAQAPAQAPAQAPAQAPAQAPAQAX+PAQAPAQAXfPAQA3BEAAavXUaBXFamBBafBA6oBAACvDvABCCDBAFCCADDACADFFBCBgjBAADAaFADHCCADABETDMATBDlBADABEDABBG3BGFATABNHAGWBIGGBAGGBAGGBAGGBAGGBAGGBAGGBAGGBAHfTBRASARASATCRASATARASATIOATBOATARASATBRASAPAQAPAQAPAQAPAQATEFATJOBTDOATAPATMaBTCPAQAPAQAPAQAPAQAOABhaZBA6YBAABL6VDAABZaLBDUATCaAFAGALAPAQAPAQAPAQAPAQAPAQAaBPAQAPAQAPAQAPAQAOAPAQBaALIHDIBOAFEaBLCFAGATAaBBAmVBAABBHBZBFBGAOAmZBAATAFCGABEGqBAmdBAABAaBMDaJGfajBLGPaeBAMJadMHaAMOafMJamMO6/EAAm/mBAa/mUIFAFAm2RAABCa2BIGnFFTBmLEAAFATCGPKJGBBTAtGAHAJCTAHJTAFAAbFBHBmFBAALJHBTFBHZWFIZBANDBA9FADHADCAAJFAZBADGAADDBATCDABCDAPCCADBECADABADABADAADBXFCCADAGAFBDAGGHAGCHAGDHAGWIBHBIAaDHABCMFaBYAaABFGzTDBHIBGxIPHBBHTBKJBFHRGFTCGATAGBHAKJGbHHTBGWHKIBBKTAGcBCHCIAGuHAIBHDIBHBICTMBAFAKJBDTBGEHAFAGIKJGEBAGoHFIBHBIBHBBIGCHAGHHAIABBKJBBTDGPFAGFaCGAIAHAIAGxHAGAHCGBHBGEHBGAHAGABXGBFATBGKIAHBIBTBGAFBIAHABJGFBBGFBBGFBIGGBAGGBADqZAFDDIFAZBBDjPBAAGiIBHAIBHAIBTAIAHABBKJBFmjuCABLGWBDGwhDgAA9/jBAmtFAABBmpBAABlDGBLDEBEGAHAGJXAGMBAGEBAGABAGBBAGBBAmrBAAZQBPmqFAAQAPAaPG/BBG1BGaABfGLYAaCHPTGPAQATABFHPTAOBNBPAQAPAQAPAQAPAQAPAQAPAQAPAQAPAQATBPAQATDNCTCBATDOAPAQAPAQAPAQATCXAOAXCBATAYATBBDGEBAmGCAABBcABATCYATCPAQATAXATAOATBKJTBXCTBCZPATAQAZANAZADZPAXAQAXAPAQATAPAQATBGJFAGsFBGeBCGFBBGFBBGFBBGCBCYBXAZAaAYBBAaAXDaBBJcCaBBBGLBAGZBAGSBAGBBAGOBBGNBhm6BAABETCBDMsBCaIL0MDaQMBaCBAaMBCaABuasHAhBCAAGcBCGwBOHAMaBDGfMDBIGTLAGHLABEGlHEBEGdBATAGjBDGHTALEBpCnDnmNBAABBKJBFCjBDDjBDGnBHGzBKTACKBACOBACGBACBBADKBADOBADGBADBhCBAAm2EAABIGVBJGHBXFFBAFpBAFIhEBAAGFBBGABAGrBAGBBCGABBGWBATAMHGWaBMGGeBHMIBvGSBAGBBEMEGVMFBCTAGZBETAB/G3BDMBGBMPBBMtGAHCBAHBBEHDGDBAGCBAGcBBHCBDHAMIBGTIBGGcMBTAGcMCBfGHaAGbHBBDMETGBIG1BCTGGVBBMHGSBEMHGRBGTDBLMGhPBAAmIBAAB2CyBMDyBGMFGjHDBHKJhlEAAMeBAGpBAHBOABBGBhKBAAHCGcMJGABHGVHKMDTEBVGRHDTDBlGUMGBTGWBIIAHAIAG0HOTGBDMTKJHAGBHBGABIHCIAGsICHDIBHBTBcATDHABJcABBGYBGKJBFHCGjHEIAHHBAKJTDGAIBGABHGiHATBGABIHBIAGvICHIIBGDTDHDTAIAHAKJGATAGATCBAMTBKGRBAGYICHCIBHAIAHBTFHAGBHAB9GGBAGABAGDBAGOBAGJTABFGuHAICHHBEKJBFHBIBBAGHBBGBBBGVBAGGBAGBBAGEBAHBGAIBHAIDBBIBBBICBBGABFIABEGEIBBBHGBCHEhKCAAG0ICHHIBHCIAHAGDTEKJTBBATAHAGCBdGvICHFIAHAIDHBIAHBGBTAGABHKJhlCAAGuICHDBBIDHBIAHBTWGDHBBhGvICHHIBHAIAHBTCGABKKJBFTMBSGqHAIAHAIBHFIAHAGATABFKJB1GaBBHCIBHDIAHEBDKJMBTCaAGGh4CAAGrICHIIAHBTAhjBAACfDfKJMIBLGHBBGABBGHBAGBBAGXIFBAIBBBHBIAHAGAIAGAIAHATCBIKJhFBAAGHBBGmICHDBBHBIDHAGATAGAIABaGAHJGnHFIAGAHDTHHABHGAHFIBHCGtHMIAHBTCGATEBMmIBAABGTJh1DAAGIBAGkIAHGBAHFIAHAGATEBJKJMSBCTBGdBBHVBAIAHGIAHBIAHBhIBAAGGBAGBBAGlHFBCHABAHBBAHGGAHABHKJBFGFBAGBBAGfIEBAHBBAIBHAIAHAGABGKJh1EAAGSHBIBTBBGHBGAIAGMBAGhIBHEBCIBHAIAHATMKJhVBAAGABOMUaHYDaQBMTAmZOAAhlBAAruBAABATEBKmDDAAhLpAAmgBAATBBMmvQAAcPHAGFHOhp+AAmGJAAh4GCAm4IAABGGeBAKJBDTBmOBAABAKJBFGdBBHETABJGvHGTEaDFDTAaABJKJBAMGBAGUBEGShvKAACfDfMWTDhkBAAmKBAABDHAGAI2BGHDFMB/FBTAFAHABKIBBNm3fBABHmVTAABpGIhmLCAFDBAFGBAFBBAmiEAABOGABcGCBBGABNGDBHmLGAAhDkAAmqBAABEGMBCGIBGGJBBaAHBTAcDhbJBAHtBBHWBI6zBAAB761DAABJamBBa7IBHCaCIFcHHHaBHGadHDa8BU6BBAAHCaAh5BAAMTBLMTBL6WBAABIMYhGCAACZDZCZDGBADRCZDZCABACBBBCABBCBBBCDBACHDDBADABADGBADKCZDZCBBACDBBCHBACGBADZCBBACDBACEBACABCCGBADZCZDZCZDZCZDZCZDZCZDZCZDbBBCYXADYXADFCYXADYXADFCYXADYXADFCYXADYXADFCYXADYXADFCADABBKx6/HAAH2aDHxaHHAaNHAaBTEBOHEBAHOhPRAADJGADTBFDFhUDAAHGBAHQBBHGBAHBBAHEBEF9BgHAhvBAAGsBCHGFGBBKJBDGAaAh/EAAGdHABQGrHDKJBEYAhPHAAGaFAHDKJhlLAAGGBAGDBAGBBAGOBAmEDAABBMIHGBoChDhHGFABDKJBDTBhQMAAM6aAMCYAMDhLBAAMsaAMOhBDAAGDBAGaBAGBBAGABBGABAGJBAGDBAGABAGABFGABDGABAGABAGABAGCBAGBBAGABBGABAGABAGABAGABAGABAGBBAGABBGDBAGGBAGDBAGDBAGABAGJBAGQBEGCBAGEBAGQBzXBhNEAAarBD6jBAABLaOBBaOBAaOBAakBJMM6gCAAB3acBMarBDaIBGaBBNaFhZCAA66DAAZE6XLAABDaQBCaMBC62BAABD6eBAABFaLBDaABOaLBDa3BHaJBFanBHadBBaBhNBAA6TFAABLaNBBaMBCaIBGatBAaGBHaNBDaIBGaIBG6SCAABAa2BkKJhFQAAmfbKABfm5ABABFmdDAABBmBaBABNmw0BAhewAAmdIAAhhXAAmKNBABEmfBBAhQxtCcABd8fBAAh/BAAnvDAAhP4PA99/PABB99/PA"); function O(A) { return 32 === A || 9 === A || 10 === A || 13 === A } const k = [a(b(":")), n(b("A"), b("Z")), a(b("_")), n(b("a"), b("z")), n(192, 214), n(216, 246), n(192, 214), n(216, 246), n(248, 767), n(880, 893), n(895, 8191), n(8204, 8205), n(8304, 8591), n(11264, 12271), n(12289, 55295), n(63744, 64975), n(65008, 65533), n(65536, 983039)].reduce(G), N = [k, a(b("-")), a(b(".")), n(b("0"), b("9")), a(183), n(768, 879), n(8255, 8256)].reduce(G), v = Z.get("Nd"), w = Q(v), Y = L(n(0, 1114111), [Z.get("P"), Z.get("Z"), Z.get("C")].reduce(G)), U = Q(Y); function j(A) { return 10 !== A && 13 !== A && !x(A) } const R = { s: O, S: Q(O), i: k, I: Q(k), c: N, C: Q(N), d: v, D: w, w: Y, W: U }, V = c("*"), W = c("\\"), q = c("{"), z = c("}"), $ = c("["), _ = c("]"), AA = c("^"), BA = c("$"), aA = c(","), nA = c("-"), eA = c("("), tA = c(")"), GA = c("."), iA = c("|"), rA = c("+"), oA = c("?"), lA = c("-["), HA = b("0"); function uA(A) { function B(A) { return new Set(A.split("").map((A => b(A)))) } function t(A, B) { const a = A.codePointAt(B); return void 0 === a ? s(B, ["any character"]) : u(B + String.fromCodePoint(a).length, a) } const o = "xpath" === A.language ? E(W, d([D(c("n"), (() => 10)), D(c("r"), (() => 13)), D(c("t"), (() => 9)), D(d([W, iA, GA, nA, AA, oA, V, rA, q, z, BA, eA, tA, $, _]), (A => b(A)))])) : E(W, d([D(c("n"), (() => 10)), D(c("r"), (() => 13)), D(c("t"), (() => 9)), D(d([W, iA, GA, nA, AA, oA, V, rA, q, z, eA, tA, $, _]), (A => b(A)))])); function l(A, a) { const n = B(a); return p(c(A), I(m(t, (A => n.has(A)), a.split(""))), ((A, B) => function (A) { const B = Z.get(A); if (null == B) throw new Error(`${A} is not a valid unicode category`); return B }(null === B ? A : A + String.fromCodePoint(B)))) } const H = d([l("L", "ultmo"), l("M", "nce"), l("N", "dlo"), l("P", "cdseifo"), l("Z", "slp"), l("S", "mcko"), l("C", "cfon")]), C = [n(b("a"), b("z")), n(b("A"), b("Z")), n(b("0"), b("9")), a(45)].reduce(G), F = D(E(c("Is"), function (A) { return (B, a) => { const n = A(B, a); return n.success ? u(n.offset, B.slice(a, n.offset)) : n } }(T(m(t, C, ["block identifier"])))), (B => function (A, B) { const a = X.get(A); if (void 0 === a) { if (B) return e; throw new Error(`The unicode block identifier "${A}" is not known.`) } return a }(B, "xpath" !== A.language))), K = d([H, F]), y = M(c("\\p{"), K, z, !0), x = D(M(c("\\P{"), K, z, !0), Q), O = E(W, D(d("sSiIcCdDwW".split("").map((A => c(A)))), (A => R[A]))), k = D(GA, (() => j)), N = d([O, y, x]), v = B("\\[]"), w = m(t, (A => !v.has(A)), ["unescaped character"]), Y = d([o, w]), U = d([D(nA, (() => null)), Y]), uA = p(U, E(nA, U), n); function CA(A, B) { return [A].concat(B || []) } const sA = D(function (A) { return (B, a) => { const n = A(B, a); return n.success ? u(a, n.value) : n } }(d([_, lA])), (() => null)), cA = b("-"), DA = d([D(g(g(nA, P($, ["not ["])), sA), (() => cA)), E(P(nA, ["not -"]), Y)]), mA = d([p(D(DA, a), d([function (A, B) { return mA(A, B) }, sA]), CA), p(d([uA, N]), d([IA, sA]), CA)]); const dA = d([p(D(Y, a), d([mA, sA]), CA), p(d([uA, N]), d([IA, sA]), CA)]); function IA(A, B) { return dA(A, B) } const hA = D(dA, (A => A.reduce(G))), pA = D(E(AA, hA), Q), TA = p(d([E(P(AA, ["not ^"]), hA), pA]), I(E(nA, (function (A, B) { return fA(A, B) }))), L), fA = M($, TA, _, !0); const FA = "xpath" === A.language ? d([D(o, a), N, fA, k, D(AA, (() => A => A === i)), D(BA, (() => A => A === r))]) : d([D(o, a), N, fA, k]), EA = "xpath" === A.language ? B(".\\?*+{}()|^$[]") : B(".\\?*+{}()|[]"), gA = m(t, (A => !EA.has(A)), ["NormalChar"]), MA = D(E(W, p(D(m(t, n(b("1"), b("9")), ["digit"]), (A => A - HA)), h(D(m(t, n(HA, b("9")), ["digit"]), (A => A - HA))), ((A, B) => { B.reduce(((A, B) => 10 * A + B), A); }))), (A => { throw new Error("Backreferences in XPath patterns are not yet implemented.") })), PA = "xpath" === A.language ? d([D(gA, (A => ({ kind: "predicate", value: a(A) }))), D(FA, (A => ({ kind: "predicate", value: A }))), D(M(eA, E(I(c("?:")), QA), tA, !0), (A => ({ kind: "regexp", value: A }))), MA]) : d([D(gA, (A => ({ kind: "predicate", value: a(A) }))), D(FA, (A => ({ kind: "predicate", value: A }))), D(M(eA, QA, tA, !0), (A => ({ kind: "regexp", value: A })))]), JA = D(T(D(m(t, n(HA, b("9")), ["digit"]), (A => A - HA))), (A => A.reduce(((A, B) => 10 * A + B)))), SA = d([p(JA, E(aA, JA), ((A, B) => { if (B < A) throw new Error("quantifier range is in the wrong order"); return { min: A, max: B } })), p(JA, aA, (A => ({ min: A, max: null }))), D(JA, (A => ({ min: A, max: A })))]), KA = "xpath" === A.language ? p(d([D(oA, (() => ({ min: 0, max: 1 }))), D(V, (() => ({ min: 0, max: null }))), D(rA, (() => ({ min: 1, max: null }))), M(q, SA, z, !0)]), I(oA), ((A, B) => A)) : d([D(oA, (() => ({ min: 0, max: 1 }))), D(V, (() => ({ min: 0, max: null }))), D(rA, (() => ({ min: 1, max: null }))), M(q, SA, z, !0)]), yA = p(PA, D(I(KA), (A => null === A ? { min: 1, max: 1 } : A)), ((A, B) => [A, B])), bA = h(yA), xA = p(bA, h(E(iA, J(bA))), ((A, B) => [A].concat(B))); function QA(A, B) { return xA(A, B) } const LA = function (A) { return p(A, S, f) }(xA); return function (A) { let B; try { B = LA(A, 0); } catch (B) { throw new Error(`Error parsing pattern "${A}": ${B instanceof Error ? B.message : B}`) } return B.success ? B.value : function (A, B, a) { const n = a.map((A => `"${A}"`)); throw new Error(`Error parsing pattern "${A}" at offset ${B}: expected ${n.length > 1 ? "one of " + n.join(", ") : n[0]} but found "${A.slice(B, B + 1)}"`) }(A, B.offset, B.expected) } } function CA(A) { return [...A].map((A => A.codePointAt(0))) } A.compile = function (A, a = { language: "xsd" }) { const n = uA(a)(A), e = B.compileVM((A => { H(A, n, "xpath" === a.language), A.accept(); })); return function (A) { const B = "xpath" === a.language ? [i, ...CA(A), r] : CA(A); return e.execute(B).success } }, Object.defineProperty(A, "__esModule", { value: !0 }); }));

  	// fontoxpath 3.29.0

  	// 	MIT License

  	// Copyright (c) 2017 Fonto Group BV

  	// Permission is hereby granted, free of charge, to any person obtaining a copy
  	// of this software and associated documentation files (the "Software"), to deal
  	// in the Software without restriction, including without limitation the rights
  	// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  	// copies of the Software, and to permit persons to whom the Software is
  	// furnished to do so, subject to the following conditions:

  	// The above copyright notice and this permission notice shall be included in all
  	// copies or substantial portions of the Software.

  	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  	// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  	// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  	// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  	// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  	// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  	// SOFTWARE.

  	(function (root, factory) {

  		// Browser globals (root is window)
  		// Maybe it is in scope:
  		root.fontoxpath = factory(root.xspattern, root.prsc);
  	})(this, function (xspattern, prsc) {
  		return (function (xspattern, prsc) {
  			const VERSION = '3.29.0';
  			const fontoxpathGlobal = {};
  			var h; function aa(a) { var b = 0; return function () { return b < a.length ? { done: !1, value: a[b++] } : { done: !0 } } } var ba = "function" == typeof Object.defineProperties ? Object.defineProperty : function (a, b, c) { if (a == Array.prototype || a == Object.prototype) return a; a[b] = c.value; return a };
  			function ca(a) { a = ["object" == typeof globalThis && globalThis, a, "object" == typeof window && window, "object" == typeof self && self, "object" == typeof global && global]; for (var b = 0; b < a.length; ++b) { var c = a[b]; if (c && c.Math == Math) return c } throw Error("Cannot find global object"); } var da = ca(this);			function fa(a, b) { if (b) a: { var c = da; a = a.split("."); for (var d = 0; d < a.length - 1; d++) { var e = a[d]; if (!(e in c)) break a; c = c[e]; } a = a[a.length - 1]; d = c[a]; b = b(d); b != d && null != b && ba(c, a, { configurable: !0, writable: !0, value: b }); } }
  			fa("Symbol", function (a) { function b(f) { if (this instanceof b) throw new TypeError("Symbol is not a constructor"); return new c(d + (f || "") + "_" + e++, f) } function c(f, g) { this.h = f; ba(this, "description", { configurable: !0, writable: !0, value: g }); } if (a) return a; c.prototype.toString = function () { return this.h }; var d = "jscomp_symbol_" + (1E9 * Math.random() >>> 0) + "_", e = 0; return b });
  			fa("Symbol.iterator", function (a) { if (a) return a; a = Symbol("Symbol.iterator"); for (var b = "Array Int8Array Uint8Array Uint8ClampedArray Int16Array Uint16Array Int32Array Uint32Array Float32Array Float64Array".split(" "), c = 0; c < b.length; c++) { var d = da[b[c]]; "function" === typeof d && "function" != typeof d.prototype[a] && ba(d.prototype, a, { configurable: !0, writable: !0, value: function () { return ha(aa(this)) } }); } return a }); fa("Symbol.asyncIterator", function (a) { return a ? a : Symbol("Symbol.asyncIterator") });
  			function ha(a) { a = { next: a }; a[Symbol.iterator] = function () { return this }; return a } function p(a) { var b = "undefined" != typeof Symbol && Symbol.iterator && a[Symbol.iterator]; return b ? b.call(a) : { next: aa(a) } } function ja(a) { for (var b, c = []; !(b = a.next()).done;)c.push(b.value); return c } function t(a) { return a instanceof Array ? a : ja(p(a)) } var ka = "function" == typeof Object.create ? Object.create : function (a) { function b() { } b.prototype = a; return new b }, la;
  			if ("function" == typeof Object.setPrototypeOf) la = Object.setPrototypeOf; else { var ma; a: { var na = { a: !0 }, pa = {}; try { pa.__proto__ = na; ma = pa.a; break a } catch (a) { } ma = !1; } la = ma ? function (a, b) { a.__proto__ = b; if (a.__proto__ !== b) throw new TypeError(a + " is not extensible"); return a } : null; } var qa = la;
  			function v(a, b) { a.prototype = ka(b.prototype); a.prototype.constructor = a; if (qa) qa(a, b); else for (var c in b) if ("prototype" != c) if (Object.defineProperties) { var d = Object.getOwnPropertyDescriptor(b, c); d && Object.defineProperty(a, c, d); } else a[c] = b[c]; a.zc = b.prototype; } function ra() { this.B = !1; this.o = null; this.l = void 0; this.h = 1; this.da = 0; this.v = null; } function sa(a) { if (a.B) throw new TypeError("Generator is already running"); a.B = !0; } ra.prototype.s = function (a) { this.l = a; }; function ta(a, b) { a.v = { kc: b, mc: !0 }; a.h = a.da; }
  			ra.prototype.return = function (a) { this.v = { return: a }; this.h = this.da; }; function ua(a) { this.h = new ra; this.o = a; } function va(a, b) { sa(a.h); var c = a.h.o; if (c) return wa(a, "return" in c ? c["return"] : function (d) { return { value: d, done: !0 } }, b, a.h.return); a.h.return(b); return xa(a) }
  			function wa(a, b, c, d) { try { var e = b.call(a.h.o, c); if (!(e instanceof Object)) throw new TypeError("Iterator result " + e + " is not an object"); if (!e.done) return a.h.B = !1, e; var f = e.value; } catch (g) { return a.h.o = null, ta(a.h, g), xa(a) } a.h.o = null; d.call(a.h, f); return xa(a) } function xa(a) { for (; a.h.h;)try { var b = a.o(a.h); if (b) return a.h.B = !1, { value: b.value, done: !1 } } catch (c) { a.h.l = void 0, ta(a.h, c); } a.h.B = !1; if (a.h.v) { b = a.h.v; a.h.v = null; if (b.mc) throw b.kc; return { value: b.return, done: !0 } } return { value: void 0, done: !0 } }
  			function ya(a) { this.next = function (b) { sa(a.h); a.h.o ? b = wa(a, a.h.o.next, b, a.h.s) : (a.h.s(b), b = xa(a)); return b }; this.throw = function (b) { sa(a.h); a.h.o ? b = wa(a, a.h.o["throw"], b, a.h.s) : (ta(a.h, b), b = xa(a)); return b }; this.return = function (b) { return va(a, b) }; this[Symbol.iterator] = function () { return this }; } function za(a) { function b(d) { return a.next(d) } function c(d) { return a.throw(d) } return new Promise(function (d, e) { function f(g) { g.done ? d(g.value) : Promise.resolve(g.value).then(b, c).then(f, e); } f(a.next()); }) }
  			function Aa() { for (var a = Number(this), b = [], c = a; c < arguments.length; c++)b[c - a] = arguments[c]; return b }
  			fa("Promise", function (a) {
  				function b(g) { this.o = 0; this.v = void 0; this.h = []; this.da = !1; var k = this.B(); try { g(k.resolve, k.reject); } catch (l) { k.reject(l); } } function c() { this.h = null; } function d(g) { return g instanceof b ? g : new b(function (k) { k(g); }) } if (a) return a; c.prototype.o = function (g) { if (null == this.h) { this.h = []; var k = this; this.v(function () { k.l(); }); } this.h.push(g); }; var e = da.setTimeout; c.prototype.v = function (g) { e(g, 0); }; c.prototype.l = function () {
  					for (; this.h && this.h.length;) {
  						var g = this.h; this.h = []; for (var k = 0; k < g.length; ++k) {
  							var l =
  								g[k]; g[k] = null; try { l(); } catch (m) { this.B(m); }
  						}
  					} this.h = null;
  				}; c.prototype.B = function (g) { this.v(function () { throw g; }); }; b.prototype.B = function () { function g(m) { return function (q) { l || (l = !0, m.call(k, q)); } } var k = this, l = !1; return { resolve: g(this.la), reject: g(this.l) } }; b.prototype.la = function (g) {
  					if (g === this) this.l(new TypeError("A Promise cannot resolve to itself")); else if (g instanceof b) this.ab(g); else {
  						a: switch (typeof g) { case "object": var k = null != g; break a; case "function": k = !0; break a; default: k = !1; }k ? this.S(g) :
  							this.s(g);
  					}
  				}; b.prototype.S = function (g) { var k = void 0; try { k = g.then; } catch (l) { this.l(l); return } "function" == typeof k ? this.Ba(k, g) : this.s(g); }; b.prototype.l = function (g) { this.ta(2, g); }; b.prototype.s = function (g) { this.ta(1, g); }; b.prototype.ta = function (g, k) { if (0 != this.o) throw Error("Cannot settle(" + g + ", " + k + "): Promise already settled in state" + this.o); this.o = g; this.v = k; 2 === this.o && this.pa(); this.A(); }; b.prototype.pa = function () {
  					var g = this; e(function () { if (g.K()) { var k = da.console; "undefined" !== typeof k && k.error(g.v); } },
  						1);
  				}; b.prototype.K = function () { if (this.da) return !1; var g = da.CustomEvent, k = da.Event, l = da.dispatchEvent; if ("undefined" === typeof l) return !0; "function" === typeof g ? g = new g("unhandledrejection", { cancelable: !0 }) : "function" === typeof k ? g = new k("unhandledrejection", { cancelable: !0 }) : (g = da.document.createEvent("CustomEvent"), g.initCustomEvent("unhandledrejection", !1, !0, g)); g.promise = this; g.reason = this.v; return l(g) }; b.prototype.A = function () {
  					if (null != this.h) {
  						for (var g = 0; g < this.h.length; ++g)f.o(this.h[g]); this.h =
  							null;
  					}
  				}; var f = new c; b.prototype.ab = function (g) { var k = this.B(); g.rb(k.resolve, k.reject); }; b.prototype.Ba = function (g, k) { var l = this.B(); try { g.call(k, l.resolve, l.reject); } catch (m) { l.reject(m); } }; b.prototype.then = function (g, k) { function l(z, A) { return "function" == typeof z ? function (D) { try { m(z(D)); } catch (F) { q(F); } } : A } var m, q, u = new b(function (z, A) { m = z; q = A; }); this.rb(l(g, m), l(k, q)); return u }; b.prototype.catch = function (g) { return this.then(void 0, g) }; b.prototype.rb = function (g, k) {
  					function l() {
  						switch (m.o) {
  							case 1: g(m.v);
  								break; case 2: k(m.v); break; default: throw Error("Unexpected state: " + m.o);
  						}
  					} var m = this; null == this.h ? f.o(l) : this.h.push(l); this.da = !0;
  				}; b.resolve = d; b.reject = function (g) { return new b(function (k, l) { l(g); }) }; b.race = function (g) { return new b(function (k, l) { for (var m = p(g), q = m.next(); !q.done; q = m.next())d(q.value).rb(k, l); }) }; b.all = function (g) {
  					var k = p(g), l = k.next(); return l.done ? d([]) : new b(function (m, q) {
  						function u(D) { return function (F) { z[D] = F; A--; 0 == A && m(z); } } var z = [], A = 0; do z.push(void 0), A++, d(l.value).rb(u(z.length -
  							1), q), l = k.next(); while (!l.done)
  					})
  				}; return b
  			}); function Ca(a, b) { return Object.prototype.hasOwnProperty.call(a, b) } var Da = "function" == typeof Object.assign ? Object.assign : function (a, b) { for (var c = 1; c < arguments.length; c++) { var d = arguments[c]; if (d) for (var e in d) Ca(d, e) && (a[e] = d[e]); } return a }; fa("Object.assign", function (a) { return a || Da });
  			function Ea(a, b, c) { if (null == a) throw new TypeError("The 'this' value for String.prototype." + c + " must not be null or undefined"); if (b instanceof RegExp) throw new TypeError("First argument to String.prototype." + c + " must not be a regular expression"); return a + "" } fa("String.prototype.repeat", function (a) { return a ? a : function (b) { var c = Ea(this, null, "repeat"); if (0 > b || 1342177279 < b) throw new RangeError("Invalid count value"); b |= 0; for (var d = ""; b;)if (b & 1 && (d += c), b >>>= 1) c += c; return d } });
  			function Fa(a, b) { a = void 0 !== a ? String(a) : " "; return 0 < b && a ? a.repeat(Math.ceil(b / a.length)).substring(0, b) : "" } fa("String.prototype.padEnd", function (a) { return a ? a : function (b, c) { var d = Ea(this, null, "padStart"); return d + Fa(c, b - d.length) } });
  			fa("WeakMap", function (a) {
  				function b(l) { this.h = (k += Math.random() + 1).toString(); if (l) { l = p(l); for (var m; !(m = l.next()).done;)m = m.value, this.set(m[0], m[1]); } } function c() { } function d(l) { var m = typeof l; return "object" === m && null !== l || "function" === m } function e(l) { if (!Ca(l, g)) { var m = new c; ba(l, g, { value: m }); } } function f(l) { var m = Object[l]; m && (Object[l] = function (q) { if (q instanceof c) return q; Object.isExtensible(q) && e(q); return m(q) }); } if (function () {
  					if (!a || !Object.seal) return !1; try {
  						var l = Object.seal({}), m = Object.seal({}),
  						q = new a([[l, 2], [m, 3]]); if (2 != q.get(l) || 3 != q.get(m)) return !1; q.delete(l); q.set(m, 4); return !q.has(l) && 4 == q.get(m)
  					} catch (u) { return !1 }
  				}()) return a; var g = "$jscomp_hidden_" + Math.random(); f("freeze"); f("preventExtensions"); f("seal"); var k = 0; b.prototype.set = function (l, m) { if (!d(l)) throw Error("Invalid WeakMap key"); e(l); if (!Ca(l, g)) throw Error("WeakMap key fail: " + l); l[g][this.h] = m; return this }; b.prototype.get = function (l) { return d(l) && Ca(l, g) ? l[g][this.h] : void 0 }; b.prototype.has = function (l) {
  					return d(l) && Ca(l,
  						g) && Ca(l[g], this.h)
  				}; b.prototype.delete = function (l) { return d(l) && Ca(l, g) && Ca(l[g], this.h) ? delete l[g][this.h] : !1 }; return b
  			});
  			fa("Map", function (a) {
  				function b() { var k = {}; return k.Ga = k.next = k.head = k } function c(k, l) { var m = k.h; return ha(function () { if (m) { for (; m.head != k.h;)m = m.Ga; for (; m.next != m.head;)return m = m.next, { done: !1, value: l(m) }; m = null; } return { done: !0, value: void 0 } }) } function d(k, l) {
  					var m = l && typeof l; "object" == m || "function" == m ? f.has(l) ? m = f.get(l) : (m = "" + ++g, f.set(l, m)) : m = "p_" + l; var q = k.o[m]; if (q && Ca(k.o, m)) for (k = 0; k < q.length; k++) { var u = q[k]; if (l !== l && u.key !== u.key || l === u.key) return { id: m, list: q, index: k, ka: u } } return {
  						id: m,
  						list: q, index: -1, ka: void 0
  					}
  				} function e(k) { this.o = {}; this.h = b(); this.size = 0; if (k) { k = p(k); for (var l; !(l = k.next()).done;)l = l.value, this.set(l[0], l[1]); } } if (function () {
  					if (!a || "function" != typeof a || !a.prototype.entries || "function" != typeof Object.seal) return !1; try {
  						var k = Object.seal({ x: 4 }), l = new a(p([[k, "s"]])); if ("s" != l.get(k) || 1 != l.size || l.get({ x: 4 }) || l.set({ x: 4 }, "t") != l || 2 != l.size) return !1; var m = l.entries(), q = m.next(); if (q.done || q.value[0] != k || "s" != q.value[1]) return !1; q = m.next(); return q.done || 4 != q.value[0].x ||
  							"t" != q.value[1] || !m.next().done ? !1 : !0
  					} catch (u) { return !1 }
  				}()) return a; var f = new WeakMap; e.prototype.set = function (k, l) { k = 0 === k ? 0 : k; var m = d(this, k); m.list || (m.list = this.o[m.id] = []); m.ka ? m.ka.value = l : (m.ka = { next: this.h, Ga: this.h.Ga, head: this.h, key: k, value: l }, m.list.push(m.ka), this.h.Ga.next = m.ka, this.h.Ga = m.ka, this.size++); return this }; e.prototype.delete = function (k) {
  					k = d(this, k); return k.ka && k.list ? (k.list.splice(k.index, 1), k.list.length || delete this.o[k.id], k.ka.Ga.next = k.ka.next, k.ka.next.Ga = k.ka.Ga,
  						k.ka.head = null, this.size--, !0) : !1
  				}; e.prototype.clear = function () { this.o = {}; this.h = this.h.Ga = b(); this.size = 0; }; e.prototype.has = function (k) { return !!d(this, k).ka }; e.prototype.get = function (k) { return (k = d(this, k).ka) && k.value }; e.prototype.entries = function () { return c(this, function (k) { return [k.key, k.value] }) }; e.prototype.keys = function () { return c(this, function (k) { return k.key }) }; e.prototype.values = function () { return c(this, function (k) { return k.value }) }; e.prototype.forEach = function (k, l) {
  					for (var m = this.entries(),
  						q; !(q = m.next()).done;)q = q.value, k.call(l, q[1], q[0], this);
  				}; e.prototype[Symbol.iterator] = e.prototype.entries; var g = 0; return e
  			}); function Ga(a, b, c) { a instanceof String && (a = String(a)); for (var d = a.length, e = 0; e < d; e++) { var f = a[e]; if (b.call(c, f, e, a)) return { Jb: e, Qb: f } } return { Jb: -1, Qb: void 0 } } fa("Array.prototype.find", function (a) { return a ? a : function (b, c) { return Ga(this, b, c).Qb } });
  			fa("String.prototype.startsWith", function (a) { return a ? a : function (b, c) { var d = Ea(this, b, "startsWith"), e = d.length, f = b.length; c = Math.max(0, Math.min(c | 0, d.length)); for (var g = 0; g < f && c < e;)if (d[c++] != b[g++]) return !1; return g >= f } }); fa("Array.prototype.fill", function (a) { return a ? a : function (b, c, d) { var e = this.length || 0; 0 > c && (c = Math.max(0, e + c)); if (null == d || d > e) d = e; d = Number(d); 0 > d && (d = Math.max(0, e + d)); for (c = Number(c || 0); c < d; c++)this[c] = b; return this } }); function Ha(a) { return a ? a : Array.prototype.fill }
  			fa("Int8Array.prototype.fill", Ha); fa("Uint8Array.prototype.fill", Ha); fa("Uint8ClampedArray.prototype.fill", Ha); fa("Int16Array.prototype.fill", Ha); fa("Uint16Array.prototype.fill", Ha); fa("Int32Array.prototype.fill", Ha); fa("Uint32Array.prototype.fill", Ha); fa("Float32Array.prototype.fill", Ha); fa("Float64Array.prototype.fill", Ha);
  			fa("Array.from", function (a) { return a ? a : function (b, c, d) { c = null != c ? c : function (k) { return k }; var e = [], f = "undefined" != typeof Symbol && Symbol.iterator && b[Symbol.iterator]; if ("function" == typeof f) { b = f.call(b); for (var g = 0; !(f = b.next()).done;)e.push(c.call(d, f.value, g++)); } else for (f = b.length, g = 0; g < f; g++)e.push(c.call(d, b[g], g)); return e } }); fa("Object.is", function (a) { return a ? a : function (b, c) { return b === c ? 0 !== b || 1 / b === 1 / c : b !== b && c !== c } });
  			fa("Array.prototype.includes", function (a) { return a ? a : function (b, c) { var d = this; d instanceof String && (d = String(d)); var e = d.length; c = c || 0; for (0 > c && (c = Math.max(c + e, 0)); c < e; c++) { var f = d[c]; if (f === b || Object.is(f, b)) return !0 } return !1 } }); fa("String.prototype.includes", function (a) { return a ? a : function (b, c) { return -1 !== Ea(this, b, "includes").indexOf(b, c || 0) } }); fa("Number.MAX_SAFE_INTEGER", function () { return 9007199254740991 }); fa("Number.MIN_SAFE_INTEGER", function () { return -9007199254740991 });
  			fa("Math.trunc", function (a) { return a ? a : function (b) { b = Number(b); if (isNaN(b) || Infinity === b || -Infinity === b || 0 === b) return b; var c = Math.floor(Math.abs(b)); return 0 > b ? -c : c } }); fa("Number.isFinite", function (a) { return a ? a : function (b) { return "number" !== typeof b ? !1 : !isNaN(b) && Infinity !== b && -Infinity !== b } }); fa("String.prototype.padStart", function (a) { return a ? a : function (b, c) { var d = Ea(this, null, "padStart"); return Fa(c, b - d.length) + d } });
  			function Ia(a, b) { a instanceof String && (a += ""); var c = 0, d = !1, e = { next: function () { if (!d && c < a.length) { var f = c++; return { value: b(f, a[f]), done: !1 } } d = !0; return { done: !0, value: void 0 } } }; e[Symbol.iterator] = function () { return e }; return e } fa("Array.prototype.keys", function (a) { return a ? a : function () { return Ia(this, function (b) { return b }) } }); fa("Number.isInteger", function (a) { return a ? a : function (b) { return Number.isFinite(b) ? b === Math.floor(b) : !1 } });
  			fa("Number.isSafeInteger", function (a) { return a ? a : function (b) { return Number.isInteger(b) && Math.abs(b) <= Number.MAX_SAFE_INTEGER } }); fa("Array.prototype.findIndex", function (a) { return a ? a : function (b, c) { return Ga(this, b, c).Jb } }); fa("String.prototype.endsWith", function (a) { return a ? a : function (b, c) { var d = Ea(this, b, "endsWith"); void 0 === c && (c = d.length); c = Math.max(0, Math.min(c | 0, d.length)); for (var e = b.length; 0 < e && 0 < c;)if (d[--c] != b[--e]) return !1; return 0 >= e } });
  			fa("String.fromCodePoint", function (a) { return a ? a : function (b) { for (var c = "", d = 0; d < arguments.length; d++) { var e = Number(arguments[d]); if (0 > e || 1114111 < e || e !== Math.floor(e)) throw new RangeError("invalid_code_point " + e); 65535 >= e ? c += String.fromCharCode(e) : (e -= 65536, c += String.fromCharCode(e >>> 10 & 1023 | 55296), c += String.fromCharCode(e & 1023 | 56320)); } return c } });
  			fa("String.prototype.codePointAt", function (a) { return a ? a : function (b) { var c = Ea(this, null, "codePointAt"), d = c.length; b = Number(b) || 0; if (0 <= b && b < d) { b |= 0; var e = c.charCodeAt(b); if (55296 > e || 56319 < e || b + 1 === d) return e; b = c.charCodeAt(b + 1); return 56320 > b || 57343 < b ? e : 1024 * (e - 55296) + b + 9216 } } });
  			fa("Set", function (a) {
  				function b(c) { this.h = new Map; if (c) { c = p(c); for (var d; !(d = c.next()).done;)this.add(d.value); } this.size = this.h.size; } if (function () {
  					if (!a || "function" != typeof a || !a.prototype.entries || "function" != typeof Object.seal) return !1; try {
  						var c = Object.seal({ x: 4 }), d = new a(p([c])); if (!d.has(c) || 1 != d.size || d.add(c) != d || 1 != d.size || d.add({ x: 4 }) != d || 2 != d.size) return !1; var e = d.entries(), f = e.next(); if (f.done || f.value[0] != c || f.value[1] != c) return !1; f = e.next(); return f.done || f.value[0] == c || 4 != f.value[0].x ||
  							f.value[1] != f.value[0] ? !1 : e.next().done
  					} catch (g) { return !1 }
  				}()) return a; b.prototype.add = function (c) { c = 0 === c ? 0 : c; this.h.set(c, c); this.size = this.h.size; return this }; b.prototype.delete = function (c) { c = this.h.delete(c); this.size = this.h.size; return c }; b.prototype.clear = function () { this.h.clear(); this.size = 0; }; b.prototype.has = function (c) { return this.h.has(c) }; b.prototype.entries = function () { return this.h.entries() }; b.prototype.values = function () { return this.h.values() }; b.prototype.keys = b.prototype.values; b.prototype[Symbol.iterator] =
  					b.prototype.values; b.prototype.forEach = function (c, d) { var e = this; this.h.forEach(function (f) { return c.call(d, f, f, e) }); }; return b
  			}); fa("Math.log10", function (a) { return a ? a : function (b) { return Math.log(b) / Math.LN10 } }); fa("Object.values", function (a) { return a ? a : function (b) { var c = [], d; for (d in b) Ca(b, d) && c.push(b[d]); return c } }); fa("Number.isNaN", function (a) { return a ? a : function (b) { return "number" === typeof b && isNaN(b) } });
  			fa("Array.prototype.values", function (a) { return a ? a : function () { return Ia(this, function (b, c) { return c }) } }); fa("Object.getOwnPropertySymbols", function (a) { return a ? a : function () { return [] } }); function Ka(a, b) { if (!("0" !== a && "-0" !== a || "0" !== b && "-0" !== b)) return 0; var c = /(?:\+|(-))?(\d+)?(?:\.(\d+))?/; a = c.exec(a + ""); var d = c.exec(b + ""), e = !a[1], f = !d[1]; b = (a[2] || "").replace(/^0*/, ""); c = (d[2] || "").replace(/^0*/, ""); a = a[3] || ""; d = d[3] || ""; if (e && !f) return 1; if (!e && f) return -1; e = e && f; if (b.length > c.length) return e ? 1 : -1; if (b.length < c.length) return e ? -1 : 1; if (b > c) return e ? 1 : -1; if (b < c) return e ? -1 : 1; b = Math.max(a.length, d.length); c = a.padEnd(b, "0"); b = d.padEnd(b, "0"); return c > b ? e ? 1 : -1 : c < b ? e ? -1 : 1 : 0 } function La(a, b) { a = a.toString(); if (-1 < a.indexOf(".") && 0 === b) return !1; a = /^[-+]?0*([1-9]\d*)?(?:\.((?:\d*[1-9])*)0*)?$/.exec(a); return a[2] ? a[2].length <= b : !0 } function Ma() { return function (a, b) { return 1 > Ka(a, b) } } function Na() { return function (a, b) { return 0 > Ka(a, b) } } function Oa() { return function (a, b) { return -1 < Ka(a, b) } } function Pa() { return function (a, b) { return 0 < Ka(a, b) } }
  			function Qa(a, b) { switch (b) { case "required": return /(Z)|([+-])([01]\d):([0-5]\d)$/.test(a.toString()); case "prohibited": return !/(Z)|([+-])([01]\d):([0-5]\d)$/.test(a.toString()); case "optional": return !0 } } function Sa(a) { switch (a) { case 1: case 0: case 6: case 3: return {}; case 4: return { na: La, ya: Ma(), xc: Na(), za: Oa(), yc: Pa() }; case 18: return {}; case 9: case 8: case 7: case 11: case 12: case 13: case 15: case 14: return { Ea: Qa }; case 22: case 21: case 20: case 23: case 44: return {}; default: return null } } var Ta = {}, Ua = {}; function Va(a) { return /^([+-]?(\d*(\.\d*)?([eE][+-]?\d*)?|INF)|NaN)$/.test(a) } function Wa(a) { return /^[_:A-Za-z][-._:A-Za-z0-9]*$/.test(a) } function Xa(a) { return Wa(a) && /^[_A-Za-z]([-._A-Za-z0-9])*$/.test(a) } function Ya(a) { a = a.split(":"); return 1 === a.length ? Xa(a[0]) : 2 !== a.length ? !1 : Xa(a[0]) && Xa(a[1]) } function Za(a) { return !/[\u0009\u000A\u000D]/.test(a) } function $a(a) { return Xa(a) }
  			var ab = new Map([[45, function () { return !0 }], [46, function () { return !0 }], [1, function () { return !0 }], [0, function (a) { return /^(0|1|true|false)$/.test(a) }], [6, function (a) { return Va(a) }], [3, Va], [4, function (a) { return /^[+-]?\d*(\.\d*)?$/.test(a) }], [18, function (a) { return /^(-)?P(\d+Y)?(\d+M)?(\d+D)?(T(\d+H)?(\d+M)?(\d+(\.\d*)?S)?)?$/.test(a) }], [9, function (a) { return /^-?([1-9][0-9]{3,}|0[0-9]{3})-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])T(([01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9](\.[0-9]+)?|(24:00:00(\.0+)?))(Z|(\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00))?$/.test(a) }],
  			[8, function (a) { return /^(([01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9](\.[0-9]+)?|(24:00:00(\.0+)?))(Z|(\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00))?$/.test(a) }], [7, function (a) { return /^-?([1-9][0-9]{3,}|0[0-9]{3})-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])(Z|(\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00))?$/.test(a) }], [11, function (a) { return /^-?([1-9][0-9]{3,}|0[0-9]{3})-(0[1-9]|1[0-2])(Z|(\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00))?$/.test(a) }], [12, function (a) { return /^-?([1-9][0-9]{3,}|0[0-9]{3})(Z|(\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00))?$/.test(a) }],
  			[13, function (a) { return /^--(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])(Z|(\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00))?$/.test(a) }], [15, function (a) { return /^---(0[1-9]|[12][0-9]|3[01])(Z|(\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00))?$/.test(a) }], [14, function (a) { return /^--(0[1-9]|1[0-2])(Z|(\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00))?$/.test(a) }], [22, function (a) { return /^([0-9A-Fa-f]{2})*$/.test(a) }], [21, function (a) { return (new RegExp(/^((([A-Za-z0-9+/] ?){4})*((([A-Za-z0-9+/] ?){3}[A-Za-z0-9+/])|(([A-Za-z0-9+/] ?){2}[AEIMQUYcgkosw048] ?=)|(([A-Za-z0-9+/] ?)[AQgw] ?= ?=)))?$/)).test(a) }],
  			[20, function () { return !0 }], [44, Ya], [48, Za], [52, function (a) { return Za(a) && !/^ | {2,}| $/.test(a) }], [51, function (a) { return /^[a-zA-Z]{1,8}(-[a-zA-Z0-9]{1,8})*$/.test(a) }], [50, function (a) { return /^[-._:A-Za-z0-9]+$/.test(a) }], [25, Wa], [23, Ya], [24, Xa], [42, $a], [41, $a], [26, function (a) { return Xa(a) }], [5, function (a) { return /^[+-]?\d+$/.test(a) }], [16, function (a) { return /^-?P[0-9]+(Y([0-9]+M)?|M)$/.test(a) }], [17, function (a) { return /^-?P([0-9]+D)?(T([0-9]+H)?([0-9]+M)?([0-9]+(\.[0-9]+)?S)?)?$/.test(a) }]]); var bb = Object.create(null);
  			[{ D: 0, name: 59 }, { D: 0, name: 46, parent: 59, L: { whiteSpace: "preserve" } }, { D: 0, name: 19, parent: 46 }, { D: 0, name: 1, parent: 46 }, { D: 0, name: 0, parent: 46, L: { whiteSpace: "collapse" } }, { D: 0, name: 4, parent: 46, L: { whiteSpace: "collapse" } }, { D: 0, name: 6, parent: 46, L: { whiteSpace: "collapse" } }, { D: 0, name: 3, parent: 46, L: { whiteSpace: "collapse" } }, { D: 0, name: 18, parent: 46, L: { whiteSpace: "collapse" } }, { D: 0, name: 9, parent: 46, L: { Ea: "optional", whiteSpace: "collapse" } }, { D: 0, name: 8, parent: 46, L: { Ea: "optional", whiteSpace: "collapse" } }, {
  				D: 0, name: 7, parent: 46,
  				L: { Ea: "optional", whiteSpace: "collapse" }
  			}, { D: 0, name: 11, parent: 46, L: { Ea: "optional", whiteSpace: "collapse" } }, { D: 0, name: 12, parent: 46, L: { Ea: "optional", whiteSpace: "collapse" } }, { D: 0, name: 13, parent: 46, L: { Ea: "optional", whiteSpace: "collapse" } }, { D: 0, name: 15, parent: 46, L: { Ea: "optional", whiteSpace: "collapse" } }, { D: 0, name: 14, parent: 46, L: { Ea: "optional", whiteSpace: "collapse" } }, { D: 0, name: 22, parent: 46, L: { whiteSpace: "collapse" } }, { D: 0, name: 21, parent: 46, L: { whiteSpace: "collapse" } }, { D: 0, name: 20, parent: 46, L: { whiteSpace: "collapse" } },
  			{ D: 0, name: 23, parent: 46, L: { whiteSpace: "collapse" } }, { D: 0, name: 44, parent: 46, L: { whiteSpace: "collapse" } }, { D: 1, name: 10, T: 9, L: { whiteSpace: "collapse", Ea: "required" } }, { D: 1, name: 48, T: 1, L: { whiteSpace: "replace" } }, { D: 1, name: 52, T: 48, L: { whiteSpace: "collapse" } }, { D: 1, name: 51, T: 52, L: { whiteSpace: "collapse" } }, { D: 1, name: 50, T: 52, L: { whiteSpace: "collapse" } }, { D: 2, name: 49, type: 50, L: { minLength: 1, whiteSpace: "collapse" } }, { D: 1, name: 25, T: 52, L: { whiteSpace: "collapse" } }, { D: 1, name: 24, T: 25, L: { whiteSpace: "collapse" } }, {
  				D: 1, name: 42, T: 24,
  				L: { whiteSpace: "collapse" }
  			}, { D: 1, name: 41, T: 24, L: { whiteSpace: "collapse" } }, { D: 2, name: 43, type: 41, L: { minLength: 1, whiteSpace: "collapse" } }, { D: 1, name: 26, T: 24, L: { whiteSpace: "collapse" } }, { D: 2, name: 40, type: 26, L: { minLength: 1, whiteSpace: "collapse" } }, { D: 0, name: 5, parent: 4, L: { na: 0, whiteSpace: "collapse" } }, { D: 1, name: 27, T: 5, L: { na: 0, ya: "0", whiteSpace: "collapse" } }, { D: 1, name: 28, T: 27, L: { na: 0, ya: "-1", whiteSpace: "collapse" } }, { D: 1, name: 31, T: 5, L: { na: 0, ya: "9223372036854775807", za: "-9223372036854775808", whiteSpace: "collapse" } },
  			{ D: 1, name: 32, T: 31, L: { na: 0, ya: "2147483647", za: "-2147483648", whiteSpace: "collapse" } }, { D: 1, name: 33, T: 32, L: { na: 0, ya: "32767", za: "-32768", whiteSpace: "collapse" } }, { D: 1, name: 34, T: 33, L: { na: 0, ya: "127", za: "-128", whiteSpace: "collapse" } }, { D: 1, name: 30, T: 5, L: { na: 0, za: "0", whiteSpace: "collapse" } }, { D: 1, name: 36, T: 30, L: { na: 0, ya: "18446744073709551615", za: "0", whiteSpace: "collapse" } }, { D: 1, name: 35, T: 36, L: { na: 0, ya: "4294967295", za: "0", whiteSpace: "collapse" } }, { D: 1, name: 38, T: 35, L: { na: 0, ya: "65535", za: "0", whiteSpace: "collapse" } },
  			{ D: 1, name: 37, T: 38, L: { na: 0, ya: "255", za: "0", whiteSpace: "collapse" } }, { D: 1, name: 29, T: 30, L: { na: 0, za: "1", whiteSpace: "collapse" } }, { D: 1, name: 16, T: 18, L: { whiteSpace: "collapse" } }, { D: 1, name: 17, T: 18, L: { whiteSpace: "collapse" } }, { D: 1, name: 60, T: 59 }, { D: 3, name: 39, Fa: [] }, { D: 1, name: 61, T: 60 }, { D: 1, name: 62, T: 60 }, { D: 0, name: 53, parent: 59 }, { D: 1, name: 54, T: 53 }, { D: 1, name: 58, T: 53 }, { D: 1, name: 47, T: 53 }, { D: 1, name: 56, T: 53 }, { D: 1, name: 57, T: 53 }, { D: 1, name: 55, T: 53 }, { D: 3, name: 2, Fa: [4, 5, 6, 3] }, { D: 3, name: 63, Fa: [] }].forEach(function (a) {
  				var b =
  					a.name, c = a.L || {}; switch (a.D) { case 0: a = a.parent ? bb[a.parent] : null; var d = ab.get(b) || null; bb[b] = { D: 0, type: b, Na: c, parent: a, kb: d, Pa: Sa(b), Fa: [] }; break; case 1: a = bb[a.T]; d = ab.get(b) || null; bb[b] = { D: 1, type: b, Na: c, parent: a, kb: d, Pa: a.Pa, Fa: [] }; break; case 2: bb[b] = { D: 2, type: b, Na: c, parent: bb[a.type], kb: null, Pa: Ta, Fa: [] }; break; case 3: a = a.Fa.map(function (e) { return bb[e] }), bb[b] = { D: 3, type: b, Na: c, parent: null, kb: null, Pa: Ua, Fa: a }; }
  			}); function w(a, b) { if (!bb[b]) throw Error("Unknown type"); return { type: b, value: a } } var cb = w(!0, 0), db = w(!1, 0); function eb(a) { return Error("FORG0006: " + (void 0 === a ? "A wrong argument type was specified in a function call." : a)) } function fb(a, b) { this.done = a; this.value = b; } var x = new fb(!0, void 0); function y(a) { return new fb(!1, a) } function gb(a, b) { if (3 === b.D) return !!b.Fa.find(function (c) { return gb(a, c) }); for (; a;) { if (a.type === b.type) return !0; if (3 === a.D) return !!a.Fa.find(function (c) { return B(c.type, b.type) }); a = a.parent; } return !1 } function B(a, b) { return a === b ? !0 : gb(bb[a], bb[b]) } function hb(a) { this.o = C; this.h = a; var b = -1; this.value = { next: function () { b++; return b >= a.length ? x : y(a[b]) } }; } h = hb.prototype; h.sb = function () { return this }; h.filter = function (a) { var b = this, c = -1; return this.o.create({ next: function () { for (c++; c < b.h.length && !a(b.h[c], c, b);)c++; return c >= b.h.length ? x : y(b.h[c]) } }) }; h.first = function () { return this.h[0] }; h.O = function () { return this.h };
  			h.ha = function () { if (B(this.h[0].type, 53)) return !0; throw eb("Cannot determine the effective boolean value of a sequence with a length higher than one."); }; h.Qa = function () { return this.h.length }; h.G = function () { return !1 }; h.wa = function () { return !1 }; h.map = function (a) { var b = this, c = -1; return this.o.create({ next: function () { return ++c >= b.h.length ? x : y(a(b.h[c], c, b)) } }, this.h.length) }; h.M = function (a) { return a(this.h) }; h.Z = function (a) { return a.multiple ? a.multiple(this) : a.default(this) }; function ib() { this.value = { next: function () { return x } }; } h = ib.prototype; h.sb = function () { return this }; h.filter = function () { return this }; h.first = function () { return null }; h.O = function () { return [] }; h.ha = function () { return !1 }; h.Qa = function () { return 0 }; h.G = function () { return !0 }; h.wa = function () { return !1 }; h.map = function () { return this }; h.M = function (a) { return a([]) }; h.Z = function (a) { return a.empty ? a.empty(this) : a.default(this) }; function jb(a, b) { this.type = a; this.value = b; }
  			var E = {}, mb = (E[0] = "xs:boolean", E[1] = "xs:string", E[2] = "xs:numeric", E[3] = "xs:double", E[4] = "xs:decimal", E[5] = "xs:integer", E[6] = "xs:float", E[7] = "xs:date", E[8] = "xs:time", E[9] = "xs:dateTime", E[10] = "xs:dateTimeStamp", E[11] = "xs:gYearMonth", E[12] = "xs:gYear", E[13] = "xs:gMonthDay", E[14] = "xs:gMonth", E[15] = "xs:gDay", E[16] = "xs:yearMonthDuration", E[17] = "xs:dayTimeDuration", E[18] = "xs:duration", E[19] = "xs:untypedAtomic", E[20] = "xs:anyURI", E[21] = "xs:base64Binary", E[22] = "xs:hexBinary", E[23] = "xs:QName", E[24] = "xs:NCName",
  				E[25] = "xs:Name", E[26] = "xs:ENTITY", E[27] = "xs:nonPositiveInteger", E[28] = "xs:negativeInteger", E[29] = "xs:positiveInteger", E[30] = "xs:nonNegativeInteger", E[31] = "xs:long", E[32] = "xs:int", E[33] = "xs:short", E[34] = "xs:byte", E[35] = "xs:unsignedInt", E[36] = "xs:unsignedLong", E[37] = "xs:unsignedByte", E[38] = "xs:unsignedShort", E[39] = "xs:error", E[40] = "xs:ENTITIES", E[41] = "xs:IDREF", E[42] = "xs:ID", E[43] = "xs:IDREFS", E[44] = "xs:NOTATION", E[45] = "xs:anySimpleType", E[46] = "xs:anyAtomicType", E[47] = "attribute()", E[48] = "xs:normalizedString",
  				E[49] = "xs:NMTOKENS", E[50] = "xs:NMTOKEN", E[51] = "xs:language", E[52] = "xs:token", E[53] = "node()", E[54] = "element()", E[55] = "document-node()", E[56] = "text()", E[57] = "processing-instruction()", E[58] = "comment()", E[59] = "item()", E[60] = "function(*)", E[61] = "map(*)", E[62] = "array(*)", E[63] = "none", E), nb = {
  					"xs:boolean": 0, "xs:string": 1, "xs:numeric": 2, "xs:double": 3, "xs:decimal": 4, "xs:integer": 5, "xs:float": 6, "xs:date": 7, "xs:time": 8, "xs:dateTime": 9, "xs:dateTimeStamp": 10, "xs:gYearMonth": 11, "xs:gYear": 12, "xs:gMonthDay": 13, "xs:gMonth": 14,
  					"xs:gDay": 15, "xs:yearMonthDuration": 16, "xs:dayTimeDuration": 17, "xs:duration": 18, "xs:untypedAtomic": 19, "xs:anyURI": 20, "xs:base64Binary": 21, "xs:hexBinary": 22, "xs:QName": 23, "xs:NCName": 24, "xs:Name": 25, "xs:ENTITY": 26, "xs:nonPositiveInteger": 27, "xs:negativeInteger": 28, "xs:positiveInteger": 29, "xs:nonNegativeInteger": 30, "xs:long": 31, "xs:int": 32, "xs:short": 33, "xs:byte": 34, "xs:unsignedInt": 35, "xs:unsignedLong": 36, "xs:unsignedByte": 37, "xs:unsignedShort": 38, "xs:error": 39, "xs:ENTITIES": 40, "xs:IDREF": 41, "xs:ID": 42,
  					"xs:IDREFS": 43, "xs:NOTATION": 44, "xs:anySimpleType": 45, "xs:anyAtomicType": 46, "attribute()": 47, "xs:normalizedString": 48, "xs:NMTOKENS": 49, "xs:NMTOKEN": 50, "xs:language": 51, "xs:token": 52, "node()": 53, "element()": 54, "document-node()": 55, "text()": 56, "processing-instruction()": 57, "comment()": 58, "item()": 59, "function(*)": 60, "map(*)": 61, "array(*)": 62
  				}; function ob(a) { return 2 === a.g ? mb[a.type] + "*" : 1 === a.g ? mb[a.type] + "+" : 0 === a.g ? mb[a.type] + "?" : mb[a.type] }
  			function pb(a) { if ("none" === a) throw Error('XPST0051: The type "none" could not be found'); if (!a.startsWith("xs:") && 0 <= a.indexOf(":")) throw Error("XPST0081: Invalid prefix for input " + a); var b = nb[a]; if (void 0 === b) throw Error('XPST0051: The type "' + a + '" could not be found'); return b }
  			function qb(a) { switch (a[a.length - 1]) { case "*": return { type: pb(a.substr(0, a.length - 1)), g: 2 }; case "?": return { type: pb(a.substr(0, a.length - 1)), g: 0 }; case "+": return { type: pb(a.substr(0, a.length - 1)), g: 1 }; default: return { type: pb(a), g: 3 } } } function rb(a) { switch (a) { case "*": return 2; case "?": return 0; case "+": return 1; default: return 3 } } function sb(a) { var b = a.value; if (B(a.type, 53)) return !0; if (B(a.type, 0)) return b; if (B(a.type, 1) || B(a.type, 20) || B(a.type, 19)) return 0 !== b.length; if (B(a.type, 2)) return !isNaN(b) && 0 !== b; throw eb("Cannot determine the effective boolean value of a value with the type " + mb[a.type]); } function tb(a, b) { var c = this; this.B = C; this.value = { next: function (d) { if (null !== c.o && c.h >= c.o) return x; if (void 0 !== c.v[c.h]) return y(c.v[c.h++]); d = a.next(d); if (d.done) return c.o = c.h, d; if (c.l || 2 > c.h) c.v[c.h] = d.value; c.h++; return d } }; this.l = !1; this.v = []; this.h = 0; this.o = void 0 === b ? null : b; } h = tb.prototype; h.sb = function () { return this.B.create(this.O()) }; h.filter = function (a) { var b = this, c = -1, d = this.value; return this.B.create({ next: function (e) { c++; for (e = d.next(e); !e.done && !a(e.value, c, b);)c++, e = d.next(0); return e } }) };
  			h.first = function () { if (void 0 !== this.v[0]) return this.v[0]; var a = this.value.next(0); ub(this); return a.done ? null : a.value }; h.O = function () { if (this.h > this.v.length && this.o !== this.v.length) throw Error("Implementation error: Sequence Iterator has progressed."); var a = this.value; this.l = !0; for (var b = a.next(0); !b.done;)b = a.next(0); return this.v };
  			h.ha = function () { var a = this.value, b = this.h; ub(this); var c = a.next(0); if (c.done) return ub(this, b), !1; c = c.value; if (B(c.type, 53)) return ub(this, b), !0; if (!a.next(0).done) throw eb("Cannot determine the effective boolean value of a sequence with a length higher than one."); ub(this, b); return sb(c) }; h.Qa = function (a) { if (null !== this.o) return this.o; if (void 0 === a ? 0 : a) return -1; a = this.h; var b = this.O().length; ub(this, a); return b }; h.G = function () { return 0 === this.o ? !0 : null === this.first() };
  			h.wa = function () { if (null !== this.o) return 1 === this.o; var a = this.value, b = this.h; ub(this); if (a.next(0).done) return ub(this, b), !1; a = a.next(0); ub(this, b); return a.done }; h.map = function (a) { var b = this, c = 0, d = this.value; return this.B.create({ next: function (e) { e = d.next(e); return e.done ? x : y(a(e.value, c++, b)) } }, this.o) }; h.M = function (a, b) { var c = this.value, d, e = [], f = !0; (function () { for (var g = c.next(f ? 0 : b); !g.done; g = c.next(b))f = !1, e.push(g.value); d = a(e).value; })(); return this.B.create({ next: function () { return d.next(0) } }) };
  			h.Z = function (a) { function b(e) { d = e.value; e = e.Qa(!0); -1 !== e && (c.o = e); } var c = this, d = null; return this.B.create({ next: function (e) { if (d) return d.next(e); if (c.G()) return b(a.empty ? a.empty(c) : a.default(c)), d.next(e); if (c.wa()) return b(a.m ? a.m(c) : a.default(c)), d.next(e); b(a.multiple ? a.multiple(c) : a.default(c)); return d.next(e) } }) }; function ub(a, b) { a.h = void 0 === b ? 0 : b; } function vb(a) { this.v = C; this.h = a; var b = !1; this.value = { next: function () { if (b) return x; b = !0; return y(a) } }; this.o = null; } h = vb.prototype; h.sb = function () { return this }; h.filter = function (a) { return a(this.h, 0, this) ? this : this.v.create() }; h.first = function () { return this.h }; h.O = function () { return [this.h] }; h.ha = function () { null === this.o && (this.o = sb(this.h)); return this.o }; h.Qa = function () { return 1 }; h.G = function () { return !1 }; h.wa = function () { return !0 }; h.map = function (a) { return this.v.create(a(this.h, 0, this)) }; h.M = function (a) { return a([this.h]) };
  			h.Z = function (a) { return a.m ? a.m(this) : a.default(this) }; var wb = new ib; function xb(a, b) { a = void 0 === a ? null : a; if (null === a) return wb; if (Array.isArray(a)) switch (a.length) { case 0: return wb; case 1: return new vb(a[0]); default: return new hb(a) }return a.next ? new tb(a, void 0 === b ? null : b) : new vb(a) } var C = { create: xb, m: function (a) { return new vb(a) }, empty: function () { return xb() }, ba: function () { return xb(cb) }, W: function () { return xb(db) } }; function yb(a) { var b = [], c = a.value; return function () { var d = 0; return C.create({ next: function () { if (void 0 !== b[d]) return b[d++]; var e = c.next(0); return e.done ? e : b[d++] = e } }) } } function zb(a, b, c) { this.namespaceURI = b || null; this.prefix = a || ""; this.localName = c; } zb.prototype.Ca = function () { return this.prefix ? this.prefix + ":" + this.localName : this.localName }; function Ab(a) { var b = a.j, c = a.arity, d = void 0 === a.Sa ? !1 : a.Sa, e = void 0 === a.I ? !1 : a.I, f = a.localName, g = a.namespaceURI, k = a.i; a = a.value; jb.call(this, 60, null); this.value = a; this.I = e; e = -1; for (a = 0; a < b.length; a++)4 === b[a] && (e = a); -1 < e && (a = Array(c - (b.length - 1)).fill(b[e - 1]), b = b.slice(0, e).concat(a)); this.o = b; this.v = c; this.da = d; this.B = f; this.l = g; this.s = k; } v(Ab, jb);
  			function Cb(a, b) { var c = a.value, d = b.map(function (e) { return null === e ? null : yb(e) }); b = b.reduce(function (e, f, g) { null === f && e.push(a.o[g]); return e }, []); b = new Ab({ j: b, arity: b.length, Sa: !0, I: a.I, localName: "boundFunction", namespaceURI: a.l, i: a.s, value: function (e, f, g) { var k = Array.from(arguments).slice(3), l = d.map(function (m) { return null === m ? k.shift() : m() }); return c.apply(void 0, [e, f, g].concat(l)) } }); return C.m(b) } Ab.prototype.Sa = function () { return this.da }; function Db(a, b) { var c = []; 2 !== a && 1 !== a || c.push("type-1-or-type-2"); c.push("type-" + a); b && c.push("name-" + b); return c } function Eb(a) { var b = a.node.nodeType; if (2 === b || 1 === b) var c = a.node.localName; return Db(b, c) } function Fb(a) { var b = a.nodeType; if (2 === b || 1 === b) var c = a.localName; return Db(b, c) } function Gb() { } Gb.prototype.getAllAttributes = function (a, b) { b = void 0 === b ? null : b; if (1 !== a.nodeType) return []; a = Array.from(a.attributes); return null === b ? a : a.filter(function (c) { return Fb(c).includes(b) }) }; Gb.prototype.getAttribute = function (a, b) { return 1 !== a.nodeType ? null : a.getAttribute(b) }; Gb.prototype.getChildNodes = function (a, b) { b = void 0 === b ? null : b; a = Array.from(a.childNodes); return null === b ? a : a.filter(function (c) { return Fb(c).includes(b) }) };
  			Gb.prototype.getData = function (a) { return 2 === a.nodeType ? a.value : a.data }; Gb.prototype.getFirstChild = function (a, b) { b = void 0 === b ? null : b; for (a = a.firstChild; a; a = a.nextSibling)if (null === b || Fb(a).includes(b)) return a; return null }; Gb.prototype.getLastChild = function (a, b) { b = void 0 === b ? null : b; for (a = a.lastChild; a; a = a.previousSibling)if (null === b || Fb(a).includes(b)) return a; return null };
  			Gb.prototype.getNextSibling = function (a, b) { b = void 0 === b ? null : b; for (a = a.nextSibling; a; a = a.nextSibling)if (null === b || Fb(a).includes(b)) return a; return null }; Gb.prototype.getParentNode = function (a, b) { b = void 0 === b ? null : b; return (a = 2 === a.nodeType ? a.ownerElement : a.parentNode) ? null === b || Fb(a).includes(b) ? a : null : null }; Gb.prototype.getPreviousSibling = function (a, b) { b = void 0 === b ? null : b; for (a = a.previousSibling; a; a = a.previousSibling)if (null === b || Fb(a).includes(b)) return a; return null }; function Hb() { } h = Hb.prototype; h.insertBefore = function (a, b, c) { return a.insertBefore(b, c) }; h.removeAttributeNS = function (a, b, c) { return a.removeAttributeNS(b, c) }; h.removeChild = function (a, b) { return a.removeChild(b) }; h.setAttributeNS = function (a, b, c, d) { a.setAttributeNS(b, c, d); }; h.setData = function (a, b) { a.data = b; }; var Ib = new Hb; function Jb(a) { this.h = a; } h = Jb.prototype; h.insertBefore = function (a, b, c) { return this.h.insertBefore(a, b, c) }; h.removeAttributeNS = function (a, b, c) { return this.h.removeAttributeNS(a, b, c) }; h.removeChild = function (a, b) { return this.h.removeChild(a, b) }; h.setAttributeNS = function (a, b, c, d) { this.h.setAttributeNS(a, b, c, d); }; h.setData = function (a, b) { this.h.setData(a, b); }; function Kb(a) { return void 0 !== a.Ta } function Lb(a, b, c) { var d = null; b && (Kb(b.node) ? d = { F: b.F, offset: c, parent: b.node } : b.F && (d = b.F)); return { node: a, F: d } } function Mb(a) { this.h = a; this.o = []; } function Nb(a, b, c) { return a.getAllAttributes(b.node, void 0 === c ? null : c).map(function (d) { return Lb(d, b, d.nodeName) }) } Mb.prototype.getAllAttributes = function (a, b) { return Kb(a) ? a.attributes : this.h.getAllAttributes(a, void 0 === b ? null : b) };
  			function Ob(a, b, c) { b = b.node; return Kb(b) ? (a = b.attributes.find(function (d) { return c === d.name })) ? a.value : null : (a = a.h.getAttribute(b, c)) ? a : null } function Pb(a, b, c) { return a.getChildNodes(b.node, void 0 === c ? null : c).map(function (d, e) { return Lb(d, b, e) }) } Mb.prototype.getChildNodes = function (a, b) { b = Kb(a) ? a.childNodes : this.h.getChildNodes(a, void 0 === b ? null : b); return 9 === a.nodeType ? b.filter(function (c) { return 10 !== c.nodeType }) : b };
  			Mb.prototype.getData = function (a) { return Kb(a) ? 2 === a.nodeType ? a.value : a.data : this.h.getData(a) || "" }; function Qb(a, b) { return a.getData(b.node) } function Sb(a, b, c) { var d = b.node; Kb(d) ? a = d.childNodes[0] : ((c = a.h.getFirstChild(d, void 0 === c ? null : c)) && 10 === c.nodeType && (c = a.h.getNextSibling(c)), a = c); return a ? Lb(a, b, 0) : null }
  			function Tb(a, b, c) { c = void 0 === c ? null : c; var d = b.node; Kb(d) ? (a = d.childNodes.length - 1, d = d.childNodes[a]) : ((d = a.h.getLastChild(d, c)) && 10 === d.nodeType && (d = a.h.getPreviousSibling(d)), a = a.getChildNodes(b.node, c).length - 1); return d ? Lb(d, b, a) : null }
  			function Ub(a, b, c) { c = void 0 === c ? null : c; var d = b.node, e = b.F; if (Kb(d)) { if (e) { var f = e.offset + 1; var g = e.parent.childNodes[f]; } } else if (e) { f = e.offset + 1; var k = Vb(a, b, null); g = a.getChildNodes(k.node, c)[f]; } else { for (g = d; g && (!(g = a.h.getNextSibling(g, c)) || 10 === g.nodeType);); return g ? { node: g, F: null } : null } return g ? Lb(g, k || Vb(a, b, c), f) : null } Mb.prototype.getParentNode = function (a, b) { return this.h.getParentNode(a, void 0 === b ? null : b) };
  			function Vb(a, b, c) { c = void 0 === c ? null : c; var d = b.node, e = b.F; if (e) "number" === typeof e.offset && d === e.parent.childNodes[e.offset] || "string" === typeof e.offset && d === e.parent.attributes.find(function (f) { return e.offset === f.nodeName }) ? (a = e.parent, b = e.F) : (a = a.getParentNode(d, c), b = e); else { if (Kb(d)) return null; a = a.getParentNode(d, c); b = null; } return a ? { node: a, F: b } : null }
  			function Wb(a, b, c) { c = void 0 === c ? null : c; var d = b.node, e = b.F; if (Kb(d)) { if (e) { var f = e.offset - 1; var g = e.parent.childNodes[f]; } } else if (e) { f = e.offset - 1; var k = Vb(a, b, null); g = a.getChildNodes(k.node, c)[f]; } else { for (g = d; g && (!(g = a.h.getPreviousSibling(g, c)) || 10 === g.nodeType);); return g ? { node: g, F: null } : null } return g ? Lb(g, k || Vb(a, b, c), f) : null } function Xb(a, b, c, d, e) { return e.M(function (f) { var g = p(f).next().value; return d.M(function (k) { k = p(k).next().value; var l = g.value; if (0 >= l || l > k.P.length) throw Error("FOAY0001: array position out of bounds."); return k.P[l - 1]() }) }) } function Yb(a) { Ab.call(this, { value: function (c, d, e, f) { return Xb(c, d, e, C.m(b), f) }, localName: "get", namespaceURI: "http://www.w3.org/2005/xpath-functions/array", j: [{ type: 5, g: 3 }], arity: 1, i: { type: 59, g: 2 } }); var b = this; this.type = 62; this.P = a; } v(Yb, Ab); function Zb(a) { switch (a.node.nodeType) { case 2: return 47; case 1: return 54; case 3: case 4: return 56; case 7: return 57; case 8: return 58; case 9: return 55; default: return 53 } } function $b(a) { return { type: Zb(a), value: a } } function ac(a, b) { a = a.map(function (c) { return c.first() }); return b(a) } function bc(a, b) { var c = B(a.type, 1) || B(a.type, 20) || B(a.type, 19), d = B(b.type, 1) || B(b.type, 20) || B(b.type, 19); if (c && d) return a.value === b.value; c = B(a.type, 4) || B(a.type, 3) || B(a.type, 6); d = B(b.type, 4) || B(b.type, 3) || B(b.type, 6); if (c && d) return isNaN(a.value) && isNaN(b.value) ? !0 : a.value === b.value; c = B(a.type, 0) || B(a.type, 22) || B(a.type, 18) || B(a.type, 23) || B(a.type, 44); d = B(b.type, 0) || B(b.type, 22) || B(b.type, 18) || B(b.type, 23) || B(b.type, 44); return c && d ? a.value === b.value : !1 } function cc(a, b, c, d, e) { return ac([d, e], function (f) { f = p(f); var g = f.next().value, k = f.next().value; return (f = g.h.find(function (l) { return bc(l.key, k) })) ? f.value() : C.empty() }) } function dc(a) { Ab.call(this, { j: [{ type: 59, g: 3 }], arity: 1, localName: "get", namespaceURI: "http://www.w3.org/2005/xpath-functions/map", value: function (c, d, e, f) { return cc(c, d, e, C.m(b), f) }, i: { type: 59, g: 2 } }); var b = this; this.type = 61; this.h = a; } v(dc, Ab); function ec() { } function fc(a, b) { return a.eb() === b.eb() && a.fb() === b.fb() } h = ec.prototype; h.bb = function () { return 0 }; h.getHours = function () { return 0 }; h.getMinutes = function () { return 0 }; h.cb = function () { return 0 }; h.eb = function () { return 0 }; h.fb = function () { return 0 }; h.getSeconds = function () { return 0 }; h.gb = function () { return 0 }; h.qa = function () { return !0 }; function gc(a) { if (a > Number.MAX_SAFE_INTEGER || a < Number.MIN_SAFE_INTEGER) throw Error("FODT0002: Number of seconds given to construct DayTimeDuration overflows MAX_SAFE_INTEGER or MIN_SAFE_INTEGER"); this.ea = a; } v(gc, ec); h = gc.prototype; h.bb = function () { return Math.trunc(this.ea / 86400) }; h.getHours = function () { return Math.trunc(this.ea % 86400 / 3600) }; h.getMinutes = function () { return Math.trunc(this.ea % 3600 / 60) }; h.fb = function () { return this.ea }; h.getSeconds = function () { var a = this.ea % 60; return Object.is(-0, a) ? 0 : a };
  			h.qa = function () { return Object.is(-0, this.ea) ? !1 : 0 <= this.ea }; h.toString = function () { return (this.qa() ? "P" : "-P") + hc(this) }; function hc(a) { var b = Math.abs(a.bb()), c = Math.abs(a.getHours()), d = Math.abs(a.getMinutes()); a = Math.abs(a.getSeconds()); b = b ? b + "D" : ""; c = (c ? c + "H" : "") + (d ? d + "M" : "") + (a ? a + "S" : ""); return b && c ? b + "T" + c : b ? b : c ? "T" + c : "T0S" } function ic(a, b, c, d, e, f) { a = 86400 * a + 3600 * b + 60 * c + d + e; return new gc(f || 0 === a ? a : -a) }
  			function jc(a) { return (a = /^(-)?P(\d+Y)?(\d+M)?(\d+D)?(?:T(\d+H)?(\d+M)?(\d+(\.\d*)?S)?)?$/.exec(a)) ? ic(a[4] ? parseInt(a[4], 10) : 0, a[5] ? parseInt(a[5], 10) : 0, a[6] ? parseInt(a[6], 10) : 0, a[7] ? parseInt(a[7], 10) : 0, a[8] ? parseFloat(a[8]) : 0, !a[1]) : null } function kc(a) { a = /^(Z)|([+-])([01]\d):([0-5]\d)$/.exec(a); return "Z" === a[1] ? ic(0, 0, 0, 0, 0, !0) : ic(0, a[3] ? parseInt(a[3], 10) : 0, a[4] ? parseInt(a[4], 10) : 0, 0, 0, "+" === a[2]) }
  			function lc(a, b) { if (isNaN(b)) throw Error("FOCA0005: Cannot multiply xs:dayTimeDuration by NaN"); a = a.ea * b; if (a > Number.MAX_SAFE_INTEGER || !Number.isFinite(a)) throw Error("FODT0002: Value overflow while multiplying xs:dayTimeDuration"); return new gc(a < Number.MIN_SAFE_INTEGER || Object.is(-0, a) ? 0 : a) } function mc(a) { return a ? parseInt(a, 10) : null } function nc(a) { a += ""; var b = a.startsWith("-"); b && (a = a.substring(1)); return (b ? "-" : "") + a.padStart(4, "0") } function oc(a) { return (a + "").padStart(2, "0") } function pc(a) { a += ""; 1 === a.split(".")[0].length && (a = a.padStart(a.length + 1, "0")); return a } function qc(a) { return 0 === a.getHours() && 0 === a.getMinutes() ? "Z" : (a.qa() ? "+" : "-") + oc(Math.abs(a.getHours())) + ":" + oc(Math.abs(a.getMinutes())) }
  			function rc(a, b, c, d, e, f, g, k, l) { this.v = a; this.h = b; this.o = c + (24 === d ? 1 : 0); this.l = 24 === d ? 0 : d; this.s = e; this.B = f; this.sa = g; this.Y = k; this.type = void 0 === l ? 9 : l; }
  			function sc(a) {
  				var b = /^(?:(-?\d{4,}))?(?:--?(\d\d))?(?:-{1,3}(\d\d))?(T)?(?:(\d\d):(\d\d):(\d\d))?(\.\d+)?(Z|(?:[+-]\d\d:\d\d))?$/.exec(a); a = b[1] ? parseInt(b[1], 10) : null; var c = mc(b[2]), d = mc(b[3]), e = b[4], f = mc(b[5]), g = mc(b[6]), k = mc(b[7]), l = b[8] ? parseFloat(b[8]) : 0; b = b[9] ? kc(b[9]) : null; if (a && (-271821 > a || 273860 < a)) throw Error("FODT0001: Datetime year is out of bounds"); return e ? new rc(a, c, d, f, g, k, l, b, 9) : null !== f && null !== g && null !== k ? new rc(1972, 12, 31, f, g, k, l, b, 8) : null !== a && null !== c && null !== d ? new rc(a, c,
  					d, 0, 0, 0, 0, b, 7) : null !== a && null !== c ? new rc(a, c, 1, 0, 0, 0, 0, b, 11) : null !== c && null !== d ? new rc(1972, c, d, 0, 0, 0, 0, b, 13) : null !== a ? new rc(a, 1, 1, 0, 0, 0, 0, b, 12) : null !== c ? new rc(1972, c, 1, 0, 0, 0, 0, b, 14) : new rc(1972, 12, d, 0, 0, 0, 0, b, 15)
  			}
  			function tc(a, b) { switch (b) { case 15: return new rc(1972, 12, a.o, 0, 0, 0, 0, a.Y, 15); case 14: return new rc(1972, a.h, 1, 0, 0, 0, 0, a.Y, 14); case 12: return new rc(a.v, 1, 1, 0, 0, 0, 0, a.Y, 12); case 13: return new rc(1972, a.h, a.o, 0, 0, 0, 0, a.Y, 13); case 11: return new rc(a.v, a.h, 1, 0, 0, 0, 0, a.Y, 11); case 8: return new rc(1972, 12, 31, a.l, a.s, a.B, a.sa, a.Y, 8); case 7: return new rc(a.v, a.h, a.o, 0, 0, 0, 0, a.Y, 7); default: return new rc(a.v, a.h, a.o, a.l, a.s, a.B, a.sa, a.Y, 9) } } h = rc.prototype; h.getDay = function () { return this.o }; h.getHours = function () { return this.l };
  			h.getMinutes = function () { return this.s }; h.getMonth = function () { return this.h }; h.getSeconds = function () { return this.B }; h.getYear = function () { return this.v }; function uc(a, b) { b = a.Y || b || kc("Z"); return new Date(Date.UTC(a.v, a.h - 1, a.o, a.l - b.getHours(), a.s - b.getMinutes(), a.B, 1E3 * a.sa)) }
  			h.toString = function () {
  				switch (this.type) {
  					case 9: return nc(this.v) + "-" + oc(this.h) + "-" + oc(this.o) + "T" + oc(this.l) + ":" + oc(this.s) + ":" + pc(this.B + this.sa) + (this.Y ? qc(this.Y) : ""); case 7: return nc(this.v) + "-" + oc(this.h) + "-" + oc(this.o) + (this.Y ? qc(this.Y) : ""); case 8: return oc(this.l) + ":" + oc(this.s) + ":" + pc(this.B + this.sa) + (this.Y ? qc(this.Y) : ""); case 15: return "---" + oc(this.o) + (this.Y ? qc(this.Y) : ""); case 14: return "--" + oc(this.h) + (this.Y ? qc(this.Y) : ""); case 13: return "--" + oc(this.h) + "-" + oc(this.o) + (this.Y ? qc(this.Y) :
  						""); case 12: return nc(this.v) + (this.Y ? qc(this.Y) : ""); case 11: return nc(this.v) + "-" + oc(this.h) + (this.Y ? qc(this.Y) : "")
  				}throw Error("Unexpected subType");
  			}; function vc(a, b, c) { var d = uc(a, c).getTime(); c = uc(b, c).getTime(); return d === c ? a.sa === b.sa ? 0 : a.sa > b.sa ? 1 : -1 : d > c ? 1 : -1 } function wc(a, b, c) { return 0 === vc(a, b, c) } function xc(a, b, c) { a = (uc(a, c).getTime() - uc(b, c).getTime()) / 1E3; return new gc(a) } function yc(a) { throw Error("Not implemented: adding durations to " + mb[a.type]); }
  			function zc(a) { throw Error("Not implemented: subtracting durations from " + mb[a.type]); } function Ac(a, b) {
  				if (null === a) return null; switch (typeof a) {
  					case "boolean": return a ? cb : db; case "number": return w(a, 3); case "string": return w(a, 1); case "object": if ("nodeType" in a) return $b({ node: a, F: null }); if (Array.isArray(a)) return new Yb(a.map(function (d) { if (void 0 === d) return function () { return C.empty() }; d = Ac(d); d = null === d ? C.empty() : C.m(d); return yb(d) })); if (a instanceof Date) { var c = sc(a.toISOString()); return w(c, c.type) } return new dc(Object.keys(a).filter(function (d) { return void 0 !== a[d] }).map(function (d) {
  						var e =
  							Ac(a[d]); e = null === e ? C.empty() : C.m(e); return { key: w(d, 1), value: yb(e) }
  					}))
  				}throw Error("Value " + String(a) + ' of type "' + typeof a + '" is not adaptable to an XPath value.');
  			} function Bc(a, b) { if ("number" !== typeof a && ("string" !== typeof a || !ab.get(b)(a))) throw Error("Cannot convert JavaScript value '" + a + "' to the XPath type " + mb[b] + " since it is not valid."); }
  			function Cc(a, b, c) {
  				if (null === b) return null; switch (a) {
  					case 0: return b ? cb : db; case 1: return w(b + "", 1); case 3: case 2: return Bc(b, 3), w(+b, 3); case 4: return Bc(b, a), w(+b, 4); case 5: return Bc(b, a), w(b | 0, 5); case 6: return Bc(b, a), w(+b, 6); case 7: case 8: case 9: case 11: case 12: case 13: case 14: case 15: if (!(b instanceof Date)) throw Error("The JavaScript value " + b + " with type " + typeof b + " is not a valid type to be converted to an XPath " + mb[a] + "."); return w(tc(sc(b.toISOString()), a), a); case 53: case 47: case 55: case 54: case 56: case 57: case 58: if ("object" !==
  						typeof b || !("nodeType" in b)) throw Error("The JavaScript value " + b + " with type " + typeof b + " is not a valid type to be converted to an XPath " + mb[a] + "."); return $b({ node: b, F: null }); case 59: return Ac(b); case 61: return Ac(b); default: throw Error('Values of the type "' + mb[a] + '" can not be adapted from JavaScript to equivalent XPath values.');
  				}
  			}
  			function Dc(a, b, c) { if (0 === c.g) return b = Cc(c.type, b), null === b ? [] : [b]; if (2 === c.g || 1 === c.g) { if (!Array.isArray(b)) throw Error("The JavaScript value " + b + " should be an array if it is to be converted to " + ob(c) + "."); return b.map(function (e) { return Cc(c.type, e) }).filter(function (e) { return null !== e }) } var d = Cc(c.type, b); if (null === d) throw Error("The JavaScript value " + b + " should be a single entry if it is to be converted to " + ob(c) + "."); return [d] }
  			function Ec(a, b, c) { c = void 0 === c ? { type: 59, g: 0 } : c; return C.create(Dc(a, b, c)) } function Fc() { var a = void 0 === a ? Math.floor(Math.random() * Gc) : a; this.h = Math.abs(a % Gc); } var Gc = Math.pow(2, 32); function Hc(a, b, c) { b = void 0 === b ? { yb: null, Db: null, tb: !1 } : b; c = void 0 === c ? new Fc : c; this.h = b; this.Ka = a.Ka; this.Da = a.Da; this.N = a.N; this.Aa = a.Aa || Object.create(null); this.o = c; } function Ic(a, b) { var c = 0, d = b.value; return { next: function (e) { e = d.next(e); return e.done ? x : y(Jc(a, c++, e.value, b)) } } } function Kc(a) { a.h.tb || (a.h.tb = !0, a.h.yb = sc((new Date).toISOString()), a.h.Db = jc("PT0S")); return a.h.yb } function Lc(a) { a.h.tb || (a.h.tb = !0, a.h.yb = sc((new Date).toISOString()), a.h.Db = jc("PT0S")); return a.h.Db }
  			function Mc(a, b) { b = void 0 === b ? null : b; a = 29421 * (null !== b && void 0 !== b ? b : a.o.h) % Gc; return { zb: Math.floor(a), jc: a / Gc } } function Jc(a, b, c, d) { return new Hc({ N: c, Ka: b, Da: d || a.Da, Aa: a.Aa }, a.h, a.o) } function Nc(a, b) { return new Hc({ N: a.N, Ka: a.Ka, Da: a.Da, Aa: Object.assign(Object.create(null), a.Aa, b) }, a.h, a.o) } function Oc(a, b, c, d, e, f, g, k, l) { this.debug = a; this.La = b; this.h = c; this.o = d; this.B = e; this.l = f; this.da = g; this.s = k; this.v = l; } function Pc(a) { var b = 0, c = null, d = !0; return C.create({ next: function (e) { for (; b < a.length;) { c || (c = a[b].value, d = !0); var f = c.next(d ? 0 : e); d = !1; if (f.done) b++, c = null; else return f } return x } }) } function Qc(a, b, c) { return Error("FORG0001: Cannot cast " + a + " to " + mb[b] + (c ? ", " + c : "")) } function Rc(a) { return Error("XPDY0002: " + a) } function Sc(a) { return Error("XPTY0004: " + a) } function Tc(a) { return Error("FOTY0013: Atomization is not supported for " + mb[a] + ".") } function Uc(a) { return Error("XPST0081: The prefix " + a + " could not be resolved.") } function Yc(a, b) {
  				if (B(a.type, 46) || B(a.type, 19) || B(a.type, 0) || B(a.type, 4) || B(a.type, 3) || B(a.type, 6) || B(a.type, 5) || B(a.type, 2) || B(a.type, 23) || B(a.type, 1)) return C.create(a); var c = b.h; if (B(a.type, 53)) {
  					var d = a.value; if (2 === d.node.nodeType || 3 === d.node.nodeType) return C.create(w(Qb(c, d), 19)); if (8 === d.node.nodeType || 7 === d.node.nodeType) return C.create(w(Qb(c, d), 1)); var e = []; (function k(g) { if (8 !== d.node.nodeType && 7 !== d.node.nodeType) { var l = g.nodeType; 3 === l || 4 === l ? e.push(c.getData(g)) : 1 !== l && 9 !== l || c.getChildNodes(g).forEach(function (m) { k(m); }); } })(d.node);
  					return C.create(w(e.join(""), 19))
  				} if (B(a.type, 60) && !B(a.type, 62)) throw Tc(a.type); if (B(a.type, 62)) return Pc(a.P.map(function (f) { return Zc(f(), b) })); throw Error("Atomizing " + a.type + " is not implemented.");
  			} function Zc(a, b) { var c = !1, d = a.value, e = null; return C.create({ next: function () { for (; !c;) { if (!e) { var f = d.next(0); if (f.done) { c = !0; break } e = Yc(f.value, b).value; } f = e.next(0); if (f.done) e = null; else return f } return x } }) } function $c(a) { for (a = bb[a]; a && 0 !== a.D;)a = a.parent; return a ? a.type : null } function ad(a, b) { b = bb[b]; var c = b.Na; if (!c || !c.whiteSpace) return b.parent ? ad(a, b.parent.type) : a; switch (b.Na.whiteSpace) { case "replace": return a.replace(/[\u0009\u000A\u000D]/g, " "); case "collapse": return a.replace(/[\u0009\u000A\u000D]/g, " ").replace(/ {2,}/g, " ").replace(/^ | $/g, "") }return a } function bd(a, b) { for (b = bb[b]; b && null === b.kb;) { if (2 === b.D || 3 === b.D) return !0; b = b.parent; } return b ? b.kb(a) : !0 }
  			function cd(a, b) { for (; a;) { if (a.Pa && a.Pa[b]) return a.Pa[b]; a = a.parent; } return function () { return !0 } } function dd(a, b) { for (var c = bb[b]; c;) { if (c.Na && !Object.keys(c.Na).every(function (d) { if ("whiteSpace" === d) return !0; var e = cd(c, d); return e ? e(a, c.Na[d]) : !0 })) return !1; c = c.parent; } return !0 } function ed(a) { return a ? 2 === a.g || 0 === a.g : !0 } function fd(a) { return a(1) || a(19) ? function (b) { return { u: !0, value: w(b, 20) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:anyURI or any of its derived types.") } } } function gd(a) { return a(22) ? function (b) { for (var c = "", d = 0; d < b.length; d += 2)c += String.fromCharCode(parseInt(b.substr(d, 2), 16)); return { u: !0, value: w(btoa(c), 21) } } : a(1) || a(19) ? function (b) { return { u: !0, value: w(b, 21) } } : function () { return { error: Error("XPTY0004: Casting not supported from given type to xs:base64Binary or any of its derived types."), u: !1 } } } function hd(a) { return a(2) ? function (b) { return { u: !0, value: 0 === b || isNaN(b) ? db : cb } } : a(1) || a(19) ? function (b) { switch (b) { case "true": case "1": return { u: !0, value: cb }; case "false": case "0": return { u: !0, value: db }; default: return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:boolean or any of its derived types.") } } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:boolean or any of its derived types.") } } } function id(a) { return a(9) ? function (b) { return { u: !0, value: w(tc(b, 7), 7) } } : a(19) || a(1) ? function (b) { return { u: !0, value: w(sc(b), 7) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:date or any of its derived types.") } } } function jd(a) { return a(7) ? function (b) { return { u: !0, value: w(tc(b, 9), 9) } } : a(19) || a(1) ? function (b) { return { u: !0, value: w(sc(b), 9) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:dateTime or any of its derived types.") } } } function kd(a) { return a(18) && !a(16) ? function (b) { return { u: !0, value: w(b.Ja, 17) } } : a(16) ? function () { return { u: !0, value: w(jc("PT0.0S"), 17) } } : a(19) || a(1) ? function (b) { var c = jc(b); return c ? { u: !0, value: w(c, 17) } : { u: !1, error: Error("FORG0001: Can not cast " + b + " to xs:dayTimeDuration") } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:dayTimeDuration or any of its derived types.") } } } function ld(a) {
  				return a(5) ? function (b) { return { u: !0, value: w(b, 4) } } : a(6) || a(3) ? function (b) { return isNaN(b) || !isFinite(b) ? { u: !1, error: Error("FOCA0002: Can not cast " + b + " to xs:decimal") } : Math.abs(b) > Number.MAX_VALUE ? { u: !1, error: Error("FOAR0002: Can not cast " + b + " to xs:decimal, it is out of bounds for JavaScript numbers") } : { u: !0, value: w(b, 4) } } : a(0) ? function (b) { return { u: !0, value: w(b ? 1 : 0, 4) } } : a(1) || a(19) ? function (b) {
  					var c = parseFloat(b); return !isNaN(c) || isFinite(c) ? { u: !0, value: w(c, 4) } : {
  						u: !1, error: Error("FORG0001: Can not cast " +
  							b + " to xs:decimal")
  					}
  				} : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:decimal or any of its derived types.") } }
  			} function md(a, b) {
  				return a(2) ? function (c) { return { u: !0, value: c } } : a(0) ? function (c) { return { u: !0, value: c ? 1 : 0 } } : a(1) || a(19) ? function (c) { switch (c) { case "NaN": return { u: !0, value: NaN }; case "INF": case "+INF": return { u: !0, value: Infinity }; case "-INF": return { u: !0, value: -Infinity }; case "0": case "+0": return { u: !0, value: 0 }; case "-0": return { u: !0, value: -0 } }var d = parseFloat(c); return isNaN(d) ? { u: !1, error: Qc(c, b) } : { u: !0, value: d } } : function () {
  					return {
  						u: !1, error: Error("XPTY0004: Casting not supported from given type to " + b +
  							" or any of its derived types.")
  					}
  				}
  			} function nd(a) { var b = md(a, 3); return function (c) { c = b(c); return c.u ? { u: !0, value: w(c.value, 3) } : c } } function od(a) { if (a > Number.MAX_SAFE_INTEGER || a < Number.MIN_SAFE_INTEGER) throw Error("FODT0002: Number of months given to construct YearMonthDuration overflows MAX_SAFE_INTEGER or MIN_SAFE_INTEGER"); this.ga = a; } v(od, ec); h = od.prototype; h.cb = function () { var a = this.ga % 12; return 0 === a ? 0 : a }; h.eb = function () { return this.ga }; h.gb = function () { return Math.trunc(this.ga / 12) }; h.qa = function () { return Object.is(-0, this.ga) ? !1 : 0 <= this.ga }; h.toString = function () { return (this.qa() ? "P" : "-P") + pd(this) };
  			function pd(a) { var b = Math.abs(a.gb()); a = Math.abs(a.cb()); return (b ? b + "Y" : "") + (a ? a + "M" : "") || "0M" } function qd(a) { var b = /^(-)?P(\d+Y)?(\d+M)?(\d+D)?(?:T(\d+H)?(\d+M)?(\d+(\.\d*)?S)?)?$/.exec(a); if (b) { a = !b[1]; b = 12 * (b[2] ? parseInt(b[2], 10) : 0) + (b[3] ? parseInt(b[3], 10) : 0); if (b > Number.MAX_SAFE_INTEGER || !Number.isFinite(b)) throw Error("FODT0002: Value overflow while constructing xs:yearMonthDuration"); a = new od(a || 0 === b ? b : -b); } else a = null; return a }
  			function rd(a, b) { if (isNaN(b)) throw Error("FOCA0005: Cannot multiply xs:yearMonthDuration by NaN"); a = Math.round(a.ga * b); if (a > Number.MAX_SAFE_INTEGER || !Number.isFinite(a)) throw Error("FODT0002: Value overflow while constructing xs:yearMonthDuration"); return new od(a < Number.MIN_SAFE_INTEGER || 0 === a ? 0 : a) } function sd(a, b) { this.Wa = a; this.Ja = b; } v(sd, ec); h = sd.prototype; h.bb = function () { return this.Ja.bb() }; h.getHours = function () { return this.Ja.getHours() }; h.getMinutes = function () { return this.Ja.getMinutes() }; h.cb = function () { return this.Wa.cb() }; h.eb = function () { return this.Wa.eb() }; h.fb = function () { return this.Ja.fb() }; h.getSeconds = function () { return this.Ja.getSeconds() }; h.gb = function () { return this.Wa.gb() }; h.qa = function () { return this.Wa.qa() && this.Ja.qa() };
  			h.toString = function () { var a = this.qa() ? "P" : "-P", b = pd(this.Wa), c = hc(this.Ja); return "0M" === b ? a + c : "T0S" === c ? a + b : a + b + c }; function td(a) { return a(16) ? function (b) { return { u: !0, value: w(new sd(b, new gc(b.qa() ? 0 : -0)), 18) } } : a(17) ? function (b) { b = new sd(new od(b.qa() ? 0 : -0), b); return { u: !0, value: w(b, 18) } } : a(18) ? function (b) { return { u: !0, value: w(b, 18) } } : a(19) || a(1) ? function (b) { var c; return c = new sd(qd(b), jc(b)), { u: !0, value: w(c, 18) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:duration or any of its derived types.") } } } function ud(a) { var b = md(a, 6); return function (c) { c = b(c); return c.u ? { u: !0, value: w(c.value, 6) } : c } } function vd(a) { return a(7) || a(9) ? function (b) { return { u: !0, value: w(tc(b, 15), 15) } } : a(19) || a(1) ? function (b) { return { u: !0, value: w(sc(b), 15) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:gDay or any of its derived types.") } } } function wd(a) { return a(7) || a(9) ? function (b) { return { u: !0, value: w(tc(b, 14), 14) } } : a(19) || a(1) ? function (b) { return { u: !0, value: w(sc(b), 14) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:gMonth or any of its derived types.") } } } function xd(a) { return a(7) || a(9) ? function (b) { return { u: !0, value: w(tc(b, 13), 13) } } : a(19) || a(1) ? function (b) { return { u: !0, value: w(sc(b), 13) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:gMonthDay or any of its derived types.") } } } function yd(a) { return a(7) || a(9) ? function (b) { return { u: !0, value: w(tc(b, 12), 12) } } : a(19) || a(1) ? function (b) { return { u: !0, value: w(sc(b), 12) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:gYear or any of its derived types.") } } } function zd(a) { return a(7) || a(9) ? function (b) { return { u: !0, value: w(tc(b, 11), 11) } } : a(19) || a(1) ? function (b) { return { u: !0, value: w(sc(b), 11) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:gYearMonth or any of its derived types.") } } } function Ad(a) { return a(21) ? function (b) { b = atob(b); for (var c = "", d = 0, e = b.length; d < e; d++)c += Number(b.charCodeAt(d)).toString(16); return { u: !0, value: w(c.toUpperCase(), 22) } } : a(1) || a(19) ? function (b) { return { u: !0, value: w(b, 22) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:hexBinary or any of its derived types.") } } } function Bd(a) {
  				return a(0) ? function (b) { return { u: !0, value: w(b ? 1 : 0, 5) } } : a(2) ? function (b) { var c = Math.trunc(b); return !isFinite(c) || isNaN(c) ? { u: !1, error: Error("FOCA0002: can not cast " + b + " to xs:integer") } : Number.isSafeInteger(c) ? { u: !0, value: w(c, 5) } : { u: !1, error: Error("FOAR0002: can not cast " + b + " to xs:integer, it is out of bounds for JavaScript numbers.") } } : a(1) || a(19) ? function (b) {
  					var c = parseInt(b, 10); return isNaN(c) ? { u: !1, error: Qc(b, 5) } : Number.isSafeInteger(c) ? { u: !0, value: w(c, 5) } : {
  						u: !1, error: Error("FOCA0003: can not cast " +
  							b + " to xs:integer, it is out of bounds for JavaScript numbers.")
  					}
  				} : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:integer or any of its derived types.") } }
  			} var Cd = [3, 6, 4, 5]; function Dd(a) { var b = Ed; return function (c) { for (var d = p(Cd), e = d.next(); !e.done; e = d.next())if (e = b(a, e.value)(c), e.u) return e; return { u: !1, error: Error('XPTY0004: Casting not supported from "' + c + '" given type to xs:numeric or any of its derived types.') } } } function Fd(a) {
  				if (a(1) || a(19)) return function (b) { return { u: !0, value: b + "" } }; if (a(20)) return function (b) { return { u: !0, value: b } }; if (a(23)) return function (b) { return { u: !0, value: b.prefix ? b.prefix + ":" + b.localName : b.localName } }; if (a(44)) return function (b) { return { u: !0, value: b.toString() } }; if (a(2)) {
  					if (a(5) || a(4)) return function (b) { return { u: !0, value: (b + "").replace("e", "E") } }; if (a(6) || a(3)) return function (b) {
  						return isNaN(b) ? { u: !0, value: "NaN" } : isFinite(b) ? Object.is(b, -0) ? { u: !0, value: "-0" } : {
  							u: !0, value: (b + "").replace("e",
  								"E").replace("E+", "E")
  						} : { u: !0, value: (0 > b ? "-" : "") + "INF" }
  					}
  				} return a(9) || a(7) || a(8) || a(15) || a(14) || a(13) || a(12) || a(11) ? function (b) { return { u: !0, value: b.toString() } } : a(16) ? function (b) { return { u: !0, value: b.toString() } } : a(17) ? function (b) { return { u: !0, value: b.toString() } } : a(18) ? function (b) { return { u: !0, value: b.toString() } } : a(22) ? function (b) { return { u: !0, value: b.toUpperCase() } } : function (b) { return { u: !0, value: b + "" } }
  			} function Hd(a) { var b = Fd(a); return function (c) { c = b(c); return c.u ? { u: !0, value: w(c.value, 1) } : c } } function Id(a) { return a(9) ? function (b) { return { u: !0, value: w(tc(b, 8), 8) } } : a(19) || a(1) ? function (b) { return { u: !0, value: w(sc(b), 8) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:time or any of its derived types.") } } } function Jd(a) { var b = Fd(a); return function (c) { c = b(c); return c.u ? { u: !0, value: w(c.value, 19) } : c } } function Kd(a) { return a(18) && !a(17) ? function (b) { return { u: !0, value: w(b.Wa, 16) } } : a(17) ? function () { return { u: !0, value: w(qd("P0M"), 16) } } : a(19) || a(1) ? function (b) { var c = qd(b); return c ? { u: !0, value: w(c, 16) } : { u: !1, error: Qc(b, 16) } } : function () { return { u: !1, error: Error("XPTY0004: Casting not supported from given type to xs:yearMonthDuration or any of its derived types.") } } } var Ld = [2, 5, 17, 16];
  			function Ed(a, b) {
  				function c(d) { return B(a, d) } if (39 === b) return function () { return { u: !1, error: Error("FORG0001: Casting to xs:error is always invalid.") } }; switch (b) {
  					case 19: return Jd(c); case 1: return Hd(c); case 6: return ud(c); case 3: return nd(c); case 4: return ld(c); case 5: return Bd(c); case 2: return Dd(a); case 18: return td(c); case 16: return Kd(c); case 17: return kd(c); case 9: return jd(c); case 8: return Id(c); case 7: return id(c); case 11: return zd(c); case 12: return yd(c); case 13: return xd(c); case 15: return vd(c);
  					case 14: return wd(c); case 0: return hd(c); case 21: return gd(c); case 22: return Ad(c); case 20: return fd(c); case 23: throw Error("Casting to xs:QName is not implemented.");
  				}return function () { return { u: !1, error: Error("XPTY0004: Casting not supported from " + a + " to " + b + ".") } }
  			} var Md = Object.create(null);
  			function Nd(a, b) {
  				if (19 === a && 1 === b) return function (f) { return { u: !0, value: w(f, 1) } }; if (44 === b) return function () { return { u: !1, error: Error("XPST0080: Casting to xs:NOTATION is not permitted.") } }; if (39 === b) return function () { return { u: !1, error: Error("FORG0001: Casting to xs:error is not permitted.") } }; if (45 === a || 45 === b) return function () { return { u: !1, error: Error("XPST0080: Casting from or to xs:anySimpleType is not permitted.") } }; if (46 === a || 46 === b) return function () { return { u: !1, error: Error("XPST0080: Casting from or to xs:anyAtomicType is not permitted.") } };
  				if (B(a, 60) && 1 === b) return function () { return { u: !1, error: Error("FOTY0014: Casting from function item to xs:string is not permitted.") } }; if (a === b) return function (f) { return { u: !0, value: { type: b, value: f } } }; var c = Ld.includes(a) ? a : $c(a), d = Ld.includes(b) ? b : $c(b); if (null === d || null === c) return function () { return { u: !1, error: Error("XPST0081: Can not cast: type " + (d ? mb[a] : mb[b]) + " is unknown.") } }; var e = []; 1 !== c && 19 !== c || e.push(function (f) { var g = ad(f, b); return bd(g, b) ? { u: !0, value: g } : { u: !1, error: Qc(f, b, "pattern validation failed.") } });
  				c !== d && (e.push(Ed(c, d)), e.push(function (f) { return { u: !0, value: f.value } })); 19 !== d && 1 !== d || e.push(function (f) { return bd(f, b) ? { u: !0, value: f } : { u: !1, error: Qc(f, b, "pattern validation failed.") } }); e.push(function (f) { return dd(f, b) ? { u: !0, value: f } : { u: !1, error: Qc(f, b, "pattern validation failed.") } }); e.push(function (f) { return { u: !0, value: { type: b, value: f } } }); return function (f) { f = { u: !0, value: f }; for (var g = 0, k = e.length; g < k && (f = e[g](f.value), !1 !== f.u); ++g); return f }
  			}
  			function Od(a, b) { var c = a.type + 1E4 * b, d = Md[c]; d || (d = Md[c] = Nd(a.type, b)); return d.call(void 0, a.value, b) } function Pd(a, b) { a = Od(a, b); if (!0 === a.u) return a.value; throw a.error; } function Qd(a) { var b = !1; return { next: function () { if (b) return x; b = !0; return y(a) } } } function Rd(a, b) { return a === b ? !0 : a && b && a.offset === b.offset && a.parent === b.parent ? Rd(a.F, b.F) : !1 } function Sd(a, b) { return a === b || a.node === b.node && Rd(a.F, b.F) ? !0 : !1 } function Td(a, b, c) { var d = Vb(a, b, null); a = Pb(a, d, null); d = 0; for (var e = a.length; d < e; ++d) { var f = a[d]; if (Sd(f, b)) return -1; if (Sd(f, c)) return 1 } } function Ud(a, b) { for (var c = []; b; b = Vb(a, b, null))c.unshift(b); return c } function Vd(a, b) { for (var c = []; b; b = a.getParentNode(b, null))c.unshift(b); return c }
  			function Wd(a, b, c, d) {
  				if (c.F || d.F || Kb(c.node) || Kb(d.node)) { if (Sd(c, d)) return 0; c = Ud(b, c); d = Ud(b, d); var e = c[0], f = d[0]; if (!Sd(e, f)) return b = a.findIndex(function (q) { return Sd(q, e) }), c = a.findIndex(function (q) { return Sd(q, f) }), -1 === b && (b = a.push(e)), -1 === c && (c = a.push(f)), b - c; a = 1; for (var g = Math.min(c.length, d.length); a < g && Sd(c[a], d[a]); ++a); return c[a] ? d[a] ? Td(b, c[a], d[a]) : 1 : -1 } c = c.node; d = d.node; if (c === d) return 0; c = Vd(b, c); d = Vd(b, d); if (c[0] !== d[0]) {
  					var k = { node: c[0], F: null }, l = { node: d[0], F: null }; b = a.findIndex(function (q) {
  						return Sd(q,
  							k)
  					}); c = a.findIndex(function (q) { return Sd(q, l) }); -1 === b && (b = a.push(k)); -1 === c && (c = a.push(l)); return b - c
  				} g = 1; for (a = Math.min(c.length, d.length); g < a && c[g] === d[g]; ++g); a = c[g]; c = d[g]; if (!a) return -1; if (!c) return 1; b = b.getChildNodes(d[g - 1], null); d = 0; for (g = b.length; d < g; ++d) { var m = b[d]; if (m === a) return -1; if (m === c) return 1 }
  			}
  			function Xd(a, b, c, d) { var e = B(c.type, 47), f = B(d.type, 47); if (e && !f) { if (c = Vb(b, c.value), d = d.value, Sd(c, d)) return 1 } else if (f && !e) { if (c = c.value, d = Vb(b, d.value), Sd(c, d)) return -1 } else if (e && f) { if (Sd(Vb(b, d.value), Vb(b, c.value))) return c.value.node.localName > d.value.node.localName ? 1 : -1; c = Vb(b, c.value); d = Vb(b, d.value); } else c = c.value, d = d.value; return Wd(a, b, c, d) } function Yd(a, b, c) { return Xd(a.o, a, b, c) }
  			function Zd(a, b) { return $d(b, function (c, d) { return Xd(a.o, a, c, d) }).filter(function (c, d, e) { return 0 === d ? !0 : !Sd(c.value, e[d - 1].value) }) } function ae(a, b) { return a < b ? -1 : 0 } function $d(a, b) { b = void 0 === b ? ae : b; if (1 >= a.length) return a; var c = Math.floor(a.length / 2), d = $d(a.slice(0, c), b); a = $d(a.slice(c), b); for (c = []; d.length && a.length;)0 > b(d[0], a[0]) ? c.push(d.shift()) : c.push(a.shift()); return c.concat(d.concat(a)) } var be = xspattern; function ce(a, b) { if (B(a.type, 2)) { if (B(a.type, 6)) return 3 === b ? w(a.value, 3) : null; if (B(a.type, 4)) { if (6 === b) return w(a.value, 6); if (3 === b) return w(a.value, 3) } return null } return B(a.type, 20) && 1 === b ? w(a.value, 1) : null } function de(a, b, c, d, e) { if (B(a.type, b.type)) return a; B(b.type, 46) && B(a.type, 53) && (a = Yc(a, c).first()); if (B(a.type, b.type) || 46 === b.type) return a; if (B(a.type, 19)) { c = Pd(a, b.type); if (!c) throw Error("XPTY0004 Unable to convert " + (e ? "return" : "argument") + " of type " + mb[a.type] + " to type " + ob(b) + " while calling " + d); return c } c = ce(a, b.type); if (!c) throw Error("XPTY0004 Unable to cast " + (e ? "return" : "argument") + " of type " + mb[a.type] + " to type " + ob(b) + " while calling " + d); return c }
  			function ee(a) { switch (a) { case 2: return "*"; case 1: return "+"; case 0: return "?"; case 3: return "" } }
  			function fe(a, b, c, d, e) {
  				return 0 === a.g ? b.Z({ default: function () { return b.map(function (f) { return de(f, a, c, d, e) }) }, multiple: function () { throw Error("XPTY0004: Multiplicity of " + (e ? "function return value" : "function argument") + " of type " + mb[a.type] + ee(a.g) + " for " + d + ' is incorrect. Expected "?", but got "+".'); } }) : 1 === a.g ? b.Z({
  					empty: function () {
  						throw Error("XPTY0004: Multiplicity of " + (e ? "function return value" : "function argument") + " of type " + mb[a.type] + ee(a.g) + " for " + d + ' is incorrect. Expected "+", but got "empty-sequence()"');
  					}, default: function () { return b.map(function (f) { return de(f, a, c, d, e) }) }
  				}) : 2 === a.g ? b.map(function (f) { return de(f, a, c, d, e) }) : b.Z({ m: function () { return b.map(function (f) { return de(f, a, c, d, e) }) }, default: function () { throw Error("XPTY0004: Multiplicity of " + (e ? "function return value" : "function argument") + " of type " + mb[a.type] + ee(a.g) + " for " + d + " is incorrect. Expected exactly one"); } })
  			} function ge(a, b) { return B(a, 5) ? w(b, 5) : B(a, 6) ? w(b, 6) : B(a, 3) ? w(b, 3) : w(b, 4) } var he = [{ oa: "M", ma: 1E3 }, { oa: "CM", ma: 900 }, { oa: "D", ma: 500 }, { oa: "CD", ma: 400 }, { oa: "C", ma: 100 }, { oa: "XC", ma: 90 }, { oa: "L", ma: 50 }, { oa: "XL", ma: 40 }, { oa: "X", ma: 10 }, { oa: "IX", ma: 9 }, { oa: "V", ma: 5 }, { oa: "IV", ma: 4 }, { oa: "I", ma: 1 }]; function ie(a, b) { var c = parseInt(a, 10); a = 0 > c; c = Math.abs(c); if (!c) return "-"; var d = he.reduce(function (e, f) { var g = Math.floor(c / f.ma); c -= g * f.ma; return e + f.oa.repeat(g) }, ""); b && (d = d.toLowerCase()); a && (d = "-" + d); return d }
  			var je = "ABCDEFGHIJKLMNOPQRSTUVWXYZ".split(""); function ke(a, b) { a = parseInt(a, 10); var c = 0 > a; a = Math.abs(a); if (!a) return "-"; for (var d = "", e; 0 < a;)e = (a - 1) % je.length, d = je[e] + d, a = (a - e) / je.length | 0; b && (d = d.toLowerCase()); c && (d = "-" + d); return d } function le(a) { if (Math.floor(a) === a || isNaN(a)) return 0; a = /\d+(?:\.(\d*))?(?:[Ee](-)?(\d+))*/.exec("" + a); var b = a[1] ? a[1].length : 0; if (a[3]) { if (a[2]) return b + parseInt(a[3], 10); a = b - parseInt(a[3], 10); return 0 > a ? 0 : a } return b }
  			function me(a, b, c) { return b && 0 === a * c % 1 % .5 ? 0 === Math.floor(a * c) % 2 ? Math.floor(a * c) / c : Math.ceil(a * c) / c : Math.round(a * c) / c }
  			function ne(a, b, c, d, e, f) { var g = !1; return C.create({ next: function () { if (g) return x; var k = e.first(); if (!k) return g = !0, x; if ((B(k.type, 6) || B(k.type, 3)) && (0 === k.value || isNaN(k.value) || Infinity === k.value || -Infinity === k.value)) return g = !0, y(k); var l; f ? l = f.first().value : l = 0; g = !0; if (le(k.value) < l) return y(k); var m = [5, 4, 3, 6].find(function (u) { return B(k.type, u) }), q = Pd(k, 4); l = me(q.value, a, Math.pow(10, l)); switch (m) { case 4: return y(w(l, 4)); case 3: return y(w(l, 3)); case 6: return y(w(l, 6)); case 5: return y(w(l, 5)) } } }) }
  			function oe(a, b, c, d) { return Zc(d, b).Z({ empty: function () { return C.m(w(NaN, 3)) }, m: function () { var e = Od(d.first(), 3); return e.u ? C.m(e.value) : C.m(w(NaN, 3)) }, multiple: function () { throw Error("fn:number may only be called with zero or one values"); } }) } function pe(a) { for (var b = 5381, c = 0; c < a.length; ++c)b = 33 * b + a.charCodeAt(c), b %= Number.MAX_SAFE_INTEGER; return b }
  			function qe(a, b, c, d) {
  				function e(f) {
  					function g(k, l, m, q) { if (q.G() || q.wa()) return q; k = q.O(); l = f; for (m = k.length - 1; 1 < m; m--) { l = Mc(a, l).zb; q = l % m; var u = k[q]; k[q] = k[m]; k[m] = u; } return C.create(k) } return C.m(new dc([{ key: w("number", 1), value: function () { return C.m(w(Mc(a, f).jc, 3)) } }, { key: w("next", 1), value: function () { return C.m(new Ab({ value: function () { return e(Mc(a, f).zb) }, Sa: !0, localName: "", namespaceURI: "", j: [], arity: 0, i: { type: 61, g: 3 } })) } }, {
  						key: w("permute", 1), value: function () {
  							return C.m(new Ab({
  								value: g, Sa: !0,
  								localName: "", namespaceURI: "", j: [{ type: 59, g: 2 }], arity: 1, i: { type: 59, g: 2 }
  							}))
  						}
  					}]))
  				} d = void 0 === d ? C.empty() : d; b = d.G() ? Mc(a) : Mc(a, pe(Pd(d.first(), 1).value)); return e(b.zb)
  			}
  			var re = [{ namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "abs", j: [{ type: 2, g: 0 }], i: { type: 2, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return ge(e.type, Math.abs(e.value)) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "format-integer", j: [{ type: 5, g: 0 }, { type: 1, g: 3 }], i: { type: 1, g: 3 }, callFunction: function (a, b, c, d, e) {
  					a = d.first(); e = e.first(); if (d.G()) return C.m(w("", 1)); switch (e.value) {
  						case "I": return d = ie(a.value), C.m(w(d, 1)); case "i": return d = ie(a.value,
  							!0), C.m(w(d, 1)); case "A": return C.m(w(ke(a.value), 1)); case "a": return C.m(w(ke(a.value, !0), 1)); default: throw Error("Picture: " + e.value + ' is not implemented yet. The supported picture strings are "A", "a", "I", and "i"');
  					}
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "ceiling", j: [{ type: 2, g: 0 }], i: { type: 2, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return ge(e.type, Math.ceil(e.value)) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "floor",
  				j: [{ type: 2, g: 0 }], i: { type: 2, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return ge(e.type, Math.floor(e.value)) }) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "round", j: [{ type: 2, g: 0 }], i: { type: 2, g: 0 }, callFunction: ne.bind(null, !1) }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "round", j: [{ type: 2, g: 0 }, { type: 5, g: 3 }], i: { type: 2, g: 0 }, callFunction: ne.bind(null, !1) }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "round-half-to-even",
  				j: [{ type: 2, g: 0 }], i: { type: 2, g: 0 }, callFunction: ne.bind(null, !0)
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "round-half-to-even", j: [{ type: 2, g: 0 }, { type: 5, g: 3 }], i: { type: 2, g: 0 }, callFunction: ne.bind(null, !0) }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "number", j: [{ type: 46, g: 0 }], i: { type: 3, g: 3 }, callFunction: oe }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "number", j: [], i: { type: 3, g: 3 }, callFunction: function (a, b, c) {
  					var d = a.N && fe({ type: 46, g: 0 }, C.m(a.N),
  						b, "fn:number", !1); if (!d) throw Rc("fn:number needs an atomizable context item."); return oe(a, b, c, d)
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "random-number-generator", j: [], i: { type: 61, g: 3 }, callFunction: qe }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "random-number-generator", j: [{ type: 46, g: 0 }], i: { type: 61, g: 3 }, callFunction: qe }]; function se() { throw Error("FOCH0002: No collations are supported"); } function te(a, b, c, d) { if (null === b.N) throw Rc("The function which was called depends on dynamic context, which is absent."); return a(b, c, d, C.m(b.N)) } function ue(a, b, c, d) { return d.Z({ empty: function () { return C.m(w("", 1)) }, default: function () { return d.map(function (e) { if (B(e.type, 53)) { var f = Yc(e, b).first(); return B(e.type, 47) ? Pd(f, 1) : f } return Pd(e, 1) }) } }) }
  			function ve(a, b, c, d, e) { return ac([e], function (f) { var g = p(f).next().value; return Zc(d, b).M(function (k) { k = k.map(function (l) { return Pd(l, 1).value }).join(g.value); return C.m(w(k, 1)) }) }) } function we(a, b, c, d) { if (d.G()) return C.m(w(0, 5)); a = d.first().value; return C.m(w(Array.from(a).length, 5)) }
  			function xe(a, b, c, d, e, f) { var g = ne(!1, a, b, c, e, null), k = null !== f ? ne(!1, a, b, c, f, null) : null, l = !1, m = null, q = null, u = null; return C.create({ next: function () { if (l) return x; if (!m && (m = d.first(), null === m)) return l = !0, y(w("", 1)); q || (q = g.first()); !u && f && (u = null, u = k.first()); l = !0; return y(w(Array.from(m.value).slice(Math.max(q.value - 1, 0), f ? q.value + u.value - 1 : void 0).join(""), 1)) } }) }
  			function ye(a, b, c, d, e) { if (d.G() || 0 === d.first().value.length) return C.empty(); a = d.first().value; e = e.first().value; e = ze(e); return C.create(a.split(e).map(function (f) { return w(f, 1) })) } function Ae(a, b, c, d) { if (d.G()) return C.m(w("", 1)); a = d.first().value.trim(); return C.m(w(a.replace(/\s+/g, " "), 1)) } var Be = new Map, Ce = new Map;
  			function ze(a) { if (Ce.has(a)) return Ce.get(a); try { var b = new RegExp(a, "g"); } catch (c) { throw Error("FORX0002: " + c); } if (b.test("")) throw Error("FORX0003: the pattern " + a + " matches the zero length string"); Ce.set(a, b); return b }
  			var De = [{ namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "compare", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }], i: { type: 5, g: 0 }, callFunction: function (a, b, c, d, e) { if (d.G() || e.G()) return C.empty(); a = d.first().value; e = e.first().value; return a > e ? C.m(w(1, 5)) : a < e ? C.m(w(-1, 5)) : C.m(w(0, 5)) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "compare", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }, { type: 1, g: 3 }], i: { type: 5, g: 0 }, callFunction: se }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "concat",
  				j: [{ type: 46, g: 0 }, { type: 46, g: 0 }, 4], i: { type: 1, g: 3 }, callFunction: function (a, b, c) { var d = Aa.apply(3, arguments); d = d.map(function (e) { return Zc(e, b).M(function (f) { return C.m(w(f.map(function (g) { return null === g ? "" : Pd(g, 1).value }).join(""), 1)) }) }); return ac(d, function (e) { return C.m(w(e.map(function (f) { return f.value }).join(""), 1)) }) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "contains", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }, { type: 1, g: 0 }], i: { type: 0, g: 3 }, callFunction: se }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				localName: "contains", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }], i: { type: 0, g: 3 }, callFunction: function (a, b, c, d, e) { a = d.G() ? "" : d.first().value; e = e.G() ? "" : e.first().value; return 0 === e.length ? C.ba() : 0 === a.length ? C.W() : a.includes(e) ? C.ba() : C.W() }
  			}, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "ends-with", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }], i: { type: 0, g: 3 }, callFunction: function (a, b, c, d, e) {
  					a = e.G() ? "" : e.first().value; if (0 === a.length) return C.ba(); d = d.G() ? "" : d.first().value; return 0 === d.length ? C.W() : d.endsWith(a) ?
  						C.ba() : C.W()
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "ends-with", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }, { type: 1, g: 3 }], i: { type: 0, g: 3 }, callFunction: se }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "normalize-space", j: [{ type: 1, g: 0 }], i: { type: 1, g: 3 }, callFunction: Ae }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "normalize-space", j: [], i: { type: 1, g: 3 }, callFunction: te.bind(null, function (a, b, c, d) { return Ae(a, b, c, ue(a, b, c, d)) }) }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				localName: "starts-with", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }], i: { type: 0, g: 3 }, callFunction: function (a, b, c, d, e) { a = e.G() ? "" : e.first().value; if (0 === a.length) return C.ba(); d = d.G() ? "" : d.first().value; return 0 === d.length ? C.W() : d.startsWith(a) ? C.ba() : C.W() }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "starts-with", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }, { type: 1, g: 3 }], i: { type: 0, g: 3 }, callFunction: se }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "string", j: [{ type: 59, g: 0 }], i: { type: 1, g: 3 },
  				callFunction: ue
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "string", j: [], i: { type: 1, g: 3 }, callFunction: te.bind(null, ue) }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "substring-before", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }], i: { type: 1, g: 3 }, callFunction: function (a, b, c, d, e) { a = d.G() ? "" : d.first().value; e = e.G() ? "" : e.first().value; if ("" === e) return C.m(w("", 1)); e = a.indexOf(e); return -1 === e ? C.m(w("", 1)) : C.m(w(a.substring(0, e), 1)) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				localName: "substring-after", j: [{ type: 1, g: 0 }, { type: 1, g: 0 }], i: { type: 1, g: 3 }, callFunction: function (a, b, c, d, e) { a = d.G() ? "" : d.first().value; e = e.G() ? "" : e.first().value; if ("" === e) return C.m(w(a, 1)); b = a.indexOf(e); return -1 === b ? C.m(w("", 1)) : C.m(w(a.substring(b + e.length), 1)) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "substring", j: [{ type: 1, g: 0 }, { type: 3, g: 3 }], i: { type: 1, g: 3 }, callFunction: xe }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "substring", j: [{ type: 1, g: 0 }, {
  					type: 3,
  					g: 3
  				}, { type: 3, g: 3 }], i: { type: 1, g: 3 }, callFunction: xe
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "upper-case", j: [{ type: 1, g: 0 }], i: { type: 1, g: 3 }, callFunction: function (a, b, c, d) { return d.G() ? C.m(w("", 1)) : d.map(function (e) { return w(e.value.toUpperCase(), 1) }) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "lower-case", j: [{ type: 1, g: 0 }], i: { type: 1, g: 3 }, callFunction: function (a, b, c, d) { return d.G() ? C.m(w("", 1)) : d.map(function (e) { return w(e.value.toLowerCase(), 1) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				localName: "string-join", j: [{ type: 46, g: 2 }, { type: 1, g: 3 }], i: { type: 1, g: 3 }, callFunction: ve
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "string-join", j: [{ type: 46, g: 2 }], i: { type: 1, g: 3 }, callFunction: function (a, b, c, d) { return ve(a, b, c, d, C.m(w("", 1))) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "string-length", j: [{ type: 1, g: 0 }], i: { type: 5, g: 3 }, callFunction: we }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "string-length", j: [], i: { type: 5, g: 3 }, callFunction: te.bind(null,
  					function (a, b, c, d) { return we(a, b, c, ue(a, b, c, d)) })
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "tokenize", j: [{ type: 1, g: 0 }, { type: 1, g: 3 }, { type: 1, g: 3 }], i: { type: 1, g: 2 }, callFunction: function () { throw Error("Not implemented: Using flags in tokenize is not supported"); } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "tokenize", j: [{ type: 1, g: 0 }, { type: 1, g: 3 }], i: { type: 1, g: 2 }, callFunction: ye }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "tokenize", j: [{
  					type: 1,
  					g: 0
  				}], i: { type: 1, g: 2 }, callFunction: function (a, b, c, d) { return ye(a, b, c, Ae(a, b, c, d), C.m(w(" ", 1))) }
  			}, {
  				j: [{ type: 1, g: 0 }, { type: 1, g: 3 }, { type: 1, g: 3 }], callFunction: function (a, b, c, d, e, f) { return ac([d, e, f], function (g) { var k = p(g), l = k.next().value; g = k.next().value; k = k.next().value; l = Array.from(l ? l.value : ""); var m = Array.from(g.value), q = Array.from(k.value); g = l.map(function (u) { if (m.includes(u)) { if (u = m.indexOf(u), u <= q.length) return q[u] } else return u }); return C.m(w(g.join(""), 1)) }) }, localName: "translate", namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				i: { type: 1, g: 3 }
  			}, { j: [{ type: 5, g: 2 }], callFunction: function (a, b, c, d) { return d.M(function (e) { e = e.map(function (f) { f = f.value; if (9 === f || 10 === f || 13 === f || 32 <= f && 55295 >= f || 57344 <= f && 65533 >= f || 65536 <= f && 1114111 >= f) return String.fromCodePoint(f); throw Error("FOCH0001"); }).join(""); return C.m(w(e, 1)) }) }, localName: "codepoints-to-string", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 } }, {
  				j: [{ type: 1, g: 0 }], callFunction: function (a, b, c, d) {
  					return ac([d], function (e) {
  						e = (e = p(e).next().value) ? e.value.split("") :
  							[]; return 0 === e.length ? C.empty() : C.create(e.map(function (f) { return w(f.codePointAt(0), 5) }))
  					})
  				}, localName: "string-to-codepoints", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 5, g: 2 }
  			}, {
  				j: [{ type: 1, g: 0 }], callFunction: function (a, b, c, d) { return ac([d], function (e) { e = p(e).next().value; return null === e || 0 === e.value.length ? C.create(w("", 1)) : C.create(w(encodeURIComponent(e.value).replace(/[!'()*]/g, function (f) { return "%" + f.charCodeAt(0).toString(16).toUpperCase() }), 1)) }) }, localName: "encode-for-uri",
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 }
  			}, { j: [{ type: 1, g: 0 }], callFunction: function (a, b, c, d) { return ac([d], function (e) { e = p(e).next().value; return null === e || 0 === e.value.length ? C.create(w("", 1)) : C.create(w(e.value.replace(/([\u00A0-\uD7FF\uE000-\uFDCF\uFDF0-\uFFEF "<>{}|\\^`/\n\u007f\u0080-\u009f]|[\uD800-\uDBFF][\uDC00-\uDFFF])/g, function (f) { return encodeURI(f) }), 1)) }) }, localName: "iri-to-uri", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 } }, {
  				j: [{
  					type: 1,
  					g: 0
  				}, { type: 1, g: 0 }], callFunction: function (a, b, c, d, e) { return ac([d, e], function (f) { var g = p(f); f = g.next().value; g = g.next().value; if (null === f || null === g) return C.empty(); f = f.value; g = g.value; if (f.length !== g.length) return C.W(); f = f.split(""); g = g.split(""); for (var k = 0; k < f.length; k++)if (f[k].codePointAt(0) !== g[k].codePointAt(0)) return C.W(); return C.ba() }) }, localName: "codepoint-equal", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 0, g: 0 }
  			}, {
  				j: [{ type: 1, g: 0 }, { type: 1, g: 3 }], callFunction: function (a,
  					b, c, d, e) { return ac([d, e], function (f) { var g = p(f); f = g.next().value; g = g.next().value; f = f ? f.value : ""; g = g.value; var k = Be.get(g); if (!k) { try { k = be.compile(g, { language: "xpath" }); } catch (l) { throw Error("FORX0002: " + l); } Be.set(g, k); } return k(f) ? C.ba() : C.W() }) }, localName: "matches", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 0, g: 3 }
  			}, {
  				j: [{ type: 1, g: 0 }, { type: 1, g: 3 }, { type: 1, g: 3 }], callFunction: function (a, b, c, d, e, f) {
  					return ac([d, e, f], function (g) {
  						var k = p(g); g = k.next().value; var l = k.next().value; k = k.next().value;
  						g = g ? g.value : ""; l = l.value; k = k.value; if (k.includes("$0")) throw Error("Using $0 in fn:replace to replace substrings with full matches is not supported."); k = k.split(/((?:\$\$)|(?:\\\$)|(?:\\\\))/).map(function (m) { switch (m) { case "\\$": return "$$"; case "\\\\": return "\\"; case "$$": throw Error('FORX0004: invalid replacement: "$$"'); default: return m } }).join(""); l = ze(l); g = g.replace(l, k); return C.m(w(g, 1))
  					})
  				}, localName: "replace", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 }
  			}, {
  				j: [{
  					type: 1,
  					g: 0
  				}, { type: 1, g: 3 }, { type: 1, g: 3 }, { type: 1, g: 3 }], localName: "replace", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 }, callFunction: function () { throw Error("Not implemented: Using flags in replace is not supported"); }
  			}]; function Ee(a, b, c, d, e) { if (null === c.N) throw Rc("The function " + a + " depends on dynamic context, which is absent."); return b(c, d, e, C.m(c.N)) } function Fe(a, b, c, d) { return ac([d], function (e) { e = p(e).next().value; if (null === e) return C.empty(); e = e.value; switch (e.node.nodeType) { case 1: case 2: return C.m(w(new zb(e.node.prefix, e.node.namespaceURI, e.node.localName), 23)); case 7: return C.m(w(new zb("", "", e.node.target), 23)); default: return C.empty() } }) }
  			function Ge(a, b, c, d) { return d.Z({ default: function () { return ue(a, b, c, Fe(a, b, c, d)) }, empty: function () { return C.m(w("", 1)) } }) } function He(a, b, c, d) { return Zc(d, b) } function Ie(a, b, c, d) { return ac([d], function (e) { e = (e = p(e).next().value) ? e.value : null; return null !== e && Sb(b.h, e, null) ? C.ba() : C.W() }) }
  			function Je(a, b, c, d) {
  				return ac([d], function (e) {
  					function f(m) { for (var q = 0, u = m; null !== u;)(m.node.nodeType !== u.node.nodeType ? 0 : 1 === u.node.nodeType ? u.node.localName === m.node.localName && u.node.namespaceURI === m.node.namespaceURI : 7 === u.node.nodeType ? u.node.target === m.node.target : 1) && q++, u = Wb(k, u, null); return q } var g = p(e).next().value; if (null === g) return C.empty(); var k = b.h; e = ""; for (g = g.value; null !== Vb(b.h, g, null); g = Vb(b.h, g, null))switch (g.node.nodeType) {
  						case 1: var l = g; e = "/Q{" + (l.node.namespaceURI || "") + "}" +
  							l.node.localName + "[" + f(l) + "]" + e; break; case 2: l = g; e = "/@" + (l.node.namespaceURI ? "Q{" + l.node.namespaceURI + "}" : "") + l.node.localName + e; break; case 3: e = "/text()[" + f(g) + "]" + e; break; case 7: l = g; e = "/processing-instruction(" + l.node.target + ")[" + f(l) + "]" + e; break; case 8: e = "/comment()[" + f(g) + "]" + e;
  					}return 9 === g.node.nodeType ? C.create(w(e || "/", 1)) : C.create(w("Q{http://www.w3.org/2005/xpath-functions}root()" + e, 1))
  				})
  			} function Ke(a, b, c, d) { return d.map(function (e) { return w(e.value.node.namespaceURI || "", 20) }) }
  			function Le(a, b, c, d) { return d.Z({ default: function () { return d.map(function (e) { return 7 === e.value.node.nodeType ? w(e.value.node.target, 1) : w(e.value.node.localName || "", 1) }) }, empty: function () { return C.m(w("", 1)) } }) } function Me(a, b, c) { if (2 === b.node.nodeType) return Sd(b, c); for (; c;) { if (Sd(b, c)) return !0; if (9 === c.node.nodeType) break; c = Vb(a, c, null); } return !1 }
  			function Ne(a, b, c, d) { return d.map(function (e) { if (!B(e.type, 53)) throw Error("XPTY0004 Argument passed to fn:root() should be of the type node()"); for (e = e.value; e;) { var f = e; e = Vb(b.h, f, null); } return $b(f) }) }
  			var Oe = [{ j: [{ type: 53, g: 0 }], callFunction: Ge, localName: "name", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 } }, { j: [], callFunction: Ee.bind(null, "name", Ge), localName: "name", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 } }, { j: [{ type: 53, g: 3 }], callFunction: Ke, localName: "namespace-uri", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 20, g: 3 } }, {
  				j: [], callFunction: Ee.bind(null, "namespace-uri", Ke), localName: "namespace-uri", namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				i: { type: 20, g: 3 }
  			}, { j: [{ type: 53, g: 2 }], callFunction: function (a, b, c, d) { return d.M(function (e) { if (!e.length) return C.empty(); e = Zd(b.h, e).reduceRight(function (f, g, k, l) { if (k === l.length - 1) return f.push(g), f; if (Me(b.h, g.value, f[0].value)) return f; f.unshift(g); return f }, []); return C.create(e) }) }, localName: "innermost", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 53, g: 2 } }, {
  				j: [{ type: 53, g: 2 }], callFunction: function (a, b, c, d) {
  					return d.M(function (e) {
  						if (!e.length) return C.empty(); e = Zd(b.h, e).reduce(function (f,
  							g, k) { if (0 === k) return f.push(g), f; if (Me(b.h, f[f.length - 1].value, g.value)) return f; f.push(g); return f }, []); return C.create(e)
  					}, 1)
  				}, localName: "outermost", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 53, g: 2 }
  			}, { j: [{ type: 53, g: 0 }], callFunction: Ie, localName: "has-children", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 0, g: 3 } }, { j: [], callFunction: Ee.bind(null, "has-children", Ie), localName: "has-children", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 0, g: 3 } }, {
  				j: [{
  					type: 53,
  					g: 0
  				}], callFunction: Je, localName: "path", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 0 }
  			}, { j: [], callFunction: Ee.bind(null, "path", Je), localName: "path", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 0 } }, { j: [{ type: 53, g: 0 }], callFunction: Fe, localName: "node-name", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 23, g: 0 } }, {
  				j: [], callFunction: Ee.bind(null, "node-name", Fe), localName: "node-name", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: {
  					type: 23,
  					g: 0
  				}
  			}, { j: [{ type: 53, g: 0 }], callFunction: Le, localName: "local-name", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 } }, { j: [], callFunction: Ee.bind(null, "local-name", Le), localName: "local-name", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 } }, { j: [{ type: 53, g: 0 }], callFunction: Ne, localName: "root", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 53, g: 0 } }, {
  				j: [], callFunction: Ee.bind(null, "root", Ne), localName: "root", namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				i: { type: 53, g: 0 }
  			}, { j: [], callFunction: Ee.bind(null, "data", He), localName: "data", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 2 } }, { j: [{ type: 59, g: 2 }], callFunction: He, localName: "data", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 2 } }]; function Pe(a, b) { var c = 0, d = a.length, e = !1, f = null; return { next: function () { if (!e) { for (; c < d;) { f || (f = b(a[c], c, a)); var g = f.next(0); f = null; if (g.value) c++; else return y(!1) } e = !0; return y(!0) } return x } } } function Qe(a) { a = a.node.nodeType; return 1 === a || 3 === a }
  			function Re(a, b) {
  				if ((B(a.type, 4) || B(a.type, 6)) && (B(b.type, 4) || B(b.type, 6))) { var c = Pd(a, 6), d = Pd(b, 6); return c.value === d.value || isNaN(a.value) && isNaN(b.value) } return (B(a.type, 4) || B(a.type, 6) || B(a.type, 3)) && (B(b.type, 4) || B(b.type, 6) || B(b.type, 3)) ? (c = Pd(a, 3), d = Pd(b, 3), c.value === d.value || isNaN(a.value) && isNaN(b.value)) : B(a.type, 23) && B(b.type, 23) ? a.value.namespaceURI === b.value.namespaceURI && a.value.localName === b.value.localName : (B(a.type, 9) || B(a.type, 7) || B(a.type, 8) || B(a.type, 11) || B(a.type, 12) || B(a.type,
  					13) || B(a.type, 14) || B(a.type, 15)) && (B(b.type, 9) || B(b.type, 7) || B(b.type, 8) || B(b.type, 11) || B(b.type, 12) || B(b.type, 13) || B(b.type, 14) || B(b.type, 15)) ? wc(a.value, b.value) : (B(a.type, 16) || B(a.type, 17) || B(a.type, 18)) && (B(b.type, 16) || B(b.type, 17) || B(b.type, 17)) ? fc(a.value, b.value) : a.value === b.value
  			} function Se(a, b, c) { c = p([b, c].map(function (d) { return { type: 1, value: d.reduce(function (e, f) { return e += Yc(f, a).first().value }, "") } })); b = c.next().value; c = c.next().value; return y(Re(b, c)) }
  			function Te(a, b, c, d) { for (; a.value && B(a.value.type, 56);) { b.push(a.value); var e = Ub(d, a.value.value); a = c.next(0); if (e && 3 !== e.node.nodeType) break } return a }
  			function Ue(a, b, c, d, e) { var f = b.h, g = d.value, k = e.value, l = null, m = null, q = null, u, z = [], A = []; return { next: function () { for (; !u;)if (l || (l = g.next(0)), l = Te(l, z, g, f), m || (m = k.next(0)), m = Te(m, A, k, f), z.length || A.length) { var D = Se(b, z, A); z.length = 0; A.length = 0; if (!1 === D.value) return u = !0, D } else { if (l.done || m.done) return u = !0, y(l.done === m.done); q || (q = Ve(a, b, c, l.value, m.value)); D = q.next(0); q = null; if (!1 === D.value) return u = !0, D; m = l = null; } return x } } }
  			function We(a, b, c, d, e) { return d.h.length !== e.h.length ? Qd(!1) : Pe(d.h, function (f) { var g = e.h.find(function (k) { return Re(k.key, f.key) }); return g ? Ue(a, b, c, f.value(), g.value()) : Qd(!1) }) } function Xe(a, b, c, d, e) { return d.P.length !== e.P.length ? Qd(!1) : Pe(d.P, function (f, g) { g = e.P[g]; return Ue(a, b, c, f(), g()) }) }
  			function Ye(a, b, c, d, e) { d = Pb(b.h, d.value); e = Pb(b.h, e.value); d = d.filter(function (f) { return Qe(f) }); e = e.filter(function (f) { return Qe(f) }); d = C.create(d.map(function (f) { return $b(f) })); e = C.create(e.map(function (f) { return $b(f) })); return Ue(a, b, c, d, e) }
  			function Ze(a, b, c, d, e) {
  				var f = Ue(a, b, c, Fe(a, b, c, C.m(d)), Fe(a, b, c, C.m(e))), g = Ye(a, b, c, d, e); d = Nb(b.h, d.value).filter(function (m) { return "http://www.w3.org/2000/xmlns/" !== m.node.namespaceURI }).sort(function (m, q) { return m.node.nodeName > q.node.nodeName ? 1 : -1 }).map(function (m) { return $b(m) }); e = Nb(b.h, e.value).filter(function (m) { return "http://www.w3.org/2000/xmlns/" !== m.node.namespaceURI }).sort(function (m, q) { return m.node.nodeName > q.node.nodeName ? 1 : -1 }).map(function (m) { return $b(m) }); var k = Ue(a, b, c, C.create(d),
  					C.create(e)), l = !1; return { next: function () { if (l) return x; var m = f.next(0); if (!m.done && !1 === m.value) return l = !0, m; m = k.next(0); if (!m.done && !1 === m.value) return l = !0, m; m = g.next(0); l = !0; return m } }
  			} function $e(a, b, c, d, e) { var f = Ue(a, b, c, Fe(a, b, c, C.m(d)), Fe(a, b, c, C.m(e))), g = !1; return { next: function () { if (g) return x; var k = f.next(0); return k.done || !1 !== k.value ? y(Re(Yc(d, b).first(), Yc(e, b).first())) : (g = !0, k) } } }
  			function Ve(a, b, c, d, e) { if (B(d.type, 46) && B(e.type, 46)) return Qd(Re(d, e)); if (B(d.type, 61) && B(e.type, 61)) return We(a, b, c, d, e); if (B(d.type, 62) && B(e.type, 62)) return Xe(a, b, c, d, e); if (B(d.type, 53) && B(e.type, 53)) { if (B(d.type, 55) && B(e.type, 55)) return Ye(a, b, c, d, e); if (B(d.type, 54) && B(e.type, 54)) return Ze(a, b, c, d, e); if (B(d.type, 47) && B(e.type, 47) || B(d.type, 57) && B(e.type, 57) || B(d.type, 58) && B(e.type, 58)) return $e(a, b, c, d, e) } return Qd(!1) } function af(a) { return Error("XUST0001: " + (void 0 === a ? "Can not execute an updating expression in a non-updating context." : a)) } function bf(a) { return Error("XUTY0004: The attribute " + a.name + '="' + a.value + '" follows a node that is not an attribute node.') } function cf() { return Error("XUTY0005: The target of a insert expression with into must be a single element or document node.") }
  			function df() { return Error("XUTY0006: The target of a insert expression with before or after must be a single element, text, comment, or processing instruction node.") } function ff() { return Error("XUTY0008: The target of a replace expression must be a single element, attribute, text, comment, or processing instruction node.") } function gf() { return Error("XUTY0012: The target of a rename expression must be a single element, attribute, or processing instruction node.") }
  			function hf(a) { return Error("XUDY0017: The target " + a.outerHTML + " is used in more than one replace value of expression.") } function jf(a) { return Error("XUDY0021: Applying the updates will result in the XDM instance violating constraint: '" + a + "'") } function kf(a) { return Error("XUDY0023: The namespace binding " + a + " is conflicting.") } function lf(a) { return Error("XUDY0024: The namespace binding " + a + " is conflicting.") }
  			function mf() { return Error("XUDY0027: The target for an insert, replace, or rename expression expression should not be empty.") } function H(a, b, c, d, e) { c = void 0 === c ? { C: !1, X: !1, R: "unsorted", subtree: !1 } : c; this.o = a; this.da = c.R || "unsorted"; this.subtree = !!c.subtree; this.X = !!c.X; this.C = !!c.C; this.ta = b; this.I = !1; this.ab = null; this.Yb = void 0 === d ? !1 : d; this.type = e; } function I(a, b, c) { b && null !== b.N ? a.C ? (null === a.ab && (a.ab = yb(a.h(null, c).sb())), a = a.ab()) : a = a.h(b, c) : a = a.h(b, c); return a } H.prototype.B = function () { return null };
  			H.prototype.v = function (a) { this.ta.forEach(function (b) { return b.v(a) }); if (!this.Yb && this.ta.some(function (b) { return b.I })) throw af(); }; function nf(a, b) { this.J = a; this.fa = b; } function of(a) { a && "object" === typeof a && "nodeType" in a && (a = a.ownerDocument || a, "function" === typeof a.createElementNS && "function" === typeof a.createProcessingInstruction && "function" === typeof a.createTextNode && "function" === typeof a.createComment && (this.h = a)); this.h || (this.h = null); } h = of.prototype; h.createAttributeNS = function (a, b) { if (!this.h) throw Error("Please pass a node factory if an XQuery script uses node constructors"); return this.h.createAttributeNS(a, b) };
  			h.createCDATASection = function (a) { if (!this.h) throw Error("Please pass a node factory if an XQuery script uses node constructors"); return this.h.createCDATASection(a) }; h.createComment = function (a) { if (!this.h) throw Error("Please pass a node factory if an XQuery script uses node constructors"); return this.h.createComment(a) }; h.createDocument = function () { if (!this.h) throw Error("Please pass a node factory if an XQuery script uses node constructors"); return this.h.implementation.createDocument(null, null, null) };
  			h.createElementNS = function (a, b) { if (!this.h) throw Error("Please pass a node factory if an XQuery script uses node constructors"); return this.h.createElementNS(a, b) }; h.createProcessingInstruction = function (a, b) { if (!this.h) throw Error("Please pass a node factory if an XQuery script uses node constructors"); return this.h.createProcessingInstruction(a, b) }; h.createTextNode = function (a) { if (!this.h) throw Error("Please pass a node factory if an XQuery script uses node constructors"); return this.h.createTextNode(a) }; function pf(a, b, c, d) { var e = Vb(c, a).node, f = (a = Ub(c, a)) ? a.node : null; b.forEach(function (g) { d.insertBefore(e, g.node, f); }); } function qf(a, b, c, d) { var e = Vb(c, a).node; b.forEach(function (f) { d.insertBefore(e, f.node, a.node); }); } function rf(a, b, c, d) { var e = (c = Sb(c, a)) ? c.node : null; b.forEach(function (f) { d.insertBefore(a.node, f.node, e); }); } function sf(a, b, c) { b.forEach(function (d) { c.insertBefore(a.node, d.node, null); }); }
  			function tf(a, b, c, d) { b.forEach(function (e) { var f = e.node.nodeName; if (Ob(c, a, f)) throw jf("An attribute " + f + " already exists."); d.setAttributeNS(a.node, e.node.namespaceURI, f, Qb(c, e)); }); }
  			function uf(a, b, c, d, e) {
  				d || (d = new of(a ? a.node : null)); switch (a.node.nodeType) { case 1: var f = c.getAllAttributes(a.node), g = c.getChildNodes(a.node), k = d.createElementNS(b.namespaceURI, b.Ca()); var l = { node: k, F: null }; f.forEach(function (m) { e.setAttributeNS(k, m.namespaceURI, m.nodeName, m.value); }); g.forEach(function (m) { e.insertBefore(k, m, null); }); break; case 2: b = d.createAttributeNS(b.namespaceURI, b.Ca()); b.value = Qb(c, a); l = { node: b, F: null }; break; case 7: l = { node: d.createProcessingInstruction(b.Ca(), Qb(c, a)), F: null }; }if (!Vb(c,
  					a)) throw Error("Not supported: renaming detached nodes."); vf(a, [l], c, e);
  			} function wf(a, b, c, d) { c.getChildNodes(a.node).forEach(function (e) { return d.removeChild(a.node, e) }); b && d.insertBefore(a.node, b.node, null); }
  			function vf(a, b, c, d) {
  				var e = Vb(c, a), f = a.node.nodeType; if (2 === f) { if (b.some(function (l) { return 2 !== l.node.nodeType })) throw Error('Constraint "If $target is an attribute node, $replacement must consist of zero or more attribute nodes." failed.'); var g = e ? e.node : null; d.removeAttributeNS(g, a.node.namespaceURI, a.node.nodeName); b.forEach(function (l) { var m = l.node.nodeName; if (Ob(c, e, m)) throw jf("An attribute " + m + " already exists."); d.setAttributeNS(g, l.node.namespaceURI, m, Qb(c, l)); }); } if (1 === f || 3 === f || 8 === f ||
  					7 === f) { var k = (f = Ub(c, a)) ? f.node : null; d.removeChild(e.node, a.node); b.forEach(function (l) { d.insertBefore(e.node, l.node, k); }); }
  			} function xf(a, b, c, d) {
  				yf(a, b); a.filter(function (e) { return -1 !== ["insertInto", "insertAttributes", "replaceValue", "rename"].indexOf(e.type) }).forEach(function (e) {
  					switch (e.type) {
  						case "insertInto": sf(e.target, e.content, d); break; case "insertAttributes": tf(e.target, e.content, b, d); break; case "rename": uf(e.target, e.o, b, c, d); break; case "replaceValue": var f = e.target; e = e.o; if (2 === f.node.nodeType) { var g = Vb(b, f); g ? d.setAttributeNS(g.node, f.node.namespaceURI, f.node.nodeName, e) : f.node.value = e; } else d.setData(f.node,
  							e);
  					}
  				}); a.filter(function (e) { return -1 !== ["insertBefore", "insertAfter", "insertIntoAsFirst", "insertIntoAsLast"].indexOf(e.type) }).forEach(function (e) { switch (e.type) { case "insertAfter": pf(e.target, e.content, b, d); break; case "insertBefore": qf(e.target, e.content, b, d); break; case "insertIntoAsFirst": rf(e.target, e.content, b, d); break; case "insertIntoAsLast": sf(e.target, e.content, d); } }); a.filter(function (e) { return "replaceNode" === e.type }).forEach(function (e) { vf(e.target, e.o, b, d); }); a.filter(function (e) {
  					return "replaceElementContent" ===
  						e.type
  				}).forEach(function (e) { wf(e.target, e.text, b, d); }); a.filter(function (e) { return "delete" === e.type }).forEach(function (e) { e = e.target; var f = Vb(b, e); (f = f ? f.node : null) && (2 === e.node.nodeType ? d.removeAttributeNS(f, e.node.namespaceURI, e.node.nodeName) : d.removeChild(f, e.node)); }); if (a.some(function (e) { return "put" === e.type })) throw Error('Not implemented: the execution for pendingUpdate "put" is not yet implemented.');
  			}
  			function yf(a, b) {
  				function c(f) { return new zb(f.node.prefix, f.node.namespaceURI, f.node.localName) } function d(f, g) { var k = new Set; a.filter(function (l) { return l.type === f }).map(function (l) { return l.target }).forEach(function (l) { l = l ? l.node : null; k.has(l) && g(l); k.add(l); }); } d("rename", function (f) { throw Error("XUDY0015: The target " + f.outerHTML + " is used in more than one rename expression."); }); d("replaceNode", function (f) {
  					throw Error("XUDY0016: The target " + f.outerHTML + " is used in more than one replace expression.");
  				}); d("replaceValue", function (f) { throw hf(f); }); d("replaceElementContent", function (f) { throw hf(f); }); var e = new Map; a.filter(function (f) { return "replaceNode" === f.type && 2 === f.target.node.nodeType }).forEach(function (f) { var g = Vb(b, f.target); g = g ? g.node : null; var k = e.get(g); k ? k.push.apply(k, t(f.o.map(c))) : e.set(g, f.o.map(c)); }); a.filter(function (f) { return "rename" === f.type && 2 === f.target.node.nodeType }).forEach(function (f) { var g = Vb(b, f.target); if (g) { g = g.node; var k = e.get(g); k ? k.push(f.o) : e.set(g, [f.o]); } }); e.forEach(function (f) {
  					var g =
  						{}; f.forEach(function (k) { g[k.prefix] || (g[k.prefix] = k.namespaceURI); if (g[k.prefix] !== k.namespaceURI) throw lf(k.namespaceURI); });
  				});
  			} function zf(a) { return a.concat.apply(a, t(Aa.apply(1, arguments).filter(Boolean))) } function Af(a, b, c, d) { H.call(this, a, b, c, !0, d); this.I = !0; } v(Af, H); Af.prototype.h = function () { throw af(); }; function Bf(a) { return a.I ? function (b, c) { return a.s(b, c) } : function (b, c) { var d = a.h(b, c); return { next: function () { var e = d.O(); return y({ fa: [], J: e }) } } } } function Cf(a, b) { a = a.next(0); b(a.value.fa); return C.create(a.value.J) } function Df(a, b, c, d) { Af.call(this, a, b, c, d); this.I = this.ta.some(function (e) { return e.I }); } v(Df, Af); Df.prototype.h = function (a, b) { return this.A(a, b, this.ta.map(function (c) { return function (d) { return c.h(d, b) } })) };
  			Df.prototype.s = function (a, b) { var c = [], d = this.A(a, b, this.ta.map(function (f) { return f.I ? function (g) { g = f.s(g, b); return Cf(g, function (k) { return c = zf(c, k) }) } : function (g) { return f.h(g, b) } })), e = !1; return { next: function () { if (e) return x; var f = d.O(); e = !0; return y(new nf(f, c)) } } }; Df.prototype.v = function (a) { Af.prototype.v.call(this, a); Ef(this); }; function Ef(a) { a.ta.some(function (b) { return b.I }) && (a.I = !0); } var Ff = ["external", "attribute", "nodeName", "nodeType", "universal"], Gf = Ff.length; function Hf(a) { this.h = Ff.map(function (b) { return a[b] || 0 }); if (Object.keys(a).some(function (b) { return !Ff.includes(b) })) throw Error("Invalid specificity kind passed"); } Hf.prototype.add = function (a) { var b = this, c = Ff.reduce(function (d, e, f) { d[e] = b.h[f] + a.h[f]; return d }, Object.create(null)); return new Hf(c) }; function If(a, b) { for (var c = 0; c < Gf; ++c) { if (b.h[c] < a.h[c]) return 1; if (b.h[c] > a.h[c]) return -1 } return 0 } function Jf() { return Sc("Expected base expression of a function call to evaluate to a sequence of single function item") } function Kf(a, b, c, d) { for (var e = [], f = 0; f < b.length; ++f)if (null === b[f]) e.push(null); else { var g = fe(a[f], b[f], c, d, !1); e.push(g); } return e } function Lf(a, b) { if (!B(a.type, 60)) throw Sc("Expected base expression to evaluate to a function item"); if (a.v !== b) throw Jf(); return a }
  			function Mf(a, b, c, d, e, f, g) { var k = 0; e = e.map(function (l) { return l ? null : f[k++](c) }); e = Kf(a.o, e, d, a.B); if (0 <= e.indexOf(null)) return Cb(a, e); b = b.apply(void 0, [c, d, g].concat(t(e))); return fe(a.s, b, d, a.B, !0) } function Nf(a, b, c) { var d = {}; Df.call(this, new Hf((d.external = 1, d)), [a].concat(b.filter(function (e) { return !!e })), { R: "unsorted", X: !1, subtree: !1, C: !1 }, c); this.la = b.length; this.S = b.map(function (e) { return null === e }); this.K = null; this.pa = a; this.Ba = b; } v(Nf, Df);
  			Nf.prototype.s = function (a, b) { var c = this; if (!this.l || !this.l.I) return Df.prototype.s.call(this, a, b); var d = [], e = Mf(this.l, function (g, k, l) { return Cf(c.l.value.apply(c.l, [g, k, l].concat(t(Aa.apply(3, arguments)))), function (m) { d = zf(d, m); }) }, a, b, this.S, this.Ba.map(function (g) { return function () { return g.I ? Cf(g.s(a, b), function (k) { d = zf(d, k); }) : I(g, a, b) } }), this.K), f = !1; return { next: function () { if (f) return x; var g = e.O(); f = !0; return y({ fa: d, J: g }) } } };
  			Nf.prototype.A = function (a, b, c) {
  				var d = this; c = p(c); var e = c.next().value, f = ja(c); if (this.l) return Mf(this.l, function (k, l, m) { return d.l.value.apply(d.l, [k, l, m].concat(t(Aa.apply(3, arguments)))) }, a, b, this.S, f, this.K); var g = e(a); return g.Z({
  					default: function () { throw Jf(); }, m: function () {
  						return g.M(function (k) {
  							k = p(k).next().value; k = Lf(k, d.la); if (k.I) throw Error("XUDY0038: The function returned by the PrimaryExpr of a dynamic function invocation can not be an updating function"); return Mf(k, k.value, a, b, d.S,
  								f, d.K)
  						})
  					}
  				})
  			}; Nf.prototype.v = function (a) { this.K = Of(a); Df.prototype.v.call(this, a); if (this.pa.C) { a = I(this.pa, null, null); if (!a.wa()) throw Jf(); this.l = Lf(a.first(), this.la); this.l.I && (this.I = !0); } }; function Pf(a, b, c, d, e, f) { return ac([d, e, f], function (g) { var k = p(g); g = k.next().value; var l = k.next().value; k = k.next().value; l = l.value; k = k.value; if (l > g.P.length || 0 >= l) throw Error("FOAY0001: subarray start out of bounds."); if (0 > k) throw Error("FOAY0002: subarray length out of bounds."); if (l + k > g.P.length + 1) throw Error("FOAY0001: subarray start + length out of bounds."); return C.m(new Yb(g.P.slice(l - 1, k + l - 1))) }) }
  			function Qf(a, b, c, d, e) { return ac([d], function (f) { var g = p(f).next().value; return e.M(function (k) { k = k.map(function (z) { return z.value }).sort(function (z, A) { return A - z }).filter(function (z, A, D) { return D[A - 1] !== z }); for (var l = g.P.concat(), m = 0, q = k.length; m < q; ++m) { var u = k[m]; if (u > g.P.length || 0 >= u) throw Error("FOAY0001: subarray position out of bounds."); l.splice(u - 1, 1); } return C.m(new Yb(l)) }) }) } function Rf(a) { return B(a, 1) || B(a, 20) || B(a, 19) }
  			function Sf(a, b, c, d, e) { return 0 === d.length ? 0 !== e.length : 0 !== e.length && Ve(a, b, c, d[0], e[0]).next(0).value ? Sf(a, b, c, d.slice(1), e.slice(1)) : d[0].value !== d[0].value ? !0 : Rf(d[0].type) && 0 !== e.length && Rf(e[0].type) ? d[0].value < e[0].value : 0 === e.length ? !1 : d[0].value < e[0].value } function Tf(a, b, c, d) { d.sort(function (e, f) { return Ue(a, b, c, C.create(e), C.create(f)).next(0).value ? 0 : Sf(a, b, c, e, f) ? -1 : 1 }); return C.m(new Yb(d.map(function (e) { return function () { return C.create(e) } }))) }
  			function Uf(a, b) { return B(b.type, 62) ? b.P.reduce(function (c, d) { return d().M(function (e) { return e.reduce(Uf, c) }) }, a) : Pc([a, C.m(b)]) }
  			var Vf = [{ namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "size", j: [{ type: 62, g: 3 }], i: { type: 5, g: 3 }, callFunction: function (a, b, c, d) { return ac([d], function (e) { e = p(e).next().value; return C.m(w(e.P.length, 5)) }) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "get", j: [{ type: 62, g: 3 }, { type: 5, g: 3 }], i: { type: 59, g: 2 }, callFunction: Xb }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "put", j: [{ type: 62, g: 3 }, { type: 5, g: 3 }, { type: 59, g: 2 }], i: {
  					type: 62,
  					g: 3
  				}, callFunction: function (a, b, c, d, e, f) { return ac([e, d], function (g) { var k = p(g); g = k.next().value; k = k.next().value; g = g.value; if (0 >= g || g > k.P.length) throw Error("FOAY0001: array position out of bounds."); k = k.P.concat(); k.splice(g - 1, 1, yb(f)); return C.m(new Yb(k)) }) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "append", j: [{ type: 62, g: 3 }, { type: 59, g: 2 }], i: { type: 62, g: 3 }, callFunction: function (a, b, c, d, e) { return ac([d], function (f) { f = p(f).next().value.P.concat([yb(e)]); return C.m(new Yb(f)) }) } },
  			{ namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "subarray", j: [{ type: 62, g: 3 }, { type: 5, g: 3 }, { type: 5, g: 3 }], i: { type: 62, g: 3 }, callFunction: Pf }, { namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "subarray", j: [{ type: 62, g: 3 }, { type: 5, g: 3 }], i: { type: 62, g: 3 }, callFunction: function (a, b, c, d, e) { var f = C.m(w(d.first().value.length - e.first().value + 1, 5)); return Pf(a, b, c, d, e, f) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "remove", j: [{ type: 62, g: 3 }, {
  					type: 5,
  					g: 2
  				}], i: { type: 62, g: 3 }, callFunction: Qf
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "insert-before", j: [{ type: 62, g: 3 }, { type: 5, g: 3 }, { type: 59, g: 2 }], i: { type: 62, g: 3 }, callFunction: function (a, b, c, d, e, f) { return ac([d, e], function (g) { var k = p(g); g = k.next().value; k = k.next().value.value; if (k > g.P.length + 1 || 0 >= k) throw Error("FOAY0001: subarray position out of bounds."); g = g.P.concat(); g.splice(k - 1, 0, yb(f)); return C.m(new Yb(g)) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/array",
  				localName: "head", j: [{ type: 62, g: 3 }], i: { type: 59, g: 2 }, callFunction: function (a, b, c, d) { return Xb(a, b, c, d, C.m(w(1, 5))) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "tail", j: [{ type: 62, g: 3 }], i: { type: 59, g: 2 }, callFunction: function (a, b, c, d) { return Qf(a, b, c, d, C.m(w(1, 5))) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "reverse", j: [{ type: 62, g: 3 }], i: { type: 62, g: 3 }, callFunction: function (a, b, c, d) { return ac([d], function (e) { e = p(e).next().value; return C.m(new Yb(e.P.concat().reverse())) }) } },
  			{ namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "join", j: [{ type: 62, g: 2 }], i: { type: 62, g: 3 }, callFunction: function (a, b, c, d) { return d.M(function (e) { e = e.reduce(function (f, g) { return f.concat(g.P) }, []); return C.m(new Yb(e)) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "for-each", j: [{ type: 62, g: 3 }, { type: 60, g: 3 }], i: { type: 62, g: 3 }, callFunction: function (a, b, c, d, e) {
  					return ac([d, e], function (f) {
  						f = p(f); var g = f.next().value, k = f.next().value; if (1 !== k.v) throw Sc("The callback passed into array:for-each has a wrong arity.");
  						f = g.P.map(function (l) { return yb(k.value.call(void 0, a, b, c, Kf(k.o, [l()], b, "array:for-each")[0])) }); return C.m(new Yb(f))
  					})
  				}
  			}, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "filter", j: [{ type: 62, g: 3 }, { type: 60, g: 3 }], i: { type: 62, g: 3 }, callFunction: function (a, b, c, d, e) {
  					return ac([d, e], function (f) {
  						f = p(f); var g = f.next().value, k = f.next().value; if (1 !== k.v) throw Sc("The callback passed into array:filter has a wrong arity."); var l = g.P.map(function (u) {
  							u = Kf(k.o, [u()], b, "array:filter")[0]; var z =
  								k.value; return z(a, b, c, u)
  						}), m = [], q = !1; return C.create({ next: function () { if (q) return x; for (var u = 0, z = g.P.length; u < z; ++u) { var A = l[u].ha(); m[u] = A; } u = g.P.filter(function (D, F) { return m[F] }); q = !0; return y(new Yb(u)) } })
  					})
  				}
  			}, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "fold-left", j: [{ type: 62, g: 3 }, { type: 59, g: 2 }, { type: 60, g: 3 }], i: { type: 59, g: 2 }, callFunction: function (a, b, c, d, e, f) {
  					return ac([d, f], function (g) {
  						g = p(g); var k = g.next().value, l = g.next().value; if (2 !== l.v) throw Sc("The callback passed into array:fold-left has a wrong arity.");
  						return k.P.reduce(function (m, q) { q = Kf(l.o, [q()], b, "array:fold-left")[0]; return l.value.call(void 0, a, b, c, m, q) }, e)
  					})
  				}
  			}, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "fold-right", j: [{ type: 62, g: 3 }, { type: 59, g: 2 }, { type: 60, g: 3 }], i: { type: 59, g: 2 }, callFunction: function (a, b, c, d, e, f) {
  					return ac([d, f], function (g) {
  						g = p(g); var k = g.next().value, l = g.next().value; if (2 !== l.v) throw Sc("The callback passed into array:fold-right has a wrong arity."); return k.P.reduceRight(function (m, q) {
  							q = Kf(l.o,
  								[q()], b, "array:fold-right")[0]; return l.value.call(void 0, a, b, c, m, q)
  						}, e)
  					})
  				}
  			}, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "for-each-pair", j: [{ type: 62, g: 3 }, { type: 62, g: 3 }, { type: 60, g: 3 }], i: { type: 62, g: 3 }, callFunction: function (a, b, c, d, e, f) {
  					return ac([d, e, f], function (g) {
  						var k = p(g); g = k.next().value; var l = k.next().value; k = k.next().value; if (2 !== k.v) throw Sc("The callback passed into array:for-each-pair has a wrong arity."); for (var m = [], q = 0, u = Math.min(g.P.length, l.P.length); q < u; ++q) {
  							var z =
  								p(Kf(k.o, [g.P[q](), l.P[q]()], b, "array:for-each-pair")), A = z.next().value; z = z.next().value; m[q] = yb(k.value.call(void 0, a, b, c, A, z));
  						} return C.m(new Yb(m))
  					})
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "sort", j: [{ type: 62, g: 3 }], i: { type: 62, g: 3 }, callFunction: function (a, b, c, d) { return ac([d], function (e) { e = p(e).next().value.P.map(function (f) { return f().O() }); return Tf(a, b, c, e) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/array", localName: "flatten", j: [{ type: 59, g: 2 }],
  				i: { type: 59, g: 2 }, callFunction: function (a, b, c, d) { return d.M(function (e) { return e.reduce(Uf, C.empty()) }) }
  			}]; function K(a, b, c, d, e) { return e.G() ? e : C.m(Pd(e.first(), a)) }
  			var Wf = [{ namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "untypedAtomic", j: [{ type: 46, g: 0 }], i: { type: 19, g: 0 }, callFunction: K.bind(null, 19) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "error", j: [{ type: 46, g: 0 }], i: { type: 39, g: 0 }, callFunction: K.bind(null, 39) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "string", j: [{ type: 46, g: 0 }], i: { type: 1, g: 0 }, callFunction: K.bind(null, 1) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "boolean", j: [{ type: 46, g: 0 }], i: {
  					type: 0,
  					g: 0
  				}, callFunction: K.bind(null, 0)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "decimal", j: [{ type: 46, g: 0 }], i: { type: 4, g: 0 }, callFunction: K.bind(null, 4) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "float", j: [{ type: 46, g: 0 }], i: { type: 6, g: 0 }, callFunction: K.bind(null, 6) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "double", j: [{ type: 46, g: 0 }], i: { type: 3, g: 0 }, callFunction: K.bind(null, 3) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "duration", j: [{
  					type: 46,
  					g: 0
  				}], i: { type: 18, g: 0 }, callFunction: K.bind(null, 18)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "dateTime", j: [{ type: 46, g: 0 }], i: { type: 9, g: 0 }, callFunction: K.bind(null, 9) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "dateTimeStamp", j: [{ type: 46, g: 0 }], i: { type: 10, g: 0 }, callFunction: K.bind(null, 10) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "time", j: [{ type: 46, g: 0 }], i: { type: 8, g: 0 }, callFunction: K.bind(null, 8) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "date",
  				j: [{ type: 46, g: 0 }], i: { type: 7, g: 0 }, callFunction: K.bind(null, 7)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "gYearMonth", j: [{ type: 46, g: 0 }], i: { type: 11, g: 0 }, callFunction: K.bind(null, 11) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "gYear", j: [{ type: 46, g: 0 }], i: { type: 12, g: 0 }, callFunction: K.bind(null, 12) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "gMonthDay", j: [{ type: 46, g: 0 }], i: { type: 13, g: 0 }, callFunction: K.bind(null, 13) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema",
  				localName: "gDay", j: [{ type: 46, g: 0 }], i: { type: 15, g: 0 }, callFunction: K.bind(null, 15)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "gMonth", j: [{ type: 46, g: 0 }], i: { type: 14, g: 0 }, callFunction: K.bind(null, 14) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "hexBinary", j: [{ type: 46, g: 0 }], i: { type: 22, g: 0 }, callFunction: K.bind(null, 22) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "base64Binary", j: [{ type: 46, g: 0 }], i: { type: 21, g: 0 }, callFunction: K.bind(null, 21) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema",
  				localName: "QName", j: [{ type: 46, g: 0 }], i: { type: 23, g: 0 }, callFunction: function (a, b, c, d) {
  					if (d.G()) return d; a = d.first(); if (B(a.type, 2)) throw Error("XPTY0004: The provided QName is not a string-like value."); a = Pd(a, 1).value; a = ad(a, 23); if (!bd(a, 23)) throw Error("FORG0001: The provided QName is invalid."); if (!a.includes(":")) return c = c.aa(""), C.m(w(new zb("", c, a), 23)); d = p(a.split(":")); b = d.next().value; d = d.next().value; c = c.aa(b); if (!c) throw Error("FONS0004: The value " + a + " can not be cast to a QName. Did you mean to use fn:QName?");
  					return C.m(w(new zb(b, c, d), 23))
  				}
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "anyURI", j: [{ type: 46, g: 0 }], i: { type: 20, g: 0 }, callFunction: K.bind(null, 20) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "normalizedString", j: [{ type: 46, g: 0 }], i: { type: 48, g: 0 }, callFunction: K.bind(null, 48) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "token", j: [{ type: 46, g: 0 }], i: { type: 52, g: 0 }, callFunction: K.bind(null, 52) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "language",
  				j: [{ type: 46, g: 0 }], i: { type: 51, g: 0 }, callFunction: K.bind(null, 51)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "NMTOKEN", j: [{ type: 46, g: 0 }], i: { type: 50, g: 0 }, callFunction: K.bind(null, 50) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "NMTOKENS", j: [{ type: 46, g: 0 }], i: { type: 49, g: 2 }, callFunction: K.bind(null, 49) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "Name", j: [{ type: 46, g: 0 }], i: { type: 25, g: 0 }, callFunction: K.bind(null, 25) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema",
  				localName: "NCName", j: [{ type: 46, g: 0 }], i: { type: 24, g: 0 }, callFunction: K.bind(null, 24)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "ID", j: [{ type: 46, g: 0 }], i: { type: 42, g: 0 }, callFunction: K.bind(null, 42) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "IDREF", j: [{ type: 46, g: 0 }], i: { type: 41, g: 0 }, callFunction: K.bind(null, 41) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "IDREFS", j: [{ type: 46, g: 0 }], i: { type: 43, g: 2 }, callFunction: K.bind(null, 43) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema",
  				localName: "ENTITY", j: [{ type: 46, g: 0 }], i: { type: 26, g: 0 }, callFunction: K.bind(null, 26)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "ENTITIES", j: [{ type: 46, g: 0 }], i: { type: 40, g: 2 }, callFunction: K.bind(null, 40) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "integer", j: [{ type: 46, g: 0 }], i: { type: 5, g: 0 }, callFunction: K.bind(null, 5) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "nonPositiveInteger", j: [{ type: 46, g: 0 }], i: { type: 27, g: 0 }, callFunction: K.bind(null, 27) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema",
  				localName: "negativeInteger", j: [{ type: 46, g: 0 }], i: { type: 28, g: 0 }, callFunction: K.bind(null, 28)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "long", j: [{ type: 46, g: 0 }], i: { type: 31, g: 0 }, callFunction: K.bind(null, 31) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "int", j: [{ type: 46, g: 0 }], i: { type: 32, g: 0 }, callFunction: K.bind(null, 32) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "short", j: [{ type: 46, g: 0 }], i: { type: 33, g: 0 }, callFunction: K.bind(null, 33) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema",
  				localName: "byte", j: [{ type: 46, g: 0 }], i: { type: 34, g: 0 }, callFunction: K.bind(null, 34)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "nonNegativeInteger", j: [{ type: 46, g: 0 }], i: { type: 30, g: 0 }, callFunction: K.bind(null, 30) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "unsignedLong", j: [{ type: 46, g: 0 }], i: { type: 36, g: 0 }, callFunction: K.bind(null, 36) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "unsignedInt", j: [{ type: 46, g: 0 }], i: { type: 35, g: 0 }, callFunction: K.bind(null, 35) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema",
  				localName: "unsignedShort", j: [{ type: 46, g: 0 }], i: { type: 38, g: 0 }, callFunction: K.bind(null, 38)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "unsignedByte", j: [{ type: 46, g: 0 }], i: { type: 37, g: 0 }, callFunction: K.bind(null, 37) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "positiveInteger", j: [{ type: 46, g: 0 }], i: { type: 29, g: 0 }, callFunction: K.bind(null, 29) }, {
  				namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "yearMonthDuration", j: [{ type: 46, g: 0 }], i: { type: 16, g: 0 }, callFunction: K.bind(null,
  					16)
  			}, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "dayTimeDuration", j: [{ type: 46, g: 0 }], i: { type: 17, g: 0 }, callFunction: K.bind(null, 17) }, { namespaceURI: "http://www.w3.org/2001/XMLSchema", localName: "dateTimeStamp", j: [{ type: 46, g: 0 }], i: { type: 10, g: 0 }, callFunction: K.bind(null, 10) }]; function Xf(a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.getYear(), 5)) } function Yf(a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.getMonth(), 5)) } function Zf(a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.getDay(), 5)) } function $f(a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.getHours(), 5)) } function ag(a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.getMinutes(), 5)) } function bg(a, b, c, d) { d.G() || (a = C, b = a.m, d = d.first().value, d = b.call(a, w(d.B + d.sa, 4))); return d }
  			function cg(a, b, c, d) { return d.G() ? d : (a = d.first().value.Y) ? C.m(w(a, 17)) : C.empty() }
  			var dg = [{
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "dateTime", j: [{ type: 7, g: 0 }, { type: 8, g: 0 }], i: { type: 9, g: 0 }, callFunction: function (a, b, c, d, e) {
  					if (d.G()) return d; if (e.G()) return e; a = d.first().value; e = e.first().value; b = a.Y; c = e.Y; if (b || c) { if (!b || c) if (!b && c) b = c; else if (!fc(b, c)) throw Error("FORG0008: fn:dateTime: got a date and time value with different timezones."); } else b = null; return C.m(w(new rc(a.getYear(), a.getMonth(), a.getDay(), e.getHours(), e.getMinutes(), e.getSeconds(), e.sa,
  						b), 9))
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "year-from-dateTime", j: [{ type: 9, g: 0 }], i: { type: 5, g: 0 }, callFunction: Xf }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "month-from-dateTime", j: [{ type: 9, g: 0 }], i: { type: 5, g: 0 }, callFunction: Yf }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "day-from-dateTime", j: [{ type: 9, g: 0 }], i: { type: 5, g: 0 }, callFunction: Zf }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "hours-from-dateTime",
  				j: [{ type: 9, g: 0 }], i: { type: 5, g: 0 }, callFunction: $f
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "minutes-from-dateTime", j: [{ type: 9, g: 0 }], i: { type: 5, g: 0 }, callFunction: ag }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "seconds-from-dateTime", j: [{ type: 9, g: 0 }], i: { type: 4, g: 0 }, callFunction: bg }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "timezone-from-dateTime", j: [{ type: 9, g: 0 }], i: { type: 17, g: 0 }, callFunction: cg }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				localName: "year-from-date", j: [{ type: 7, g: 0 }], i: { type: 5, g: 0 }, callFunction: Xf
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "month-from-date", j: [{ type: 7, g: 0 }], i: { type: 5, g: 0 }, callFunction: Yf }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "day-from-date", j: [{ type: 7, g: 0 }], i: { type: 5, g: 0 }, callFunction: Zf }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "timezone-from-date", j: [{ type: 7, g: 0 }], i: { type: 17, g: 0 }, callFunction: cg }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				localName: "hours-from-time", j: [{ type: 8, g: 0 }], i: { type: 5, g: 0 }, callFunction: $f
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "minutes-from-time", j: [{ type: 8, g: 0 }], i: { type: 5, g: 0 }, callFunction: ag }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "seconds-from-time", j: [{ type: 8, g: 0 }], i: { type: 4, g: 0 }, callFunction: bg }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "timezone-from-time", j: [{ type: 8, g: 0 }], i: { type: 17, g: 0 }, callFunction: cg }]; function eg(a, b) {
  				var c = b.h, d = b.o, e = b.B; switch (a.node.nodeType) {
  					case 1: var f = d.createElementNS(a.node.namespaceURI, a.node.nodeName); c.getAllAttributes(a.node).forEach(function (g) { return e.setAttributeNS(f, g.namespaceURI, g.nodeName, g.value) }); d = p(Pb(c, a)); for (a = d.next(); !a.done; a = d.next())a = eg(a.value, b), e.insertBefore(f, a.node, null); return { node: f, F: null }; case 2: return b = d.createAttributeNS(a.node.namespaceURI, a.node.nodeName), b.value = Qb(c, a), { node: b, F: null }; case 4: return {
  						node: d.createCDATASection(Qb(c,
  							a)), F: null
  					}; case 8: return { node: d.createComment(Qb(c, a)), F: null }; case 9: d = d.createDocument(); c = p(Pb(c, a)); for (a = c.next(); !a.done; a = c.next())a = eg(a.value, b), e.insertBefore(d, a.node, null); return { node: d, F: null }; case 7: return { node: d.createProcessingInstruction(a.node.target, Qb(c, a)), F: null }; case 3: return { node: d.createTextNode(Qb(c, a)), F: null }
  				}
  			} function fg(a, b) {
  				var c = b.B, d = b.o, e = b.h; if (Kb(a.node)) switch (a.node.nodeType) {
  					case 2: return d = d.createAttributeNS(a.node.namespaceURI, a.node.nodeName), d.value = Qb(e, a), d; case 8: return d.createComment(Qb(e, a)); case 1: var f = a.node.prefix, g = a.node.localName, k = d.createElementNS(a.node.namespaceURI, f ? f + ":" + g : g); Pb(e, a).forEach(function (l) { l = fg(l, b); c.insertBefore(k, l, null); }); Nb(e, a).forEach(function (l) { c.setAttributeNS(k, l.node.namespaceURI, l.node.nodeName, Qb(e, l)); }); k.normalize(); return k; case 7: return d.createProcessingInstruction(a.node.target,
  						Qb(e, a)); case 3: return d.createTextNode(Qb(e, a))
  				} else return eg(a, b).node
  			} function gg(a, b, c) { var d = a; for (a = Vb(c, d); null !== a;) { if (2 === d.node.nodeType) b.push(d.node.nodeName); else { var e = Pb(c, a); b.push(e.findIndex(function (f) { return Sd(f, d) })); } d = a; a = Vb(c, d); } return d } function hg(a, b, c) { for (var d = {}; 0 < b.length;)d.$a = b.pop(), "string" === typeof d.$a ? a = Nb(c, a).find(function (e) { return function (f) { return f.node.nodeName === e.$a } }(d)) : a = Pb(c, a)[d.$a], d = { $a: d.$a }; return a.node }
  			function ig(a, b, c) { var d = a.node; if (!(Kb(d) || c || a.F)) return d; d = b.da; var e = []; if (c) return fg(a, b); a = gg(a, e, b.h); c = d.get(a.node); c || (c = { node: fg(a, b), F: null }, d.set(a.node, c)); return hg(c, e, b.h) } function jg(a, b, c, d, e) { return d.M(function (f) { for (var g = "", k = 0; k < f.length; k++) { var l = f[k], m = b.v && B(l.type, 53) ? b.v.serializeToString(ig(l.value, b, !1)) : Zc(C.m(l), b).map(function (q) { return Pd(q, 1) }).first().value; g += "{type: " + mb[l.type] + ", value: " + m + "}\n"; } void 0 !== e && (g += e.first().value); b.s.trace(g); return C.create(f) }) }
  			var kg = [{ j: [{ type: 59, g: 2 }], callFunction: jg, localName: "trace", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 } }, { j: [{ type: 59, g: 2 }, { type: 1, g: 3 }], callFunction: jg, localName: "trace", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 } }]; function lg(a, b, c, d, e) { a = void 0 === d || d.G() ? new zb("err", "http://www.w3.org/2005/xqt-errors", "FOER0000") : d.first().value; b = ""; void 0 === e || e.G() || (b = ": " + e.first().value); throw Error("" + a.localName + b); }
  			var mg = [{ namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "error", j: [], i: { type: 63, g: 3 }, callFunction: lg }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "error", j: [{ type: 23, g: 0 }], i: { type: 63, g: 3 }, callFunction: lg }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "error", j: [{ type: 23, g: 0 }, { type: 1, g: 3 }], i: { type: 63, g: 3 }, callFunction: lg }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "error", j: [{ type: 23, g: 0 }, { type: 1, g: 3 }, { type: 59, g: 2 }], i: {
  					type: 63,
  					g: 3
  				}, callFunction: function () { throw Error("Not implemented: Using an error object in error is not supported"); }
  			}]; function ng(a) { return "string" === typeof a ? a : (a = (new Gb).getChildNodes(a).find(function (b) { return 8 === b.nodeType })) ? a.data : "some expression" } function og(a, b) { a = Error.call(this, a); this.message = a.message; "stack" in a && (this.stack = a.stack); this.position = { end: { ja: b.end.ja, line: b.end.line, offset: b.end.offset }, start: { ja: b.start.ja, line: b.start.line, offset: b.start.offset } }; } v(og, Error); function pg(a, b) {
  				if (b instanceof Error) throw b; "string" !== typeof a && (a = ng(a)); var c = qg(b); a = a.replace("\r", "").split("\n"); var d = Math.floor(Math.log10(Math.min(c.end.line + 2, a.length))) + 1; a = a.reduce(function (e, f, g) {
  					var k = g + 1; if (2 < c.start.line - k || 2 < k - c.end.line) return e; g = Array(d).fill(" ", 0, Math.floor(Math.log10(k)) + 1 - d).join("") + k + ": "; e.push(g + f); if (k >= c.start.line && k <= c.end.line) {
  						var l = k < c.end.line ? f.length + g.length : c.end.ja - 1 + g.length; k = k > c.start.line ? g.length : c.start.ja - 1 + g.length; f = " ".repeat(g.length) +
  							Array.from(f.substring(0, k - g.length), function (m) { return "\t" === m ? "\t" : " " }).join("") + "^".repeat(l - k); e.push(f);
  					} return e
  				}, []); b = rg(b).join("\n"); throw new og(a.join("\n") + "\n\n" + b, c);
  			} var sg = Object.create(null); function tg(a, b) { for (var c = [], d = 0; d < a.length + 1; ++d)c[d] = []; return function k(f, g) { if (0 === f) return g; if (0 === g) return f; if (void 0 !== c[f][g]) return c[f][g]; var l = 0; a[f - 1] !== b[g - 1] && (l = 1); l = Math.min(k(f - 1, g) + 1, k(f, g - 1) + 1, k(f - 1, g - 1) + l); return c[f][g] = l }(a.length, b.length) }
  			function ug(a) {
  				var b = sg[a] ? sg[a] : Object.keys(sg).map(function (c) { return { name: c, Ab: tg(a, c.slice(c.lastIndexOf(":") + 1)) } }).sort(function (c, d) { return c.Ab - d.Ab }).slice(0, 5).filter(function (c) { return c.Ab < a.length / 2 }).reduce(function (c, d) { return c.concat(sg[d.name]) }, []).slice(0, 5); return b.length ? b.map(function (c) { return '"Q{' + c.namespaceURI + "}" + c.localName + " (" + c.j.map(function (d) { return 4 === d ? "..." : ob(d) }).join(", ") + ')"' }).reduce(function (c, d, e, f) { return 0 === e ? c + d : c + ((e !== f.length - 1 ? ", " : " or ") + d) },
  					"Did you mean ") + "?" : "No similar functions found."
  			} function vg(a, b, c) { var d = sg[a + ":" + b]; return d ? (d = d.find(function (e) { return e.j.some(function (f) { return 4 === f }) ? e.j.length - 1 <= c : e.j.length === c })) ? { j: d.j, arity: c, callFunction: d.callFunction, I: d.I, localName: b, namespaceURI: a, i: d.i } : null : null } function wg(a, b, c, d, e) { sg[a + ":" + b] || (sg[a + ":" + b] = []); sg[a + ":" + b].push({ j: c, arity: c.length, callFunction: e, I: !1, localName: b, namespaceURI: a, i: d }); } var xg = {}, yg = (xg.xml = "http://www.w3.org/XML/1998/namespace", xg.xs = "http://www.w3.org/2001/XMLSchema", xg.fn = "http://www.w3.org/2005/xpath-functions", xg.map = "http://www.w3.org/2005/xpath-functions/map", xg.array = "http://www.w3.org/2005/xpath-functions/array", xg.math = "http://www.w3.org/2005/xpath-functions/math", xg.fontoxpath = "http://fontoxml.com/fontoxpath", xg.local = "http://www.w3.org/2005/xquery-local-functions", xg); function zg(a, b, c, d) { this.Ha = [Object.create(null)]; this.Ia = Object.create(null); this.s = a; this.da = Object.keys(b).reduce(function (e, f) { if (void 0 === b[f]) return e; e[f] = "Q{}" + f + "[0]"; return e }, Object.create(null)); this.o = Object.create(null); this.h = Object.create(null); this.v = c; this.l = d; this.B = []; } zg.prototype.xa = function (a, b, c) { return vg(a, b, c) }; zg.prototype.ib = function (a, b) { if (a) return null; a = this.da[b]; this.o[b] || (this.o[b] = { name: b }); return a };
  			zg.prototype.Ua = function (a, b) { var c = this.l(a, b); if (c) this.B.push({ nc: a, arity: b, Lb: c }); else if ("" === a.prefix) { if (this.v) return { namespaceURI: this.v, localName: a.localName } } else if (b = this.aa(a.prefix, !0)) return { namespaceURI: b, localName: a.localName }; return c }; zg.prototype.aa = function (a, b) { if (void 0 !== b && !b) return null; if (yg[a]) return yg[a]; b = this.s(a); this.h[a] || (this.h[a] = { namespaceURI: b, prefix: a }); return void 0 !== b || a ? b : null }; function Ag(a) { return Error("XPTY0004: " + a) } function Bg(a, b) { a = 2 === a.node.nodeType ? a.node.nodeName + '="' + Qb(b, a) + '"' : a.node.outerHTML; return Error("XQTY0024: The node " + a + " follows a node that is not an attribute node or a namespace node.") } function Cg(a) { return Error('XQDY0044: The node name "' + a.Ca() + '" is invalid for a computed attribute constructor.') } function Dg() { return Error("XQST0045: Functions and variables may not be declared in one of the reserved namespace URIs.") }
  			function Eg() { return Error("XQST0060: Functions declared in a module or as an external function must reside in a namespace.") } function Fg() { return Error("XQST0066: A Prolog may contain at most one default function namespace declaration.") } function Gg() { return Error("XQST0070: The prefixes xml and xmlns may not be used in a namespace declaration or be bound to another namespaceURI.") }
  			function Hg(a) { return Error('XQDY0074: The value "' + a + '" of a name expressions cannot be converted to an expanded QName.') } function Ig(a) { return Error('XPST0081: The prefix "' + a + '" could not be resolved') } function Jg(a, b) { return "Q{" + (a || "") + "}" + b } function Kg(a, b) { for (var c = a.length - 1; 0 <= c; --c)if (b in a[c]) return a[c][b] } function Lg(a) { this.o = a; this.s = this.h = 0; this.B = [Object.create(null)]; this.l = Object.create(null); this.v = null; this.Ia = a && a.Ia; this.Ha = a && a.Ha; } function Of(a) { for (var b = new Lg(a.o), c = 0; c < a.h + 1; ++c)b.B = [Object.assign(Object.create(null), b.B[0], a.B[c])], b.Ha = [Object.assign(Object.create(null), b.Ha[0], a.Ha[c])], b.l = Object.assign(Object.create(null), a.l), b.Ia = a.Ia, b.v = a.v; return b }
  			function Mg(a) { a.s++; a.h++; a.B[a.h] = Object.create(null); a.Ha[a.h] = Object.create(null); } Lg.prototype.xa = function (a, b, c, d) { d = void 0 === d ? !1 : d; var e = this.l[Jg(a, b) + "~" + c]; return !e || d && e.Eb ? null === this.o ? null : this.o.xa(a, b, c, d) : e }; Lg.prototype.ib = function (a, b) { var c = Kg(this.Ha, Jg(a, b)); return c ? c : null === this.o ? null : this.o.ib(a, b) }; function Ng(a, b, c) { return (a = a.Ia[Jg(b, c)]) ? a : null }
  			function Og(a, b, c, d, e) { d = Jg(b, c) + "~" + d; if (a.l[d]) throw Error('XQST0049: The function or variable "Q{' + b + "}" + c + '" is declared more than once.'); a.l[d] = e; } function Pg(a, b, c) { a.B[a.h][b] = c; } function Qg(a, b, c) { b = Jg(b || "", c); return a.Ha[a.h][b] = b + "[" + a.s + "]" } function Rg(a, b, c, d) { a.Ia[Jg(b || "", c) + "[" + a.s + "]"] = d; } function Sg(a) { a.B.length = a.h; a.Ha.length = a.h; a.h--; }
  			Lg.prototype.Ua = function (a, b) { var c = a.prefix, d = a.localName; return "" === c && this.v ? { localName: d, namespaceURI: this.v } : c && (c = this.aa(c, !1)) ? { localName: d, namespaceURI: c } : null === this.o ? null : this.o.Ua(a, b) }; Lg.prototype.aa = function (a, b) { var c = Kg(this.B, a || ""); return void 0 === c ? null === this.o ? void 0 : this.o.aa(a || "", void 0 === b ? !0 : b) : c }; function L(a, b) { "*" === b || Array.isArray(b) || (b = [b]); for (var c = 1; c < a.length; ++c)if (Array.isArray(a[c])) { var d = a[c]; if ("*" === b || b.includes(d[0])) return d } return null } function Tg(a) { return 2 > a.length ? "" : "object" === typeof a[1] ? a[2] || "" : a[1] || "" } function M(a, b) { if (!Array.isArray(a)) return null; a = a[1]; return "object" !== typeof a || Array.isArray(a) ? null : b in a ? a[b] : null } function N(a, b) { return b.reduce(L, a) }
  			function O(a, b) { for (var c = [], d = 1; d < a.length; ++d)if (Array.isArray(a[d])) { var e = a[d]; "*" !== b && e[0] !== b || c.push(e); } return c } function Ug(a) { return { localName: Tg(a), namespaceURI: M(a, "URI"), prefix: M(a, "prefix") } }
  			function Vg(a) {
  				function b(f) {
  					switch (f[0]) {
  						case "documentTest": return 55; case "elementTest": return 54; case "attributeTest": return 47; case "piTest": return 57; case "commentTest": return 58; case "textTest": return 56; case "anyKindTest": return 53; case "anyItemType": return 59; case "anyFunctionTest": case "functionTest": case "typedFunctionTest": return 60; case "anyMapTest": case "typedMapTest": return 61; case "anyArrayTest": case "typedArrayTest": return 62; case "atomicType": return pb([M(f, "prefix"), Tg(f)].join(":"));
  						case "parenthesizedItemType": return b(L(f, "*")); default: throw Error('Type declaration "' + L(c, "*")[0] + '" is not supported.');
  					}
  				} var c = L(a, "typeDeclaration"); if (!c || L(c, "voidSequenceType")) return { type: 59, g: 2 }; a = { type: b(L(c, "*")), g: 3 }; var d = null, e = L(c, "occurrenceIndicator"); e && (d = Tg(e)); switch (d) { case "*": return a.g = 2, a; case "?": return a.g = 0, a; case "+": return a.g = 1, a; case "": case null: return a }
  			}
  			function P(a, b, c) { if ("object" !== typeof a[1] || Array.isArray(a[1])) { var d = {}; d[b] = c; a.splice(1, 0, d); } else a[1][b] = c; } function Wg(a) { var b = { type: 62, g: 3 }; P(a, "type", b); return b } function Xg(a, b) { if (!b || !b.ia) return { type: 59, g: 2 }; var c = L(a, "EQName"); if (!c) return { type: 59, g: 2 }; var d = Ug(c); c = d.localName; var e = d.prefix; d = O(L(a, "arguments"), "*"); c = b.ia.Ua({ localName: c, prefix: e }, d.length); if (!c) return { type: 59, g: 2 }; b = b.ia.xa(c.namespaceURI, c.localName, d.length + 1); if (!b) return { type: 59, g: 2 }; 59 !== b.i.type && P(a, "type", b.i); return b.i } function Q(a, b, c) { return (a << 20) + (b << 12) + (c.charCodeAt(0) << 8) + c.charCodeAt(1) }
  			var Yg = {}, Zg = (Yg[Q(2, 2, "idivOp")] = 5, Yg[Q(16, 16, "addOp")] = 16, Yg[Q(16, 16, "subtractOp")] = 16, Yg[Q(16, 16, "divOp")] = 4, Yg[Q(16, 2, "multiplyOp")] = 16, Yg[Q(16, 2, "divOp")] = 16, Yg[Q(2, 16, "multiplyOp")] = 16, Yg[Q(17, 17, "addOp")] = 17, Yg[Q(17, 17, "subtractOp")] = 17, Yg[Q(17, 17, "divOp")] = 4, Yg[Q(17, 2, "multiplyOp")] = 17, Yg[Q(17, 2, "divOp")] = 17, Yg[Q(2, 17, "multiplyOp")] = 17, Yg[Q(9, 9, "subtractOp")] = 17, Yg[Q(7, 7, "subtractOp")] = 17, Yg[Q(8, 8, "subtractOp")] = 17, Yg[Q(9, 16, "addOp")] = 9, Yg[Q(9, 16, "subtractOp")] = 9, Yg[Q(9, 17, "addOp")] = 9, Yg[Q(9,
  				17, "subtractOp")] = 9, Yg[Q(7, 16, "addOp")] = 7, Yg[Q(7, 16, "subtractOp")] = 7, Yg[Q(7, 17, "addOp")] = 7, Yg[Q(7, 17, "subtractOp")] = 7, Yg[Q(8, 17, "addOp")] = 8, Yg[Q(8, 17, "subtractOp")] = 8, Yg[Q(9, 16, "addOp")] = 9, Yg[Q(9, 16, "subtractOp")] = 9, Yg[Q(9, 17, "addOp")] = 9, Yg[Q(9, 17, "subtractOp")] = 9, Yg[Q(7, 17, "addOp")] = 7, Yg[Q(7, 17, "subtractOp")] = 7, Yg[Q(7, 16, "addOp")] = 7, Yg[Q(7, 16, "subtractOp")] = 7, Yg[Q(8, 17, "addOp")] = 8, Yg[Q(8, 17, "subtractOp")] = 8, Yg), $g = {}, ah = ($g[Q(2, 2, "addOp")] = function (a, b) { return a + b }, $g[Q(2, 2, "subtractOp")] = function (a,
  					b) { return a - b }, $g[Q(2, 2, "multiplyOp")] = function (a, b) { return a * b }, $g[Q(2, 2, "divOp")] = function (a, b) { return a / b }, $g[Q(2, 2, "modOp")] = function (a, b) { return a % b }, $g[Q(2, 2, "idivOp")] = function (a, b) { return Math.trunc(a / b) }, $g[Q(16, 16, "addOp")] = function (a, b) { return new od(a.ga + b.ga) }, $g[Q(16, 16, "subtractOp")] = function (a, b) { return new od(a.ga - b.ga) }, $g[Q(16, 16, "divOp")] = function (a, b) { return a.ga / b.ga }, $g[Q(16, 2, "multiplyOp")] = rd, $g[Q(16, 2, "divOp")] = function (a, b) {
  						if (isNaN(b)) throw Error("FOCA0005: Cannot divide xs:yearMonthDuration by NaN");
  						a = Math.round(a.ga / b); if (a > Number.MAX_SAFE_INTEGER || !Number.isFinite(a)) throw Error("FODT0002: Value overflow while dividing xs:yearMonthDuration"); return new od(a < Number.MIN_SAFE_INTEGER || 0 === a ? 0 : a)
  					}, $g[Q(2, 16, "multiplyOp")] = function (a, b) { return rd(b, a) }, $g[Q(17, 17, "addOp")] = function (a, b) { return new gc(a.ea + b.ea) }, $g[Q(17, 17, "subtractOp")] = function (a, b) { return new gc(a.ea - b.ea) }, $g[Q(17, 17, "divOp")] = function (a, b) { if (0 === b.ea) throw Error("FOAR0001: Division by 0"); return a.ea / b.ea }, $g[Q(17, 2, "multiplyOp")] =
  					lc, $g[Q(17, 2, "divOp")] = function (a, b) { if (isNaN(b)) throw Error("FOCA0005: Cannot divide xs:dayTimeDuration by NaN"); a = a.ea / b; if (a > Number.MAX_SAFE_INTEGER || !Number.isFinite(a)) throw Error("FODT0002: Value overflow while dividing xs:dayTimeDuration"); return new gc(a < Number.MIN_SAFE_INTEGER || Object.is(-0, a) ? 0 : a) }, $g[Q(2, 17, "multiplyOp")] = function (a, b) { return lc(b, a) }, $g[Q(9, 9, "subtractOp")] = xc, $g[Q(7, 7, "subtractOp")] = xc, $g[Q(8, 8, "subtractOp")] = xc, $g[Q(9, 16, "addOp")] = yc, $g[Q(9, 16, "subtractOp")] = zc, $g[Q(9,
  						17, "addOp")] = yc, $g[Q(9, 17, "subtractOp")] = zc, $g[Q(7, 16, "addOp")] = yc, $g[Q(7, 16, "subtractOp")] = zc, $g[Q(7, 17, "addOp")] = yc, $g[Q(7, 17, "subtractOp")] = zc, $g[Q(8, 17, "addOp")] = yc, $g[Q(8, 17, "subtractOp")] = zc, $g[Q(9, 16, "addOp")] = yc, $g[Q(9, 16, "subtractOp")] = zc, $g[Q(9, 17, "addOp")] = yc, $g[Q(9, 17, "subtractOp")] = zc, $g[Q(7, 17, "addOp")] = yc, $g[Q(7, 17, "subtractOp")] = zc, $g[Q(7, 16, "addOp")] = yc, $g[Q(7, 16, "subtractOp")] = zc, $g[Q(8, 17, "addOp")] = yc, $g[Q(8, 17, "subtractOp")] = zc, $g); function bh(a, b) { return B(a, 5) && B(b, 5) ? 5 : B(a, 4) && B(b, 4) ? 4 : B(a, 6) && B(b, 6) ? 6 : 3 } var ch = [2, 16, 17, 9, 7, 8];
  			function dh(a, b, c) {
  				function d(D, F) { return { U: e ? e(D) : D, V: f ? f(F) : F } } var e = null, f = null; B(b, 19) && (e = function (D) { return Pd(D, 3) }, b = 3); B(c, 19) && (f = function (D) { return Pd(D, 3) }, c = 3); var g = ch.filter(function (D) { return B(b, D) }), k = ch.filter(function (D) { return B(c, D) }); if (g.includes(2) && k.includes(2)) { var l = ah[Q(2, 2, a)], m = Zg[Q(2, 2, a)]; m || (m = bh(b, c)); "divOp" === a && 5 === m && (m = 4); return "idivOp" === a ? eh(d, l)[0] : function (D, F) { D = d(D, F); return w(l(D.U.value, D.V.value), m) } } g = p(g); for (var q = g.next(); !q.done; q = g.next()) {
  					q =
  					q.value; for (var u = {}, z = p(k), A = z.next(); !A.done; u = { nb: u.nb, qb: u.qb }, A = z.next())if (A = A.value, u.nb = ah[Q(q, A, a)], u.qb = Zg[Q(q, A, a)], u.nb && void 0 !== u.qb) return function (D) { return function (F, J) { F = d(F, J); return w(D.nb(F.U.value, F.V.value), D.qb) } }(u)
  				}
  			}
  			function fh(a, b, c) {
  				function d(u, z) { return { U: f ? f(u) : u, V: g ? g(z) : z } } var e = [2, 53, 59, 46, 47]; if (e.includes(b) || e.includes(c)) return 2; var f = null, g = null; B(b, 19) && (f = function (u) { return Pd(u, 3) }, b = 3); B(c, 19) && (g = function (u) { return Pd(u, 3) }, c = 3); var k = ch.filter(function (u) { return B(b, u) }); e = ch.filter(function (u) { return B(c, u) }); if (k.includes(2) && e.includes(2)) return e = Zg[Q(2, 2, a)], void 0 === e && (e = bh(b, c)), "divOp" === a && 5 === e && (e = 4), "idivOp" === a ? eh(d, function (u, z) { return Math.trunc(u / z) })[1] : e; k = p(k); for (var l =
  					k.next(); !l.done; l = k.next()) { l = l.value; for (var m = p(e), q = m.next(); !q.done; q = m.next())if (q = Zg[Q(l, q.value, a)], void 0 !== q) return q }
  			}
  			function eh(a, b) { return [function (c, d) { d = a(c, d); c = d.U; d = d.V; if (0 === d.value) throw Error("FOAR0001: Divisor of idiv operator cannot be (-)0"); if (Number.isNaN(c.value) || Number.isNaN(d.value) || !Number.isFinite(c.value)) throw Error("FOAR0002: One of the operands of idiv is NaN or the first operand is (-)INF"); return Number.isFinite(c.value) && !Number.isFinite(d.value) ? w(0, 5) : w(b(c.value, d.value), 5) }, 5] } var gh = Object.create(null);
  			function mh(a, b, c, d, e) { H.call(this, b.o.add(c.o), [b, c], { C: !1 }, !1, d); this.A = b; this.K = c; this.l = a; this.s = e; } v(mh, H);
  			mh.prototype.h = function (a, b) {
  				var c = this; return Zc(I(this.A, a, b), b).M(function (d) {
  					return 0 === d.length ? C.empty() : Zc(I(c.K, a, b), b).M(function (e) {
  						if (0 === e.length) return C.empty(); if (1 < d.length || 1 < e.length) throw Error('XPTY0004: the operands of the "' + c.l + '" operator should be empty or singleton.'); var f = d[0]; e = e[0]; if (c.s && c.type) return C.m(c.s(f, e)); var g = f.type; var k = e.type, l = c.l, m = g + "~" + k + "~" + l, q = gh[m]; q || (q = gh[m] = dh(l, g, k)); g = q; if (!g) throw Error("XPTY0004: " + c.l + " not available for types " + mb[f.type] +
  							" and " + mb[e.type]); return C.m(g(f, e))
  					})
  				})
  			}; function nh(a, b) {
  				for (var c = oh, d = !1, e = 1; e < a.length; e++)switch (a[e][0]) {
  					case "letClause": ph(b); var f = a[e], g = b, k = c, l = N(f, ["letClauseItem", "typedVariableBinding", "varName"]); l = Ug(l); f = N(f, ["letClauseItem", "letExpr"]); k = k(f[1], g); qh(g, l.localName, k); break; case "forClause": d = !0; ph(b); rh(a[e], b, c); break; case "whereClause": ph(b); g = a[e]; c(g, b); P(g, "type", { type: 0, g: 3 }); break; case "orderByClause": ph(b); break; case "returnClause": e = a[e]; g = c; c = N(e, ["*"]); b = g(c, b); P(c, "type", b); P(e, "type", b); c = b; if (!c) return {
  						type: 59,
  						g: 2
  					}; d && (c = { type: c.type, g: 2 }); 59 !== c.type && P(a, "type", c); return c; default: c = c(a[e], b); if (!c) return { type: 59, g: 2 }; d && (c = { type: c.type, g: 2 }); 59 !== c.type && P(a, "type", c); return c
  				}if (0 < b.h) b.h--, b.o.pop(), b.v.pop(); else throw Error("Variable scope out of bound");
  			}
  			function rh(a, b, c) { var d = Ug(N(a, ["forClauseItem", "typedVariableBinding", "varName"])); if (a = N(a, ["forClauseItem", "forExpr", "sequenceExpr"])) a = O(a, "*").map(function (e) { return c(e, b) }), a.includes(void 0) || a.includes(null) || (a = sh(a), 1 === a.length && qh(b, d.localName, a[0])); } function sh(a) { return a.filter(function (b, c, d) { return d.findIndex(function (e) { return e.type === b.type && e.g === b.g }) === c }) } function th(a, b) { if (!b || !b.ia) return { type: 59, g: 2 }; var c = L(a, "functionName"), d = Ug(c), e = d.localName, f = d.prefix, g = d.namespaceURI; d = O(L(a, "arguments"), "*"); if (null === g) { f = b.ia.Ua({ localName: e, prefix: f }, d.length); if (!f) return { type: 59, g: 2 }; e = f.localName; g = f.namespaceURI; P(c, "URI", g); c[2] = e; } b = b.ia.xa(g, e, d.length); if (!b || 63 === b.i.type) return { type: 59, g: 2 }; 59 !== b.i.type && P(a, "type", b.i); return b.i } function uh(a) { var b = { type: 61, g: 3 }; P(a, "type", b); return b } function vh(a, b) { if (!b || !b.ia) return { type: 59, g: 2 }; var c = L(a, "functionName"), d = Ug(c), e = d.localName, f = d.namespaceURI, g = d.prefix; d = Number(N(a, ["integerConstantExpr", "value"])[1]); if (!f) { f = b.ia.Ua({ localName: e, prefix: g }, d); if (!f) return { type: 59, g: 2 }; e = f.localName; f = f.namespaceURI; P(c, "URI", f); } b = b.ia.xa(f, e, d) || null; if (!b) return { type: 59, g: 2 }; 59 !== b.i.type && 63 !== b.i.type && P(a, "type", b.i); return b.i } function wh(a, b) {
  				var c = O(a, "stepExpr"); if (!c) return { type: 59, g: 2 }; c = p(c); for (var d = c.next(); !d.done; d = c.next()) {
  					var e = d.value; d = b; var f = null; if (e) {
  						var g = O(e, "*"), k = ""; g = p(g); for (var l = g.next(); !l.done; l = g.next())switch (l = l.value, l[0]) {
  							case "filterExpr": f = M(N(l, ["*"]), "type"); break; case "xpathAxis": k = l[1]; b: {
  								switch (k) {
  									case "attribute": f = { type: 47, g: 2 }; break b; case "child": case "decendant": case "self": case "descendant-or-self": case "following-sibling": case "following": case "namespace": case "parent": case "ancestor": case "preceding-sibling": case "preceding": case "ancestor-or-self": f =
  										{ type: 53, g: 2 }; break b
  								}f = void 0;
  							} break; case "nameTest": var m = Ug(l); if (null !== m.namespaceURI) break; if ("attribute" === k && !m.prefix) break; m = d.aa(m.prefix || ""); void 0 !== m && P(l, "URI", m); break; case "lookup": f = { type: 59, g: 2 };
  						}f && 59 !== f.type && P(e, "type", f);
  					} e = M(e, "type");
  				} e && 59 !== e.type && P(a, "type", e); return e
  			} function xh(a) { var b = { type: 0, g: 3 }; P(a, "type", b); return b } function yh(a, b, c) { 0 === b ? b = { type: 53, g: 2 } : 1 === b ? b = c[0] : c.includes(void 0) || c.includes(null) ? b = { type: 59, g: 2 } : (b = sh(c), b = 1 < b.length ? { type: 59, g: 2 } : { type: b[0].type, g: 2 }); b && 59 !== b.type && P(a, "type", b); return b } function zh(a, b, c, d) { if (!b || c.includes(void 0)) return { type: 59, g: 2 }; for (var e = O(a, "typeswitchExprCaseClause"), f = 0; f < c.length; f++) { var g = L(e[f], "*"); switch (g[0]) { case "sequenceType": if (g = Ah(g, b, c[f])) return 59 !== g.type && P(a, "type", g), g; continue; case "sequenceTypeUnion": for (d = O(g, "*"), e = 0; 2 > e; e++)if (g = Ah(d[e], b, c[f])) return 59 !== g.type && P(a, "type", g), g; default: return { type: 59, g: 2 } } } 59 !== d.type && P(a, "type", d); return d }
  			function Ah(a, b, c) { var d = O(a, "*"), e = L(a, "atomicType"); if (!e) return { type: 59, g: 2 }; if (pb(M(e, "prefix") + ":" + e[2]) === b.type) if (1 === d.length) { if (3 === b.g) return c } else if (a = L(a, "occurrenceIndicator")[1], b.g === rb(a)) return c } function Bh(a, b) { oh(a, b); } function oh(a, b) { var c = Ch.get(a[0]); if (c) return c(a, b); for (c = 1; c < a.length; c++)a[c] && oh(a[c], b); } function Dh(a, b) { var c = oh(L(a, "firstOperand")[1], b), d = oh(L(a, "secondOperand")[1], b); var e = a[0]; if (c && d) if (b = fh(e, c.type, d.type)) c = { type: b, g: c.g }, 2 !== b && 59 !== b && P(a, "type", c), a = c; else throw Error("XPTY0004: " + e + " not available for types " + ob(c) + " and " + ob(d)); else a = { type: 2, g: 3 }; return a }
  			function Eh(a, b) { oh(L(a, "firstOperand")[1], b); oh(L(a, "secondOperand")[1], b); a: { switch (a[0]) { case "orOp": b = { type: 0, g: 3 }; P(a, "type", b); a = b; break a; case "andOp": b = { type: 0, g: 3 }; P(a, "type", b); a = b; break a }a = void 0; } return a }
  			function Fh(a, b) { oh(L(a, "firstOperand")[1], b); oh(L(a, "secondOperand")[1], b); a: { switch (a[0]) { case "unionOp": b = { type: 53, g: 2 }; P(a, "type", b); a = b; break a; case "intersectOp": b = { type: 53, g: 2 }; P(a, "type", b); a = b; break a; case "exceptOp": b = { type: 53, g: 2 }; P(a, "type", b); a = b; break a }a = void 0; } return a } function Gh(a, b) { oh(L(a, "firstOperand")[1], b); oh(L(a, "secondOperand")[1], b); b = { type: 0, g: 3 }; P(a, "type", b); return b }
  			function Hh(a, b) { oh(L(a, "firstOperand")[1], b); oh(L(a, "secondOperand")[1], b); b = M(N(a, ["firstOperand", "*"]), "type"); var c = M(N(a, ["secondOperand", "*"]), "type"); b = { type: 0, g: ed(b) || ed(c) ? 0 : 3 }; P(a, "type", b); return b } function Ih(a, b) { oh(L(a, "firstOperand")[1], b); oh(L(a, "secondOperand")[1], b); b = M(N(a, ["firstOperand", "*"]), "type"); var c = M(N(a, ["secondOperand", "*"]), "type"); b = { type: 0, g: ed(b) || ed(c) ? 0 : 3 }; P(a, "type", b); return b }
  			var Ch = new Map([["unaryMinusOp", function (a, b) { b = oh(L(a, "operand")[1], b); b ? B(b.type, 2) ? (b = { type: b.type, g: b.g }, P(a, "type", b), a = b) : (b = { type: 3, g: 3 }, P(a, "type", b), a = b) : (b = { type: 2, g: 2 }, P(a, "type", b), a = b); return a }], ["unaryPlusOp", function (a, b) { b = oh(L(a, "operand")[1], b); b ? B(b.type, 2) ? (b = { type: b.type, g: b.g }, P(a, "type", b), a = b) : (b = { type: 3, g: 3 }, P(a, "type", b), a = b) : (b = { type: 2, g: 2 }, P(a, "type", b), a = b); return a }], ["addOp", Dh], ["subtractOp", Dh], ["divOp", Dh], ["idivOp", Dh], ["modOp", Dh], ["multiplyOp", Dh], ["andOp", Eh],
  			["orOp", Eh], ["sequenceExpr", function (a, b) { var c = O(a, "*"), d = c.map(function (e) { return oh(e, b) }); return yh(a, c.length, d) }], ["unionOp", Fh], ["intersectOp", Fh], ["exceptOp", Fh], ["stringConcatenateOp", function (a, b) { oh(L(a, "firstOperand")[1], b); oh(L(a, "secondOperand")[1], b); b = { type: 1, g: 3 }; P(a, "type", b); return b }], ["rangeSequenceExpr", function (a, b) { oh(L(a, "startExpr")[1], b); oh(L(a, "endExpr")[1], b); b = { type: 5, g: 1 }; P(a, "type", b); return b }], ["equalOp", Gh], ["notEqualOp", Gh], ["lessThanOrEqualOp", Gh], ["lessThanOp",
  				Gh], ["greaterThanOrEqualOp", Gh], ["greaterThanOp", Gh], ["eqOp", Hh], ["neOp", Hh], ["ltOp", Hh], ["leOp", Hh], ["gtOp", Hh], ["geOp", Hh], ["isOp", Ih], ["nodeBeforeOp", Ih], ["nodeAfterOp", Ih], ["pathExpr", function (a, b) { var c = L(a, "rootExpr"); c && c[1] && oh(c[1], b); O(a, "stepExpr").map(function (d) { return oh(d, b) }); return wh(a, b) }], ["contextItemExpr", function () { return { type: 59, g: 2 } }], ["ifThenElseExpr", function (a, b) {
  					oh(L(L(a, "ifClause"), "*"), b); var c = oh(L(L(a, "thenClause"), "*"), b); b = oh(L(L(a, "elseClause"), "*"), b); c && b ? c.type ===
  						b.type && c.g === b.g ? (59 !== c.type && P(a, "type", c), a = c) : a = { type: 59, g: 2 } : a = { type: 59, g: 2 }; return a
  				}], ["instanceOfExpr", function (a, b) { oh(L(a, "argExpr"), b); oh(L(a, "sequenceType"), b); b = { type: 0, g: 3 }; P(a, "type", b); return b }], ["integerConstantExpr", function (a) { var b = { type: 5, g: 3 }; P(a, "type", b); return b }], ["doubleConstantExpr", function (a) { var b = { type: 3, g: 3 }; P(a, "type", b); return b }], ["decimalConstantExpr", function (a) { var b = { type: 4, g: 3 }; P(a, "type", b); return b }], ["stringConstantExpr", function (a) {
  					var b = { type: 1, g: 3 };
  					P(a, "type", b); return b
  				}], ["functionCallExpr", function (a, b) { var c = L(a, "arguments"); O(c, "*").map(function (d) { return oh(d, b) }); return th(a, b) }], ["arrowExpr", function (a, b) { oh(L(a, "argExpr")[1], b); return Xg(a, b) }], ["dynamicFunctionInvocationExpr", function (a, b) { oh(N(a, ["functionItem", "*"]), b); (a = L(a, "arguments")) && oh(a, b); return { type: 59, g: 2 } }], ["namedFunctionRef", function (a, b) { return vh(a, b) }], ["inlineFunctionExpr", function (a, b) { oh(L(a, "functionBody")[1], b); b = { type: 60, g: 3 }; P(a, "type", b); return b }], ["castExpr",
  				function (a) { var b = N(a, ["singleType", "atomicType"]); b = { type: pb(M(b, "prefix") + ":" + b[2]), g: 3 }; 59 !== b.type && P(a, "type", b); return b }], ["castableExpr", function (a) { var b = { type: 0, g: 3 }; P(a, "type", b); return b }], ["simpleMapExpr", function (a, b) { for (var c = O(a, "pathExpr"), d, e = 0; e < c.length; e++)d = oh(c[e], b); void 0 !== d && null !== d ? ((b = { type: d.type, g: 2 }, 59 !== b.type) && P(a, "type", b), a = b) : a = { type: 59, g: 2 }; return a }], ["mapConstructor", function (a, b) {
  					O(a, "mapConstructorEntry").map(function (c) {
  						return {
  							key: oh(N(c, ["mapKeyExpr",
  								"*"]), b), value: oh(N(c, ["mapValueExpr", "*"]), b)
  						}
  					}); return uh(a)
  				}], ["arrayConstructor", function (a, b) { O(L(a, "*"), "arrayElem").map(function (c) { return oh(c, b) }); return Wg(a) }], ["unaryLookup", function (a) { L(a, "NCName"); return { type: 59, g: 2 } }], ["typeswitchExpr", function (a, b) { var c = oh(L(a, "argExpr")[1], b), d = O(a, "typeswitchExprCaseClause").map(function (f) { return oh(N(f, ["resultExpr"])[1], b) }), e = oh(N(a, ["typeswitchExprDefaultClause", "resultExpr"])[1], b); return zh(a, c, d, e) }], ["quantifiedExpr", function (a, b) {
  					O(a,
  						"*").map(function (c) { return oh(c, b) }); return xh(a)
  				}], ["x:stackTrace", function (a, b) { a = O(a, "*"); return oh(a[0], b) }], ["queryBody", function (a, b) { return oh(a[1], b) }], ["flworExpr", function (a, b) { return nh(a, b) }], ["varRef", function (a, b) { var c = Ug(L(a, "name")), d; a: { for (d = b.h; 0 <= d; d--) { var e = b.o[d][c.localName]; if (e) { d = e; break a } } d = void 0; } d && 59 !== d.type && P(a, "type", d); null === c.namespaceURI && (b = b.aa(c.prefix), void 0 !== b && P(a, "URI", b)); return d }]]); function Jh(a) { this.h = 0; this.ia = a; this.o = [{}]; this.v = [{}]; } function qh(a, b, c) { if (a.o[a.h][b]) throw Error("Another variable of in the scope " + a.h + " with the same name " + b + " already exists"); a.o[a.h][b] = c; } function ph(a) { a.h++; a.o.push({}); a.v.push({}); } Jh.prototype.aa = function (a) { for (var b = this.h; 0 <= b; b--) { var c = this.v[b][a]; if (void 0 !== c) return c } return this.ia ? this.ia.aa(a) : void 0 }; function Kh(a, b) { var c = {}; H.call(this, new Hf((c.external = 1, c)), a, { C: a.every(function (d) { return d.C }) }, !1, b); this.l = a; } v(Kh, H); Kh.prototype.h = function (a, b) { return 0 === this.l.length ? C.m(new Yb([])) : I(this.l[0], a, b).M(function (c) { return C.m(new Yb(c.map(function (d) { return yb(C.m(d)) }))) }) }; function Lh(a, b) { var c = {}; H.call(this, new Hf((c.external = 1, c)), a, { C: a.every(function (d) { return d.C }) }, !1, b); this.l = a; } v(Lh, H); Lh.prototype.h = function (a, b) { return C.m(new Yb(this.l.map(function (c) { return yb(I(c, a, b)) }))) }; function Mh(a) { if (null === a) throw Rc("context is absent, it needs to be present to use axes."); if (!B(a.type, 53)) throw Error("XPTY0020: Axes can only be applied to nodes."); return a.value } function Nh(a, b, c) { var d = b; return { next: function () { if (!d) return x; var e = d; d = Vb(a, e, c); return y($b(e)) } } } function Oh(a, b) { b = b || { Ra: !1 }; H.call(this, a.o, [a], { R: "reverse-sorted", X: !1, subtree: !1, C: !1 }); this.l = a; this.s = !!b.Ra; } v(Oh, H); Oh.prototype.h = function (a, b) { var c = this; b = b.h; a = Mh(a.N); var d = this.l.B(); d = d && (d.startsWith("name-") || "type-1" === d) ? "type-1" : null; return C.create(Nh(b, this.s ? a : Vb(b, a, d), d)).filter(function (e) { return c.l.l(e) }) }; var Ph = new Map([["type-1-or-type-2", ["name", "type-1", "type-2"]], ["type-1", ["name"]], ["type-2", ["name"]]]); function Qh(a, b) { if (null === a) return b; if (null === b || a === b) return a; var c = a.startsWith("name-") ? "name" : a, d = b.startsWith("name-") ? "name" : b, e = Ph.get(c); if (void 0 !== e && e.includes(d)) return b; b = Ph.get(d); return void 0 !== b && b.includes(c) ? a : "empty" } function Rh(a, b) { var c = {}; H.call(this, new Hf((c.attribute = 1, c)), [a], { R: "unsorted", subtree: !0, X: !0, C: !1 }); this.l = a; this.s = Qh(this.l.B(), b); } v(Rh, H); Rh.prototype.h = function (a, b) { var c = this; b = b.h; a = Mh(a.N); if (1 !== a.node.nodeType) return C.empty(); a = Nb(b, a, this.s).filter(function (d) { return "http://www.w3.org/2000/xmlns/" !== d.node.namespaceURI }).map(function (d) { return $b(d) }).filter(function (d) { return c.l.l(d) }); return C.create(a) }; Rh.prototype.B = function () { return "type-1" }; function Sh(a, b) { H.call(this, a.o, [a], { R: "sorted", subtree: !0, X: !0, C: !1 }); this.s = a; this.l = Qh(b, a.B()); } v(Sh, H); Sh.prototype.h = function (a, b) { var c = this, d = b.h, e = Mh(a.N); a = e.node.nodeType; if (1 !== a && 9 !== a) return C.empty(); var f = null, g = !1; return C.create({ next: function () { for (; !g;) { if (!f) { f = Sb(d, e, c.l); if (!f) { g = !0; continue } return y($b(f)) } if (f = Ub(d, f, c.l)) return y($b(f)); g = !0; } return x } }).filter(function (k) { return c.s.l(k) }) }; function Th(a, b, c) { var d = b.node.nodeType; if (1 !== d && 9 !== d) return { next: function () { return x } }; var e = Sb(a, b, c); return { next: function () { if (!e) return x; var f = e; e = Ub(a, e, c); return y(f) } } } function Uh(a, b, c) { var d = [Qd(b)]; return { next: function (e) { 0 < d.length && 0 !== (e & 1) && d.shift(); if (!d.length) return x; for (e = d[0].next(0); e.done;) { d.shift(); if (!d.length) return x; e = d[0].next(0); } d.unshift(Th(a, e.value, c)); return y($b(e.value)) } } } function Vh(a, b) { b = b || { Ra: !1 }; H.call(this, a.o, [a], { C: !1, X: !1, R: "sorted", subtree: !0 }); this.l = a; this.s = !!b.Ra; this.A = (a = this.l.B()) && (a.startsWith("name-") || "type-1" === a) || "type-1-or-type-2" === a ? "type-1" : null; } v(Vh, H);
  			Vh.prototype.h = function (a, b) { var c = this; b = b.h; a = Mh(a.N); a = Uh(b, a, this.A); this.s || a.next(0); return C.create(a).filter(function (d) { return c.l.l(d) }) }; function Wh(a, b, c) { var d = a.node.nodeType; if (1 !== d && 9 !== d) return a; for (d = Tb(b, a, c); null !== d;) { if (1 !== d.node.nodeType) return d; a = d; d = Tb(b, a, c); } return a }
  			function Xh(a, b, c, d) { if (void 0 === c ? 0 : c) { var e = b, f = !1; return { next: function () { if (f) return x; if (Sd(e, b)) return e = Wh(b, a, d), Sd(e, b) ? (f = !0, x) : y($b(e)); var k = e.node.nodeType, l = 9 === k || 2 === k ? null : Wb(a, e, d); if (null !== l) return e = Wh(l, a, d), y($b(e)); e = 9 === k ? null : Vb(a, e, d); return Sd(e, b) ? (f = !0, x) : y($b(e)) } } } var g = [Th(a, b, d)]; return { next: function () { if (!g.length) return x; for (var k = g[0].next(0); k.done;) { g.shift(); if (!g.length) return x; k = g[0].next(0); } g.unshift(Th(a, k.value, d)); return y($b(k.value)) } } } function Yh(a, b, c) { for (var d = []; b && 9 !== b.node.nodeType; b = Vb(a, b, null)) { var e = Ub(a, b, c); e && d.push(e); } var f = null; return { next: function () { for (; f || d.length;) { if (!f) { f = Xh(a, d[0], !1, c); var g = y($b(d[0])), k = Ub(a, d[0], c); k ? d[0] = k : d.shift(); return g } g = f.next(0); if (g.done) f = null; else return g } return x } } } function Zh(a) { H.call(this, a.o, [a], { R: "sorted", X: !0, subtree: !1, C: !1 }); this.l = a; this.s = (a = this.l.B()) && (a.startsWith("name-") || "type-1" === a) ? "type-1" : null; } v(Zh, H);
  			Zh.prototype.h = function (a, b) { var c = this; b = b.h; a = Mh(a.N); return C.create(Yh(b, a, this.s)).filter(function (d) { return c.l.l(d) }) }; function $h(a, b, c) { return { next: function () { return (b = b && Ub(a, b, c)) ? y($b(b)) : x } } } function ai(a, b) { H.call(this, a.o, [a], { R: "sorted", X: !0, subtree: !1, C: !1 }); this.l = a; this.s = Qh(this.l.B(), b); } v(ai, H); ai.prototype.h = function (a, b) { var c = this; b = b.h; a = Mh(a.N); return C.create($h(b, a, this.s)).filter(function (d) { return c.l.l(d) }) }; function bi(a, b) { H.call(this, a.o, [a], { R: "reverse-sorted", X: !0, subtree: !0, C: !1 }); this.l = a; this.s = Qh(b, this.l.B()); } v(bi, H); bi.prototype.h = function (a, b) { b = b.h; a = Mh(a.N); a = Vb(b, a, this.s); if (!a) return C.empty(); a = $b(a); return this.l.l(a) ? C.m(a) : C.empty() }; function ci(a, b, c) { for (var d = []; b && 9 !== b.node.nodeType; b = Vb(a, b, null)) { var e = Wb(a, b, c); null !== e && d.push(e); } var f = null; return { next: function () { for (; f || d.length;) { f || (f = Xh(a, d[0], !0, c)); var g = f.next(0); if (g.done) { f = null; g = Wb(a, d[0], c); var k = y($b(d[0])); null === g ? d.shift() : d[0] = g; return k } return g } return x } } } function di(a) { H.call(this, a.o, [a], { C: !1, X: !0, R: "reverse-sorted", subtree: !1 }); this.l = a; this.s = (a = this.l.B()) && (a.startsWith("name-") || "type-1" === a) ? "type-1" : null; } v(di, H);
  			di.prototype.h = function (a, b) { var c = this; b = b.h; a = Mh(a.N); return C.create(ci(b, a, this.s)).filter(function (d) { return c.l.l(d) }) }; function ei(a, b, c) { return { next: function () { return (b = b && Wb(a, b, c)) ? y($b(b)) : x } } } function fi(a, b) { H.call(this, a.o, [a], { C: !1, X: !0, R: "reverse-sorted", subtree: !1 }); this.l = a; this.s = Qh(this.l.B(), b); } v(fi, H); fi.prototype.h = function (a, b) { var c = this; b = b.h; a = Mh(a.N); return C.create(ei(b, a, this.s)).filter(function (d) { return c.l.l(d) }) }; function gi(a, b) { H.call(this, a.o, [a], { R: "sorted", subtree: !0, X: !0, C: !1 }); this.l = a; this.s = Qh(this.l.B(), b); } v(gi, H); gi.prototype.h = function (a) { Mh(a.N); return this.l.l(a.N) ? C.m(a.N) : C.empty() }; gi.prototype.B = function () { return this.s }; function hi(a, b, c, d) { var e = a.o.add(b.o).add(c.o); Df.call(this, e, [a, b, c], { C: a.C && b.C && c.C, X: b.X === c.X && b.X, R: b.da === c.da ? b.da : "unsorted", subtree: b.subtree === c.subtree && b.subtree }, d); this.l = a; } v(hi, Df); hi.prototype.A = function (a, b, c) { var d = null, e = c[0](a); return C.create({ next: function (f) { d || (d = (e.ha() ? c[1](a) : c[2](a)).value); return d.next(f) } }) }; hi.prototype.v = function (a) { Df.prototype.v.call(this, a); if (this.l.I) throw af(); }; function ii(a, b, c) { this.location = a; this.o = b; this.h = c; } function qg(a) { return a.h instanceof Error ? a.location : qg(a.h) } function rg(a) { var b = a.h instanceof og ? ["Inner error:", a.h.message] : a.h instanceof Error ? [a.h.toString()] : rg(a.h); b.push("  at <" + a.o + ">:" + a.location.start.line + ":" + a.location.start.ja + " - " + a.location.end.line + ":" + a.location.end.ja); return b } function ji(a, b, c) { Df.call(this, c.o, [c], { C: c.C, X: c.X, R: c.da, subtree: c.subtree }); this.l = b; this.K = { end: { ja: a.end.ja, line: a.end.line, offset: a.end.offset }, start: { ja: a.start.ja, line: a.start.line, offset: a.start.offset } }; } v(ji, Df); ji.prototype.A = function (a, b, c) { var d = this; b = p(c).next().value; try { var e = b(a); } catch (f) { throw new ii(this.K, this.l, f); } return C.create({ next: function (f) { try { return e.value.next(f) } catch (g) { throw new ii(d.K, d.l, g); } } }) };
  			ji.prototype.v = function (a) { try { Df.prototype.v.call(this, a); } catch (b) { throw new ii(this.K, this.l, b); } }; function ki(a, b, c, d) { H.call(this, a, b, c, !0); this.l = d; this.I = this.l.I; } v(ki, H); function li(a, b, c, d) { var e = [], f = a.K(b, c, d, function (k) { if (a.l instanceof ki) { var l = li(a.l, b, k, d); return Cf(l, function (q) { return e = q }) } var m = null; return C.create({ next: function () { for (; ;) { if (!m) { var q = k.next(0); if (q.done) return x; q = a.l.s(q.value, d); m = Cf(q, function (u) { return e = zf(e, u) }).value; } q = m.next(0); if (q.done) m = null; else return q } } }) }), g = !1; return { next: function () { if (g) return x; var k = f.O(); g = !0; return y(new nf(k, e)) } } }
  			ki.prototype.h = function (a, b) { var c = this; return this.K(a, Qd(a), b, function (d) { if (c.l instanceof ki) return mi(c.l, a, d, b); var e = null; return C.create({ next: function (f) { for (; ;) { if (!e) { var g = d.next(0); if (g.done) return x; e = I(c.l, g.value, b).value; } g = e.next(f); if (g.done) e = null; else return g } } }) }) }; ki.prototype.s = function (a, b) { return li(this, a, Qd(a), b) };
  			ki.prototype.v = function (a) { H.prototype.v.call(this, a); this.I = this.l.I; a = p(this.ta); for (var b = a.next(); !b.done; b = a.next())if (b = b.value, b !== this.l && b.I) throw af(); }; function mi(a, b, c, d) { return a.K(b, c, d, function (e) { if (a.l instanceof ki) return mi(a.l, b, e, d); var f = null; return C.create({ next: function () { for (; ;) { if (!f) { var g = e.next(0); if (g.done) return x; f = I(a.l, g.value, d).value; } g = f.next(0); if (g.done) f = null; else return g } } }) }) } function ni(a, b, c, d) { ki.call(this, b.o.add(d.o), [b, d], { C: !1 }, d); this.S = a.prefix; this.la = a.namespaceURI; this.Xb = a.localName; this.Gb = null; this.A = c; this.Ba = null; this.pa = b; } v(ni, ki);
  			ni.prototype.K = function (a, b, c, d) { var e = this, f = null, g = null, k = 0; return d({ next: function () { for (var l = {}; ;) { if (!f) { var m = b.next(0); if (m.done) return x; g = m.value; k = 0; f = I(e.pa, g, c).value; } l.mb = f.next(0); if (l.mb.done) f = null; else return k++, m = {}, l = (m[e.Gb] = function (q) { return function () { return C.m(q.mb.value) } }(l), m), e.Ba && (l[e.Ba] = function () { return C.m(new jb(5, k)) }), y(Nc(g, l)); l = { mb: l.mb }; } } }) };
  			ni.prototype.v = function (a) {
  				if (this.S && (this.la = a.aa(this.S), !this.la && this.S)) throw Error("XPST0081: Could not resolve namespace for prefix " + this.S + " in a for expression"); this.pa.v(a); Mg(a); this.Gb = Qg(a, this.la, this.Xb); if (this.A) { if (this.A.prefix && (this.A.namespaceURI = a.aa(this.A.prefix), !this.A.namespaceURI && this.A.prefix)) throw Error("XPST0081: Could not resolve namespace for prefix " + this.S + " in the positionalVariableBinding in a for expression"); this.Ba = Qg(a, this.A.namespaceURI, this.A.localName); } this.l.v(a);
  				Sg(a); if (this.pa.I) throw af(); this.l.I && (this.I = !0);
  			}; function oi(a, b, c) { var d = {}; H.call(this, new Hf((d.external = 1, d)), [c], { C: !1, R: "unsorted" }); this.S = a.map(function (e) { return e.name }); this.A = a.map(function (e) { return e.type }); this.s = null; this.K = b; this.l = c; } v(oi, H);
  			oi.prototype.h = function (a, b) { var c = this, d = new Ab({ j: this.A, arity: this.A.length, Sa: !0, I: this.l.I, localName: "dynamic-function", namespaceURI: "", i: this.K, value: function (e, f, g) { var k = Aa.apply(3, arguments), l = Nc(Jc(a, -1, null, C.empty()), c.s.reduce(function (m, q, u) { m[q] = yb(k[u]); return m }, Object.create(null))); return I(c.l, l, b) } }); return C.m(d) };
  			oi.prototype.v = function (a) { Mg(a); this.s = this.S.map(function (b) { return Qg(a, b.namespaceURI, b.localName) }); this.l.v(a); Sg(a); if (this.l.I) throw Error("Not implemented: inline functions can not yet be updating."); }; function pi(a, b, c) { ki.call(this, b.o.add(c.o), [b, c], { C: !1, X: c.X, R: c.da, subtree: c.subtree }, c); if (a.prefix || a.namespaceURI) throw Error("Not implemented: let expressions with namespace usage."); this.A = a.prefix; this.S = a.namespaceURI; this.Ba = a.localName; this.la = b; this.pa = null; } v(pi, ki); pi.prototype.K = function (a, b, c, d) { var e = this; return d({ next: function () { var f = b.next(0); if (f.done) return x; f = f.value; var g = {}; f = Nc(f, (g[e.pa] = yb(I(e.la, f, c)), g)); return y(f) } }) };
  			pi.prototype.v = function (a) { if (this.A && (this.S = a.aa(this.A), !this.S && this.A)) throw Error("XPST0081: Could not resolve namespace for prefix " + this.A + " using in a for expression"); this.la.v(a); Mg(a); this.pa = Qg(a, this.S, this.Ba); this.l.v(a); Sg(a); this.I = this.l.I; if (this.la.I) throw af(); }; function qi(a, b) { H.call(this, new Hf({}), [], { C: !0, R: "sorted" }, !1, b); switch (b.type) { case 5: var c = w(parseInt(a, 10), b.type); break; case 1: c = w(a, b.type); break; case 4: case 3: c = w(parseFloat(a), b.type); break; default: throw new TypeError("Type " + b + " not expected in a literal"); }this.l = function () { return C.m(c) }; } v(qi, H); qi.prototype.h = function () { return this.l() }; function ri(a, b) { var c = {}; H.call(this, new Hf((c.external = 1, c)), a.reduce(function (d, e) { return d.concat(e.key, e.value) }, []), { C: !1 }, !1, b); this.l = a; } v(ri, H); ri.prototype.h = function (a, b) { var c = this, d = this.l.map(function (e) { return Zc(I(e.key, a, b), b).Z({ default: function () { throw Error("XPTY0004: A key of a map should be a single atomizable value."); }, m: function (f) { return f } }) }); return ac(d, function (e) { return C.m(new dc(e.map(function (f, g) { return { key: f, value: yb(I(c.l[g].value, a, b)) } }))) }) }; function si(a, b, c) { var d = {}; H.call(this, new Hf((d.external = 1, d)), [], { C: !0 }, !1, c); this.s = b; this.A = a; this.l = null; } v(si, H); si.prototype.h = function () { var a = new Ab({ j: this.l.j, I: this.l.I, arity: this.s, localName: this.l.localName, namespaceURI: this.l.namespaceURI, i: this.l.i, value: this.l.callFunction }); return C.m(a) };
  			si.prototype.v = function (a) {
  				var b = this.A.namespaceURI, c = this.A.localName, d = this.A.prefix; if (null === b) { var e = a.Ua({ localName: c, prefix: d }, this.s); if (!e) throw Error("XPST0017: The function " + (d ? d + ":" : "") + c + " with arity " + this.s + " could not be resolved. " + ug(c)); b = e.namespaceURI; c = e.localName; } this.l = a.xa(b, c, this.s) || null; if (!this.l) throw a = this.A, Error("XPST0017: Function " + ((a.namespaceURI ? "Q{" + a.namespaceURI + "}" : a.prefix ? a.prefix + ":" : "") + a.localName) + " with arity of " + this.s + " not registered. " + ug(c));
  				H.prototype.v.call(this, a);
  			}; var ti = {}, ui = (ti[5] = 5, ti[27] = 5, ti[28] = 5, ti[31] = 5, ti[32] = 5, ti[33] = 5, ti[34] = 5, ti[30] = 5, ti[36] = 5, ti[35] = 5, ti[38] = 5, ti[37] = 5, ti[29] = 5, ti[4] = 4, ti[6] = 6, ti[3] = 3, ti); function vi(a, b, c) { H.call(this, b.o, [b], { C: !1 }, !1, c); this.s = b; this.l = a; } v(vi, H);
  			vi.prototype.h = function (a, b) { var c = this; return Zc(I(this.s, a, b), b).M(function (d) { if (0 === d.length) return C.empty(); var e = d[0]; if (c.type) return d = "+" === c.l ? +e.value : -e.value, 0 === e.type && (d = Number.NaN), C.m(w(d, c.type.type)); if (1 < d.length) throw Error("XPTY0004: The operand to a unary operator must be a sequence with a length less than one"); return B(e.type, 19) ? (e = Pd(e, 3).value, C.m(w("+" === c.l ? e : -e, 3))) : B(e.type, 2) ? "+" === c.l ? C.m(e) : C.m(w(-1 * e.value, ui[e.type])) : C.m(w(Number.NaN, 3)) }) }; function wi(a, b) { H.call(this, a.reduce(function (c, d) { return c.add(d.o) }, new Hf({})), a, { C: a.every(function (c) { return c.C }) }, !1, b); this.l = a; this.s = a.reduce(function (c, d) { return Qh(c, d.B()) }, null); } v(wi, H);
  			wi.prototype.h = function (a, b) { var c = this, d = 0, e = null, f = !1, g = null; if (null !== a) { var k = a.N; null !== k && B(k.type, 53) && (g = Eb(k.value)); } return C.create({ next: function () { if (!f) { for (; d < c.l.length;) { if (!e) { var l = c.l[d]; if (null !== g && null !== l.B() && !g.includes(l.B())) return d++, f = !0, y(db); e = I(l, a, b); } if (!1 === e.ha()) return f = !0, y(db); e = null; d++; } f = !0; return y(cb) } return x } }) }; wi.prototype.B = function () { return this.s }; function xi(a, b) { var c = a.reduce(function (e, f) { return 0 < If(e, f.o) ? e : f.o }, new Hf({})); H.call(this, c, a, { C: a.every(function (e) { return e.C }) }, !1, b); var d; for (b = 0; b < a.length; ++b) { void 0 === d && (d = a[b].B()); if (null === d) break; if (d !== a[b].B()) { d = null; break } } this.s = d; this.l = a; } v(xi, H);
  			xi.prototype.h = function (a, b) { var c = this, d = 0, e = null, f = !1, g = null; if (null !== a) { var k = a.N; null !== k && B(k.type, 53) && (g = Eb(k.value)); } return C.create({ next: function () { if (!f) { for (; d < c.l.length;) { if (!e) { var l = c.l[d]; if (null !== g && null !== l.B() && !g.includes(l.B())) { d++; continue } e = I(l, a, b); } if (!0 === e.ha()) return f = !0, y(cb); e = null; d++; } f = !0; return y(db) } return x } }) }; xi.prototype.B = function () { return this.s }; function yi(a, b) { var c; return C.create({ next: function (d) { for (; ;) { if (!c) { var e = a.value.next(d); if (e.done) return x; c = Yc(e.value, b); } e = c.value.next(d); if (e.done) c = null; else return e } } }) } function zi(a, b) { if ("eqOp" === a) return function (c, d) { d = b(c, d); c = d.U; d = d.V; return c.value.namespaceURI === d.value.namespaceURI && c.value.localName === d.value.localName }; if ("neOp" === a) return function (c, d) { d = b(c, d); c = d.U; d = d.V; return c.value.namespaceURI !== d.value.namespaceURI || c.value.localName !== d.value.localName }; throw Error('XPTY0004: Only the "eq" and "ne" comparison is defined for xs:QName'); }
  			function Ai(a, b) { switch (a) { case "eqOp": return function (c, d) { c = b(c, d); return c.U.value === c.V.value }; case "neOp": return function (c, d) { c = b(c, d); return c.U.value !== c.V.value }; case "ltOp": return function (c, d) { c = b(c, d); return c.U.value < c.V.value }; case "leOp": return function (c, d) { c = b(c, d); return c.U.value <= c.V.value }; case "gtOp": return function (c, d) { c = b(c, d); return c.U.value > c.V.value }; case "geOp": return function (c, d) { c = b(c, d); return c.U.value >= c.V.value } } }
  			function Bi(a, b) { switch (a) { case "ltOp": return function (c, d) { c = b(c, d); return c.U.value.ga < c.V.value.ga }; case "leOp": return function (c, d) { d = b(c, d); c = d.U; d = d.V; return fc(c.value, d.value) || c.value.ga < d.value.ga }; case "gtOp": return function (c, d) { c = b(c, d); return c.U.value.ga > c.V.value.ga }; case "geOp": return function (c, d) { d = b(c, d); c = d.U; d = d.V; return fc(c.value, d.value) || c.value.ga > d.value.ga } } }
  			function Ci(a, b) { switch (a) { case "eqOp": return function (c, d) { c = b(c, d); return fc(c.U.value, c.V.value) }; case "ltOp": return function (c, d) { c = b(c, d); return c.U.value.ea < c.V.value.ea }; case "leOp": return function (c, d) { d = b(c, d); c = d.U; d = d.V; return fc(c.value, d.value) || c.value.ea < d.value.ea }; case "gtOp": return function (c, d) { c = b(c, d); return c.U.value.ea > c.V.value.ea }; case "geOp": return function (c, d) { d = b(c, d); c = d.U; d = d.V; return fc(c.value, d.value) || c.value.ea > d.value.ea } } }
  			function Di(a, b) { switch (a) { case "eqOp": return function (c, d) { c = b(c, d); return fc(c.U.value, c.V.value) }; case "neOp": return function (c, d) { c = b(c, d); return !fc(c.U.value, c.V.value) } } }
  			function Ei(a, b) {
  				switch (a) {
  					case "eqOp": return function (c, d, e) { c = b(c, d); return wc(c.U.value, c.V.value, Lc(e)) }; case "neOp": return function (c, d, e) { c = b(c, d); return !wc(c.U.value, c.V.value, Lc(e)) }; case "ltOp": return function (c, d, e) { c = b(c, d); e = Lc(e); return 0 > vc(c.U.value, c.V.value, e) }; case "leOp": return function (c, d, e) { d = b(c, d); c = d.U; d = d.V; var f; (f = wc(c.value, d.value, Lc(e))) || (e = Lc(e), f = 0 > vc(c.value, d.value, e)); return f }; case "gtOp": return function (c, d, e) {
  						c = b(c, d); e = Lc(e); return 0 < vc(c.U.value, c.V.value,
  							e)
  					}; case "geOp": return function (c, d, e) { d = b(c, d); c = d.U; d = d.V; var f; (f = wc(c.value, d.value, Lc(e))) || (e = Lc(e), f = 0 < vc(c.value, d.value, e)); return f }
  				}
  			} function Fi(a, b) { switch (a) { case "eqOp": return function (c, d, e) { c = b(c, d); return wc(c.U.value, c.V.value, Lc(e)) }; case "neOp": return function (c, d, e) { c = b(c, d); return !wc(c.U.value, c.V.value, Lc(e)) } } }
  			function Gi(a, b, c) {
  				function d(m, q) { return { U: g ? g(m) : m, V: k ? k(q) : q } } function e(m) { return B(b, m) && B(c, m) } function f(m) { return 0 < m.filter(function (q) { return B(b, q) }).length && 0 < m.filter(function (q) { return B(c, q) }).length } var g = null, k = null; B(b, 19) && B(c, 19) ? b = c = 1 : B(b, 19) ? (g = function (m) { return Pd(m, c) }, b = c) : B(c, 19) && (k = function (m) { return Pd(m, b) }, c = b); if (B(b, 23) && B(c, 23)) return zi(a, d); if (e(0) || f([1, 47, 61]) || f([2, 47, 61]) || e(20) || e(22) || e(21) || f([1, 20])) { var l = Ai(a, d); if (void 0 !== l) return l } if (e(16) && (l = Bi(a,
  					d), void 0 !== l) || e(17) && (l = Ci(a, d), void 0 !== l) || e(18) && (l = Di(a, d), void 0 !== l)) return l; if (e(9) || e(7) || e(8)) if (l = Ei(a, d), void 0 !== l) return l; if (e(11) || e(12) || e(13) || e(14) || e(15)) if (l = Fi(a, d), void 0 !== l) return l; throw Error("XPTY0004: " + a + " not available for " + mb[b] + " and " + mb[c]);
  			} var Hi = Object.create(null); function Ii(a, b, c) { var d = b + "~" + c + "~" + a, e = Hi[d]; e || (e = Hi[d] = Gi(a, b, c)); return e } function Ji(a, b, c) { H.call(this, b.o.add(c.o), [b, c], { C: !1 }); this.l = b; this.A = c; this.s = a; } v(Ji, H);
  			Ji.prototype.h = function (a, b) { var c = this, d = I(this.l, a, b), e = I(this.A, a, b), f = yi(d, b), g = yi(e, b); return f.Z({ empty: function () { return C.empty() }, m: function () { return g.Z({ empty: function () { return C.empty() }, m: function () { var k = f.first(), l = g.first(); return Ii(c.s, k.type, l.type)(k, l, a) ? C.ba() : C.W() }, multiple: function () { throw Error("XPTY0004: Sequences to compare are not singleton."); } }) }, multiple: function () { throw Error("XPTY0004: Sequences to compare are not singleton."); } }) }; var Ki = {}, Li = (Ki.equalOp = "eqOp", Ki.notEqualOp = "neOp", Ki.lessThanOrEqualOp = "leOp", Ki.lessThanOp = "ltOp", Ki.greaterThanOrEqualOp = "geOp", Ki.greaterThanOp = "gtOp", Ki);
  			function Mi(a, b, c, d) { a = Li[a]; return c.M(function (e) { return b.filter(function (f) { for (var g = 0, k = e.length; g < k; ++g) { var l = e[g], m = void 0, q = void 0, u = f.type, z = l.type; if (B(u, 19) || B(z, 19)) B(u, 2) ? m = 3 : B(z, 2) ? q = 3 : B(u, 17) ? m = 17 : B(z, 17) ? q = 17 : B(u, 16) ? m = 16 : B(z, 16) ? q = 16 : B(u, 19) ? q = z : B(z, 19) && (m = u); q = p([q, m]); m = q.next().value; q = q.next().value; m ? f = Pd(f, m) : q && (l = Pd(l, q)); if (Ii(a, f.type, l.type)(f, l, d)) return !0 } return !1 }).Z({ default: function () { return C.ba() }, empty: function () { return C.W() } }) }) }
  			function Ni(a, b, c) { H.call(this, b.o.add(c.o), [b, c], { C: !1 }); this.l = b; this.A = c; this.s = a; } v(Ni, H); Ni.prototype.h = function (a, b) { var c = this, d = I(this.l, a, b), e = I(this.A, a, b); return d.Z({ empty: function () { return C.W() }, default: function () { return e.Z({ empty: function () { return C.W() }, default: function () { var f = yi(d, b), g = yi(e, b); return Mi(c.s, f, g, a) } }) } }) }; function Oi(a, b, c, d) { if (!B(c, 53) || !B(d, 53)) throw Error("XPTY0004: Sequences to compare are not nodes"); switch (a) { case "isOp": return Pi(c, d); case "nodeBeforeOp": return b ? function (e, f) { return 0 > Yd(b, e.first(), f.first()) } : void 0; case "nodeAfterOp": return b ? function (e, f) { return 0 < Yd(b, e.first(), f.first()) } : void 0; default: throw Error("Unexpected operator"); } }
  			function Pi(a, b) { return a !== b || 47 !== a && 53 !== a && 54 !== a && 55 !== a && 56 !== a && 57 !== a && 58 !== a ? function () { return !1 } : function (c, d) { return Sd(c.first().value, d.first().value) } } function Qi(a, b, c) { H.call(this, b.o.add(c.o), [b, c], { C: !1 }); this.l = b; this.A = c; this.s = a; } v(Qi, H);
  			Qi.prototype.h = function (a, b) { var c = this, d = I(this.l, a, b), e = I(this.A, a, b); return d.Z({ empty: function () { return C.empty() }, multiple: function () { throw Error("XPTY0004: Sequences to compare are not singleton"); }, m: function () { return e.Z({ empty: function () { return C.empty() }, multiple: function () { throw Error("XPTY0004: Sequences to compare are not singleton"); }, m: function () { var f = d.first(), g = e.first(); return Oi(c.s, b.h, f.type, g.type)(d, e, a) ? C.ba() : C.W() } }) } }) }; function Ri(a, b, c, d) { return c.M(function (e) { if (e.some(function (f) { return !B(f.type, 53) })) throw Error("XPTY0004: Sequences given to " + a + " should only contain nodes."); return "sorted" === d ? C.create(e) : "reverse-sorted" === d ? C.create(e.reverse()) : C.create(Zd(b, e)) }) } function Si(a, b, c, d) { H.call(this, 0 < If(b.o, c.o) ? b.o : c.o, [b, c], { C: b.C && c.C }, !1, d); this.l = a; this.s = b; this.A = c; } v(Si, H);
  			Si.prototype.h = function (a, b) {
  				var c = this, d = Ri(this.l, b.h, I(this.s, a, b), this.s.da); a = Ri(this.l, b.h, I(this.A, a, b), this.A.da); var e = d.value, f = a.value, g = null, k = null, l = !1, m = !1; return C.create({
  					next: function () {
  						if (l) return x; for (; !m;) { if (!g) { var q = e.next(0); if (q.done) return l = !0, x; g = q.value; } if (!k) { q = f.next(0); if (q.done) { m = !0; break } k = q.value; } if (Sd(g.value, k.value)) { if (q = y(g), k = g = null, "intersectOp" === c.l) return q } else if (0 > Yd(b.h, g, k)) { if (q = y(g), g = null, "exceptOp" === c.l) return q } else k = null; } if ("exceptOp" ===
  							c.l) return null !== g ? (q = y(g), g = null, q) : e.next(0); l = !0; return x
  					}
  				})
  			}; function Ti(a, b) { Df.call(this, a.reduce(function (c, d) { return c.add(d.o) }, new Hf({})), a, { R: "unsorted", C: a.every(function (c) { return c.C }) }, b); } v(Ti, Df); Ti.prototype.A = function (a, b, c) { return c.length ? Pc(c.map(function (d) { return d(a) })) : C.empty() }; function Ui(a, b, c) { H.call(this, (new Hf({})).add(a.o), [a, b], { C: a.C && b.C }, !1, c); this.l = a; this.s = b; } v(Ui, H); Ui.prototype.h = function (a, b) { var c = this, d = I(this.l, a, b), e = Ic(a, d), f = null, g = null, k = !1; return C.create({ next: function (l) { for (; !k;) { if (!f && (f = e.next(l), f.done)) return k = !0, x; g || (g = I(c.s, f.value, b)); var m = g.value.next(l); if (m.done) f = g = null; else return m } } }) }; function Vi(a, b, c) { H.call(this, a.o, [a], { C: !1 }); this.l = pb(b.prefix ? b.prefix + ":" + b.localName : b.localName); if (46 === this.l || 45 === this.l || 44 === this.l) throw Error("XPST0080: Casting to xs:anyAtomicType, xs:anySimpleType or xs:NOTATION is not permitted."); if (b.namespaceURI) throw Error("Not implemented: castable as expressions with a namespace URI."); this.A = a; this.s = c; } v(Vi, H);
  			Vi.prototype.h = function (a, b) { var c = this, d = Zc(I(this.A, a, b), b); return d.Z({ empty: function () { return c.s ? C.ba() : C.W() }, m: function () { return d.map(function (e) { return Od(e, c.l).u ? cb : db }) }, multiple: function () { return C.W() } }) }; function Wi(a, b, c) { H.call(this, a.o, [a], { C: !1 }); this.l = pb(b.prefix ? b.prefix + ":" + b.localName : b.localName); if (46 === this.l || 45 === this.l || 44 === this.l) throw Error("XPST0080: Casting to xs:anyAtomicType, xs:anySimpleType or xs:NOTATION is not permitted."); if (b.namespaceURI) throw Error("Not implemented: casting expressions with a namespace URI."); this.A = a; this.s = c; } v(Wi, H);
  			Wi.prototype.h = function (a, b) { var c = this, d = Zc(I(this.A, a, b), b); return d.Z({ empty: function () { if (!c.s) throw Error("XPTY0004: Sequence to cast is empty while target type is singleton."); return C.empty() }, m: function () { return d.map(function (e) { return Pd(e, c.l) }) }, multiple: function () { throw Error("XPTY0004: Sequence to cast is not singleton or empty."); } }) }; function Xi(a, b) { var c = a.value, d = null, e = !1; return C.create({ next: function () { for (; !e;) { if (!d) { var f = c.next(0); if (f.done) return e = !0, y(cb); d = b(f.value); } f = d.ha(); d = null; if (!1 === f) return e = !0, y(db) } return x } }) } function Yi(a, b, c, d) { H.call(this, a.o, [a], { C: !1 }, !1, d); this.A = a; this.s = b; this.l = c; } v(Yi, H); Yi.prototype.h = function (a, b) { var c = this, d = I(this.A, a, b); return d.Z({ empty: function () { return "?" === c.l || "*" === c.l ? C.ba() : C.W() }, multiple: function () { return "+" === c.l || "*" === c.l ? Xi(d, function (e) { var f = C.m(e); e = Jc(a, 0, e, f); return I(c.s, e, b) }) : C.W() }, m: function () { return Xi(d, function (e) { var f = C.m(e); e = Jc(a, 0, e, f); return I(c.s, e, b) }) } }) }; function Zi(a, b) { return null !== a && null !== b && B(a.type, 53) && B(b.type, 53) ? Sd(a.value, b.value) : !1 } function $i(a) { var b = a.next(0); if (b.done) return C.empty(); var c = null, d = null; return C.create({ next: function (e) { if (b.done) return x; c || (c = b.value.value); do { var f = c.next(e); if (f.done) { b = a.next(0); if (b.done) return f; c = b.value.value; } } while (f.done || Zi(f.value, d)); d = f.value; return f } }) }
  			function aj(a, b) {
  				var c = []; (function () { for (var g = b.next(0), k = {}; !g.done;)k.ob = g.value.value, g = { current: k.ob.next(0), next: function (l) { return function (m) { return l.ob.next(m) } }(k) }, g.current.done || c.push(g), g = b.next(0), k = { ob: k.ob }; })(); var d = null, e = !1, f = {}; return C.create((f[Symbol.iterator] = function () { return this }, f.next = function () {
  					e || (e = !0, c.every(function (z) { return B(z.current.value.type, 53) }) && c.sort(function (z, A) { return Yd(a, z.current.value, A.current.value) })); do {
  						if (!c.length) return x; var g = c.shift();
  						var k = g.current; g.current = g.next(0); if (!B(k.value.type, 53)) return k; if (!g.current.done) { for (var l = 0, m = c.length - 1, q = 0; l <= m;) { q = Math.floor((l + m) / 2); var u = Yd(a, g.current.value, c[q].current.value); if (0 === u) { l = q; break } 0 < u ? l = q + 1 : m = q - 1; } c.splice(l, 0, g); }
  					} while (Zi(k.value, d)); d = k.value; return k
  				}, f))
  			} function bj(a, b) { var c = a.reduce(function (d, e) { return 0 < If(d, e.o) ? d : e.o }, new Hf({})); H.call(this, c, a, { C: a.every(function (d) { return d.C }) }, !1, b); this.l = a; } v(bj, H);
  			bj.prototype.h = function (a, b) { var c = this; if (this.l.every(function (e) { return "sorted" === e.da })) { var d = 0; return aj(b.h, { next: function () { return d >= c.l.length ? x : y(I(c.l[d++], a, b)) } }).map(function (e) { if (!B(e.type, 53)) throw Error("XPTY0004: The sequences to union are not of type node()*"); return e }) } return Pc(this.l.map(function (e) { return I(e, a, b) })).M(function (e) { if (e.some(function (f) { return !B(f.type, 53) })) throw Error("XPTY0004: The sequences to union are not of type node()*"); e = Zd(b.h, e); return C.create(e) }) }; function cj(a) {
  				return a.every(function (b) { return null === b || B(b.type, 5) || B(b.type, 4) }) || null !== a.map(function (b) { return b ? $c(b.type) : null }).reduce(function (b, c) { return null === c ? b : c === b ? b : null }) ? a : a.every(function (b) { return null === b || B(b.type, 1) || B(b.type, 20) }) ? a.map(function (b) { return b ? Pd(b, 1) : null }) : a.every(function (b) { return null === b || B(b.type, 4) || B(b.type, 6) }) ? a.map(function (b) { return b ? Pd(b, 6) : b }) : a.every(function (b) { return null === b || B(b.type, 4) || B(b.type, 6) || B(b.type, 3) }) ? a.map(function (b) {
  					return b ?
  						Pd(b, 3) : b
  				}) : null
  			} function dj(a) { return (a = a.find(function (b) { return !!b })) ? $c(a.type) : null } function ej(a, b) { var c = new Hf({}); ki.call(this, c, [b].concat(t(a.map(function (d) { return d.ca }))), { C: !1, X: !1, R: "unsorted", subtree: !1 }, b); this.A = a; } v(ej, ki);
  			ej.prototype.K = function (a, b, c, d) {
  				if (this.A[1]) throw Error("More than one order spec is not supported for the order by clause."); var e = [], f = !1, g, k, l = null, m = this.A[0]; return C.create({
  					next: function () {
  						if (!f) {
  							for (var q = b.next(0); !q.done;)e.push(q.value), q = b.next(0); q = e.map(function (ia) { return m.ca.h(ia, c) }).map(function (ia) { return Zc(ia, c) }); if (q.find(function (ia) { return !ia.G() && !ia.wa() })) throw Ag("Order by only accepts empty or singleton sequences"); g = q.map(function (ia) { return ia.first() }); g = g.map(function (ia) {
  								return null ===
  									ia ? ia : B(19, ia.type) ? Pd(ia, 1) : ia
  							}); if (dj(g) && (g = cj(g), !g)) throw Ag("Could not cast values"); q = g.length; k = g.map(function (ia, Ba) { return Ba }); for (var u = 0; u < q; u++)if (u + 1 !== q) for (var z = u; 0 <= z; z--) {
  								var A = z, D = z + 1; if (D !== q) {
  									var F = g[k[A]], J = g[k[D]]; if (null !== J || null !== F) {
  										if (m.lc) { if (null === F) continue; if (null === J && null !== F) { F = p([k[D], k[A]]); k[A] = F.next().value; k[D] = F.next().value; continue } if (isNaN(J.value) && null !== F && !isNaN(F.value)) { F = p([k[D], k[A]]); k[A] = F.next().value; k[D] = F.next().value; continue } } else {
  											if (null ===
  												J) continue; if (null === F && null !== J) { F = p([k[D], k[A]]); k[A] = F.next().value; k[D] = F.next().value; continue } if (isNaN(F.value) && null !== J && !isNaN(J.value)) { F = p([k[D], k[A]]); k[A] = F.next().value; k[D] = F.next().value; continue }
  										} Ii("gtOp", F.type, J.type)(F, J, a) && (F = p([k[D], k[A]]), k[A] = F.next().value, k[D] = F.next().value);
  									}
  								}
  							} var T = m.Kb ? 0 : g.length - 1; l = d({ next: function () { return m.Kb ? T >= g.length ? x : y(e[k[T++]]) : 0 > T ? x : y(e[k[T--]]) } }).value; f = !0;
  						} return l.next(0)
  					}
  				})
  			}; function fj(a) { H.call(this, a ? a.o : new Hf({}), a ? [a] : [], { R: "sorted", subtree: !1, X: !1, C: !1 }); this.l = a; } v(fj, H); fj.prototype.h = function (a, b) { if (null === a.N) throw Rc("context is absent, it needs to be present to use paths."); for (var c = b.h, d = a.N.value; 9 !== d.node.nodeType;)if (d = Vb(c, d), null === d) throw Error("XPDY0050: the root node of the context node is not a document node."); c = C.m($b(d)); return this.l ? I(this.l, Jc(a, 0, c.first(), c), b) : c }; function gj(a) { H.call(this, new Hf({}), [], { R: "sorted" }, !1, a); } v(gj, H); gj.prototype.h = function (a) { if (null === a.N) throw Rc('context is absent, it needs to be present to use the "." operator'); return C.m(a.N) }; function hj(a, b) { var c = !1, d = !1; b.forEach(function (e) { B(e.type, 53) ? c = !0 : d = !0; }); if (d && c) throw Error("XPTY0018: The path operator should either return nodes or non-nodes. Mixed sequences are not allowed."); return c ? Zd(a, b) : b } function ij(a, b) { var c = a.every(function (e) { return e.X }), d = a.every(function (e) { return e.subtree }); H.call(this, a.reduce(function (e, f) { return e.add(f.o) }, new Hf({})), a, { C: !1, X: c, R: b ? "sorted" : "unsorted", subtree: d }); this.l = a; this.s = b; } v(ij, H);
  			ij.prototype.h = function (a, b) {
  				var c = this, d = !0; return this.l.reduce(function (e, f, g) {
  					var k = null === e ? Qd(a) : Ic(a, e); e = { next: function (q) { q = k.next(q); if (q.done) return x; if (null !== q.value.N && !B(q.value.N.type, 53) && 0 < g) throw Error("XPTY0019: The result of E1 in a path expression E1/E2 should not evaluate to a sequence of nodes."); return y(I(f, q.value, b)) } }; if (c.s) switch (f.da) {
  						case "reverse-sorted": var l = e; e = { next: function (q) { q = l.next(q); return q.done ? q : y(q.value.M(function (u) { return C.create(u.reverse()) })) } };
  						case "sorted": if (f.subtree && d) { var m = $i(e); break } m = aj(b.h, e); break; case "unsorted": return $i(e).M(function (q) { return C.create(hj(b.h, q)) })
  					} else m = $i(e); d = d && f.X; return m
  				}, null)
  			}; ij.prototype.B = function () { return this.l[0].B() }; function jj(a, b) { H.call(this, a.o.add(b.o), [a, b], { C: a.C && b.C, X: a.X, R: a.da, subtree: a.subtree }); this.s = a; this.l = b; } v(jj, H);
  			jj.prototype.h = function (a, b) {
  				var c = this, d = I(this.s, a, b); if (this.l.C) { var e = I(this.l, a, b); if (e.G()) return e; var f = e.first(); if (B(f.type, 2)) { var g = f.value; if (!Number.isInteger(g)) return C.empty(); var k = d.value, l = !1; return C.create({ next: function () { if (!l) { for (var A = k.next(0); !A.done; A = k.next(0))if (1 === g--) return l = !0, A; l = !0; } return x } }) } return e.ha() ? d : C.empty() } var m = d.value, q = null, u = 0, z = null; return C.create({
  					next: function (A) {
  						for (var D = !1; !q || !q.done;) {
  							q || (q = m.next(D ? 0 : A), D = !0); if (q.done) break; z || (z =
  								I(c.l, Jc(a, u, q.value, d), b)); var F = z.first(); F = null === F ? !1 : B(F.type, 2) ? F.value === u + 1 : z.ha(); z = null; var J = q.value; q = null; u++; if (F) return y(J)
  						} return q
  					}
  				})
  			}; jj.prototype.B = function () { return this.s.B() }; function kj(a, b, c) {
  				c = [c]; if (B(a.type, 62)) if ("*" === b) c.push.apply(c, t(a.P.map(function (e) { return e() }))); else if (B(b.type, 5)) { var d = b.value; if (a.P.length < d || 0 >= d) throw Error("FOAY0001: Array index out of bounds"); c.push(a.P[d - 1]()); } else throw Ag("The key specifier is not an integer."); else if (B(a.type, 61)) "*" === b ? c.push.apply(c, t(a.h.map(function (e) { return e.value() }))) : (a = a.h.find(function (e) { return bc(e.key, b) })) && c.push(a.value()); else throw Ag("The provided context item is not a map or an array.");
  				return Pc(c)
  			} function lj(a, b, c, d, e) { if ("*" === b) return kj(a, b, c); b = I(b, d, e); b = yb(b)().M(function (f) { return f.reduce(function (g, k) { return kj(a, k, g) }, new ib) }); return Pc([c, b]) } function mj(a, b) { H.call(this, a.o, [a].concat("*" === b ? [] : [b]), { C: a.C, R: a.da, subtree: a.subtree }); this.l = a; this.s = b; } v(mj, H); mj.prototype.h = function (a, b) { var c = this; return I(this.l, a, b).M(function (d) { return d.reduce(function (e, f) { return lj(f, c.s, e, a, b) }, new ib) }) }; mj.prototype.B = function () { return this.l.B() }; function nj(a, b) { var c = {}; H.call(this, new Hf((c.external = 1, c)), "*" === a ? [] : [a], { C: !1 }, !1, b); this.l = a; } v(nj, H); nj.prototype.h = function (a, b) { return lj(a.N, this.l, new ib, a, b) }; function oj(a, b, c, d) { var e = b.map(function (g) { return g.jb }); b = b.map(function (g) { return g.name }); var f = e.reduce(function (g, k) { return g.add(k.o) }, c.o); H.call(this, f, e.concat(c), { C: !1 }, !1, d); this.s = a; this.A = b; this.K = e; this.S = c; this.l = null; } v(oj, H);
  			oj.prototype.h = function (a, b) {
  				var c = this, d = a, e = this.l.map(function (q, u) { var z = I(c.K[u], d, b).O(); u = {}; d = Nc(a, (u[q] = function () { return C.create(z) }, u)); return z }); if (e.some(function (q) { return 0 === q.length })) return "every" === this.s ? C.ba() : C.W(); var f = Array(e.length).fill(0); f[0] = -1; for (var g = !0; g;) {
  					g = !1; for (var k = 0, l = f.length; k < l; ++k) {
  						var m = e[k]; if (++f[k] > m.length - 1) f[k] = 0; else {
  							g = Object.create(null); k = {}; for (l = 0; l < f.length; k = { xb: k.xb }, l++)k.xb = e[l][f[l]], g[this.l[l]] = function (q) { return function () { return C.m(q.xb) } }(k);
  							g = Nc(a, g); g = I(this.S, g, b); if (g.ha() && "some" === this.s) return C.ba(); if (!g.ha() && "every" === this.s) return C.W(); g = !0; break
  						}
  					}
  				} return "every" === this.s ? C.ba() : C.W()
  			}; oj.prototype.v = function (a) { this.l = []; for (var b = 0, c = this.A.length; b < c; ++b) { this.K[b].v(a); Mg(a); var d = this.A[b], e = d.prefix ? a.aa(d.prefix) : null; d = Qg(a, e, d.localName); this.l[b] = d; } this.S.v(a); b = 0; for (c = this.A.length; b < c; ++b)Sg(a); }; function pj(a) { H.call(this, a, [], { C: !1 }); } v(pj, H); pj.prototype.h = function (a) { return this.l(a.N) ? C.ba() : C.W() }; function qj(a) { var b = {}; pj.call(this, new Hf((b.nodeType = 1, b))); this.s = a; } v(qj, pj); qj.prototype.l = function (a) { if (!B(a.type, 53)) return !1; a = a.value.node.nodeType; return 3 === this.s && 4 === a ? !0 : this.s === a }; qj.prototype.B = function () { return "type-" + this.s }; function rj(a, b) { b = void 0 === b ? { kind: null } : b; var c = a.prefix, d = a.namespaceURI; a = a.localName; var e = {}; "*" !== a && (e.nodeName = 1); e.nodeType = 1; pj.call(this, new Hf(e)); this.s = a; this.K = d; this.A = c; this.S = b.kind; } v(rj, pj);
  			rj.prototype.l = function (a) { var b = B(a.type, 54), c = B(a.type, 47); if (!b && !c) return !1; a = a.value; return null !== this.S && (1 === this.S && !b || 2 === this.S && !c) ? !1 : null === this.A && "" !== this.K && "*" === this.s ? !0 : "*" === this.A ? "*" === this.s ? !0 : this.s === a.node.localName : "*" !== this.s && this.s !== a.node.localName ? !1 : (a.node.namespaceURI || null) === (("" === this.A ? b ? this.K : null : this.K) || null) }; rj.prototype.B = function () { return "*" === this.s ? null === this.S ? "type-1-or-type-2" : "type-" + this.S : "name-" + this.s };
  			rj.prototype.v = function (a) { if (null === this.K && "*" !== this.A && (this.K = a.aa(this.A || "") || null, !this.K && this.A)) throw Error("XPST0081: The prefix " + this.A + " could not be resolved."); }; function sj(a) { var b = {}; pj.call(this, new Hf((b.nodeName = 1, b))); this.s = a; } v(sj, pj); sj.prototype.l = function (a) { return B(a.type, 57) && a.value.node.target === this.s }; sj.prototype.B = function () { return "type-7" }; function tj(a) { pj.call(this, new Hf({})); this.s = a; } v(tj, pj); tj.prototype.l = function (a) { return B(a.type, pb(this.s.prefix ? this.s.prefix + ":" + this.s.localName : this.s.localName)) }; function uj(a, b, c) { H.call(this, new Hf({}), [], { C: !1, R: "unsorted" }); this.A = c; this.s = b; this.K = a; this.l = null; } v(uj, H); uj.prototype.h = function (a, b) { if (!a.Aa[this.l]) { if (this.S) return this.S(a, b); throw Error("XQDY0054: The variable " + this.A + " is declared but not in scope."); } return a.Aa[this.l]() }; uj.prototype.v = function (a) { null === this.s && this.K && (this.s = a.aa(this.K)); this.l = a.ib(this.s || "", this.A); if (!this.l) throw Error("XPST0008, The variable " + this.A + " is not in scope."); if (a = a.Ia[this.l]) this.S = a; }; function vj(a, b) { var c = new Hf({}); ki.call(this, c, [a, b], { C: !1, X: !1, R: "unsorted", subtree: !1 }, b); this.A = a; } v(vj, ki); vj.prototype.K = function (a, b, c, d) { var e = this, f = null, g = null; return d({ next: function () { for (; ;) { if (!g) { var k = b.next(0); if (k.done) return x; f = k.value; g = I(e.A, f, c); } k = g.ha(); var l = f; g = f = null; if (k) return y(l) } } }) }; function wj(a) { this.type = a; } function xj(a) { this.type = "delete"; this.target = a; } v(xj, wj); xj.prototype.h = function (a) { var b = {}; return b.type = this.type, b.target = ig(this.target, a, !1), b }; function yj(a, b, c) { this.type = c; this.target = a; this.content = b; } v(yj, wj); yj.prototype.h = function (a) { var b = {}; return b.type = this.type, b.target = ig(this.target, a, !1), b.content = this.content.map(function (c) { return ig(c, a, !0) }), b }; function zj(a, b) { yj.call(this, a, b, "insertAfter"); } v(zj, yj); function Aj(a, b) { this.type = "insertAttributes"; this.target = a; this.content = b; } v(Aj, wj); Aj.prototype.h = function (a) { var b = {}; return b.type = this.type, b.target = ig(this.target, a, !1), b.content = this.content.map(function (c) { return ig(c, a, !0) }), b }; function Bj(a, b) { yj.call(this, a, b, "insertBefore"); } v(Bj, yj); function Cj(a, b) { yj.call(this, a, b, "insertIntoAsFirst"); } v(Cj, yj); function Dj(a, b) { yj.call(this, a, b, "insertIntoAsLast"); } v(Dj, yj); function Ej(a, b) { yj.call(this, a, b, "insertInto"); } v(Ej, yj); function Fj(a, b) { this.type = "rename"; this.target = a; this.o = b.Ca ? b : new zb(b.prefix, b.namespaceURI, b.localName); } v(Fj, wj); Fj.prototype.h = function (a) { var b = {}, c = {}; return c.type = this.type, c.target = ig(this.target, a, !1), c.newName = (b.prefix = this.o.prefix, b.namespaceURI = this.o.namespaceURI, b.localName = this.o.localName, b), c }; function Gj(a, b) { this.type = "replaceElementContent"; this.target = a; this.text = b; } v(Gj, wj); Gj.prototype.h = function (a) { var b = {}; return b.type = this.type, b.target = ig(this.target, a, !1), b.text = this.text ? ig(this.text, a, !0) : null, b }; function Hj(a, b) { this.type = "replaceNode"; this.target = a; this.o = b; } v(Hj, wj); Hj.prototype.h = function (a) { var b = {}; return b.type = this.type, b.target = ig(this.target, a, !1), b.replacement = this.o.map(function (c) { return ig(c, a, !0) }), b }; function Ij(a, b) { this.type = "replaceValue"; this.target = a; this.o = b; } v(Ij, wj); Ij.prototype.h = function (a) { var b = {}; return b.type = this.type, b.target = ig(this.target, a, !1), b["string-value"] = this.o, b }; function Jj(a, b) { return new Hj(a, b) } function Kj(a) { Af.call(this, new Hf({}), [a], { C: !1, R: "unsorted" }); this.l = a; } v(Kj, Af); Kj.prototype.s = function (a, b) { var c = Bf(this.l)(a, b), d = b.h, e, f; return { next: function () { if (!e) { var g = c.next(0); if (g.value.J.some(function (k) { return !B(k.type, 53) })) throw Error("XUTY0007: The target of a delete expression must be a sequence of zero or more nodes."); e = g.value.J; f = g.value.fa; } e = e.filter(function (k) { return Vb(d, k.value) }); return y({ fa: zf(e.map(function (k) { return new xj(k.value) }), f), J: [] }) } } }; function Lj(a, b, c, d, e, f) {
  				var g = b.h; a.reduce(function q(l, m) { if (B(m.type, 62)) return m.P.forEach(function (u) { return u().O().forEach(function (z) { return q(l, z) }) }), l; l.push(m); return l }, []).forEach(function (l, m, q) {
  					if (B(l.type, 47)) { if (e) throw f(l.value, g); c.push(l.value.node); } else if (B(l.type, 46) || B(l.type, 53) && 3 === l.value.node.nodeType) {
  						var u = B(l.type, 46) ? Pd(Yc(l, b).first(), 1).value : Qb(g, l.value); 0 !== m && B(q[m - 1].type, 46) && B(l.type, 46) ? (d.push({ data: " " + u, Ta: !0, nodeType: 3 }), e = !0) : u && (d.push({
  							data: "" + u,
  							Ta: !0, nodeType: 3
  						}), e = !0);
  					} else if (B(l.type, 55)) { var z = []; Pb(g, l.value).forEach(function (A) { return z.push($b(A)) }); e = Lj(z, b, c, d, e, f); } else if (B(l.type, 53)) d.push(l.value.node), e = !0; else { if (B(l.type, 60)) throw Tc(l.type); throw Error("Atomizing " + l.type + " is not implemented."); }
  				}); return e
  			} function Mj(a, b, c) { var d = [], e = [], f = !1; a.forEach(function (g) { f = Lj(g, b, d, e, f, c); }); return { attributes: d, Xa: e } } function Nj(a, b, c, d, e) { var f = []; switch (a) { case 4: d.length && f.push(new Aj(b, d)); e.length && f.push(new Cj(b, e)); break; case 5: d.length && f.push(new Aj(b, d)); e.length && f.push(new Dj(b, e)); break; case 3: d.length && f.push(new Aj(b, d)); e.length && f.push(new Ej(b, e)); break; case 2: d.length && f.push(new Aj(c, d)); e.length && f.push(new Bj(b, e)); break; case 1: d.length && f.push(new Aj(c, d)), e.length && f.push(new zj(b, e)); }return f }
  			function Oj(a, b, c) { Af.call(this, new Hf({}), [a, c], { C: !1, R: "unsorted" }); this.K = a; this.l = b; this.A = c; } v(Oj, Af);
  			Oj.prototype.s = function (a, b) {
  				var c = this, d = Bf(this.K)(a, b), e = Bf(this.A)(a, b), f = b.h, g, k, l, m, q, u; return {
  					next: function () {
  						if (!g) { var z = d.next(0), A = Mj([z.value.J], b, bf); g = A.attributes.map(function (D) { return { node: D, F: null } }); k = A.Xa.map(function (D) { return { node: D, F: null } }); l = z.value.fa; } if (!m) {
  							z = e.next(0); if (0 === z.value.J.length) throw mf(); if (3 <= c.l) { if (1 !== z.value.J.length) throw cf(); if (!B(z.value.J[0].type, 54) && !B(z.value.J[0].type, 55)) throw cf(); } else {
  								if (1 !== z.value.J.length) throw df(); if (!(B(z.value.J[0].type,
  									54) || B(z.value.J[0].type, 56) || B(z.value.J[0].type, 58) || B(z.value.J[0].type, 57))) throw df(); u = Vb(f, z.value.J[0].value, null); if (null === u) throw Error("XUDY0029: The target " + z.value.J[0].value.outerHTML + " for inserting a node before or after must have a parent.");
  							} m = z.value.J[0]; q = z.value.fa;
  						} if (g.length) {
  							if (3 <= c.l) { if (!B(m.type, 54)) throw Error("XUTY0022: An insert expression specifies the insertion of an attribute node into a document node."); } else if (1 !== u.node.nodeType) throw Error("XUDY0030: An insert expression specifies the insertion of an attribute node before or after a child of a document node.");
  							g.reduce(function (D, F) { var J = F.node.prefix || "", T = F.node.prefix || "", ia = F.node.namespaceURI, Ba = T ? m.value.node.lookupNamespaceURI(T) : null; if (Ba && Ba !== ia) throw kf(ia); if ((T = D[T]) && ia !== T) throw lf(ia); D[J] = F.node.namespaceURI; return D }, {});
  						} return y({ J: [], fa: zf(Nj(c.l, m.value, u ? u : null, g, k), l, q) })
  					}
  				}
  			}; function Pj() { return Sc("Casting not supported from given type to a single xs:string or xs:untypedAtomic or any of its derived types.") } var Qj = /([A-Z_a-z\xC0-\xD6\xD8-\xF6\xF8-\u02FF\u0370-\u037D\u037F-\u1FFF\u200C\u200D\u2070-\u218F\u2C00-\u2FEF\u3001-\uD7FF\uF900-\uFDCF\uFDF0-\uFFFD]|[\uD800-\uDB7F][\uDC00-\uDFFF])/, Rj = new RegExp(Qj.source + (new RegExp("(" + Qj.source + "|[-.0-9\u00b7\u0300-\u036f\u203f\u2040])")).source + "*", "g"); function Sj(a) { return (a = a.match(Rj)) ? 1 === a.length : !1 }
  			function Tj(a, b) { return Zc(b, a).Z({ m: function (c) { c = c.first(); if (B(c.type, 1) || B(c.type, 19)) { if (!Sj(c.value)) throw Error('XQDY0041: The value "' + c.value + '" of a name expressions cannot be converted to a NCName.'); return C.m(c) } throw Pj(); }, default: function () { throw Pj(); } }).value }
  			function Uj(a, b, c) { return Zc(c, b).Z({ m: function (d) { d = d.first(); if (B(d.type, 23)) return C.m(d); if (B(d.type, 1) || B(d.type, 19)) { d = d.value.split(":"); if (1 === d.length) d = d[0]; else { var e = d[0]; var f = a.aa(e); d = d[1]; } if (!Sj(d) || e && !Sj(e)) throw Hg(e ? e + ":" + d : d); if (e && !f) throw Hg(e + ":" + d); return C.m({ type: 23, value: new zb(e, f, d) }) } throw Pj(); }, default: function () { throw Pj(); } }).value } function Vj(a, b) { Af.call(this, new Hf({}), [a, b], { C: !1, R: "unsorted" }); this.A = a; this.K = b; this.l = void 0; } v(Vj, Af);
  			Vj.prototype.s = function (a, b) {
  				var c = this, d = Bf(this.A)(a, b), e = Bf(this.K)(a, b); return {
  					next: function () {
  						var f = d.next(0); var g = f.value.J; if (0 === g.length) throw mf(); if (1 !== g.length) throw gf(); if (!B(g[0].type, 54) && !B(g[0].type, 47) && !B(g[0].type, 57)) throw gf(); g = g[0]; var k = e.next(0); a: {
  							var l = c.l; var m = C.create(k.value.J); switch (g.type) {
  								case 54: l = Uj(l, b, m).next(0).value.value; if ((m = g.value.node.lookupNamespaceURI(l.prefix)) && m !== l.namespaceURI) throw kf(l.namespaceURI); break a; case 47: l = Uj(l, b, m).next(0).value.value;
  									if (l.namespaceURI && (m = g.value.node.lookupNamespaceURI(l.prefix)) && m !== l.namespaceURI) throw kf(l.namespaceURI); break a; case 57: l = Tj(b, m).next(0).value.value; l = new zb("", null, l); break a
  							}l = void 0;
  						} return y({ J: [], fa: zf([new Fj(g.value, l)], f.value.fa, k.value.fa) })
  					}
  				}
  			}; Vj.prototype.v = function (a) { this.l = Of(a); Af.prototype.v.call(this, a); }; function Wj(a, b, c) {
  				var d, e, f; return {
  					next: function () {
  						if (!d) { var g = c.next(0), k = Mj([g.value.J], a, lf); d = { attributes: k.attributes.map(function (l) { return { node: l, F: null } }), Xa: k.Xa.map(function (l) { return { node: l, F: null } }) }; e = g.value.fa; } k = b.next(0); if (0 === k.value.J.length) throw mf(); if (1 !== k.value.J.length) throw ff(); if (!(B(k.value.J[0].type, 54) || B(k.value.J[0].type, 47) || B(k.value.J[0].type, 56) || B(k.value.J[0].type, 58) || B(k.value.J[0].type, 57))) throw ff(); f = Vb(a.h, k.value.J[0].value, null); if (null === f) throw Error("XUDY0009: The target " +
  							k.value.J[0].value.outerHTML + " for replacing a node must have a parent."); g = k.value.J[0]; k = k.value.fa; if (B(g.type, 47)) { if (d.Xa.length) throw Error("XUTY0011: When replacing an attribute the new value must be zero or more attribute nodes."); d.attributes.reduce(function (l, m) { var q = m.node.prefix || ""; m = m.node.namespaceURI; var u = f.node.lookupNamespaceURI(q); if (u && u !== m) throw kf(m); if ((u = l[q]) && m !== u) throw lf(m); l[q] = m; return l }, {}); } else if (d.attributes.length) throw Error("XUTY0010: When replacing an an element, text, comment, or processing instruction node the new value must be a single node.");
  						return y({ J: [], fa: zf([Jj(g.value, [].concat(d.attributes, d.Xa))], e, k) })
  					}
  				}
  			}
  			function Xj(a, b, c) {
  				var d, e, f, g, k = !1; return {
  					next: function () {
  						if (k) return x; if (!f) { var l = c.next(0), m = Zc(C.create(l.value.J), a).map(function (q) { return Pd(q, 1) }).O().map(function (q) { return q.value }).join(" "); f = 0 === m.length ? null : { node: a.o.createTextNode(m), F: null }; g = l.value.fa; } if (!d) {
  							l = b.next(0); if (0 === l.value.J.length) throw mf(); if (1 !== l.value.J.length) throw ff(); if (!(B(l.value.J[0].type, 54) || B(l.value.J[0].type, 47) || B(l.value.J[0].type, 56) || B(l.value.J[0].type, 58) || B(l.value.J[0].type, 57))) throw ff();
  							d = l.value.J[0]; e = l.value.fa;
  						} if (B(d.type, 54)) return k = !0, y({ J: [], fa: zf([new Gj(d.value, f)], g, e) }); if (B(d.type, 47) || B(d.type, 56) || B(d.type, 58) || B(d.type, 57)) {
  							l = f ? Qb(a.h, f) : ""; if (B(d.type, 58) && (l.includes("--") || l.endsWith("-"))) throw Error('XQDY0072: The content "' + l + '" for a comment node contains two adjacent hyphens or ends with a hyphen.'); if (B(d.type, 57) && l.includes("?>")) throw Error('XQDY0026: The content "' + l + '" for a processing instruction node contains "?>".'); k = !0; return y({
  								J: [], fa: zf([new Ij(d.value,
  									l)], g, e)
  							})
  						}
  					}
  				}
  			} function Yj(a, b, c) { Af.call(this, new Hf({}), [b, c], { C: !1, R: "unsorted" }); this.K = a; this.l = b; this.A = c; } v(Yj, Af); Yj.prototype.s = function (a, b) { var c = Bf(this.l)(a, b); a = Bf(this.A)(a, b); return this.K ? Xj(b, c, a) : Wj(b, c, a) }; function Zj(a) {
  				switch (a.type) {
  					case "delete": return new xj({ node: a.target, F: null }); case "insertAfter": return new zj({ node: a.target, F: null }, a.content.map(function (b) { return { node: b, F: null } })); case "insertBefore": return new Bj({ node: a.target, F: null }, a.content.map(function (b) { return { node: b, F: null } })); case "insertInto": return new Ej({ node: a.target, F: null }, a.content.map(function (b) { return { node: b, F: null } })); case "insertIntoAsFirst": return new Cj({ node: a.target, F: null }, a.content.map(function (b) {
  						return {
  							node: b,
  							F: null
  						}
  					})); case "insertIntoAsLast": return new Dj({ node: a.target, F: null }, a.content.map(function (b) { return { node: b, F: null } })); case "insertAttributes": return new Aj({ node: a.target, F: null }, a.content.map(function (b) { return { node: b, F: null } })); case "rename": return new Fj({ node: a.target, F: null }, a.newName); case "replaceNode": return new Hj({ node: a.target, F: null }, a.replacement.map(function (b) { return { node: b, F: null } })); case "replaceValue": return new Ij({ node: a.target, F: null }, a["string-value"]); case "replaceElementContent": return new Gj({
  						node: a.target,
  						F: null
  					}, a.text ? { node: a.text, F: null } : null); default: throw Error('Unexpected type "' + a.type + '" when parsing a transferable pending update.');
  				}
  			} function ak(a, b, c) { if (b.find(function (e) { return Sd(e, a) })) return !0; var d = Vb(c, a); return d ? ak(d, b, c) : !1 } function bk(a, b, c) { Af.call(this, new Hf({}), a.reduce(function (d, e) { d.push(e.jb); return d }, [b, c]), { C: !1, R: "unsorted" }); this.l = a; this.K = b; this.A = c; this.I = null; } v(bk, Af); bk.prototype.h = function (a, b) { a = this.s(a, b); return Cf(a, function () { }) };
  			bk.prototype.s = function (a, b) {
  				var c = this, d = b.h, e = b.o, f = b.B, g = [], k, l, m, q = [], u = []; return {
  					next: function () {
  						if (q.length !== c.l.length) for (var z = {}, A = q.length; A < c.l.length; z = { lb: z.lb }, A++) {
  							var D = c.l[A], F = g[A]; F || (g[A] = F = Bf(D.jb)(a, b)); F = F.next(0); if (1 !== F.value.J.length || !B(F.value.J[0].type, 53)) throw Error("XUTY0013: The source expression of a copy modify expression must return a single node."); z.lb = $b(eg(F.value.J[0].value, b)); q.push(z.lb.value); u.push(F.value.fa); F = {}; a = Nc(a, (F[D.qc] = function (J) { return function () { return C.m(J.lb) } }(z),
  								F));
  						} m || (k || (k = Bf(c.K)(a, b)), m = k.next(0).value.fa); m.forEach(function (J) { if (J.target && !ak(J.target, q, d)) throw Error("XUDY0014: The target " + J.target.node.outerHTML + " must be a node created by the copy clause."); if ("put" === J.type) throw Error("XUDY0037: The modify expression of a copy modify expression can not contain a fn:put."); }); z = m.map(function (J) { J = J.h(b); return Zj(J) }); xf(z, d, e, f); l || (l = Bf(c.A)(a, b)); z = l.next(0); return y({ J: z.value.J, fa: zf.apply(null, [z.value.fa].concat(t(u))) })
  					}
  				}
  			};
  			bk.prototype.v = function (a) { Mg(a); this.l.forEach(function (b) { return b.qc = Qg(a, b.Rb.namespaceURI, b.Rb.localName) }); Af.prototype.v.call(this, a); Sg(a); this.I = this.l.some(function (b) { return b.jb.I }) || this.A.I; }; function ck(a, b) { return { node: { nodeType: 2, Ta: !0, nodeName: a.Ca(), namespaceURI: a.namespaceURI, prefix: a.prefix, localName: a.localName, name: a.Ca(), value: b }, F: null } } function dk(a, b) { var c = b.wb || []; c = c.concat(a.Oa || []); H.call(this, new Hf({}), c, { C: !1, R: "unsorted" }); a.Oa ? this.s = a.Oa : this.name = new zb(a.prefix, a.namespaceURI, a.localName); this.l = b; this.A = void 0; } v(dk, H);
  			dk.prototype.h = function (a, b) {
  				var c = this, d, e, f, g = !1; return C.create({
  					next: function () {
  						if (g) return x; if (!e) {
  							if (c.s) { if (!d) { var k = c.s.h(a, b); d = Uj(c.A, b, k); } e = d.next(0).value.value; } else e = c.name; if (e) {
  								if ("xmlns" === e.prefix) throw Cg(e); if ("" === e.prefix && "xmlns" === e.localName) throw Cg(e); if ("http://www.w3.org/2000/xmlns/" === e.namespaceURI) throw Cg(e); if ("xml" === e.prefix && "http://www.w3.org/XML/1998/namespace" !== e.namespaceURI) throw Cg(e); if ("" !== e.prefix && "xml" !== e.prefix && "http://www.w3.org/XML/1998/namespace" ===
  									e.namespaceURI) throw Cg(e);
  							}
  						} if (c.l.wb) return k = c.l.wb, f || (f = Pc(k.map(function (l) { return Zc(l.h(a, b), b).M(function (m) { return C.m(w(m.map(function (q) { return q.value }).join(" "), 1)) }) })).M(function (l) { return C.m($b(ck(e, l.map(function (m) { return m.value }).join("")))) }).value), f.next(0); g = !0; return y($b(ck(e, c.l.value)))
  					}
  				})
  			};
  			dk.prototype.v = function (a) { this.A = Of(a); if (this.name && this.name.prefix && !this.name.namespaceURI) { var b = a.aa(this.name.prefix); if (void 0 === b && this.name.prefix) throw Uc(this.name.prefix); this.name.namespaceURI = b || null; } H.prototype.v.call(this, a); }; function ek(a) { H.call(this, a ? a.o : new Hf({}), a ? [a] : [], { C: !1, R: "unsorted" }); this.l = a; } v(ek, H); ek.prototype.h = function (a, b) { var c = { data: "", Ta: !0, nodeType: 8 }, d = { node: c, F: null }; if (!this.l) return C.m($b(d)); a = I(this.l, a, b); return Zc(a, b).M(function (e) { e = e.map(function (f) { return Pd(f, 1).value }).join(" "); if (-1 !== e.indexOf("--\x3e")) throw Error('XQDY0072: The contents of the data of a comment may not include "--\x3e"'); c.data = e; return C.m($b(d)) }) }; function fk(a, b, c, d) { H.call(this, new Hf({}), d.concat(b).concat(a.Oa || []), { C: !1, R: "unsorted" }); a.Oa ? this.s = a.Oa : this.l = new zb(a.prefix, a.namespaceURI, a.localName); this.S = c.reduce(function (e, f) { if (f.prefix in e) throw Error("XQST0071: The namespace declaration with the prefix " + f.prefix + " has already been declared on the constructed element."); e[f.prefix || ""] = f.uri; return e }, {}); this.K = b; this.la = d; this.A = void 0; } v(fk, H);
  			fk.prototype.h = function (a, b) {
  				var c = this, d = !1, e, f, g = !1, k, l, m, q = !1; return C.create({
  					next: function () {
  						if (q) return x; d || (e || (e = Pc(c.K.map(function (J) { return I(J, a, b) }))), f = e.O(), d = !0); if (!g) { k || (k = c.la.map(function (J) { return I(J, a, b) })); for (var u = [], z = 0; z < k.length; z++) { var A = k[z].O(); u.push(A); } l = u; g = !0; } c.s && (m || (u = c.s.h(a, b), m = Uj(c.A, b, u)), u = m.next(0), c.l = u.value.value); if ("xmlns" === c.l.prefix || "http://www.w3.org/2000/xmlns/" === c.l.namespaceURI || "xml" === c.l.prefix && "http://www.w3.org/XML/1998/namespace" !==
  							c.l.namespaceURI || c.l.prefix && "xml" !== c.l.prefix && "http://www.w3.org/XML/1998/namespace" === c.l.namespaceURI) throw Error('XQDY0096: The node name "' + c.l.Ca() + '" is invalid for a computed element constructor.'); var D = { nodeType: 1, Ta: !0, attributes: [], childNodes: [], nodeName: c.l.Ca(), namespaceURI: c.l.namespaceURI, prefix: c.l.prefix, localName: c.l.localName }; u = { node: D, F: null }; f.forEach(function (J) { D.attributes.push(J.value.node); }); z = Mj(l, b, Bg); z.attributes.forEach(function (J) {
  								if (D.attributes.find(function (T) {
  									return T.namespaceURI ===
  										J.namespaceURI && T.localName === J.localName
  								})) throw Error("XQDY0025: The attribute " + J.name + " does not have an unique name in the constructed element."); D.attributes.push(J);
  							}); z.Xa.forEach(function (J) { D.childNodes.push(J); }); for (z = 0; z < D.childNodes.length; z++)if (A = D.childNodes[z], Kb(A) && 3 === A.nodeType) { var F = D.childNodes[z - 1]; F && Kb(F) && 3 === F.nodeType && (F.data += A.data, D.childNodes.splice(z, 1), z--); } q = !0; return y($b(u))
  					}
  				})
  			};
  			fk.prototype.v = function (a) {
  				var b = this; Mg(a); Object.keys(this.S).forEach(function (d) { return Pg(a, d, b.S[d]) }); this.ta.forEach(function (d) { return d.v(a) }); this.K.reduce(function (d, e) { if (e.name) { e = "Q{" + (null === e.name.namespaceURI ? a.aa(e.name.prefix) : e.name.namespaceURI) + "}" + e.name.localName; if (d.includes(e)) throw Error("XQST0040: The attribute " + e + " does not have an unique name in the constructed element."); d.push(e); } return d }, []); if (this.l && null === this.l.namespaceURI) {
  					var c = a.aa(this.l.prefix); if (void 0 ===
  						c && this.l.prefix) throw Uc(this.l.prefix); this.l.namespaceURI = c;
  				} this.A = Of(a); Sg(a);
  			}; function gk(a) { if (/^xml$/i.test(a)) throw Error('XQDY0064: The target of a created PI may not be "' + a + '"'); } function hk(a, b) { return { node: { data: b, Ta: !0, nodeName: a, nodeType: 7, target: a }, F: null } } function ik(a, b) { var c = a.Fb ? [a.Fb].concat(b) : [b]; H.call(this, c.reduce(function (d, e) { return d.add(e.o) }, new Hf({})), c, { C: !1, R: "unsorted" }); this.l = a; this.s = b; } v(ik, H);
  			ik.prototype.h = function (a, b) { var c = this, d = I(this.s, a, b); return Zc(d, b).M(function (e) { var f = e.map(function (k) { return Pd(k, 1).value }).join(" "); if (-1 !== f.indexOf("?>")) throw Error('XQDY0026: The contents of the data of a processing instruction may not include "?>"'); if (null !== c.l.Nb) return e = c.l.Nb, gk(e), C.m($b(hk(e, f))); e = I(c.l.Fb, a, b); var g = Tj(b, e); return C.create({ next: function () { var k = g.next(0); if (k.done) return k; k = k.value.value; gk(k); return y($b(hk(k, f))) } }) }) }; function jk(a) { H.call(this, a ? a.o : new Hf({}), a ? [a] : [], { C: !1, R: "unsorted" }); this.l = a; } v(jk, H); jk.prototype.h = function (a, b) { if (!this.l) return C.empty(); a = I(this.l, a, b); return Zc(a, b).M(function (c) { if (0 === c.length) return C.empty(); c = { node: { data: c.map(function (d) { return Pd(d, 1).value }).join(" "), Ta: !0, nodeType: 3 }, F: null }; return C.m($b(c)) }) }; function kk(a, b, c, d) { var e = new Hf({}), f; Df.call(this, e, (f = [a].concat(t(b.map(function (g) { return g.ic })), [c])).concat.apply(f, t(b.map(function (g) { return g.Pb.map(function (k) { return k.Ob }) }))), { C: !1, X: !1, R: "unsorted", subtree: !1 }, d); this.K = a; this.l = b.length; this.S = b.map(function (g) { return g.Pb }); } v(kk, Df);
  			kk.prototype.A = function (a, b, c) { var d = this; return c[0](a).M(function (e) { for (var f = 0; f < d.l; f++)if (d.S[f].some(function (g) { switch (g.oc) { case "?": if (1 < e.length) return !1; break; case "*": break; case "+": if (1 > e.length) return !1; break; default: if (1 !== e.length) return !1 }var k = C.create(e); return e.every(function (l, m) { l = Jc(a, m, l, k); return I(g.Ob, l, b).ha() }) })) return c[f + 1](a); return c[d.l + 1](a) }) }; kk.prototype.v = function (a) { Df.prototype.v.call(this, a); if (this.K.I) throw af(); }; var lk = { $: !1, ua: !1 }, mk = { $: !0, ua: !1 }, nk = { $: !0, ua: !0 }; function ok(a) { return a.$ ? a.ua ? nk : mk : lk }
  			function R(a, b) {
  				switch (a[0]) {
  					case "andOp": var c = M(a, "type"); return new wi(pk("andOp", a, ok(b)), c); case "orOp": return c = M(a, "type"), new xi(pk("orOp", a, ok(b)), c); case "unaryPlusOp": return c = L(L(a, "operand"), "*"), a = M(a, "type"), new vi("+", R(c, b), a); case "unaryMinusOp": return c = L(L(a, "operand"), "*"), a = M(a, "type"), new vi("-", R(c, b), a); case "addOp": case "subtractOp": case "multiplyOp": case "divOp": case "idivOp": case "modOp": var d = a[0], e = R(N(a, ["firstOperand", "*"]), ok(b)); b = R(N(a, ["secondOperand", "*"]), ok(b));
  						var f = M(a, "type"), g = M(N(a, ["firstOperand", "*"]), "type"), k = M(N(a, ["secondOperand", "*"]), "type"); g && k && M(a, "type") && (c = dh(d, g.type, k.type)); return new mh(d, e, b, f, c); case "sequenceExpr": return qk(a, b); case "unionOp": return c = M(a, "type"), new bj([R(N(a, ["firstOperand", "*"]), ok(b)), R(N(a, ["secondOperand", "*"]), ok(b))], c); case "exceptOp": case "intersectOp": return c = M(a, "type"), new Si(a[0], R(N(a, ["firstOperand", "*"]), ok(b)), R(N(a, ["secondOperand", "*"]), ok(b)), c); case "stringConcatenateOp": return rk(a, b);
  					case "rangeSequenceExpr": return sk(a, b); case "equalOp": case "notEqualOp": case "lessThanOrEqualOp": case "lessThanOp": case "greaterThanOrEqualOp": case "greaterThanOp": return tk("generalCompare", a, b); case "eqOp": case "neOp": case "ltOp": case "leOp": case "gtOp": case "geOp": return tk("valueCompare", a, b); case "isOp": case "nodeBeforeOp": case "nodeAfterOp": return tk("nodeCompare", a, b); case "pathExpr": return uk(a, b); case "contextItemExpr": return new gj(M(a, "type")); case "functionCallExpr": return vk(a, b); case "inlineFunctionExpr": return wk(a,
  						b); case "arrowExpr": return xk(a, b); case "dynamicFunctionInvocationExpr": return yk(a, b); case "namedFunctionRef": return b = L(a, "functionName"), c = M(a, "type"), a = Tg(N(a, ["integerConstantExpr", "value"])), new si(Ug(b), parseInt(a, 10), c); case "integerConstantExpr": return new qi(Tg(L(a, "value")), { type: 5, g: 3 }); case "stringConstantExpr": return new qi(Tg(L(a, "value")), { type: 1, g: 3 }); case "decimalConstantExpr": return new qi(Tg(L(a, "value")), { type: 4, g: 3 }); case "doubleConstantExpr": return new qi(Tg(L(a, "value")), {
  							type: 3,
  							g: 3
  						}); case "varRef": return a = Ug(L(a, "name")), new uj(a.prefix, a.namespaceURI, a.localName); case "flworExpr": return zk(a, b); case "quantifiedExpr": return Ak(a, b); case "ifThenElseExpr": return c = M(a, "type"), new hi(R(L(L(a, "ifClause"), "*"), ok(b)), R(L(L(a, "thenClause"), "*"), b), R(L(L(a, "elseClause"), "*"), b), c); case "instanceOfExpr": return c = R(N(a, ["argExpr", "*"]), b), d = N(a, ["sequenceType", "*"]), e = N(a, ["sequenceType", "occurrenceIndicator"]), a = M(a, "type"), new Yi(c, R(d, ok(b)), e ? Tg(e) : "", a); case "castExpr": return b =
  							R(L(L(a, "argExpr"), "*"), ok(b)), c = L(a, "singleType"), a = Ug(L(c, "atomicType")), c = null !== L(c, "optional"), new Wi(b, a, c); case "castableExpr": return b = R(L(L(a, "argExpr"), "*"), ok(b)), c = L(a, "singleType"), a = Ug(L(c, "atomicType")), c = null !== L(c, "optional"), new Vi(b, a, c); case "simpleMapExpr": return Bk(a, b); case "mapConstructor": return Ck(a, b); case "arrayConstructor": return Dk(a, b); case "unaryLookup": return c = M(a, "type"), new nj(Ek(a, b), c); case "typeswitchExpr": return Fk(a, b); case "elementConstructor": return Gk(a,
  								b); case "attributeConstructor": return Hk(a, b); case "computedAttributeConstructor": return (c = L(a, "tagName")) ? c = Ug(c) : (c = L(a, "tagNameExpr"), c = { Oa: R(L(c, "*"), ok(b)) }), a = R(L(L(a, "valueExpr"), "*"), ok(b)), new dk(c, { wb: [a] }); case "computedCommentConstructor": if (!b.$) throw Error("XPST0003: Use of XQuery functionality is not allowed in XPath context"); a = (a = L(a, "argExpr")) ? R(L(a, "*"), ok(b)) : null; return new ek(a); case "computedTextConstructor": if (!b.$) throw Error("XPST0003: Use of XQuery functionality is not allowed in XPath context");
  						a = (a = L(a, "argExpr")) ? R(L(a, "*"), ok(b)) : null; return new jk(a); case "computedElementConstructor": return Ik(a, b); case "computedPIConstructor": if (!b.$) throw Error("XPST0003: Use of XQuery functionality is not allowed in XPath context"); c = L(a, "piTargetExpr"); d = L(a, "piTarget"); e = L(a, "piValueExpr"); a = M(a, "type"); return new ik({ Fb: c ? R(L(c, "*"), ok(b)) : null, Nb: d ? Tg(d) : null }, e ? R(L(e, "*"), ok(b)) : new Ti([], a)); case "CDataSection": return new qi(Tg(a), { type: 1, g: 3 }); case "deleteExpr": return a = R(N(a, ["targetExpr",
  							"*"]), b), new Kj(a); case "insertExpr": c = R(N(a, ["sourceExpr", "*"]), b); e = O(a, "*")[1]; switch (e[0]) { case "insertAfter": d = 1; break; case "insertBefore": d = 2; break; case "insertInto": d = (d = L(e, "*")) ? "insertAsFirst" === d[0] ? 4 : 5 : 3; }a = R(N(a, ["targetExpr", "*"]), b); return new Oj(c, d, a); case "renameExpr": return c = R(N(a, ["targetExpr", "*"]), b), a = R(N(a, ["newNameExpr", "*"]), b), new Vj(c, a); case "replaceExpr": return c = !!L(a, "replaceValue"), d = R(N(a, ["targetExpr", "*"]), b), a = R(N(a, ["replacementExpr", "*"]), b), new Yj(c, d, a); case "transformExpr": return Jk(a,
  								b); case "x:stackTrace": c = a[1]; for (a = a[2]; "x:stackTrace" === a[0];)a = a[2]; return new ji(c, a[0], R(a, b)); default: return Kk(a)
  				}
  			}
  			function Kk(a) {
  				switch (a[0]) {
  					case "nameTest": return new rj(Ug(a)); case "piTest": return (a = L(a, "piTarget")) ? new sj(Tg(a)) : new qj(7); case "commentTest": return new qj(8); case "textTest": return new qj(3); case "documentTest": return new qj(9); case "attributeTest": var b = (a = L(a, "attributeName")) && L(a, "star"); return !a || b ? new qj(2) : new rj(Ug(L(a, "QName")), { kind: 2 }); case "elementTest": return b = (a = L(a, "elementName")) && L(a, "star"), !a || b ? new qj(1) : new rj(Ug(L(a, "QName")), { kind: 1 }); case "anyKindTest": return new tj({
  						prefix: "",
  						namespaceURI: null, localName: "node()"
  					}); case "anyMapTest": return new tj({ prefix: "", namespaceURI: null, localName: "map(*)" }); case "anyArrayTest": return new tj({ prefix: "", namespaceURI: null, localName: "array(*)" }); case "Wildcard": return L(a, "star") ? (b = L(a, "uri")) ? a = new rj({ localName: "*", namespaceURI: Tg(b), prefix: "" }) : (b = L(a, "NCName"), a = "star" === L(a, "*")[0] ? new rj({ localName: Tg(b), namespaceURI: null, prefix: "*" }) : new rj({ localName: "*", namespaceURI: null, prefix: Tg(b) })) : a = new rj({
  						localName: "*", namespaceURI: null,
  						prefix: "*"
  					}), a; case "atomicType": return new tj(Ug(a)); case "anyItemType": return new tj({ prefix: "", namespaceURI: null, localName: "item()" }); default: throw Error("No selector counterpart for: " + a[0] + ".");
  				}
  			} function Dk(a, b) { var c = M(a, "type"); a = L(a, "*"); var d = O(a, "arrayElem").map(function (e) { return R(L(e, "*"), ok(b)) }); switch (a[0]) { case "curlyArray": return new Kh(d, c); case "squareArray": return new Lh(d, c); default: throw Error("Unrecognized arrayType: " + a[0]); } }
  			function Ck(a, b) { var c = M(a, "type"); return new ri(O(a, "mapConstructorEntry").map(function (d) { return { key: R(N(d, ["mapKeyExpr", "*"]), ok(b)), value: R(N(d, ["mapValueExpr", "*"]), ok(b)) } }), c) } function pk(a, b, c) { function d(f) { var g = L(L(f, "firstOperand"), "*"); f = L(L(f, "secondOperand"), "*"); g[0] === a ? d(g) : e.push(R(g, c)); f[0] === a ? d(f) : e.push(R(f, c)); } var e = []; d(b); return e } function Ek(a, b) { a = L(a, "*"); switch (a[0]) { case "NCName": return new qi(Tg(a), { type: 1, g: 3 }); case "star": return "*"; default: return R(a, ok(b)) } }
  			function tk(a, b, c) { var d = N(b, ["firstOperand", "*"]), e = N(b, ["secondOperand", "*"]); d = R(d, ok(c)); c = R(e, ok(c)); switch (a) { case "valueCompare": return new Ji(b[0], d, c); case "nodeCompare": return new Qi(b[0], d, c); case "generalCompare": return new Ni(b[0], d, c) } }
  			function Lk(a, b, c) { a = O(a, "*"); return new ej(a.filter(function (d) { return "stable" !== d[0] }).map(function (d) { var e = L(d, "orderModifier"), f = e ? L(e, "orderingKind") : null; e = e ? L(e, "emptyOrderingMode") : null; f = f ? "ascending" === Tg(f) : !0; e = e ? "empty least" === Tg(e) : !0; return { ca: R(N(d, ["orderByExpr", "*"]), b), Kb: f, lc: e } }), c) }
  			function zk(a, b) {
  				var c = O(a, "*"); a = L(c[c.length - 1], "*"); c = c.slice(0, -1); if (1 < c.length && !b.$) throw Error("XPST0003: Use of XQuery FLWOR expressions in XPath is no allowed"); return c.reduceRight(function (d, e) {
  					switch (e[0]) {
  						case "forClause": e = O(e, "*"); for (var f = e.length - 1; 0 <= f; --f) { var g = e[f], k = N(g, ["forExpr", "*"]), l = L(g, "positionalVariableBinding"); d = new ni(Ug(N(g, ["typedVariableBinding", "varName"])), R(k, ok(b)), l ? Ug(l) : null, d); } return d; case "letClause": e = O(e, "*"); for (f = e.length - 1; 0 <= f; --f)g = e[f], k = N(g,
  							["letExpr", "*"]), d = new pi(Ug(N(g, ["typedVariableBinding", "varName"])), R(k, ok(b)), d); return d; case "whereClause": e = O(e, "*"); for (f = e.length - 1; 0 <= f; --f)d = new vj(R(e[f], b), d); return d; case "windowClause": throw Error("Not implemented: " + e[0] + " is not implemented yet."); case "groupByClause": throw Error("Not implemented: " + e[0] + " is not implemented yet."); case "orderByClause": return Lk(e, b, d); case "countClause": throw Error("Not implemented: " + e[0] + " is not implemented yet."); default: throw Error("Not implemented: " +
  								e[0] + " is not supported in a flwor expression");
  					}
  				}, R(a, b))
  			} function vk(a, b) { var c = L(a, "functionName"), d = O(L(a, "arguments"), "*"); a = M(a, "type"); return new Nf(new si(Ug(c), d.length, a), d.map(function (e) { return "argumentPlaceholder" === e[0] ? null : R(e, b) }), a) }
  			function xk(a, b) { var c = M(a, "type"), d = N(a, ["argExpr", "*"]); a = O(a, "*").slice(1); d = [R(d, b)]; for (var e = 0; e < a.length; e++)if ("arguments" !== a[e][0]) { if ("arguments" === a[e + 1][0]) { var f = O(a[e + 1], "*"); d = d.concat(f.map(function (g) { return "argumentPlaceholder" === g[0] ? null : R(g, b) })); } f = "EQName" === a[e][0] ? new si(Ug(a[e]), d.length, c) : R(a[e], ok(b)); d = [new Nf(f, d, c)]; } return d[0] }
  			function yk(a, b) { var c = N(a, ["functionItem", "*"]), d = M(a, "type"); a = L(a, "arguments"); var e = []; a && (e = O(a, "*").map(function (f) { return "argumentPlaceholder" === f[0] ? null : R(f, b) })); return new Nf(R(c, b), e, d) } function wk(a, b) { var c = O(L(a, "paramList"), "*"), d = N(a, ["functionBody", "*"]), e = M(a, "type"); return new oi(c.map(function (f) { return { name: Ug(L(f, "varName")), type: Vg(f) } }), Vg(a), d ? R(d, b) : new Ti([], e)) }
  			function uk(a, b) {
  				var c = M(a, "type"), d = O(a, "stepExpr"), e = !1, f = d.map(function (g) {
  					var k = L(g, "xpathAxis"), l = O(g, "*"), m = [], q = null; l = p(l); for (var u = l.next(); !u.done; u = l.next())switch (u = u.value, u[0]) { case "lookup": m.push(["lookup", Ek(u, b)]); break; case "predicate": case "predicates": u = p(O(u, "*")); for (var z = u.next(); !z.done; z = u.next())z = R(z.value, ok(b)), q = Qh(q, z.B()), m.push(["predicate", z]); }if (k) switch (e = !0, g = L(g, "attributeTest anyElementTest piTest documentTest elementTest commentTest namespaceTest anyKindTest textTest anyFunctionTest typedFunctionTest schemaAttributeTest atomicType anyItemType parenthesizedItemType typedMapTest typedArrayTest nameTest Wildcard".split(" ")),
  						g = Kk(g), Tg(k)) { case "ancestor": var A = new Oh(g, { Ra: !1 }); break; case "ancestor-or-self": A = new Oh(g, { Ra: !0 }); break; case "attribute": A = new Rh(g, q); break; case "child": A = new Sh(g, q); break; case "descendant": A = new Vh(g, { Ra: !1 }); break; case "descendant-or-self": A = new Vh(g, { Ra: !0 }); break; case "parent": A = new bi(g, q); break; case "following-sibling": A = new ai(g, q); break; case "preceding-sibling": A = new fi(g, q); break; case "following": A = new Zh(g); break; case "preceding": A = new di(g); break; case "self": A = new gi(g, q); } else A =
  							N(g, ["filterExpr", "*"]), A = R(A, ok(b)); m = p(m); for (k = m.next(); !k.done; k = m.next())switch (k = k.value, k[0]) { case "lookup": A = new mj(A, k[1]); break; case "predicate": A = new jj(A, k[1]); }A.type = c; return A
  				}); a = L(a, "rootExpr"); d = e || null !== a || 1 < d.length; if (!d && 1 === f.length || !a && 1 === f.length && "sorted" === f[0].da) return f[0]; if (a && 0 === f.length) return new fj(null); f = new ij(f, d); return a ? new fj(f) : f
  			}
  			function Ak(a, b) { var c = M(a, "type"), d = Tg(L(a, "quantifier")), e = N(a, ["predicateExpr", "*"]); a = O(a, "quantifiedExprInClause").map(function (f) { var g = Ug(N(f, ["typedVariableBinding", "varName"])); f = N(f, ["sourceExpr", "*"]); return { name: g, jb: R(f, ok(b)) } }); return new oj(d, a, R(e, ok(b)), c) } function qk(a, b) { var c = O(a, "*").map(function (d) { return R(d, b) }); if (1 === c.length) return c[0]; c = M(a, "type"); return new Ti(O(a, "*").map(function (d) { return R(d, b) }), c) }
  			function Bk(a, b) { var c = M(a, "type"); return O(a, "*").reduce(function (d, e) { return null === d ? R(e, ok(b)) : new Ui(d, R(e, ok(b)), c) }, null) } function rk(a, b) { var c = M(a, "type"); a = [N(a, ["firstOperand", "*"]), N(a, ["secondOperand", "*"])]; return new Nf(new si({ localName: "concat", namespaceURI: "http://www.w3.org/2005/xpath-functions", prefix: "" }, a.length, c), a.map(function (d) { return R(d, ok(b)) }), c) }
  			function sk(a, b) { var c = M(a, "type"); a = [L(L(a, "startExpr"), "*"), L(L(a, "endExpr"), "*")]; var d = new si({ localName: "to", namespaceURI: "http://fontoxpath/operators", prefix: "" }, a.length, c); return new Nf(d, a.map(function (e) { return R(e, ok(b)) }), c) }
  			function Gk(a, b) { if (!b.$) throw Error("XPST0003: Use of XQuery functionality is not allowed in XPath context"); var c = Ug(L(a, "tagName")), d = L(a, "attributeList"), e = d ? O(d, "attributeConstructor").map(function (f) { return R(f, ok(b)) }) : []; d = d ? O(d, "namespaceDeclaration").map(function (f) { var g = L(f, "prefix"); return { prefix: g ? Tg(g) : "", uri: Tg(L(f, "uri")) } }) : []; a = (a = L(a, "elementContent")) ? O(a, "*").map(function (f) { return R(f, ok(b)) }) : []; return new fk(c, e, d, a) }
  			function Hk(a, b) { if (!b.$) throw Error("XPST0003: Use of XQuery functionality is not allowed in XPath context"); var c = Ug(L(a, "attributeName")), d = L(a, "attributeValue"); d = d ? Tg(d) : null; a = (a = L(a, "attributeValueExpr")) ? O(a, "*").map(function (e) { return R(e, ok(b)) }) : null; return new dk(c, { value: d, wb: a }) } function Ik(a, b) { var c = L(a, "tagName"); c ? c = Ug(c) : (c = L(a, "tagNameExpr"), c = { Oa: R(L(c, "*"), ok(b)) }); a = (a = L(a, "contentExpr")) ? O(a, "*").map(function (d) { return R(d, ok(b)) }) : []; return new fk(c, [], [], a) }
  			function Jk(a, b) { var c = O(L(a, "transformCopies"), "transformCopy").map(function (e) { var f = Ug(L(L(e, "varRef"), "name")); return { jb: R(L(L(e, "copySource"), "*"), b), Rb: new zb(f.prefix, f.namespaceURI, f.localName) } }), d = R(L(L(a, "modifyExpr"), "*"), b); a = R(L(L(a, "returnExpr"), "*"), b); return new bk(c, d, a) }
  			function Fk(a, b) {
  				if (!b.$) throw Error("XPST0003: Use of XQuery functionality is not allowed in XPath context"); var c = M(a, "type"), d = R(L(L(a, "argExpr"), "*"), b), e = O(a, "typeswitchExprCaseClause").map(function (f) { var g = 0 === O(f, "sequenceTypeUnion").length ? [L(f, "sequenceType")] : O(L(f, "sequenceTypeUnion"), "sequenceType"); return { ic: R(N(f, ["resultExpr", "*"]), b), Pb: g.map(function (k) { var l = L(k, "occurrenceIndicator"); return { oc: l ? Tg(l) : "", Ob: R(L(k, "*"), b) } }) } }); a = R(N(a, ["typeswitchExprDefaultClause", "resultExpr",
  					"*"]), b); return new kk(d, e, a, c)
  			} function Mk(a, b) { return R(a, b) } var Nk = new Map; function Ok(a, b, c, d, e, f) { this.B = a; this.l = b; this.h = c; this.v = d; this.o = e; this.s = f; }
  			function Pk(a, b, c, d, e, f, g, k) { a = Nk.get(a); if (!a) return null; b = a[b + (f ? "_DEBUG" : "")]; return b ? (b = b.find(function (l) { return l.o === g && l.B.every(function (m) { return c(m.prefix) === m.namespaceURI }) && l.l.every(function (m) { return void 0 !== d[m.name] }) && l.v.every(function (m) { return e[m.prefix] === m.namespaceURI }) && l.s.every(function (m) { var q = k(m.nc, m.arity); return q && q.namespaceURI === m.Lb.namespaceURI && q.localName === m.Lb.localName }) })) ? { ca: b.h, rc: !1 } : null : null }
  			function Qk(a, b, c, d, e, f, g) { var k = Nk.get(a); k || (k = Object.create(null), Nk.set(a, k)); a = b + (f ? "_DEBUG" : ""); (b = k[a]) || (b = k[a] = []); b.push(new Ok(Object.values(c.h), Object.values(c.o), e, Object.keys(d).map(function (l) { return { namespaceURI: d[l], prefix: l } }), g, c.B)); } function Rk(a) {
  				var b = new Gb; if ("http://www.w3.org/2005/XQueryX" !== a.namespaceURI && "http://www.w3.org/2005/XQueryX" !== a.namespaceURI && "http://fontoxml.com/fontoxpath" !== a.namespaceURI && "http://www.w3.org/2007/xquery-update-10" !== a.namespaceURI) throw Sc("The XML structure passed as an XQueryX program was not valid XQueryX"); var c = ["stackTrace" === a.localName ? "x:stackTrace" : a.localName], d = b.getAllAttributes(a); d && 0 < d.length && c.push(Array.from(d).reduce(function (e, f) {
  					"start" === f.localName || "end" === f.localName &&
  						"stackTrace" === a.localName ? e[f.localName] = JSON.parse(f.value) : "type" === f.localName ? e[f.localName] = qb(f.value) : e[f.localName] = f.value; return e
  				}, {})); b = b.getChildNodes(a); b = p(b); for (d = b.next(); !d.done; d = b.next())switch (d = d.value, d.nodeType) { case 1: c.push(Rk(d)); break; case 3: c.push(d.data); }return c
  			} var Sk = Object.create(null); function Tk(a, b) { var c = Sk[a]; c || (c = Sk[a] = { Ma: [], Va: [], ra: null, source: b.source }); var d = c.ra || function () { }; c.Ma = c.Ma.concat(b.Ma); c.Va = c.Va.concat(b.Va); c.ra = function (e) { d(e); b.ra && b.ra(e); }; } function Uk(a, b) { var c = Sk[b]; if (!c) throw Error("XQST0051: No modules found with the namespace uri " + b); c.Ma.forEach(function (d) { d.hb && Og(a, b, d.localName, d.arity, d); }); c.Va.forEach(function (d) { Qg(a, b, d.localName); Rg(a, b, d.localName, function (e, f) { return I(d.ca, e, f) }); }); }
  			function Vk() { Object.keys(Sk).forEach(function (a) { a = Sk[a]; if (a.ra) try { a.ra(a); } catch (b) { a.ra = null, pg(a.source, b); } a.ra = null; }); } function hl(a) { return a.replace(/(\x0D\x0A)|(\x0D(?!\x0A))/g, String.fromCharCode(10)) } var S = prsc; function il(a, b) { return function (c, d) { if (b.has(d)) return b.get(d); c = a(c, d); b.set(d, c); return c } } function U(a, b) { return S.delimited(b, a, b) } function V(a, b) { return a.reverse().reduce(function (c, d) { return S.preceded(d, c) }, b) } function jl(a, b, c, d) { return S.then(S.then(a, b, function (e, f) { return [e, f] }), c, function (e, f) { var g = p(e); e = g.next().value; g = g.next().value; return d(e, g, f) }) }
  			function kl(a, b, c, d, e) { return S.then(S.then(S.then(a, b, function (f, g) { return [f, g] }), c, function (f, g) { var k = p(f); f = k.next().value; k = k.next().value; return [f, k, g] }), d, function (f, g) { var k = p(f); f = k.next().value; var l = k.next().value; k = k.next().value; return e(f, l, k, g) }) }
  			function ll(a, b, c, d, e, f) { return S.then(S.then(S.then(S.then(a, b, function (g, k) { return [g, k] }), c, function (g, k) { var l = p(g); g = l.next().value; l = l.next().value; return [g, l, k] }), d, function (g, k) { var l = p(g); g = l.next().value; var m = l.next().value; l = l.next().value; return [g, m, l, k] }), e, function (g, k) { var l = p(g); g = l.next().value; var m = l.next().value, q = l.next().value; l = l.next().value; return f(g, m, q, l, k) }) } function ml(a) { return S.map(a, function (b) { return [b] }) }
  			function nl(a, b) { return S.map(S.or(a), function () { return b }) } function ol(a) { return function (b, c) { return (b = a.exec(b.substring(c))) && 0 === b.index ? S.okWithValue(c + b[0].length, b[0]) : S.error(c, [a.source], !1) } } var pl = S.or([S.token(" "), S.token("\t"), S.token("\r"), S.token("\n")]), ql = S.token("(:"), rl = S.token(":)"), sl = S.token("(#"), tl = S.token("#)"), ul = S.token("("), vl = S.token(")"), wl = S.token("["), xl = S.token("]"), yl = S.token("{"), zl = S.token("}"), Al = S.token("{{"), Bl = S.token("}}"), Cl = S.token("'"), Dl = S.token("''"), El = S.token('"'), Fl = S.token('""'), Gl = S.token("<![CDATA["), Hl = S.token("]]\x3e"), Il = S.token("/>"), Jl = S.token("</"), Kl = S.token("\x3c!--"), Ll = S.token("--\x3e"), Ml = S.token("<?"), Nl = S.token("?>"), Ol = S.token("&#x"),
  				Pl = S.token("&#"), Ql = S.token(":*"), Rl = S.token("*:"), Sl = S.token(":="), Tl = S.token("&"), Ul = S.token(":"), Vl = S.token(";"), Wl = S.token("*"), Xl = S.token("@"), Yl = S.token("$"), Zl = S.token("#"), $l = S.token("%"), am = S.token("?"), bm = S.token("="), cm = S.followed(S.token("!"), S.not(S.peek(bm), [])), dm = S.followed(S.token("|"), S.not(S.peek(S.token("|")), [])), em = S.token("||"), fm = S.token("!="), gm = S.token("<"), hm = S.token("<<"), im = S.token("<="), jm = S.token(">"), km = S.token(">>"), lm = S.token(">="), mm = S.token(","), nm = S.token("."),
  				om = S.token(".."), pm = S.token("+"), qm = S.token("-"), rm = S.token("/"), sm = S.token("//"), tm = S.token("=>"), um = S.token("e"), vm = S.token("E"); S.token("l"); S.token("L"); S.token("m"); S.token("M"); var wm = S.token("Q"); S.token("x"); S.token("X");
  			var xm = S.token("as"), ym = S.token("cast"), zm = S.token("castable"), Am = S.token("treat"), Bm = S.token("instance"), Cm = S.token("of"), Dm = S.token("node"), Em = S.token("nodes"), Fm = S.token("delete"), Gm = S.token("value"), Hm = S.token("function"), Im = S.token("map"), Jm = S.token("element"), Km = S.token("attribute"), Lm = S.token("schema-element"), Mm = S.token("intersect"), Nm = S.token("except"), Om = S.token("union"), Pm = S.token("to"), Qm = S.token("is"), Rm = S.token("or"), Sm = S.token("and"), Tm = S.token("div"), Um = S.token("idiv"), Vm = S.token("mod"),
  				Wm = S.token("eq"), Xm = S.token("ne"), Ym = S.token("lt"), Zm = S.token("le"), $m = S.token("gt"), an = S.token("ge"), bn = S.token("amp"), cn = S.token("quot"), dn = S.token("apos"), en = S.token("if"), fn = S.token("then"), gn = S.token("else"), hn = S.token("allowing"), jn = S.token("empty"), kn = S.token("at"), ln = S.token("in"), mn = S.token("for"), nn = S.token("let"), on = S.token("where"), pn = S.token("collation"), qn = S.token("group"), rn = S.token("by"), sn = S.token("order"), tn = S.token("stable"), un = S.token("return"), vn = S.token("array"), wn = S.token("document"),
  				xn = S.token("namespace"), yn = S.token("text"), zn = S.token("comment"), An = S.token("processing-instruction"), Bn = S.token("lax"), Cn = S.token("strict"), Dn = S.token("validate"), En = S.token("type"), Fn = S.token("declare"), Gn = S.token("default"), Hn = S.token("boundary-space"), In = S.token("strip"), Jn = S.token("preserve"), Kn = S.token("no-preserve"), Ln = S.token("inherit"), Mn = S.token("no-inherit"), Nn = S.token("greatest"), On = S.token("least"), Pn = S.token("copy-namespaces"), Qn = S.token("decimal-format"), Rn = S.token("case"), Sn = S.token("typeswitch"),
  				Tn = S.token("some"), Un = S.token("every"), Vn = S.token("satisfies"), Wn = S.token("replace"), Xn = S.token("with"), Yn = S.token("copy"), Zn = S.token("modify"), $n = S.token("first"), ao = S.token("last"), bo = S.token("before"), co = S.token("after"), eo = S.token("into"), fo = S.token("insert"), go = S.token("rename"), ho = S.token("switch"), io = S.token("variable"), jo = S.token("external"), ko = S.token("updating"), lo = S.token("import"), mo = S.token("schema"), no = S.token("module"), oo = S.token("base-uri"), po = S.token("construction"), qo = S.token("ordering"),
  				ro = S.token("ordered"), so = S.token("unordered"), to = S.token("option"), uo = S.token("context"), vo = S.token("item"), wo = S.token("xquery"), xo = S.token("version"), yo = S.token("encoding"), zo = S.token("document-node"), Ao = S.token("namespace-node"), Bo = S.token("schema-attribute"), Co = S.token("ascending"), Do = S.token("descending"), Eo = S.token("empty-sequence"), Fo = S.token("child::"), Go = S.token("descendant::"), Ho = S.token("attribute::"), Io = S.token("self::"), Jo = S.token("descendant-or-self::"), Ko = S.token("following-sibling::"),
  				Lo = S.token("following::"), Mo = S.token("parent::"), No = S.token("ancestor::"), Oo = S.token("preceding-sibling::"), Po = S.token("preceding::"), Qo = S.token("ancestor-or-self::"), Ro = S.token("decimal-separator"), So = S.token("grouping-separator"), To = S.token("infinity"), Uo = S.token("minus-sign"), Vo = S.token("NaN"), Wo = S.token("per-mille"), Xo = S.token("zero-digit"), Yo = S.token("digit"), Zo = S.token("pattern-separator"), $o = S.token("exponent-separator"), ap = S.token("schema-attribute("), bp = S.token("document-node("), cp = S.token("processing-instruction("),
  				dp = S.token("processing-instruction()"), ep = S.token("comment()"), fp = S.token("text()"), gp = S.token("namespace-node()"), hp = S.token("node()"), ip = S.token("item()"), jp = S.token("empty-sequence()"); var kp = new Map, lp = new Map, mp = S.or([ol(/[\t\n\r -\uD7FF\uE000\uFFFD]/), ol(/[\uD800-\uDBFF][\uDC00-\uDFFF]/)]), np = S.preceded(S.peek(S.not(S.or([ql, rl]), ['comment contents cannot contain "(:" or ":)"'])), mp), op = S.map(S.delimited(ql, S.star(S.or([np, function (a, b) { return op(a, b) }])), rl, !0), function (a) { return a.join("") }), pp = S.or([pl, op]), qp = S.map(S.plus(pl), function (a) { return a.join("") }), W = il(S.map(S.star(pp), function (a) { return a.join("") }), kp), X = il(S.map(S.plus(pp), function (a) { return a.join("") }), lp); var rp = S.or([ol(/[A-Z_a-z\xC0-\xD6\xD8-\xF6\xF8-\u02FF\u0370-\u037D\u037F-\u1FFF\u200C\u200D\u2070-\u218F\u2C00-\u2FEF\u3001-\uD7FF\uF900-\uFDCF\uFDF0-\uFFFD]/), S.then(ol(/[\uD800-\uDB7F]/), ol(/[\uDC00-\uDFFF]/), function (a, b) { return a + b })]), sp = S.or([rp, ol(/[\-\.0-9\xB7\u0300-\u036F\u203F\u2040]/)]), tp = S.then(rp, S.star(sp), function (a, b) { return a + b.join("") }), up = S.map(tp, function (a) { return ["prefix", a] }), vp = S.or([rp, Ul]), wp = S.or([sp, Ul]); S.then(vp, S.star(wp), function (a, b) { return a + b.join("") });
  			var xp = S.map(tp, function (a) { var b = {}; return [(b.prefix = "", b.URI = null, b), a] }), yp = S.then(tp, S.preceded(Ul, tp), function (a, b) { var c = {}; return [(c.prefix = a, c.URI = null, c), b] }), zp = S.or([yp, xp]), Ap = S.followed(V([wm, W, yl], S.map(S.star(ol(/[^{}]/)), function (a) { return a.join("").replace(/\s+/g, " ").trim() })), zl), Bp = S.then(Ap, tp, function (a, b) { return [a, b] }), Cp = S.or([S.map(Bp, function (a) { var b = {}; return [(b.prefix = null, b.URI = a[0], b), a[1]] }), zp]), Dp = S.or([S.map(Cp, function (a) { return ["QName"].concat(t(a)) }), S.map(Wl,
  				function () { return ["star"] })]), Ep = S.map(S.preceded(Yl, Cp), function (a) { return ["varRef", ["name"].concat(t(a))] }); var Fp = S.peek(S.or([ul, El, Cl, pp])), Gp = S.map(S.or([Fo, Go, Ho, Io, Jo, Ko, Lo]), function (a) { return a.substring(0, a.length - 2) }), Hp = S.map(S.or([Mo, No, Oo, Po, Qo]), function (a) { return a.substring(0, a.length - 2) }), Ip = jl(Tl, S.or([Ym, $m, bn, cn, dn]), Vl, function (a, b, c) { return a + b + c }), Jp = S.or([jl(Ol, ol(/[0-9a-fA-F]+/), Vl, function (a, b, c) { return a + b + c }), jl(Pl, ol(/[0-9]+/), Vl, function (a, b, c) { return a + b + c })]), Kp = nl([Fl], '"'), Lp = nl([Dl], "'"), Mp = ml(nl([ep], "commentTest")), Np = ml(nl([fp], "textTest")), Op = ml(nl([gp], "namespaceTest")),
  					Pp = ml(nl([hp], "anyKindTest")), Qp = ol(/[0-9]+/), Rp = S.then(S.or([S.then(nm, Qp, function (a, b) { return a + b }), S.then(Qp, S.optional(S.preceded(nm, ol(/[0-9]*/))), function (a, b) { return a + (null !== b ? "." + b : "") })]), jl(S.or([um, vm]), S.optional(S.or([pm, qm])), Qp, function (a, b, c) { return a + (b ? b : "") + c }), function (a, b) { return ["doubleConstantExpr", ["value", a + b]] }), Sp = S.or([S.map(S.preceded(nm, Qp), function (a) { return ["decimalConstantExpr", ["value", "." + a]] }), S.then(S.followed(Qp, nm), S.optional(Qp), function (a, b) {
  						return ["decimalConstantExpr",
  							["value", a + "." + (null !== b ? b : "")]]
  					})]), Tp = S.map(Qp, function (a) { return ["integerConstantExpr", ["value", a]] }), Up = S.followed(S.or([Rp, Sp, Tp]), S.peek(S.not(ol(/[a-zA-Z]/), ["no alphabetical characters after numeric literal"]))), Vp = S.map(S.followed(nm, S.peek(S.not(nm, ["context item should not be followed by another ."]))), function () { return ["contextItemExpr"] }), Wp = S.or([vn, Km, zn, zo, Jm, Eo, Hm, en, vo, Im, Ao, Dm, An, Bo, Lm, ho, yn, Sn]), Xp = ml(nl([am], "argumentPlaceholder")), Yp = S.or([am, Wl, pm]), Zp = S.preceded(S.peek(S.not(ol(/[{}<&]/),
  						["elementContentChar cannot be {, }, <, or &"])), mp), $p = S.map(S.delimited(Gl, S.star(S.preceded(S.peek(S.not(Hl, ['CDataSection content may not contain "]]\x3e"'])), mp)), Hl, !0), function (a) { return ["CDataSection", a.join("")] }), aq = S.preceded(S.peek(S.not(ol(/["{}<&]/), ['quotAttrValueContentChar cannot be ", {, }, <, or &'])), mp), bq = S.preceded(S.peek(S.not(ol(/['{}<&]/), ["aposAttrValueContentChar cannot be ', {, }, <, or &"])), mp), cq = S.map(S.star(S.or([S.preceded(S.peek(S.not(qm, [])), mp), S.map(V([qm, S.peek(S.not(qm,
  							[]))], mp), function (a) { return "-" + a })])), function (a) { return a.join("") }), dq = S.map(S.delimited(Kl, cq, Ll, !0), function (a) { return ["computedCommentConstructor", ["argExpr", ["stringConstantExpr", ["value", a]]]] }), eq = S.filter(tp, function (a) { return "xml" !== a.toLowerCase() }, ['A processing instruction target cannot be "xml"']), fq = S.map(S.star(S.preceded(S.peek(S.not(Nl, [])), mp)), function (a) { return a.join("") }), gq = S.then(S.preceded(Ml, S.cut(eq)), S.cut(S.followed(S.optional(S.preceded(qp, fq)), Nl)), function (a, b) {
  								return ["computedPIConstructor",
  									["piTarget", a], ["piValueExpr", ["stringConstantExpr", ["value", b]]]]
  							}), hq = S.map(sm, function () { return ["stepExpr", ["xpathAxis", "descendant-or-self"], ["anyKindTest"]] }), iq = S.or([Bn, Cn]), jq = S.map(S.star(S.followed(mp, S.peek(S.not(tl, ["Pragma contents should not contain '#)'"])))), function (a) { return a.join("") }), kq = S.map(S.followed(S.or([Wm, Xm, Ym, Zm, $m, an]), Fp), function (a) { return a + "Op" }), lq = S.or([S.followed(nl([Qm], "isOp"), Fp), nl([hm], "nodeBeforeOp"), nl([km], "nodeAfterOp")]), mq = S.or([nl([bm], "equalOp"),
  							nl([fm], "notEqualOp"), nl([im], "lessThanOrEqualOp"), nl([gm], "lessThanOp"), nl([lm], "greaterThanOrEqualOp"), nl([jm], "greaterThanOp")]), nq = S.map(ko, function () { return ["annotation", ["annotationName", "updating"]] }), oq = S.or([Jn, Kn]), pq = S.or([Ln, Mn]), qq = S.or([Ro, So, To, Uo, Vo, $l, Wo, Xo, Yo, Zo, $o]), rq = S.map(V([Fn, X, Hn, X], S.or([Jn, In])), function (a) { return ["boundarySpaceDecl", a] }), sq = S.map(V([Fn, X, po, X], S.or([Jn, In])), function (a) { return ["constructionDecl", a] }), tq = S.map(V([Fn, X, qo, X], S.or([ro, so])), function (a) {
  								return ["orderingModeDecl",
  									a]
  							}), uq = S.map(V([Fn, X, Gn, X, sn, X, jn, X], S.or([Nn, On])), function (a) { return ["emptyOrderDecl", a] }), vq = S.then(V([Fn, X, Pn, X], oq), V([W, mm, W], pq), function (a, b) { return ["copyNamespacesDecl", ["preserveMode", a], ["inheritMode", b]] }); function wq(a) {
  								switch (a[0]) { case "constantExpr": case "varRef": case "contextItemExpr": case "functionCallExpr": case "sequenceExpr": case "elementConstructor": case "computedElementConstructor": case "computedAttributeConstructor": case "computedDocumentConstructor": case "computedTextConstructor": case "computedCommentConstructor": case "computedNamespaceConstructor": case "computedPIConstructor": case "orderedExpr": case "unorderedExpr": case "namedFunctionRef": case "inlineFunctionExpr": case "dynamicFunctionInvocationExpr": case "mapConstructor": case "arrayConstructor": case "stringConstructor": case "unaryLookup": return a }return ["sequenceExpr",
  									a]
  							} function xq(a) { if (!(1 <= a && 55295 >= a || 57344 <= a && 65533 >= a || 65536 <= a && 1114111 >= a)) throw Error("XQST0090: The character reference " + a + " (" + a.toString(16) + ") does not reference a valid codePoint."); }
  			function yq(a) { return a.replace(/(&[^;]+);/g, function (b) { if (/^&#x/.test(b)) return b = parseInt(b.slice(3, -1), 16), xq(b), String.fromCodePoint(b); if (/^&#/.test(b)) return b = parseInt(b.slice(2, -1), 10), xq(b), String.fromCodePoint(b); switch (b) { case "&lt;": return "<"; case "&gt;": return ">"; case "&amp;": return "&"; case "&quot;": return String.fromCharCode(34); case "&apos;": return String.fromCharCode(39) }throw Error('XPST0003: Unknown character reference: "' + b + '"'); }) }
  			function zq(a, b, c) {
  				if (!a.length) return []; for (var d = [a[0]], e = 1; e < a.length; ++e) { var f = d[d.length - 1]; "string" === typeof f && "string" === typeof a[e] ? d[d.length - 1] = f + a[e] : d.push(a[e]); } if ("string" === typeof d[0] && 0 === d.length) return []; d = d.reduce(function (g, k, l) { if ("string" !== typeof k) g.push(k); else if (c && /^\s*$/.test(k)) { var m = d[l + 1]; m && "CDataSection" === m[0] ? g.push(yq(k)) : (l = d[l - 1]) && "CDataSection" === l[0] && g.push(yq(k)); } else g.push(yq(k)); return g }, []); if (!d.length) return d; if (1 < d.length || b) for (a = 0; a < d.length; a++)"string" ===
  					typeof d[a] && (d[a] = ["stringConstantExpr", ["value", d[a]]]); return d
  			} function Aq(a) { return a[0].prefix ? a[0].prefix + ":" + a[1] : a[1] } var Bq = S.then(Cp, S.optional(am), function (a, b) { return null !== b ? ["singleType", ["atomicType"].concat(t(a)), ["optional"]] : ["singleType", ["atomicType"].concat(t(a))] }), Cq = S.map(Cp, function (a) { return ["atomicType"].concat(t(a)) }); var Dq = new Map;
  			function Eq(a) {
  				function b(n, r) { return r.reduce(function (G, ea) { return [ea[0], ["firstOperand", G], ["secondOperand", ea[1]]] }, n) } function c(n, r, G) { return S.then(n, S.star(S.then(U(r, W), S.cut(n), function (ea, oa) { return [ea, oa] })), G) } function d(n, r, G, ea) { G = void 0 === G ? "firstOperand" : G; ea = void 0 === ea ? "secondOperand" : ea; return S.then(n, S.optional(S.then(U(r, W), S.cut(n), function (oa, Ja) { return [oa, Ja] })), function (oa, Ja) { return null === Ja ? oa : [Ja[0], [G, oa], [ea, Ja[1]]] }) } function e(n) {
  					return a.vb ? function (r, G) {
  						r = n(r,
  							G); if (!r.success) return r; var ea = m.has(G) ? m.get(G) : { offset: G, line: -1, ja: -1 }, oa = m.has(r.offset) ? m.get(r.offset) : { offset: r.offset, line: -1, ja: -1 }; m.set(G, ea); m.set(r.offset, oa); return S.okWithValue(r.offset, ["x:stackTrace", { start: ea, end: oa }, r.value])
  					} : n
  				} function f(n, r) { return Wk(n, r) } function g(n, r) { return hh(n, r) } function k(n, r) { return e(S.or([Yr, Zr, $r, as, bs, cs, ds, es, fs, gs, hs]))(n, r) } function l(n, r) {
  					return c(k, mm, function (G, ea) { return 0 === ea.length ? G : ["sequenceExpr", G].concat(t(ea.map(function (oa) { return oa[1] }))) })(n,
  						r)
  				} var m = new Map, q = S.preceded(wl, S.followed(U(l, W), xl)), u = S.map(a.Ya ? S.or([U(S.star(S.or([Ip, Jp, Kp, ol(/[^"&]/)])), El), U(S.star(S.or([Ip, Jp, Lp, ol(/[^'&]/)])), Cl)]) : S.or([U(S.star(S.or([Kp, ol(/[^"]/)])), El), U(S.star(S.or([Lp, ol(/[^']/)])), Cl)]), function (n) { return n.join("") }), z = S.or([S.map(V([Jm, W], S.delimited(S.followed(ul, W), S.then(Dp, V([W, mm, W], Cp), function (n, r) { return [["elementName", n], ["typeName"].concat(t(r))] }), S.preceded(W, vl))), function (n) {
  					var r = p(n); n = r.next().value; r = r.next().value; return ["elementTest",
  						n, r]
  				}), S.map(V([Jm, W], S.delimited(ul, Dp, vl)), function (n) { return ["elementTest", ["elementName", n]] }), S.map(V([Jm, W], S.delimited(ul, W, vl)), function () { return ["elementTest"] })]), A = S.or([S.map(Cp, function (n) { return ["QName"].concat(t(n)) }), S.map(Wl, function () { return ["star"] })]), D = S.or([S.map(V([Km, W], S.delimited(S.followed(ul, W), S.then(A, V([W, mm, W], Cp), function (n, r) { return [["attributeName", n], ["typeName"].concat(t(r))] }), S.preceded(W, vl))), function (n) {
  					var r = p(n); n = r.next().value; r = r.next().value; return ["attributeTest",
  						n, r]
  				}), S.map(V([Km, W], S.delimited(ul, A, vl)), function (n) { return ["attributeTest", ["attributeName", n]] }), S.map(V([Km, W], S.delimited(ul, W, vl)), function () { return ["attributeTest"] })]), F = S.map(V([Lm, W, ul], S.followed(Cp, vl)), function (n) { return ["schemaElementTest"].concat(t(n)) }), J = S.map(S.delimited(ap, U(Cp, W), vl), function (n) { return ["schemaAttributeTest"].concat(t(n)) }), T = S.map(S.preceded(bp, S.followed(U(S.optional(S.or([z, F])), W), vl)), function (n) { return ["documentTest"].concat(t(n ? [n] : [])) }), ia = S.or([S.map(S.preceded(cp,
  					S.followed(U(S.or([tp, u]), W), vl)), function (n) { return ["piTest", ["piTarget", n]] }), ml(nl([dp], "piTest"))]), Ba = S.or([T, z, D, F, J, ia, Mp, Np, Op, Pp]), Ra = S.or([S.map(S.preceded(Rl, tp), function (n) { return ["Wildcard", ["star"], ["NCName", n]] }), ml(nl([Wl], "Wildcard")), S.map(S.followed(Ap, Wl), function (n) { return ["Wildcard", ["uri", n], ["star"]] }), S.map(S.followed(tp, Ql), function (n) { return ["Wildcard", ["NCName", n], ["star"]] })]), kb = S.or([Ra, S.map(Cp, function (n) { return ["nameTest"].concat(t(n)) })]), Rb = S.or([Ba, kb]), Gd = S.then(S.optional(Xl),
  						Rb, function (n, r) { return null !== n || "attributeTest" === r[0] || "schemaAttributeTest" === r[0] ? ["stepExpr", ["xpathAxis", "attribute"], r] : ["stepExpr", ["xpathAxis", "child"], r] }), is = S.or([S.then(Gp, Rb, function (n, r) { return ["stepExpr", ["xpathAxis", n], r] }), Gd]), js = S.map(om, function () { return ["stepExpr", ["xpathAxis", "parent"], ["anyKindTest"]] }), ks = S.or([S.then(Hp, Rb, function (n, r) { return ["stepExpr", ["xpathAxis", n], r] }), js]), ls = S.map(S.star(S.preceded(W, q)), function (n) {
  							return 0 < n.length ? ["predicates"].concat(t(n)) :
  								void 0
  						}), ms = S.then(S.or([ks, is]), ls, function (n, r) { return void 0 === r ? n : n.concat([r]) }), ih = S.or([Up, S.map(u, function (n) { return ["stringConstantExpr", ["value", a.Ya ? yq(n) : n]] })]), jh = S.or([S.delimited(ul, U(l, W), vl), S.map(S.delimited(ul, W, vl), function () { return ["sequenceExpr"] })]), Xk = S.or([k, Xp]), ef = S.map(S.delimited(ul, U(S.optional(S.then(Xk, S.star(S.preceded(U(mm, W), Xk)), function (n, r) { return [n].concat(t(r)) })), W), vl), function (n) { return null !== n ? n : [] }), ns = S.preceded(S.not(jl(Wp, W, ul, function () { }), ["cannot use reseved keyword for function names"]),
  							S.then(Cp, S.preceded(W, ef), function (n, r) { return ["functionCallExpr", ["functionName"].concat(t(n)), null !== r ? ["arguments"].concat(t(r)) : ["arguments"]] })), os = S.then(Cp, S.preceded(Zl, Tp), function (n, r) { return ["namedFunctionRef", ["functionName"].concat(t(n)), r] }), lb = S.delimited(yl, U(S.optional(l), W), zl), Yk = S.map(lb, function (n) { return n ? n : ["sequenceExpr"] }), Bb = S.or([S.map(jp, function () { return [["voidSequenceType"]] }), S.then(f, S.optional(S.preceded(W, Yp)), function (n, r) {
  								return [n].concat(t(null !== r ? [["occurrenceIndicator",
  									r]] : []))
  							})]), kh = S.then(V([$l, W], Cp), S.optional(S.followed(S.then(V([ul, W], ih), S.star(V([mm, W], ih)), function (n, r) { return n.concat(r) }), vl)), function (n, r) { return ["annotation", ["annotationName"].concat(t(n))].concat(t(r ? ["arguments", r] : [])) }), ps = S.map(V([Hm, W, ul, W, Wl, W], vl), function () { return ["anyFunctionTest"] }), qs = S.then(V([Hm, W, ul, W], S.optional(c(Bb, mm, function (n, r) { return n.concat.apply(n, r.map(function (G) { return G[1] })) }))), V([W, vl, X, xm, X], Bb), function (n, r) {
  								return ["typedFunctionTest", ["paramTypeList",
  									["sequenceType"].concat(t(n ? n : []))], ["sequenceType"].concat(t(r))]
  							}), rs = S.then(S.star(kh), S.or([ps, qs]), function (n, r) { return [r[0]].concat(t(n), t(r.slice(1))) }), ss = S.map(V([Im, W, ul, W, Wl, W], vl), function () { return ["anyMapTest"] }), ts = S.then(V([Im, W, ul, W], Cq), V([W, mm], S.followed(U(Bb, W), vl)), function (n, r) { return ["typedMapTest", n, ["sequenceType"].concat(t(r))] }), us = S.or([ss, ts]), vs = S.map(V([vn, W, ul, W, Wl, W], vl), function () { return ["anyArrayTest"] }), ws = S.map(V([vn, W, ul], S.followed(U(Bb, W), vl)), function (n) {
  								return ["typedArrayTest",
  									["sequenceType"].concat(t(n))]
  							}), xs = S.or([vs, ws]), ys = S.map(S.delimited(ul, U(f, W), vl), function (n) { return ["parenthesizedItemType", n] }), Wk = S.or([Ba, ml(nl([ip], "anyItemType")), rs, us, xs, Cq, ys]), Vc = S.map(V([xm, X], Bb), function (n) { return ["typeDeclaration"].concat(t(n)) }), zs = S.then(S.preceded(Yl, Cp), S.optional(S.preceded(X, Vc)), function (n, r) { return ["param", ["varName"].concat(t(n))].concat(t(r ? [r] : [])) }), Zk = c(zs, mm, function (n, r) { return [n].concat(t(r.map(function (G) { return G[1] }))) }), As = kl(S.star(kh), V([W, Hm,
  								W, ul, W], S.optional(Zk)), V([W, vl, W], S.optional(S.map(V([xm, W], S.followed(Bb, W)), function (n) { return ["typeDeclaration"].concat(t(n)) }))), Yk, function (n, r, G, ea) { return ["inlineFunctionExpr"].concat(t(n), [["paramList"].concat(t(r ? r : []))], t(G ? [G] : []), [["functionBody", ea]]) }), Bs = S.or([os, As]), Cs = S.map(k, function (n) { return ["mapKeyExpr", n] }), Ds = S.map(k, function (n) { return ["mapValueExpr", n] }), Es = S.then(Cs, S.preceded(U(Ul, W), Ds), function (n, r) { return ["mapConstructorEntry", n, r] }), Fs = S.preceded(Im, S.delimited(U(yl,
  									W), S.map(S.optional(c(Es, U(mm, W), function (n, r) { return [n].concat(t(r.map(function (G) { return G[1] }))) })), function (n) { return n ? ["mapConstructor"].concat(t(n)) : ["mapConstructor"] }), S.preceded(W, zl))), Gs = S.map(S.delimited(wl, U(S.optional(c(k, mm, function (n, r) { return [n].concat(t(r.map(function (G) { return G[1] }))).map(function (G) { return ["arrayElem", G] }) })), W), xl), function (n) { return ["squareArray"].concat(t(null !== n ? n : [])) }), Hs = S.map(S.preceded(vn, S.preceded(W, lb)), function (n) {
  										return ["curlyArray"].concat(t(null !==
  											n ? [["arrayElem", n]] : []))
  									}), Is = S.map(S.or([Gs, Hs]), function (n) { return ["arrayConstructor", n] }), $k = S.or([tp, Tp, jh, Wl]), Js = S.map(S.preceded(am, S.preceded(W, $k)), function (n) { return "*" === n ? ["unaryLookup", ["star"]] : "string" === typeof n ? ["unaryLookup", ["NCName", n]] : ["unaryLookup", n] }), lh = S.or([Ip, Jp, nl([Al], "{"), nl([Bl], "}"), S.map(lb, function (n) { return n || ["sequenceExpr"] })]), Ks = S.or([$p, function (n, r) { return al(n, r) }, lh, Zp]), Ls = S.or([S.map(aq, function (n) { return n.replace(/[\x20\x0D\x0A\x09]/g, " ") }), lh]),
  					Ms = S.or([S.map(bq, function (n) { return n.replace(/[\x20\x0D\x0A\x09]/g, " ") }), lh]), Ns = S.map(S.or([U(S.star(S.or([Kp, Ls])), El), U(S.star(S.or([Lp, Ms])), Cl)]), function (n) { return zq(n, !1, !1) }), Os = S.then(zp, S.preceded(U(bm, S.optional(qp)), Ns), function (n, r) {
  						if ("" === n[0].prefix && "xmlns" === n[1]) { if (r.length && "string" !== typeof r[0]) throw Error("XQST0022: A namespace declaration may not contain enclosed expressions"); return ["namespaceDeclaration", r.length ? ["uri", r[0]] : ["uri"]] } if ("xmlns" === n[0].prefix) {
  							if (r.length &&
  								"string" !== typeof r[0]) throw Error("XQST0022: The namespace declaration for 'xmlns:" + n[1] + "' may not contain enclosed expressions"); return ["namespaceDeclaration", ["prefix", n[1]], r.length ? ["uri", r[0]] : ["uri"]]
  						} return ["attributeConstructor", ["attributeName"].concat(n), 0 === r.length ? ["attributeValue"] : 1 === r.length && "string" === typeof r[0] ? ["attributeValue", r[0]] : ["attributeValueExpr"].concat(r)]
  					}), Ps = S.map(S.star(S.preceded(qp, S.optional(Os))), function (n) { return n.filter(Boolean) }), Qs = jl(S.preceded(gm,
  						zp), Ps, S.or([S.map(Il, function () { return null }), S.then(S.preceded(jm, S.star(Ks)), V([W, Jl], S.followed(zp, S.then(S.optional(qp), jm, function () { return null }))), function (n, r) { return [zq(n, !0, !0), r] })]), function (n, r, G) {
  							var ea = G; if (G && G.length) { ea = Aq(n); var oa = Aq(G[1]); if (ea !== oa) throw Error('XQST0118: The start and the end tag of an element constructor must be equal. "' + ea + '" does not match "' + oa + '"'); ea = G[0]; } return ["elementConstructor", ["tagName"].concat(t(n))].concat(t(r.length ? [["attributeList"].concat(t(r))] :
  								[]), t(ea && ea.length ? [["elementContent"].concat(t(ea))] : []))
  						}), al = S.or([Qs, dq, gq]), Rs = S.map(V([wn, W], lb), function (n) { return ["computedDocumentConstructor"].concat(t(n ? [["argExpr", n]] : [])) }), Ss = S.map(lb, function (n) { return n ? [["contentExpr", n]] : [] }), Ts = S.then(V([Jm, W], S.or([S.map(Cp, function (n) { return ["tagName"].concat(t(n)) }), S.map(S.delimited(yl, U(l, W), zl), function (n) { return ["tagNameExpr", n] })])), S.preceded(W, Ss), function (n, r) { return ["computedElementConstructor", n].concat(t(r)) }), Us = S.then(S.preceded(Km,
  							S.or([S.map(V([Fp, W], Cp), function (n) { return ["tagName"].concat(t(n)) }), S.map(S.preceded(W, S.delimited(yl, U(l, W), zl)), function (n) { return ["tagNameExpr", n] })])), S.preceded(W, lb), function (n, r) { return ["computedAttributeConstructor", n, ["valueExpr", r ? r : ["sequenceExpr"]]] }), Vs = S.map(lb, function (n) { return n ? [["prefixExpr", n]] : [] }), Ws = S.map(lb, function (n) { return n ? [["URIExpr", n]] : [] }), Xs = S.then(V([xn, W], S.or([up, Vs])), S.preceded(W, Ws), function (n, r) { return ["computedNamespaceConstructor"].concat(t(n), t(r)) }),
  					Ys = S.map(V([yn, W], lb), function (n) { return ["computedTextConstructor"].concat(t(n ? [["argExpr", n]] : [])) }), Zs = S.map(V([zn, W], lb), function (n) { return ["computedCommentConstructor"].concat(t(n ? [["argExpr", n]] : [])) }), $s = V([An, W], S.then(S.or([S.map(tp, function (n) { return ["piTarget", n] }), S.map(S.delimited(yl, U(l, W), zl), function (n) { return ["piTargetExpr", n] })]), S.preceded(W, lb), function (n, r) { return ["computedPIConstructor", n].concat(t(r ? [["piValueExpr", r]] : [])) })), at = S.or([Rs, Ts, Us, Xs, Ys, Zs, $s]), bt = S.or([al, at]),
  					bl = S.or([ih, Ep, jh, Vp, ns, bt, Bs, Fs, Is, Js]), cl = S.map(V([am, W], $k), function (n) { return "*" === n ? ["lookup", ["star"]] : "string" === typeof n ? ["lookup", ["NCName", n]] : ["lookup", n] }), dt = S.then(S.map(bl, function (n) { return wq(n) }), S.star(S.or([S.map(S.preceded(W, q), function (n) { return ["predicate", n] }), S.map(S.preceded(W, ef), function (n) { return ["argumentList", n] }), S.preceded(W, cl)])), function (n, r) {
  						function G() {
  							dl && 1 === Ja.length ? Wc.push(["predicate", Ja[0]]) : 0 !== Ja.length && Wc.push(["predicates"].concat(t(Ja))); Ja.length =
  								0;
  						} function ea(ct) { G(); 0 !== Wc.length ? ("sequenceExpr" === oa[0][0] && 2 < oa[0].length && (oa = [["sequenceExpr"].concat(t(oa))]), oa = [["filterExpr"].concat(t(oa))].concat(t(Wc)), Wc.length = 0) : ct && (oa = [["filterExpr"].concat(t(oa))]); } var oa = [n], Ja = [], Wc = [], dl = !1; n = p(r); for (r = n.next(); !r.done; r = n.next())switch (r = r.value, r[0]) {
  							case "predicate": Ja.push(r[1]); break; case "lookup": dl = !0; G(); Wc.push(r); break; case "argumentList": ea(!1); 1 < oa.length && (oa = [["sequenceExpr", ["pathExpr", ["stepExpr"].concat(t(oa))]]]); oa = [["dynamicFunctionInvocationExpr",
  								["functionItem"].concat(t(oa))].concat(t(r[1].length ? [["arguments"].concat(t(r[1]))] : []))]; break; default: throw Error("unreachable");
  						}ea(!0); return oa
  					}), Xc = S.or([S.map(dt, function (n) { return ["stepExpr"].concat(t(n)) }), ms]), et = S.followed(bl, S.peek(S.not(S.preceded(W, S.or([q, ef, cl])), ["primary expression not followed by predicate, argumentList, or lookup"]))), ft = S.or([jl(Xc, S.preceded(W, hq), S.preceded(W, g), function (n, r, G) { return ["pathExpr", n, r].concat(t(G)) }), S.then(Xc, S.preceded(U(rm, W), g), function (n,
  						r) { return ["pathExpr", n].concat(t(r)) }), et, S.map(Xc, function (n) { return ["pathExpr", n] })]), hh = S.or([jl(Xc, S.preceded(W, hq), S.preceded(W, g), function (n, r, G) { return [n, r].concat(t(G)) }), S.then(Xc, S.preceded(U(rm, W), g), function (n, r) { return [n].concat(t(r)) }), S.map(Xc, function (n) { return [n] })]), gt = S.or([S.map(V([rm, W], hh), function (n) { return ["pathExpr", ["rootExpr"]].concat(t(n)) }), S.then(hq, S.preceded(W, hh), function (n, r) { return ["pathExpr", ["rootExpr"], n].concat(t(r)) }), S.map(S.followed(rm, S.not(S.preceded(W,
  							a.Ya ? ol(/[*<a-zA-Z]/) : ol(/[*a-zA-Z]/)), ["Single rootExpr cannot be by followed by something that can be interpreted as a relative path"])), function () { return ["pathExpr", ["rootExpr"]] })]), ht = il(S.or([ft, gt]), Dq), it = S.preceded(Dn, S.then(S.optional(S.or([S.map(S.preceded(W, iq), function (n) { return ["validationMode", n] }), S.map(V([W, En, W], Cp), function (n) { return ["type"].concat(t(n)) })])), S.delimited(S.preceded(W, yl), U(l, W), zl), function (n, r) { return ["validateExpr"].concat(t(n ? [n] : []), [["argExpr", r]]) })), jt = S.delimited(sl,
  								S.then(S.preceded(W, Cp), S.optional(S.preceded(X, jq)), function (n, r) { return r ? ["pragma", ["pragmaName", n], ["pragmaContents", r]] : ["pragma", ["pragmaName", n]] }), S.preceded(W, tl)), kt = S.map(S.followed(S.plus(jt), S.preceded(W, S.delimited(yl, U(S.optional(l), W), zl))), function (n) { return ["extensionExpr"].concat(t(n)) }), lt = e(c(ht, cm, function (n, r) {
  									return 0 === r.length ? n : ["simpleMapExpr", "pathExpr" === n[0] ? n : ["pathExpr", ["stepExpr", ["filterExpr", wq(n)]]]].concat(r.map(function (G) {
  										G = G[1]; return "pathExpr" === G[0] ? G : ["pathExpr",
  											["stepExpr", ["filterExpr", wq(G)]]]
  									}))
  								})), mt = S.or([it, kt, lt]), el = S.or([S.then(S.or([nl([qm], "unaryMinusOp"), nl([pm], "unaryPlusOp")]), S.preceded(W, function (n, r) { return el(n, r) }), function (n, r) { return [n, ["operand", r]] }), mt]), nt = S.or([S.map(Cp, function (n) { return ["EQName"].concat(t(n)) }), Ep, jh]), ot = S.then(el, S.star(V([W, tm, W], S.then(nt, S.preceded(W, ef), function (n, r) { return [n, r] }))), function (n, r) { return r.reduce(function (G, ea) { return ["arrowExpr", ["argExpr", G], ea[0], ["arguments"].concat(t(ea[1]))] }, n) }),
  					pt = S.then(ot, S.optional(V([W, ym, X, xm, Fp, W], Bq)), function (n, r) { return null !== r ? ["castExpr", ["argExpr", n], r] : n }), qt = S.then(pt, S.optional(V([W, zm, X, xm, Fp, W], Bq)), function (n, r) { return null !== r ? ["castableExpr", ["argExpr", n], r] : n }), rt = S.then(qt, S.optional(V([W, Am, X, xm, Fp, W], Bb)), function (n, r) { return null !== r ? ["treatExpr", ["argExpr", n], ["sequenceType"].concat(t(r))] : n }), st = S.then(rt, S.optional(V([W, Bm, X, Cm, Fp, W], Bb)), function (n, r) {
  						return null !== r ? ["instanceOfExpr", ["argExpr", n], ["sequenceType"].concat(t(r))] :
  							n
  					}), tt = c(st, S.followed(S.or([nl([Mm], "intersectOp"), nl([Nm], "exceptOp")]), Fp), b), ut = c(tt, S.or([nl([dm], "unionOp"), S.followed(nl([Om], "unionOp"), Fp)]), b), vt = c(ut, S.or([nl([Wl], "multiplyOp"), S.followed(nl([Tm], "divOp"), Fp), S.followed(nl([Um], "idivOp"), Fp), S.followed(nl([Vm], "modOp"), Fp)]), b), wt = c(vt, S.or([nl([qm], "subtractOp"), nl([pm], "addOp")]), b), xt = d(wt, S.followed(nl([Pm], "rangeSequenceExpr"), Fp), "startExpr", "endExpr"), yt = c(xt, nl([em], "stringConcatenateOp"), b), zt = d(yt, S.or([kq, lq, mq])), At = c(zt, S.followed(nl([Sm],
  						"andOp"), Fp), b), hs = c(At, S.followed(nl([Rm], "orOp"), Fp), b), bs = S.then(S.then(V([en, W, ul, W], l), V([W, vl, W, fn, Fp, W], k), function (n, r) { return [n, r] }), V([W, gn, Fp, W], k), function (n, r) { return ["ifThenElseExpr", ["ifClause", n[0]], ["thenClause", n[1]], ["elseClause", r]] }), Bt = S.delimited(hn, X, jn), Ct = S.map(V([kn, X, Yl], Cp), function (n) { return ["positionalVariableBinding"].concat(t(n)) }), Dt = ll(S.preceded(Yl, S.cut(Cp)), S.cut(S.preceded(W, S.optional(Vc))), S.cut(S.preceded(W, S.optional(Bt))), S.cut(S.preceded(W, S.optional(Ct))),
  							S.cut(S.preceded(U(ln, W), k)), function (n, r, G, ea, oa) { return ["forClauseItem", ["typedVariableBinding", ["varName"].concat(t(n), t(r ? [r] : []))]].concat(t(G ? [["allowingEmpty"]] : []), t(ea ? [ea] : []), [["forExpr", oa]]) }), Et = V([mn, X], c(Dt, mm, function (n, r) { return ["forClause", n].concat(t(r.map(function (G) { return G[1] }))) })), Ft = jl(S.preceded(Yl, Cp), S.preceded(W, S.optional(Vc)), S.preceded(U(Sl, W), k), function (n, r, G) {
  								return ["letClauseItem", ["typedVariableBinding", ["varName"].concat(t(n))].concat(t(r ? [r] : [])), ["letExpr",
  									G]]
  							}), Gt = S.map(V([nn, W], c(Ft, mm, function (n, r) { return [n].concat(t(r.map(function (G) { return G[1] }))) })), function (n) { return ["letClause"].concat(t(n)) }), fl = S.or([Et, Gt]), Ht = S.map(V([on, Fp, W], k), function (n) { return ["whereClause", n] }), It = S.map(S.preceded(Yl, Cp), function (n) { return ["varName"].concat(t(n)) }), Jt = S.then(S.preceded(W, S.optional(Vc)), S.preceded(U(Sl, W), k), function (n, r) { return ["groupVarInitialize"].concat(t(n ? [["typeDeclaration"].concat(t(n))] : []), [["varValue", r]]) }), Kt = jl(It, S.optional(Jt), S.optional(S.map(S.preceded(U(pn,
  								W), u), function (n) { return ["collation", n] })), function (n, r, G) { return ["groupingSpec", n].concat(t(r ? [r] : []), t(G ? [G] : [])) }), Lt = c(Kt, mm, function (n, r) { return [n].concat(t(r.map(function (G) { return G[1] }))) }), Mt = S.map(V([qn, X, rn, W], Lt), function (n) { return ["groupByClause"].concat(t(n)) }), Nt = jl(S.optional(S.or([Co, Do])), S.optional(V([W, jn, W], S.or([Nn, On].map(function (n) { return S.map(n, function (r) { return "empty " + r }) })))), S.preceded(W, S.optional(V([pn, W], u))), function (n, r, G) {
  									return n || r || G ? ["orderModifier"].concat(t(n ?
  										[["orderingKind", n]] : []), t(r ? [["emptyOrderingMode", r]] : []), t(G ? [["collation", G]] : [])) : null
  								}), Ot = S.then(k, S.preceded(W, Nt), function (n, r) { return ["orderBySpec", ["orderByExpr", n]].concat(t(r ? [r] : [])) }), Pt = c(Ot, mm, function (n, r) { return [n].concat(t(r.map(function (G) { return G[1] }))) }), Qt = S.then(S.or([S.map(V([sn, X], rn), function () { return !1 }), S.map(V([tn, X, sn, X], rn), function () { return !0 })]), S.preceded(W, Pt), function (n, r) { return ["orderByClause"].concat(t(n ? [["stable"]] : []), t(r)) }), Rt = S.or([fl, Ht, Mt, Qt]), St = S.map(V([un,
  									W], k), function (n) { return ["returnClause", n] }), Yr = jl(fl, S.cut(S.star(S.preceded(W, Rt))), S.cut(S.preceded(W, St)), function (n, r, G) { return ["flworExpr", n].concat(t(r), [G]) }), Tt = c(Bb, dm, function (n, r) { return 0 === r.length ? ["sequenceType"].concat(t(n)) : ["sequenceTypeUnion", ["sequenceType"].concat(t(n))].concat(t(r.map(function (G) { return ["sequenceType"].concat(t(G[1])) }))) }), Ut = jl(V([Rn, W], S.optional(S.preceded(Yl, S.followed(S.followed(Cp, X), xm)))), S.preceded(W, Tt), V([X, un, X], k), function (n, r, G) {
  										return ["typeswitchExprCaseClause"].concat(n ?
  											[["variableBinding"].concat(t(n))] : [], [r], [["resultExpr", G]])
  									}), as = kl(S.preceded(Sn, U(S.delimited(ul, U(l, W), vl), W)), S.plus(S.followed(Ut, W)), V([Gn, X], S.optional(S.preceded(Yl, S.followed(Cp, X)))), V([un, X], k), function (n, r, G, ea) { return ["typeswitchExpr", ["argExpr", n]].concat(t(r), [["typeswitchExprDefaultClause"].concat(t(G || []), [["resultExpr", ea]])]) }), Vt = jl(S.preceded(Yl, Cp), S.optional(S.preceded(X, Vc)), S.preceded(U(ln, X), k), function (n, r, G) {
  										return ["quantifiedExprInClause", ["typedVariableBinding", ["varName"].concat(t(n))].concat(t(r ?
  											[r] : [])), ["sourceExpr", G]]
  									}), Wt = c(Vt, mm, function (n, r) { return [n].concat(t(r.map(function (G) { return G[1] }))) }), Zr = jl(S.or([Tn, Un]), S.preceded(X, Wt), S.preceded(U(Vn, W), k), function (n, r, G) { return ["quantifiedExpr", ["quantifier", n]].concat(t(r), [["predicateExpr", G]]) }), ds = S.map(V([Fm, X, S.or([Em, Dm]), X], k), function (n) { return ["deleteExpr", ["targetExpr", n]] }), fs = jl(V([Wn, X], S.optional(V([Gm, X, Cm], X))), V([Dm, X], k), S.preceded(U(Xn, X), k), function (n, r, G) {
  										return n ? ["replaceExpr", ["replaceValue"], ["targetExpr", r],
  											["replacementExpr", G]] : ["replaceExpr", ["targetExpr", r], ["replacementExpr", G]]
  									}), Xt = S.then(Ep, S.preceded(U(Sl, W), k), function (n, r) { return ["transformCopy", n, ["copySource", r]] }), gs = jl(V([Yn, X], c(Xt, mm, function (n, r) { return [n].concat(t(r.map(function (G) { return G[1] }))) })), V([W, Zn, X], k), S.preceded(U(un, X), k), function (n, r, G) { return ["transformExpr", ["transformCopies"].concat(t(n)), ["modifyExpr", r], ["returnExpr", G]] }), Yt = S.or([S.followed(S.map(S.optional(S.followed(V([xm, X], S.or([S.map($n, function () { return ["insertAsFirst"] }),
  									S.map(ao, function () { return ["insertAsLast"] })])), X)), function (n) { return n ? ["insertInto", n] : ["insertInto"] }), eo), S.map(co, function () { return ["insertAfter"] }), S.map(bo, function () { return ["insertBefore"] })]), cs = jl(V([fo, X, S.or([Em, Dm]), X], k), S.preceded(X, Yt), S.preceded(X, k), function (n, r, G) { return ["insertExpr", ["sourceExpr", n], r, ["targetExpr", G]] }), es = S.then(V([go, X, Dm, W], k), V([X, xm, X], k), function (n, r) { return ["renameExpr", ["targetExpr", n], ["newNameExpr", r]] }), Zt = S.then(S.plus(S.map(V([Rn, X], k), function (n) {
  										return ["switchCaseExpr",
  											n]
  									})), V([X, un, X], k), function (n, r) { return ["switchExprCaseClause"].concat(t(n), [["resultExpr", r]]) }), $r = jl(V([ho, W, ul], l), V([W, vl, W], S.plus(S.followed(Zt, W))), V([Gn, X, un, X], k), function (n, r, G) { return ["switchExpr", ["argExpr", n]].concat(t(r), [["switchExprDefaultClause", ["resultExpr", G]]]) }), $t = S.map(l, function (n) { return ["queryBody", n] }), au = V([Fn, X, xn, X], S.cut(S.then(tp, S.preceded(U(bm, W), u), function (n, r) { return ["namespaceDecl", ["prefix", n], ["uri", r]] }))), bu = S.then(V([io, X, Yl, W], S.then(Cp, S.optional(S.preceded(W,
  										Vc)), function (n, r) { return [n, r] })), S.cut(S.or([S.map(V([W, Sl, W], k), function (n) { return ["varValue", n] }), S.map(V([X, jo], S.optional(V([W, Sl, W], k))), function (n) { return ["external"].concat(t(n ? [["varValue", n]] : [])) })])), function (n, r) { var G = p(n); n = G.next().value; G = G.next().value; return ["varDecl", ["varName"].concat(t(n))].concat(t(null !== G ? [G] : []), [r]) }), cu = kl(V([Hm, X, S.peek(S.not(Wp, ["Cannot use reserved function name"]))], Cp), S.cut(V([W, ul, W], S.optional(Zk))), S.cut(V([W, vl], S.optional(V([X, xm, X], Bb)))), S.cut(S.preceded(W,
  											S.or([S.map(Yk, function (n) { return ["functionBody", n] }), S.map(jo, function () { return ["externalDefinition"] })]))), function (n, r, G, ea) { return ["functionDecl", ["functionName"].concat(t(n)), ["paramList"].concat(t(r || []))].concat(t(G ? [["typeDeclaration"].concat(t(G))] : []), [ea]) }), du = V([Fn, X], S.then(S.star(S.followed(S.or([kh, nq]), X)), S.or([bu, cu]), function (n, r) { return [r[0]].concat(t(n), t(r.slice(1))) })), eu = S.then(V([Fn, X, Gn, X], S.or([Jm, Hm])), V([X, xn, X], u), function (n, r) {
  												return ["defaultNamespaceDecl", ["defaultNamespaceCategory",
  													n], ["uri", r]]
  											}), fu = S.or([S.map(S.followed(V([xn, X], tp), S.preceded(W, bm)), function (n) { return ["namespacePrefix", n] }), S.map(V([Gn, X, Jm, X], xn), function () { return ["defaultElementNamespace"] })]), gu = V([lo, X, mo], jl(S.optional(S.preceded(X, fu)), S.preceded(W, u), S.optional(S.then(V([X, kn, X], u), S.star(V([W, mm, W], u)), function (n, r) { return [n].concat(t(r)) })), function (n, r, G) { return ["schemaImport"].concat(t(n ? [n] : []), [["targetNamespace", r]], t(G ? [G] : [])) })), hu = V([lo, X, no], jl(S.optional(S.followed(V([X, xn, X], tp), S.preceded(W,
  												bm))), S.preceded(W, u), S.optional(S.then(V([X, kn, X], u), S.star(V([W, mm, W], u)), function (n, r) { return [n].concat(t(r)) })), function (n, r) { return ["moduleImport", ["namespacePrefix", n], ["targetNamespace", r]] })), iu = S.or([gu, hu]), ju = S.map(V([Fn, X, Gn, X, pn, X], u), function (n) { return ["defaultCollationDecl", n] }), ku = S.map(V([Fn, X, oo, X], u), function (n) { return ["baseUriDecl", n] }), lu = S.then(V([Fn, X], S.or([S.map(V([Qn, X], Cp), function (n) { return ["decimalFormatName"].concat(t(n)) }), S.map(V([Gn, X], Qn), function () { return null })])),
  													S.star(S.then(S.preceded(X, qq), S.preceded(U(bm, X), u), function (n, r) { return ["decimalFormatParam", ["decimalFormatParamName", n], ["decimalFormatParamValue", r]] })), function (n, r) { return ["decimalFormatDecl"].concat(t(n ? [n] : []), t(r)) }), mu = S.or([rq, ju, ku, sq, tq, uq, vq, lu]), nu = S.then(V([Fn, X, to, X], Cp), S.preceded(X, u), function (n, r) { return ["optionDecl", ["optionName", n], ["optionContents", r]] }), ou = S.then(V([Fn, X, uo, X, vo], S.optional(V([X, xm], Wk))), S.or([S.map(S.preceded(U(Sl, W), k), function (n) { return ["varValue", n] }),
  													S.map(V([X, jo], S.optional(S.preceded(U(Sl, W), k))), function () { return ["external"] })]), function (n, r) { return ["contextItemDecl"].concat(t(n ? [["contextItemType", n]] : []), [r]) }), gl = S.then(S.star(S.followed(S.or([eu, mu, au, iu]), S.cut(U(Vl, W)))), S.star(S.followed(S.or([ou, du, nu]), S.cut(U(Vl, W)))), function (n, r) { return 0 === n.length && 0 === r.length ? null : ["prolog"].concat(t(n), t(r)) }), pu = V([no, X, xn, X], S.then(S.followed(tp, U(bm, W)), S.followed(u, U(Vl, W)), function (n, r) { return ["moduleDecl", ["prefix", n], ["uri", r]] })),
  					qu = S.then(pu, S.preceded(W, gl), function (n, r) { return ["libraryModule", n].concat(t(r ? [r] : [])) }), ru = S.then(gl, S.preceded(W, $t), function (n, r) { return ["mainModule"].concat(t(n ? [n] : []), [r]) }), su = S.map(V([wo, W], S.followed(S.or([S.then(S.preceded(yo, X), u, function (n) { return ["encoding", n] }), S.then(V([xo, X], u), S.optional(V([X, yo, X], u)), function (n, r) { return [["version", n]].concat(t(r ? [["encoding", r]] : [])) })]), S.preceded(W, Vl))), function (n) { return ["versionDecl"].concat(t(n)) }), tu = S.then(S.optional(U(su, W)), S.or([qu,
  						ru]), function (n, r) { return ["module"].concat(t(n ? [n] : []), [r]) }), uu = S.complete(U(tu, W)); return function (n, r) { m.clear(); r = uu(n, r); for (var G = 1, ea = 1, oa = 0; oa < n.length + 1; oa++) { if (m.has(oa)) { var Ja = m.get(oa); Ja.line = ea; Ja.ja = G; } Ja = n[oa]; "\r" === Ja || "\n" === Ja ? (ea++, G = 1) : G++; } return r }
  			} var Fq = Eq({ vb: !1, Ya: !1 }), Gq = Eq({ vb: !0, Ya: !1 }), Hq = Eq({ vb: !1, Ya: !0 }), Iq = Eq({ vb: !0, Ya: !0 }); function Jq(a, b) { var c = !!b.$; b = !!b.debug; kp.clear(); lp.clear(); Dq.clear(); c = c ? b ? Iq(a, 0) : Hq(a, 0) : b ? Gq(a, 0) : Fq(a, 0); if (!0 === c.success) return c.value; a = a.substring(0, c.offset).split("\n"); b = a[a.length - 1].length + 1; throw new ii({ start: { offset: c.offset, line: a.length, ja: b }, end: { offset: c.offset + 1, line: a.length, ja: b + 1 } }, "", Error("XPST0003: Failed to parse script. Expected " + [].concat(t(new Set(c.expected))))); } var Kq = "http://www.w3.org/XML/1998/namespace http://www.w3.org/2001/XMLSchema http://www.w3.org/2001/XMLSchema-instance http://www.w3.org/2005/xpath-functions http://www.w3.org/2005/xpath-functions/math http://www.w3.org/2012/xquery http://www.w3.org/2005/xpath-functions/array http://www.w3.org/2005/xpath-functions/map".split(" ");
  			function Lq(a, b, c, d, e) {
  				var f = L(a, "functionName"), g = M(f, "prefix") || "", k = M(f, "URI"), l = Tg(f); if (null === k && (k = "" === g ? null === b.v ? "http://www.w3.org/2005/xpath-functions" : b.v : b.aa(g), !k && g)) throw Ig(g); if (Kq.includes(k)) throw Dg(); g = O(a, "annotation").map(function (J) { return L(J, "annotationName") }); f = g.every(function (J) { return !M(J, "URI") && "private" !== Tg(J) }); g = g.some(function (J) { return !M(J, "URI") && "updating" === Tg(J) }); if (!k) throw Eg(); var m = Vg(a), q = O(L(a, "paramList"), "param"), u = q.map(function (J) {
  					return L(J,
  						"varName")
  				}), z = q.map(function (J) { return Vg(J) }); if (a = L(a, "functionBody")) {
  					if (b.xa(k, l, z.length)) throw Error("XQST0049: The function Q{" + k + "}" + l + "#" + z.length + " has already been declared."); if (!e) return; var A = Mk(a[1], { ua: !1, $: !0 }), D = new Lg(b), F = u.map(function (J) { var T = M(J, "URI"), ia = M(J, "prefix"); J = Tg(J); ia && null === T && (T = b.aa(ia || "")); return Qg(D, T, J) }); e = g ? {
  						j: z, arity: u.length, callFunction: function (J, T, ia) {
  							var Ba = Aa.apply(3, arguments), Ra = Nc(Jc(J, -1, null, C.empty()), F.reduce(function (kb, Rb, Gd) {
  								kb[Rb] =
  								yb(Ba[Gd]); return kb
  							}, Object.create(null))); return A.s(Ra, T)
  						}, Eb: !1, I: !0, hb: f, localName: l, namespaceURI: k, i: m
  					} : { j: z, arity: u.length, callFunction: function (J, T, ia) { var Ba = Aa.apply(3, arguments), Ra = Nc(Jc(J, -1, null, C.empty()), F.reduce(function (kb, Rb, Gd) { kb[Rb] = yb(Ba[Gd]); return kb }, Object.create(null))); return I(A, Ra, T) }, Eb: !1, I: !1, hb: f, localName: l, namespaceURI: k, i: m }; c.push({ ca: A, Mb: D }); d.push({ arity: u.length, ca: A, Ib: e, localName: l, namespaceURI: k, hb: f });
  				} else {
  					if (g) throw Error("Updating external function declarations are not supported");
  					e = { j: z, arity: u.length, callFunction: function (J, T, ia) { var Ba = Aa.apply(3, arguments), Ra = ia.xa(k, l, u.length, !0); if (!Ra) throw Error("XPST0017: Function Q{" + k + "}" + l + " with arity of " + u.length + " not registered. " + ug(l)); if (Ra.i.type !== m.type || Ra.j.some(function (kb, Rb) { return kb.type !== z[Rb].type })) throw Error("External function declaration types do not match actual function"); return Ra.callFunction.apply(Ra, [J, T, ia].concat(t(Ba))) }, Eb: !0, I: !1, localName: l, namespaceURI: k, hb: f, i: m };
  				} Og(b, k, l, u.length, e);
  			}
  			function Mq(a, b, c, d) {
  				var e = [], f = []; O(a, "*").forEach(function (A) { switch (A[0]) { case "moduleImport": case "namespaceDecl": case "defaultNamespaceDecl": case "functionDecl": case "varDecl": break; default: throw Error("Not implemented: only module imports, namespace declarations, and function declarations are implemented in XQuery modules"); } }); var g = new Set; O(a, "moduleImport").forEach(function (A) {
  					var D = Tg(L(A, "namespacePrefix")); A = Tg(L(A, "targetNamespace")); if (g.has(A)) throw Error('XQST0047: The namespace "' +
  						A + '" is imported more than once.'); g.add(A); Pg(b, D, A);
  				}); O(a, "namespaceDecl").forEach(function (A) { var D = Tg(L(A, "prefix")); A = Tg(L(A, "uri")); if ("xml" === D || "xmlns" === D) throw Gg(); if ("http://www.w3.org/XML/1998/namespace" === A || "http://www.w3.org/2000/xmlns/" === A) throw Gg(); Pg(b, D, A); }); for (var k = null, l = null, m = p(O(a, "defaultNamespaceDecl")), q = m.next(); !q.done; q = m.next()) {
  					var u = q.value; q = Tg(L(u, "defaultNamespaceCategory")); u = Tg(L(u, "uri")); if (!u) throw Eg(); if ("http://www.w3.org/XML/1998/namespace" === u || "http://www.w3.org/2000/xmlns/" ===
  						u) throw Gg(); if ("function" === q) { if (k) throw Fg(); k = u; } else if ("element" === q) { if (l) throw Fg(); l = u; }
  				} k && (b.v = k); l && Pg(b, "", l); O(a, "functionDecl").forEach(function (A) { Lq(A, b, e, f, c); }); var z = []; O(a, "varDecl").forEach(function (A) {
  					var D = Ug(L(A, "varName")), F = D.namespaceURI; if (null === F && (F = b.aa(D.prefix), !F && D.prefix)) throw Ig(D.prefix); if (Kq.includes(F)) throw Dg(); var J = L(A, "external"); A = L(A, "varValue"); var T, ia; null !== J ? (J = L(J, "varValue"), null !== J && (T = L(J, "*"))) : null !== A && (T = L(A, "*")); if (z.some(function (Ra) {
  						return Ra.namespaceURI ===
  							F && Ra.localName === D.localName
  					})) throw Error("XQST0049: The variable " + (F ? "Q{" + F + "}" : "") + D.localName + " has already been declared."); Qg(b, F || "", D.localName); if (c && (T && (ia = Mk(T, { ua: !1, $: !0 })), T && !Ng(b, F || "", D.localName))) { var Ba = null; Rg(b, F, D.localName, function (Ra, kb) { if (Ba) return Ba(); Ba = yb(I(ia, Ra, kb)); return Ba() }); e.push({ ca: ia, Mb: b }); z.push({ ca: ia, localName: D.localName, namespaceURI: F }); }
  				}); f.forEach(function (A) {
  					if (!A.Ib.I && A.ca.I) throw af("The function Q{" + A.namespaceURI + "}" + A.localName + " is updating but the %updating annotation is missing.");
  				}); return { Ma: f.map(function (A) { return A.Ib }), Va: z, source: d, ra: function (A) { g.forEach(function (D) { Uk(b, D); }); e.forEach(function (D) { var F = D.ca, J = D.Mb; g.forEach(function (T) { Uk(J, T); }); A.Ma.forEach(function (T) { J.xa(T.namespaceURI, T.localName, T.arity, !0) || T.hb && Og(J, T.namespaceURI, T.localName, T.arity, T); }); A.Va.forEach(function (T) { J.ib(T.namespaceURI, T.localName) || Qg(J, T.namespaceURI, T.localName); }); F.v(J); }); } }
  			} function Nq(a, b, c, d, e, f, g) { var k = b.$ ? "XQuery" : "XPath"; c = b.La ? null : Pk(a, k, c, d, e, b.debug, f, g); return null !== c ? { state: c.rc ? 1 : 2, ca: c.ca } : { state: 0, hc: "string" === typeof a ? Jq(a, b) : Rk(a) } } function Oq(a, b, c, d) { var e = L(a, "mainModule"); if (!e) throw Error("Can not execute a library module."); var f = L(e, "prolog"); if (f) { if (!b.$) throw Error("XPST0003: Use of XQuery functionality is not allowed in XPath context"); Vk(); d = Mq(f, c, !0, d); d.ra(d); } oh(a, new Jh(c)); a = N(e, ["queryBody", "*"]); return R(a, b) }
  			function Pq(a, b, c, d, e, f, g) { var k = new zg(c, d, f, g), l = new Lg(k); 0 < Object.keys(e).length && Vk(); Object.keys(e).forEach(function (m) { var q = e[m]; Uk(l, q); Pg(l, m, q); }); "string" === typeof a && (a = hl(a)); c = Nq(a, b, c, d, e, f, g); switch (c.state) { case 2: return { ia: l, ca: c.ca }; case 1: return c.ca.v(l), Qk(a, b.$ ? "XQuery" : "XPath", k, e, c.ca, b.debug, f), { ia: l, ca: c.ca }; case 0: return c = Oq(c.hc, b, l, a), c.v(l), b.La || Qk(a, b.$ ? "XQuery" : "XPath", k, e, c, b.debug, f), { ia: l, ca: c } } } function Qq(a, b, c, d) {
  				a = a.first(); var e = b.first().h.reduce(function (u, z) { u[z.key.value] = yb(z.value()); return u }, Object.create(null)); b = e["."] ? e["."]() : C.empty(); delete e["."]; try {
  					if (B(a.type, 1)) var f = a.value; else if (B(a.type, 54)) f = a.value.node; else throw Sc("Unable to convert selector argument of type " + mb[a.type] + " to either an " + mb[1] + " or an " + mb[54] + " representing an XQueryX program while calling 'fontoxpath:evaluate'"); var g = Pq(f, { ua: !1, $: !0, debug: d.debug, La: d.La }, function (u) { return c.aa(u) },
  						Object.keys(e).reduce(function (u, z) { u[z] = z; return u }, {}), {}, "http://www.w3.org/2005/xpath-functions", function (u, z) { return c.Ua(u, z) }), k = g.ca, l = g.ia, m = !b.G(), q = new Hc({ N: m ? b.first() : null, Ka: m ? 0 : -1, Da: b, Aa: Object.keys(e).reduce(function (u, z) { u[l.ib(null, z)] = e[z]; return u }, Object.create(null)) }); return { sc: k.h(q, d).value, pc: a }
  				} catch (u) { pg(a.value, u); }
  			} function Rq(a, b, c) { if (1 !== b.node.nodeType && 9 !== b.node.nodeType) return []; var d = Pb(a, b).reduce(function (e, f) { f = p(Rq(a, f, c)); for (var g = f.next(); !g.done; g = f.next())e.push(g.value); return e }, []); c(b) && d.unshift(b); return d }
  			function Sq(a, b, c, d, e) {
  				a = e.first(); if (!a) throw Rc("The context is absent, it needs to be present to use id function."); if (!B(a.type, 53)) throw Sc("The context item is not a node, it needs to be node to use id function."); var f = b.h, g = d.O().reduce(function (k, l) { l.value.split(/\s+/).forEach(function (m) { k[m] = !0; }); return k }, Object.create(null)); for (b = a.value; 9 !== b.node.nodeType;)if (b = Vb(f, b), null === b) throw Error("FODC0001: the root node of the target node is not a document node."); b = Rq(f, b, function (k) {
  					if (1 !==
  						k.node.nodeType) return !1; k = Ob(f, k, "id"); if (!k || !g[k]) return !1; g[k] = !1; return !0
  				}); return C.create(b.map(function (k) { return $b(k) }))
  			}
  			function Tq(a, b, c, d, e) {
  				a = e.first(); if (!a) throw Rc("The context is absent, it needs to be present to use idref function."); if (!B(a.type, 53)) throw Sc("The context item is not a node, it needs to be node to use idref function."); var f = b.h, g = d.O().reduce(function (k, l) { k[l.value] = !0; return k }, Object.create(null)); for (b = a.value; 9 !== b.node.nodeType;)if (b = Vb(f, b), null === b) throw Error("FODC0001: the root node of the context node is not a document node."); b = Rq(f, b, function (k) {
  					return 1 !== k.node.nodeType ? !1 :
  						(k = Ob(f, k, "idref")) ? k.split(/\s+/).some(function (l) { return g[l] }) : !1
  				}); return C.create(b.map(function (k) { return $b(k) }))
  			} function Uq(a) { switch (typeof a) { case "object": return Array.isArray(a) ? C.m(new Yb(a.map(function (b) { return yb(Uq(b)) }))) : null === a ? C.empty() : C.m(new dc(Object.keys(a).map(function (b) { return { key: w(b, 1), value: yb(Uq(a[b])) } }))); case "number": return C.m(w(a, 3)); case "string": return C.m(w(a, 1)); case "boolean": return a ? C.ba() : C.W(); default: throw Error("Unexpected type in JSON parse"); } } function Vq(a, b, c, d, e) {
  				var f = C.m(w("duplicates", 1)); a = cc(a, b, c, e, f); var g = a.G() ? "use-first" : a.first().value; return d.M(function (k) {
  					return C.m(new dc(k.reduce(function (l, m) {
  						m.h.forEach(function (q) { var u = l.findIndex(function (z) { return bc(z.key, q.key) }); if (0 <= u) switch (g) { case "reject": throw Error("FOJS0003: Duplicate encountered when merging maps."); case "use-last": l.splice(u, 1, q); return; case "combine": l.splice(u, 1, { key: q.key, value: yb(C.create(l[u].value().O().concat(q.value().O()))) }); return; default: return }l.push(q); });
  						return l
  					}, [])))
  				})
  			} function Wq(a, b, c) { var d = 1, e = a.value; a = a.Qa(!0); var f = null, g = Math.max(b - 1, 0); -1 !== a && (f = Math.max(0, (null === c ? a : Math.max(0, Math.min(a, c + (b - 1)))) - g)); return C.create({ next: function (k) { for (; d < b;)e.next(k), d++; if (null !== c && d >= b + c) return x; k = e.next(k); d++; return k } }, f) } function Xq(a) { return a.map(function (b) { return B(b.type, 19) ? Pd(b, 3) : b }) }
  			function Yq(a) { a = Xq(a); if (a.some(function (b) { return Number.isNaN(b.value) })) return [w(NaN, 3)]; a = cj(a); if (!a) throw Error("FORG0006: Incompatible types to be converted to a common type"); return a }
  			function Zq(a, b, c, d, e, f) { return ac([e, f], function (g) { var k = p(g); g = k.next().value; k = k.next().value; if (Infinity === g.value) return C.empty(); if (-Infinity === g.value) return k && Infinity === k.value ? C.empty() : d; if (k) { if (isNaN(k.value)) return C.empty(); Infinity === k.value && (k = null); } return isNaN(g.value) ? C.empty() : Wq(d, Math.round(g.value), k ? Math.round(k.value) : null) }) }
  			function $q(a, b, c, d, e) { if (d.G()) return e; a = Xq(d.O()); a = cj(a); if (!a) throw Error("FORG0006: Incompatible types to be converted to a common type"); if (!a.every(function (f) { return B(f.type, 2) })) throw Error("FORG0006: items passed to fn:sum are not all numeric."); b = a.reduce(function (f, g) { return f + g.value }, 0); return a.every(function (f) { return B(f.type, 5) }) ? C.m(w(b, 5)) : a.every(function (f) { return B(f.type, 3) }) ? C.m(w(b, 3)) : a.every(function (f) { return B(f.type, 4) }) ? C.m(w(b, 4)) : C.m(w(b, 6)) } var ar = [].concat(Vf, [{ namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "boolean", j: [{ type: 59, g: 2 }], i: { type: 0, g: 3 }, callFunction: function (a, b, c, d) { return d.ha() ? C.ba() : C.W() } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "true", j: [], i: { type: 0, g: 3 }, callFunction: function () { return C.ba() } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "not", j: [{ type: 59, g: 2 }], i: { type: 0, g: 3 }, callFunction: function (a, b, c, d) { return !1 === d.ha() ? C.ba() : C.W() } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				localName: "false", j: [], i: { type: 0, g: 3 }, callFunction: function () { return C.W() }
  			}], [{ namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "last", j: [], i: { type: 5, g: 3 }, callFunction: function (a) { if (null === a.N) throw Rc("The fn:last() function depends on dynamic context, which is absent."); var b = !1; return C.create({ next: function () { if (b) return x; var c = a.Da.Qa(); b = !0; return y(w(c, 5)) } }, 1) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "position", j: [], i: { type: 5, g: 3 }, callFunction: function (a) {
  					if (null ===
  						a.N) throw Rc("The fn:position() function depends on dynamic context, which is absent."); return C.m(w(a.Ka + 1, 5))
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "current-dateTime", j: [], i: { type: 10, g: 3 }, callFunction: function (a) { return C.m(w(Kc(a), 10)) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "current-date", j: [], i: { type: 7, g: 3 }, callFunction: function (a) { return C.m(w(tc(Kc(a), 7), 7)) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "current-time",
  				j: [], i: { type: 8, g: 3 }, callFunction: function (a) { return C.m(w(tc(Kc(a), 8), 8)) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "implicit-timezone", j: [], i: { type: 17, g: 3 }, callFunction: function (a) { return C.m(w(Lc(a), 17)) } }], Wf, dg, kg, [{ namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "years-from-duration", j: [{ type: 18, g: 0 }], i: { type: 5, g: 0 }, callFunction: function (a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.gb(), 5)) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "months-from-duration",
  				j: [{ type: 18, g: 0 }], i: { type: 5, g: 0 }, callFunction: function (a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.cb(), 5)) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "days-from-duration", j: [{ type: 18, g: 0 }], i: { type: 5, g: 0 }, callFunction: function (a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.bb(), 5)) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "hours-from-duration", j: [{ type: 18, g: 0 }], i: { type: 5, g: 0 }, callFunction: function (a, b, c, d) {
  					return d.G() ? d : C.m(w(d.first().value.getHours(),
  						5))
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "minutes-from-duration", j: [{ type: 18, g: 0 }], i: { type: 5, g: 0 }, callFunction: function (a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.getMinutes(), 5)) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "seconds-from-duration", j: [{ type: 18, g: 0 }], i: { type: 4, g: 0 }, callFunction: function (a, b, c, d) { return d.G() ? d : C.m(w(d.first().value.getSeconds(), 4)) } }], mg, [{
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "id", j: [{
  					type: 1,
  					g: 2
  				}, { type: 53, g: 3 }], i: { type: 54, g: 2 }, callFunction: Sq
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "id", j: [{ type: 1, g: 2 }], i: { type: 54, g: 2 }, callFunction: function (a, b, c, d) { return Sq(a, b, c, d, C.m(a.N)) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "idref", j: [{ type: 1, g: 2 }, { type: 53, g: 3 }], i: { type: 53, g: 2 }, callFunction: Tq }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "idref", j: [{ type: 1, g: 2 }], i: { type: 53, g: 2 }, callFunction: function (a, b, c, d) {
  					return Tq(a,
  						b, c, d, C.m(a.N))
  				}
  			}], [{ namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "parse-json", j: [{ type: 1, g: 3 }], i: { type: 59, g: 0 }, callFunction: function (a, b, c, d) { try { var e = JSON.parse(d.first().value); } catch (f) { throw Error("FOJS0001: parsed JSON string contains illegal JSON."); } return Uq(e) } }], [{
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "contains", j: [{ type: 61, g: 3 }, { type: 46, g: 3 }], i: { type: 0, g: 3 }, callFunction: function (a, b, c, d, e) {
  					return ac([d, e], function (f) {
  						f = p(f); var g = f.next().value,
  							k = f.next().value; return g.h.some(function (l) { return bc(l.key, k) }) ? C.ba() : C.W()
  					})
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "entry", j: [{ type: 46, g: 3 }, { type: 59, g: 2 }], i: { type: 61, g: 3 }, callFunction: function (a, b, c, d, e) { return d.map(function (f) { return new dc([{ key: f, value: yb(e) }]) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "for-each", j: [{ type: 61, g: 3 }, { type: 59, g: 2 }], i: { type: 59, g: 2 }, callFunction: function (a, b, c, d, e) {
  					return ac([d, e], function (f) {
  						f = p(f);
  						var g = f.next().value, k = f.next().value; return Pc(g.h.map(function (l) { return k.value.call(void 0, a, b, c, C.m(l.key), l.value()) }))
  					})
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "get", j: [{ type: 61, g: 3 }, { type: 46, g: 3 }], i: { type: 59, g: 2 }, callFunction: cc }, { namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "keys", j: [{ type: 61, g: 3 }], i: { type: 46, g: 2 }, callFunction: function (a, b, c, d) { return ac([d], function (e) { e = p(e).next().value; return C.create(e.h.map(function (f) { return f.key })) }) } },
  			{ namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "merge", j: [{ type: 61, g: 2 }, { type: 61, g: 3 }], i: { type: 61, g: 3 }, callFunction: Vq }, { namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "merge", j: [{ type: 61, g: 2 }], i: { type: 61, g: 3 }, callFunction: function (a, b, c, d) { return Vq(a, b, c, d, C.m(new dc([{ key: w("duplicates", 1), value: function () { return C.m(w("use-first", 1)) } }]))) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "put", j: [{ type: 61, g: 3 }, { type: 46, g: 3 }, {
  					type: 59,
  					g: 2
  				}], i: { type: 61, g: 3 }, callFunction: function (a, b, c, d, e, f) { return ac([d, e], function (g) { g = p(g); var k = g.next().value, l = g.next().value; g = k.h.concat(); k = g.findIndex(function (m) { return bc(m.key, l) }); 0 <= k ? g.splice(k, 1, { key: l, value: yb(f) }) : g.push({ key: l, value: yb(f) }); return C.m(new dc(g)) }) }
  			}, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "remove", j: [{ type: 61, g: 3 }, { type: 46, g: 2 }], i: { type: 61, g: 3 }, callFunction: function (a, b, c, d, e) {
  					return ac([d], function (f) {
  						var g = p(f).next().value.h.concat();
  						return e.M(function (k) { k.forEach(function (l) { var m = g.findIndex(function (q) { return bc(q.key, l) }); 0 <= m && g.splice(m, 1); }); return C.m(new dc(g)) })
  					})
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/map", localName: "size", j: [{ type: 61, g: 3 }], i: { type: 5, g: 3 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(e.h.length, 5) }) } }], [{ namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "pi", j: [], i: { type: 3, g: 3 }, callFunction: function () { return C.m(w(Math.PI, 3)) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/math",
  				localName: "exp", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(Math.pow(Math.E, e.value), 3) }) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "exp10", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(Math.pow(10, e.value), 3) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "log", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) {
  					return d.map(function (e) {
  						return w(Math.log(e.value),
  							3)
  					})
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "log10", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(Math.log10(e.value), 3) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "pow", j: [{ type: 3, g: 0 }, { type: 2, g: 3 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d, e) {
  					return e.M(function (f) {
  						var g = p(f).next().value; return d.map(function (k) {
  							return 1 !== Math.abs(k.value) || Number.isFinite(g.value) ? w(Math.pow(k.value,
  								g.value), 3) : w(1, 3)
  						})
  					})
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "sqrt", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(Math.sqrt(e.value), 3) }) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "sin", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(Math.sin(e.value), 3) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "cos", j: [{
  					type: 3,
  					g: 0
  				}], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(Math.cos(e.value), 3) }) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "tan", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(Math.tan(e.value), 3) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "asin", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) {
  					return d.map(function (e) {
  						return w(Math.asin(e.value),
  							3)
  					})
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "acos", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(Math.acos(e.value), 3) }) } }, { namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "atan", j: [{ type: 3, g: 0 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(Math.atan(e.value), 3) }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions/math", localName: "atan2", j: [{ type: 3, g: 0 },
  				{ type: 3, g: 3 }], i: { type: 3, g: 0 }, callFunction: function (a, b, c, d, e) { return e.M(function (f) { var g = p(f).next().value; return d.map(function (k) { return w(Math.atan2(k.value, g.value), 3) }) }) }
  			}], Oe, re, [{ namespaceURI: "http://fontoxpath/operators", localName: "to", j: [{ type: 5, g: 0 }, { type: 5, g: 0 }], i: { type: 5, g: 2 }, callFunction: function (a, b, c, d, e) { a = d.first(); e = e.first(); if (null === a || null === e) return C.empty(); var f = a.value; e = e.value; return f > e ? C.empty() : C.create({ next: function () { return y(w(f++, 5)) } }, e - f + 1) } }], [{
  				namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				localName: "QName", j: [{ type: 1, g: 0 }, { type: 1, g: 3 }], i: { type: 23, g: 3 }, callFunction: function (a, b, c, d, e) {
  					return ac([d, e], function (f) {
  						var g = p(f); f = g.next().value; g = g.next().value.value; if (!bd(g, 23)) throw Error("FOCA0002: The provided QName is invalid."); f = f ? f.value || null : null; if (null === f && g.includes(":")) throw Error("FOCA0002: The URI of a QName may not be empty if a prefix is provided."); if (d.G()) return C.m(w(new zb("", null, g), 23)); if (!g.includes(":")) return C.m(w(new zb("", f, g), 23)); var k = p(g.split(":"));
  						g = k.next().value; k = k.next().value; return C.m(w(new zb(g, f, k), 23))
  					})
  				}
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "prefix-from-QName", j: [{ type: 23, g: 0 }], i: { type: 24, g: 0 }, callFunction: function (a, b, c, d) { return ac([d], function (e) { e = p(e).next().value; if (null === e) return C.empty(); e = e.value; return e.prefix ? C.m(w(e.prefix, 24)) : C.empty() }) } }, {
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "local-name-from-QName", j: [{ type: 23, g: 0 }], i: { type: 24, g: 0 }, callFunction: function (a,
  					b, c, d) { return d.map(function (e) { return w(e.value.localName, 24) }) }
  			}, { namespaceURI: "http://www.w3.org/2005/xpath-functions", localName: "namespace-uri-from-QName", j: [{ type: 23, g: 0 }], i: { type: 20, g: 0 }, callFunction: function (a, b, c, d) { return d.map(function (e) { return w(e.value.namespaceURI || "", 20) }) } }], [{
  				j: [{ type: 59, g: 2 }], callFunction: function (a, b, c, d) { return d.Z({ empty: function () { return C.ba() }, multiple: function () { return C.W() }, m: function () { return C.W() } }) }, localName: "empty", namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				i: { type: 0, g: 3 }
  			}, { j: [{ type: 59, g: 2 }], callFunction: function (a, b, c, d) { return d.Z({ empty: function () { return C.W() }, multiple: function () { return C.ba() }, m: function () { return C.ba() } }) }, localName: "exists", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 0, g: 3 } }, { j: [{ type: 59, g: 2 }], callFunction: function (a, b, c, d) { return Wq(d, 1, 1) }, localName: "head", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 0 } }, {
  				j: [{ type: 59, g: 2 }], callFunction: function (a, b, c, d) { return Wq(d, 2, null) }, localName: "tail",
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 }
  			}, { j: [{ type: 59, g: 2 }, { type: 5, g: 3 }, { type: 59, g: 2 }], callFunction: function (a, b, c, d, e, f) { if (d.G()) return f; if (f.G()) return d; a = d.O(); e = e.first().value - 1; 0 > e ? e = 0 : e > a.length && (e = a.length); b = a.slice(e); return C.create(a.slice(0, e).concat(f.O(), b)) }, localName: "insert-before", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 } }, {
  				j: [{ type: 59, g: 2 }, { type: 5, g: 3 }], callFunction: function (a, b, c, d, e) {
  					a = e.first().value; d = d.O(); if (!d.length ||
  						1 > a || a > d.length) return C.create(d); d.splice(a - 1, 1); return C.create(d)
  				}, localName: "remove", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 }
  			}, { j: [{ type: 59, g: 2 }], callFunction: function (a, b, c, d) { return d.M(function (e) { return C.create(e.reverse()) }) }, localName: "reverse", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 } }, {
  				j: [{ type: 59, g: 2 }, { type: 3, g: 3 }], callFunction: function (a, b, c, d, e) { return Zq(a, b, c, d, e, C.empty()) }, localName: "subsequence", namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				i: { type: 59, g: 2 }
  			}, { j: [{ type: 59, g: 2 }, { type: 3, g: 3 }, { type: 3, g: 3 }], callFunction: Zq, localName: "subsequence", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 } }, { j: [{ type: 59, g: 2 }], callFunction: function (a, b, c, d) { return d }, localName: "unordered", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 } }, {
  				j: [{ type: 46, g: 2 }], callFunction: function (a, b, c, d) { var e = Zc(d, b).O(); return C.create(e).filter(function (f, g) { return e.slice(0, g).every(function (k) { return !Re(f, k) }) }) }, localName: "distinct-values",
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 2 }
  			}, { j: [{ type: 46, g: 2 }, { type: 1, g: 3 }], callFunction: function () { throw Error("FOCH0002: No collations are supported"); }, localName: "distinct-values", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 2 } }, {
  				j: [{ type: 46, g: 2 }, { type: 46, g: 3 }], callFunction: function (a, b, c, d, e) {
  					return e.M(function (f) {
  						var g = p(f).next().value; return Zc(d, b).map(function (k, l) { return Ii("eqOp", k.type, g.type)(k, g, a) ? w(l + 1, 5) : w(-1, 5) }).filter(function (k) {
  							return -1 !==
  								k.value
  						})
  					})
  				}, localName: "index-of", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 5, g: 2 }
  			}, { j: [{ type: 46, g: 2 }, { type: 46, g: 3 }, { type: 1, g: 3 }], callFunction: function () { throw Error("FOCH0002: No collations are supported"); }, localName: "index-of", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 5, g: 2 } }, {
  				j: [{ type: 59, g: 2 }, { type: 59, g: 2 }], callFunction: function (a, b, c, d, e) {
  					var f = !1, g = Ue(a, b, c, d, e); return C.create({
  						next: function () {
  							if (f) return x; var k = g.next(0); if (k.done) return k; f = !0; return y(w(k.value,
  								0))
  						}
  					})
  				}, localName: "deep-equal", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 0, g: 3 }
  			}, { j: [{ type: 59, g: 2 }, { type: 59, g: 2 }, { type: 1, g: 3 }], callFunction: function () { throw Error("FOCH0002: No collations are supported"); }, localName: "deep-equal", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 0, g: 3 } }, {
  				j: [{ type: 59, g: 2 }], callFunction: function (a, b, c, d) { var e = !1; return C.create({ next: function () { if (e) return x; var f = d.Qa(); e = !0; return y(w(f, 5)) } }) }, localName: "count", namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				i: { type: 5, g: 3 }
  			}, {
  				j: [{ type: 46, g: 2 }], callFunction: function (a, b, c, d) { if (d.G()) return d; a = Xq(d.O()); a = cj(a); if (!a) throw Error("FORG0006: Incompatible types to be converted to a common type"); if (!a.every(function (e) { return B(e.type, 2) })) throw Error("FORG0006: items passed to fn:avg are not all numeric."); b = a.reduce(function (e, f) { return e + f.value }, 0) / a.length; return a.every(function (e) { return B(e.type, 5) || B(e.type, 3) }) ? C.m(w(b, 3)) : a.every(function (e) { return B(e.type, 4) }) ? C.m(w(b, 4)) : C.m(w(b, 6)) }, localName: "avg",
  				namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 0 }
  			}, { j: [{ type: 46, g: 2 }], callFunction: function (a, b, c, d) { if (d.G()) return d; a = Yq(d.O()); return C.m(a.reduce(function (e, f) { return e.value < f.value ? f : e })) }, localName: "max", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 0 } }, { j: [{ type: 46, g: 2 }, { type: 1, g: 3 }], callFunction: function () { throw Error("FOCH0002: No collations are supported"); }, localName: "max", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 0 } },
  			{ j: [{ type: 46, g: 2 }], callFunction: function (a, b, c, d) { if (d.G()) return d; a = Yq(d.O()); return C.m(a.reduce(function (e, f) { return e.value > f.value ? f : e })) }, localName: "min", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 0 } }, { j: [{ type: 46, g: 2 }, { type: 1, g: 3 }], callFunction: function () { throw Error("FOCH0002: No collations are supported"); }, localName: "min", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 0 } }, {
  				j: [{ type: 46, g: 2 }], callFunction: function (a, b, c, d) {
  					return $q(a, b, c, d, C.m(w(0,
  						5)))
  				}, localName: "sum", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 3 }
  			}, { j: [{ type: 46, g: 2 }, { type: 46, g: 0 }], callFunction: $q, localName: "sum", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 46, g: 0 } }, { j: [{ type: 59, g: 2 }], callFunction: function (a, b, c, d) { if (!d.G() && !d.wa()) throw Error("FORG0003: The argument passed to fn:zero-or-one contained more than one item."); return d }, localName: "zero-or-one", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 0 } }, {
  				j: [{
  					type: 59,
  					g: 2
  				}], callFunction: function (a, b, c, d) { if (d.G()) throw Error("FORG0004: The argument passed to fn:one-or-more was empty."); return d }, localName: "one-or-more", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 1 }
  			}, { j: [{ type: 59, g: 2 }], callFunction: function (a, b, c, d) { if (!d.wa()) throw Error("FORG0005: The argument passed to fn:exactly-one is empty or contained more than one item."); return d }, localName: "exactly-one", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 3 } }, {
  				j: [{
  					type: 59,
  					g: 2
  				}, { type: 60, g: 3 }], callFunction: function (a, b, c, d, e) { if (d.G()) return d; var f = e.first(), g = f.o; if (1 !== g.length) throw Error("XPTY0004: signature of function passed to fn:filter is incompatible."); return d.filter(function (k) { k = fe(g[0], C.m(k), b, "fn:filter", !1); k = f.value.call(void 0, a, b, c, k); if (!k.wa() || !B(k.first().type, 0)) throw Error("XPTY0004: signature of function passed to fn:filter is incompatible."); return k.first().value }) }, localName: "filter", namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				i: { type: 59, g: 2 }
  			}, {
  				j: [{ type: 59, g: 2 }, { type: 60, g: 3 }], callFunction: function (a, b, c, d, e) { if (d.G()) return d; var f = e.first(), g = f.o; if (1 !== g.length) throw Error("XPTY0004: signature of function passed to fn:for-each is incompatible."); var k = d.value, l; return C.create({ next: function (m) { for (; ;) { if (!l) { var q = k.next(0); if (q.done) return q; q = fe(g[0], C.m(q.value), b, "fn:for-each", !1); l = f.value.call(void 0, a, b, c, q).value; } q = l.next(m); if (!q.done) return q; l = null; } } }) }, localName: "for-each", namespaceURI: "http://www.w3.org/2005/xpath-functions",
  				i: { type: 59, g: 2 }
  			}, { j: [{ type: 59, g: 2 }, { type: 59, g: 2 }, { type: 60, g: 3 }], callFunction: function (a, b, c, d, e, f) { if (d.G()) return d; var g = f.first(), k = g.o; if (2 !== k.length) throw Error("XPTY0004: signature of function passed to fn:fold-left is incompatible."); return d.M(function (l) { return l.reduce(function (m, q) { m = fe(k[0], m, b, "fn:fold-left", !1); q = fe(k[1], C.m(q), b, "fn:fold-left", !1); return g.value.call(void 0, a, b, c, m, q) }, e) }) }, localName: "fold-left", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 } },
  			{ j: [{ type: 59, g: 2 }, { type: 59, g: 2 }, { type: 60, g: 3 }], callFunction: function (a, b, c, d, e, f) { if (d.G()) return d; var g = f.first(), k = g.o; if (2 !== k.length) throw Error("XPTY0004: signature of function passed to fn:fold-right is incompatible."); return d.M(function (l) { return l.reduceRight(function (m, q) { m = fe(k[0], m, b, "fn:fold-right", !1); q = fe(k[1], C.m(q), b, "fn:fold-right", !1); return g.value.call(void 0, a, b, c, q, m) }, e) }) }, localName: "fold-right", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 59, g: 2 } }, {
  				j: [{
  					type: 59,
  					g: 2
  				}], callFunction: function (a, b, c, d) { if (!b.v) throw Error("serialize() called but no xmlSerializer set in execution parameters."); a = d.O(); if (!a.every(function (e) { return B(e.type, 53) })) throw Error("Expected argument to fn:serialize to resolve to a sequence of Nodes."); return C.m(w(a.map(function (e) { return b.v.serializeToString(ig(e.value, b, !1)) }).join(""), 1)) }, localName: "serialize", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 1, g: 3 }
  			}], De, [{
  				j: [{ type: 59, g: 3 }, { type: 61, g: 3 }], callFunction: function (a,
  					b, c, d, e) { var f, g; return C.create({ next: function () { if (!f) { var k = Qq(d, e, c, b); f = k.sc; g = k.pc; } try { return f.next(0) } catch (l) { pg(g.value, l); } } }) }, localName: "evaluate", namespaceURI: "http://fontoxml.com/fontoxpath", i: { type: 59, g: 2 }
  			}, { j: [], callFunction: function () { return C.m(w(VERSION, 1)) }, localName: "version", namespaceURI: "http://fontoxml.com/fontoxpath", i: { type: 1, g: 3 } }], [{
  				j: [{ type: 23, g: 3 }, { type: 5, g: 3 }], callFunction: function (a, b, c, d, e) {
  					return ac([d, e], function (f) {
  						var g =
  							p(f); f = g.next().value; g = g.next().value; var k = c.xa(f.value.namespaceURI, f.value.localName, g.value); if (null === k) return C.empty(); f = new Ab({ j: k.j, arity: g.value, localName: f.value.localName, namespaceURI: f.value.namespaceURI, i: k.i, value: k.callFunction }); return C.m(f)
  					})
  				}, localName: "function-lookup", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { g: 0, type: 60 }
  			}, {
  				j: [{ type: 60, g: 3 }], callFunction: function (a, b, c, d) {
  					return ac([d], function (e) {
  						e = p(e).next().value; return e.Sa() ? C.empty() : C.m(w(new zb("", e.l,
  							e.B), 23))
  					})
  				}, localName: "function-name", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 23, g: 0 }
  			}, { j: [{ type: 60, g: 3 }], callFunction: function (a, b, c, d) { return ac([d], function (e) { e = p(e).next().value; return C.m(w(e.v, 5)) }) }, localName: "function-arity", namespaceURI: "http://www.w3.org/2005/xpath-functions", i: { type: 5, g: 3 } }]); function br(a) { this.h = a; } h = br.prototype; h.createAttributeNS = function (a, b) { return this.h.createAttributeNS(a, b) }; h.createCDATASection = function (a) { return this.h.createCDATASection(a) }; h.createComment = function (a) { return this.h.createComment(a) }; h.createDocument = function () { return this.h.createDocument() }; h.createElementNS = function (a, b) { return this.h.createElementNS(a, b) }; h.createProcessingInstruction = function (a, b) { return this.h.createProcessingInstruction(a, b) }; h.createTextNode = function (a) { return this.h.createTextNode(a) }; var cr = Symbol("IS_XPATH_VALUE_SYMBOL"); function dr(a) { return function (b, c) { b = Dc(new Mb(null === c ? new Gb : c), b, qb(a)); c = {}; return c[cr] = !0, c.Hb = b, c } } ar.forEach(function (a) { wg(a.namespaceURI, a.localName, a.j, a.i, a.callFunction); }); function er(a) { return a && "object" === typeof a && "lookupNamespaceURI" in a ? function (b) { return a.lookupNamespaceURI(b || null) } : function () { return null } } function fr(a) { return function (b) { var c = b.localName; return b.prefix ? null : { namespaceURI: a, localName: c } } }
  			function gr(a, b, c, d, e, f) {
  				if (null === d || void 0 === d) d = d || {}; if (e) { var g = e.logger || { trace: console.log.bind(console) }; var k = e.documentWriter; var l = e.moduleImports; var m = e.namespaceResolver; var q = e.functionNameResolver; var u = e.nodesFactory; var z = e.xmlSerializer; } else g = { trace: console.log.bind(console) }, l = {}, z = k = u = m = null, q = void 0; var A = new Mb(null === c ? new Gb : c); c = l || Object.create(null); l = void 0 === e.defaultFunctionNamespaceURI ? "http://www.w3.org/2005/xpath-functions" : e.defaultFunctionNamespaceURI; var D = Pq(a,
  					f, m || er(b), d, c, l, q || fr(l)); a = b ? Ec(A, b) : C.empty(); b = !u && f.$ ? new of(b) : new br(u); k = k ? new Jb(k) : Ib; u = Object.keys(d).reduce(function (T, ia) { var Ba = d[ia]; T["Q{}" + ia + "[0]"] = Ba && "object" === typeof Ba && cr in Ba ? function () { return C.create(Ba.Hb) } : function () { return Ec(A, d[ia]) }; return T }, Object.create(null)); m = {}; q = p(Object.keys(D.ia.Ia)); for (c = q.next(); !c.done; m = { Za: m.Za }, c = q.next())m.Za = c.value, u[m.Za] || (u[m.Za] = function (T) { return function () { return (0, D.ia.Ia[T.Za])(F, J) } }(m)); var F = new Hc({
  						N: a.first(), Ka: 0,
  						Da: a, Aa: u
  					}); var J = new Oc(f.debug, f.La, A, b, k, e.currentContext, new Map, g, z); return { Bb: F, Cb: J, ca: D.ca }
  			} function hr(a, b) {
  				var c = {}, d = 0, e = !1, f = null; return {
  					next: function () {
  						if (e) return x; for (var g = {}; d < a.h.length;) {
  							a: {
  								var k = a.h[d].key.value; if (!f) {
  									g.pb = a.h[d]; var l = g.pb.value().Z({ default: function (m) { return m }, multiple: function (m) { return function () { throw Error("Serialization error: The value of an entry in a map is expected to be a single item or an empty sequence. Use arrays when putting multiple values in a map. The value of the key " + m.pb.key.value + " holds multiple items"); } }(g) }).first(); if (null ===
  										l) { c[k] = null; d++; break a } f = ir(l, b);
  								} l = f.next(0); f = null; c[k] = l.value; d++;
  							} g = { pb: g.pb };
  						} e = !0; return y(c)
  					}
  				}
  			}
  			function jr(a, b) { var c = [], d = 0, e = !1, f = null; return { next: function () { if (e) return x; for (; d < a.P.length;) { if (!f) { var g = a.P[d]().Z({ default: function (k) { return k }, multiple: function () { throw Error("Serialization error: The value of an entry in an array is expected to be a single item or an empty sequence. Use nested arrays when putting multiple values in an array."); } }).first(); if (null === g) { c[d++] = null; continue } f = ir(g, b); } g = f.next(0); f = null; c[d++] = g.value; } e = !0; return y(c) } } }
  			function ir(a, b) { if (B(a.type, 61)) return hr(a, b); if (B(a.type, 62)) return jr(a, b); if (B(a.type, 23)) { var c = a.value; return { next: function () { return y("Q{" + (c.namespaceURI || "") + "}" + c.localName) } } } switch (a.type) { case 7: case 8: case 9: case 11: case 12: case 13: case 14: case 15: var d = a.value; return { next: function () { return y(uc(d)) } }; case 47: case 53: case 54: case 55: case 56: case 57: case 58: var e = a.value; return { next: function () { return y(ig(e, b, !1)) } }; default: return { next: function () { return y(a.value) } } } } var kr = { ANY: 0, NUMBER: 1, STRING: 2, BOOLEAN: 3, NODES: 7, FIRST_NODE: 9, STRINGS: 10, MAP: 11, ARRAY: 12, NUMBERS: 13, ALL_RESULTS: 14, ASYNC_ITERATOR: 99 }; kr[kr.ANY] = "ANY"; kr[kr.NUMBER] = "NUMBER"; kr[kr.STRING] = "STRING"; kr[kr.BOOLEAN] = "BOOLEAN"; kr[kr.NODES] = "NODES"; kr[kr.FIRST_NODE] = "FIRST_NODE"; kr[kr.STRINGS] = "STRINGS"; kr[kr.MAP] = "MAP"; kr[kr.ARRAY] = "ARRAY"; kr[kr.NUMBERS] = "NUMBERS"; kr[kr.ALL_RESULTS] = "ALL_RESULTS"; kr[kr.ASYNC_ITERATOR] = "ASYNC_ITERATOR";
  			function lr(a, b, c, d) {
  				switch (c) {
  					case 3: return b.ha(); case 2: return b = Zc(b, d).O(), b.length ? b.map(function (l) { return Pd(l, 1).value }).join(" ") : ""; case 10: return b = Zc(b, d).O(), b.length ? b.map(function (l) { return l.value + "" }) : []; case 1: return b = b.first(), null !== b && B(b.type, 2) ? b.value : NaN; case 9: b = b.first(); if (null === b) return null; if (!B(b.type, 53)) throw Error("Expected XPath " + ng(a) + " to resolve to Node. Got " + mb[b.type]); return ig(b.value, d, !1); case 7: b = b.O(); if (!b.every(function (l) { return B(l.type, 53) })) throw Error("Expected XPath " +
  						ng(a) + " to resolve to a sequence of Nodes."); return b.map(function (l) { return ig(l.value, d, !1) }); case 11: b = b.O(); if (1 !== b.length) throw Error("Expected XPath " + ng(a) + " to resolve to a single map."); b = b[0]; if (!B(b.type, 61)) throw Error("Expected XPath " + ng(a) + " to resolve to a map"); return hr(b, d).next(0).value; case 12: b = b.O(); if (1 !== b.length) throw Error("Expected XPath " + ng(a) + " to resolve to a single array."); b = b[0]; if (!B(b.type, 62)) throw Error("Expected XPath " + ng(a) + " to resolve to an array"); return jr(b,
  							d).next(0).value; case 13: return b.O().map(function (l) { if (!B(l.type, 2)) throw Error("Expected XPath " + ng(a) + " to resolve to numbers"); return l.value }); case 99: var e = b.value, f = null, g = !1, k = function () { for (; !g;) { if (!f) { var l = e.next(0); if (l.done) { g = !0; break } f = ir(l.value, d); } l = f.next(0); f = null; return l } return Promise.resolve({ done: !0, value: null }) }; "asyncIterator" in Symbol ? (b = {}, b = (b[Symbol.asyncIterator] = function () { return this }, b.next = function () {
  								return (new Promise(function (l) { return l(k()) })).catch(function (l) {
  									pg(a,
  										l);
  								})
  							}, b)) : b = { next: function () { return new Promise(function (l) { return l(k()) }) } }; return b; case 14: return b.O().map(function (l) { return ir(l, d).next(0).value }); default: return b = b.O(), b.every(function (l) { return B(l.type, 53) && !B(l.type, 47) }) ? (b = b.map(function (l) { return ig(l.value, d, !1) }), 1 === b.length ? b[0] : b) : 1 === b.length ? (b = b[0], B(b.type, 62) ? jr(b, d).next(0).value : B(b.type, 61) ? hr(b, d).next(0).value : Yc(b, d).first().value) : Zc(C.create(b), d).O().map(function (l) { return l.value })
  				}
  			} var mr = !1, nr = null, or = {
  				getPerformanceSummary: function () { var a = nr.getEntriesByType("measure").filter(function (b) { return b.name.startsWith("XPath: ") }); return Array.from(a.reduce(function (b, c) { var d = c.name.substring(7); b.has(d) ? (d = b.get(d), d.times += 1, d.totalDuration += c.duration) : b.set(d, { xpath: d, times: 1, totalDuration: c.duration, average: 0 }); return b }, new Map).values()).map(function (b) { b.average = b.totalDuration / b.times; return b }).sort(function (b, c) { return c.totalDuration - b.totalDuration }) }, setPerformanceImplementation: function (a) {
  					nr =
  					a;
  				}, startProfiling: function () { if (null === nr) throw Error("Performance API object must be set using `profiler.setPerformanceImplementation` before starting to profile"); nr.clearMarks(); nr.clearMeasures(); mr = !0; }, stopProfiling: function () { mr = !1; }
  			}, pr = 0; var qr = { XPATH_3_1_LANGUAGE: "XPath3.1", XQUERY_3_1_LANGUAGE: "XQuery3.1", XQUERY_UPDATE_3_1_LANGUAGE: "XQueryUpdate3.1" }; function rr(a, b, c, d, e, f) {
  				e = e || 0; if (!a || "string" !== typeof a && !("nodeType" in a)) throw new TypeError("Failed to execute 'evaluateXPath': xpathExpression must be a string or an element depicting an XQueryX DOM tree."); f = f || {}; try { var g = gr(a, b, c || null, d || {}, f, { ua: "XQueryUpdate3.1" === f.language, $: "XQuery3.1" === f.language || "XQueryUpdate3.1" === f.language, debug: !!f.debug, La: !!f.disableCache }); var k = g.Bb; var l = g.Cb; var m = g.ca; } catch (z) { pg(a, z); } if (m.I) throw Error("XUST0001: Updating expressions should be evaluated as updating expressions");
  				if (3 === e && b && "object" === typeof b && "nodeType" in b && (c = m.B(), b = Fb(b), null !== c && !b.includes(c))) return !1; try { b = a; mr && ("string" !== typeof b && (b = ng(b)), nr.mark(b + (0 === pr ? "" : "@" + pr)), pr++); var q = I(m, k, l), u = lr(a, q, e, l); e = a; mr && ("string" !== typeof e && (e = ng(e)), pr--, k = e + (0 === pr ? "" : "@" + pr), nr.measure("XPath: " + e, k), nr.clearMarks(k)); return u } catch (z) { pg(a, z); }
  			} Object.assign(rr, { tc: 14, ANY_TYPE: 0, Tb: 12, Ub: 99, BOOLEAN_TYPE: 3, Wb: 9, Zb: 11, ac: 7, bc: 13, NUMBER_TYPE: 1, cc: 10, STRING_TYPE: 2, uc: "XPath3.1", vc: "XQuery3.1", fc: "XQueryUpdate3.1" });
  			var sr = {}; Object.assign(rr, (sr.ALL_RESULTS_TYPE = 14, sr.ANY_TYPE = 0, sr.ARRAY_TYPE = 12, sr.ASYNC_ITERATOR_TYPE = 99, sr.BOOLEAN_TYPE = 3, sr.FIRST_NODE_TYPE = 9, sr.MAP_TYPE = 11, sr.NODES_TYPE = 7, sr.NUMBERS_TYPE = 13, sr.NUMBER_TYPE = 1, sr.STRINGS_TYPE = 10, sr.STRING_TYPE = 2, sr.XPATH_3_1_LANGUAGE = "XPath3.1", sr.XQUERY_3_1_LANGUAGE = "XQuery3.1", sr.XQUERY_UPDATE_3_1_LANGUAGE = "XQueryUpdate3.1", sr)); function tr(a, b, c, d, e) { return rr(a, b, c, d, rr.Ub, e) } function ur(a, b, c, d) { var e = {}; return e.pendingUpdateList = a.fa.map(function (f) { return f.h(d) }), e.xdmValue = lr(b, C.create(a.J), c, d), e } function vr(a, b, c, d, e) {
  				var f, g, k, l, m, q, u, z, A, D; return za(new ya(new ua(function (F) {
  					switch (F.h) {
  						case 1: e = e || {}; Vk(); try { l = gr(a, b, c || null, d || {}, e || {}, { ua: !0, $: !0, debug: !!e.debug, La: !!e.disableCache }), f = l.Bb, g = l.Cb, k = l.ca; } catch (T) { pg(a, T); } if (k.I) { F.h = 2; break } m = []; q = tr(a, b, c, d, Object.assign(Object.assign({}, e), { language: "XQueryUpdate3.1" })); var J = q.next(); F.h = 3; return { value: J }; case 3: u = F.l; case 4: if (u.done) { F.h = 6; break } m.push(u.value); J = q.next(); F.h = 7; return { value: J }; case 7: u = F.l; F.h = 4; break; case 6: return z =
  							{}, F.return(Promise.resolve((z.pendingUpdateList = [], z.xdmValue = m, z))); case 2: try { D = k.s(f, g), A = D.next(0); } catch (T) { pg(a, T); } return F.return(ur(A.value, a, e.returnType, g))
  					}
  				})))
  			} function wr(a, b, c, d, e) { e = e || {}; Vk(); try { var f = gr(a, b, c || null, d || {}, e || {}, { ua: !0, $: !0, debug: !!e.debug, La: !!e.disableCache }); var g = f.Bb; var k = f.Cb; var l = f.ca; } catch (q) { pg(a, q); } if (!l.I) return g = {}, k = {}, k.pendingUpdateList = [], k.xdmValue = rr(a, b, c, d, e.i, Object.assign(Object.assign({}, e), (g.language = rr.fc, g))), k; try { var m = l.s(g, k).next(0); } catch (q) { pg(a, q); } return ur(m.value, a, e.returnType, k) } function xr(a, b, c, d, e) { return rr(a, b, c, d, rr.Tb, e) } function yr(a, b, c, d, e) { return rr(a, b, c, d, rr.BOOLEAN_TYPE, e) } function zr(a, b, c, d, e) { return rr(a, b, c, d, rr.Wb, e) } function Ar(a, b, c, d, e) { return rr(a, b, c, d, rr.Zb, e) } function Br(a, b, c, d, e) { return rr(a, b, c, d, rr.ac, e) } function Cr(a, b, c, d, e) { return rr(a, b, c, d, rr.NUMBER_TYPE, e) } function Dr(a, b, c, d, e) { return rr(a, b, c, d, rr.bc, e) } function Er(a, b, c, d, e) { return rr(a, b, c, d, rr.STRING_TYPE, e) } function Fr(a, b, c, d, e) { return rr(a, b, c, d, rr.cc, e) } function Gr(a, b, c, d) { b = new Mb(b ? b : new Gb); d = d ? new Jb(d) : Ib; c = c ? c = new br(c) : null; a = a.map(Zj); xf(a, b, c, d); } function Y(a, b, c) { return { code: a, va: b, H: c, isAstAccepted: !0 } } function Hr(a) { return { isAstAccepted: !1, reason: a } } function Z(a, b) { return a.isAstAccepted ? b(a) : a } function Ir(a, b) { return a.isAstAccepted ? b(a) : [a, null] }
  			function Jr(a, b, c) { return Z(a, function (d) { switch (d.va.type) { case 0: return d; case 1: return Z(Kr(c, d, "nodes"), function (e) { return Z(Kr(c, b, "contextItem"), function (f) { return Y("(function () {\n\t\t\t\t\t\t\tconst { done, value } = " + e.code + "(" + f.code + ").next();\n\t\t\t\t\t\t\treturn done ? null : value;\n\t\t\t\t\t\t})()", { type: 0 }, [].concat(t(e.H), t(f.H))) }) }); default: throw Error("invalid generated code type to convert to value: " + d.va.type); } }) }
  			function Lr(a, b, c, d) { a = Jr(a, c, d); return b && 0 === b.type && 3 === b.g ? a : Z(a, function (e) { return Y("!!" + e.code, { type: 0 }, e.H) }) } function Mr(a, b, c) { return b ? a.isAstAccepted && 0 !== a.va.type ? Hr("Atomization only implemented for single value") : B(b.type, 1) ? a : B(b.type, 47) ? Z(Kr(c, a, "attr"), function (d) { return Y("(" + d.code + " ? domFacade.getData(" + d.code + ") : null)", { type: 0 }, d.H) }) : Hr("Atomization only implemented for string and attribute") : Hr("Can not atomize value if type was not annotated") }
  			function Nr(a, b, c, d) { a = Jr(a, c, d); d = Mr(a, b, d); return ed(b) ? Z(d, function (e) { return Y(e.code + " ?? ''", { type: 0 }, e.H) }) : d }
  			function Or(a, b, c) { return Z(Kr(c, a, "node"), function (d) { return 1 === d.va.type ? d : b && !B(b.type, 53) ? Hr("Can not evaluate to node if expression does not result in nodes") : Y("(function () {\n\t\t\t\tif (" + d.code + " !== null && !" + d.code + ".nodeType) {\n\t\t\t\t\tthrow new Error('XPDY0050: The result of the expression was not a node');\n\t\t\t\t}\n\t\t\t\treturn " + d.code + ";\n\t\t\t})()", { type: 0 }, d.H) }) }
  			function Pr(a, b, c, d) { return Z(a, function (e) { switch (e.va.type) { case 1: return Z(Kr(d, e, "nodes"), function (f) { return Z(Kr(d, c, "contextItem"), function (g) { return Y("Array.from(" + f.code + "(" + g.code + "))", { type: 0 }, [].concat(t(f.H), t(g.H))) }) }); case 0: return Z(Kr(d, Or(e, b, d), "node"), function (f) { return Y("(" + f.code + " === null ? [] : [" + f.code + "])", { type: 0 }, f.H) }); default: return Hr("Unsupported code type to evaluate to nodes") } }) }
  			function Qr(a, b) { return Z(a, function (c) { return Z(b, function (d) { if (0 !== c.va.type || 0 !== d.va.type) throw Error("can only use emitAnd with value expressions"); return Y(c.code + " && " + d.code, { type: 0 }, [].concat(t(c.H), t(d.H))) }) }) } function Rr(a, b, c, d) { return (a = N(a, [b, "*"])) ? d.h(a, c, d) : [Hr(b + " expression not found"), null] } var Sr = {}, Tr = (Sr.equalOp = "eqOp", Sr.notEqualOp = "neOp", Sr.lessThanOrEqualOp = "leOp", Sr.lessThanOp = "ltOp", Sr.greaterThanOrEqualOp = "geOp", Sr.greaterThanOp = "gtOp", Sr), Ur = {}, Vr = (Ur.eqOp = "eqOp", Ur.neOp = "neOp", Ur.leOp = "geOp", Ur.ltOp = "gtOp", Ur.geOp = "leOp", Ur.gtOp = "ltOp", Ur);
  			function Wr(a, b, c, d) {
  				var e = M(N(a, ["firstOperand", "*"]), "type"), f = M(N(a, ["secondOperand", "*"]), "type"); if (!e || !f) return Hr("Can not generate code for value compare without both types"); var g = [47, 1]; if (!g.includes(e.type) || !g.includes(f.type)) return Hr("Unsupported types in compare: [" + mb[e.type] + ", " + mb[f.type] + "]"); g = new Map([["eqOp", "==="], ["neOp", "!=="]]); if (!g.has(b)) return Hr(b + " not yet implemented"); var k = g.get(b); b = p(Rr(a, "firstOperand", c, d)); g = b.next().value; b.next(); b = Jr(g, c, d); b = Mr(b, e,
  					d); return Z(Kr(d, b, "first"), function (l) { var m = p(Rr(a, "secondOperand", c, d)), q = m.next().value; m.next(); m = Jr(q, c, d); m = Mr(m, f, d); return Z(Kr(d, m, "second"), function (u) { var z = []; ed(e) && z.push(l.code + " === null"); ed(f) && z.push(u.code + " === null"); return Y("(" + (z.length ? z.join(" || ") + " ? null : " : "") + l.code + " " + k + " " + u.code + ")", { type: 0 }, [].concat(t(l.H), t(u.H))) }) })
  			}
  			function Xr(a, b, c, d, e, f) {
  				var g = M(N(a, [b, "*"]), "type"), k = M(N(a, [c, "*"]), "type"); if (!g || !k) return Hr("Can not generate code for general compare without both types"); var l = [47, 1]; if (!l.includes(g.type) || !l.includes(k.type)) return Hr("Unsupported types in compare: [" + mb[g.type] + ", " + mb[k.type] + "]"); l = new Map([["eqOp", "==="], ["neOp", "!=="]]); if (!l.has(d)) return Hr(d + " not yet implemented"); var m = l.get(d); b = p(Rr(a, b, e, f)); d = b.next().value; b.next(); b = Jr(d, e, f); g = Mr(b, g, f); return Z(Kr(f, g, "single"), function (q) {
  					var u =
  						p(Rr(a, c, e, f)), z = u.next().value; u.next(); return Z(Kr(f, z, "multiple"), function (A) {
  							if (1 !== A.va.type) return Hr("can only generate general compare for a single value and a generator"); var D = vu(f, wu(f, "n")), F = Mr(D, k, f); return Z(e, function (J) {
  								return Z(F, function (T) {
  									return Y("(function () {\n\t\t\t\t\t\t\t\t\tfor (const " + D.code + " of " + A.code + "(" + J.code + ")) {\n\t\t\t\t\t\t\t\t\t\t" + T.H.join("\n") + "\n\t\t\t\t\t\t\t\t\t\tif (" + T.code + " " + m + " " + q.code + ") {\n\t\t\t\t\t\t\t\t\t\t\treturn true;\n\t\t\t\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\t\t\t}\n\t\t\t\t\t\t\t\t\treturn false;\n\t\t\t\t\t\t\t\t})()",
  										{ type: 0 }, [].concat(t(q.H), t(D.H), t(J.H), t(A.H)))
  								})
  							})
  						})
  				})
  			} function xu(a) { return JSON.stringify(a).replace(/\u2028/g, "\\u2028").replace(/\u2029/g, "\\u2029") } var Du = { "false#0": yu, "local-name#0": zu, "local-name#1": zu, "name#0": Au, "name#1": Au, "not#1": Bu, "true#0": Cu }, Eu = {}, Fu = (Eu["http://fontoxml.com/fontoxpath"] = ["version#0"], Eu[""] = ["root#1", "path#1"], Eu);
  			function Gu(a, b, c, d) { var e = p(d.h(a, c, d)), f = e.next().value; e.next(); a = M(a, "type"); if (b ? 2 === b.g || 1 === b.g : 1) return Hr("Not supported: sequence arguments with multiple items"); if (B(b.type, 53)) return b = Jr(f, c, d), Or(b, a, d); switch (b.type) { case 59: return Jr(f, c, d); case 0: return Lr(f, a, c, d); case 1: return Nr(f, a, c, d) }return Hr("Argument types not supported: " + (a ? mb[a.type] : "unknown") + " -> " + mb[b.type]) }
  			function Hu(a, b, c, d) { if (a.length !== b.length || b.some(function (k) { return 4 === k })) return Hr("Not supported: variadic function or mismatch in argument count"); if (0 === a.length) return Y("", { type: 0 }, []); var e = p(a); a = e.next().value; var f = ja(e); b = p(b); e = b.next().value; var g = ja(b); a = Kr(d, Gu(a, e, c, d), "arg"); return 0 === f.length ? a : Z(a, function (k) { var l = Hu(f, g, c, d); return Z(l, function (m) { return Y(k.code + ", " + m.code, { type: 0 }, [].concat(t(k.H), t(m.H))) }) }) }
  			function Iu(a, b) { return Z(a, function (c) { return (b ? 2 === b.g || 1 === b.g : 1) || ![0, 1].includes(b.type) && !B(b.type, 53) ? Hr("Function return type " + mb[b.type] + " not supported") : c }) }
  			function Ju(a, b, c) {
  				var d = Ug(L(a, "functionName")), e = d.localName, f = d.namespaceURI; d = O(L(a, "arguments"), "*"); var g = d.length, k = e + "#" + g, l = f === c.B; if (l) { var m = Du[k]; if (void 0 !== m) return m(a, b, c) } if ((a = Fu[l ? "" : f]) && !a.includes(k)) return Hr("Not supported: built-in function not on allow list: " + k); a = vg(f, e, g); if (!a) return Hr("Unknown function / arity: " + k); if (a.I) return Hr("Not supported: updating functions"); b = Hu(d, a.j, b, c); b = Z(b, function (q) {
  					return Y("runtimeLib.callFunction(domFacade, " + xu(f) + ", " + xu(e) +
  						", [" + q.code + "], options)", { type: 0 }, q.H)
  				}); return Iu(b, a.i)
  			} function Ku(a, b) { return Z(Kr(b, a, "contextItem"), function (c) { return Y(c.code, { type: 0 }, [].concat(t(c.H), ["if (" + c.code + " === undefined || " + c.code + " === null) {\n\t\t\t\t\tthrow errXPDY0002('The function which was called depends on dynamic context, which is absent.');\n\t\t\t\t}"])) }) }
  			function Lu(a, b, c, d) { if ((a = N(a, ["arguments", "*"])) && "contextItemExpr" !== a[0]) { var e = M(a, "type"); if (!e || !B(e.type, 53)) return Hr("name function only implemented if arg is a node"); a = p(c.h(a, b, c)); e = a.next().value; a.next(); a = e; } else a = Ku(b, c); b = Jr(a, b, c); return Z(Kr(c, b, "arg"), function (f) { return Y("(" + f.code + " ? " + d(f.code) + " : '')", { type: 0 }, f.H) }) }
  			function Au(a, b, c) { return Lu(a, b, c, function (d) { return "(((" + d + ".prefix || '').length !== 0 ? " + d + ".prefix + ':' : '')\n\t\t+ (" + d + ".localName || " + d + ".target || ''))" }) } function zu(a, b, c) { return Lu(a, b, c, function (d) { return "(" + d + ".localName || " + d + ".target || '')" }) } function Bu(a, b, c) { var d = N(a, ["arguments", "*"]); a = M(d, "type"); d = p(c.h(d, b, c)); var e = d.next().value; d.next(); b = Lr(e, a, b, c); return Z(b, function (f) { return Y("!" + f.code, { type: 0 }, f.H) }) } function yu() { return Y("false", { type: 0 }, []) }
  			function Cu() { return Y("true", { type: 0 }, []) } function Mu(a, b, c, d) { var e = p(Rr(a, "firstOperand", c, d)), f = e.next().value; e = e.next().value; var g = M(N(a, ["firstOperand", "*"]), "type"); f = Lr(f, g, c, d); g = p(Rr(a, "secondOperand", c, d)); var k = g.next().value; g = g.next().value; f = Z(f, function (l) { var m = M(N(a, ["secondOperand", "*"]), "type"); m = Lr(k, m, c, d); return Z(m, function (q) { return Y("(" + l.code + " " + b + " " + q.code + ")", { type: 0 }, [].concat(t(l.H), t(q.H))) }) }); e = "&&" === b ? Qh(e, g) : e === g ? e : null; return [f, e] } function Nu(a, b, c) { return Z(a, function (d) { return Z(b, function (e) { return Z(c, function (f) { return Y("for (" + d.code + ") {\n\t\t\t\t\t\t" + e.H.join("\n") + "\n\t\t\t\t\t\tif (!(" + e.code + ")) {\n\t\t\t\t\t\t\tcontinue;\n\t\t\t\t\t\t}\n\t\t\t\t\t\t" + f.H.join("\n") + "\n\t\t\t\t\t\t" + f.code + "\n\t\t\t\t\t}", { type: 2 }, d.H) }) }) }) }
  			function Ou(a, b, c, d, e) { var f = b ? ', "' + b + '"' : ""; b = Z(d, function (g) { return Z(e, function (k) { return Y("let " + g.code + " = domFacade.getFirstChild(" + k.code + f + ");\n\t\t\t\t\t\t\t" + g.code + ";\n\t\t\t\t\t\t\t" + g.code + " = domFacade.getNextSibling(" + g.code + f + ")", { type: 2 }, [].concat(t(g.H), t(k.H))) }) }); return Nu(b, a, c) }
  			function Pu(a, b, c, d, e) { var f = Qh(b, "type-2"), g = Z(e, function (k) { return Y("(" + k.code + " && " + k.code + ".nodeType === /*ELEMENT_NODE*/ 1 ? domFacade.getAllAttributes(" + k.code + (f ? ', "' + f + '"' : "") + ") : [])", { type: 0 }, k.H) }); b = Z(d, function (k) { return Z(g, function (l) { return Y("const " + k.code + " of " + l.code, { type: 2 }, [].concat(t(k.H), t(l.H))) }) }); return Nu(b, a, c) } function Qu(a, b, c, d, e) { var f = b ? ', "' + b + '"' : ""; b = Z(e, function (g) { return Y("domFacade.getParentNode(" + g.code + f + ")", { type: 0 }, g.H) }); return Ru(d, b, a, c) }
  			function Ru(a, b, c, d) { var e = Qr(a, c); return Z(a, function (f) { return Z(b, function (g) { return Z(e, function (k) { return Z(d, function (l) { return Y("const " + f.code + " = " + g.code + ";\n\t\t\t\t\t\t" + k.H.join("\n") + "\n\t\t\t\t\t\tif (" + k.code + ") {\n\t\t\t\t\t\t\t" + l.H.join("\n") + "\n\t\t\t\t\t\t\t" + l.code + "\n\t\t\t\t\t\t}", { type: 2 }, [].concat(t(f.H), t(g.H))) }) }) }) }) }
  			function Su(a, b, c, d, e, f) { a = Tg(a); switch (a) { case "attribute": return [Pu(b, c, d, e, f), "type-1"]; case "child": return [Ou(b, c, d, e, f), null]; case "parent": return [Qu(b, c, d, e, f), null]; case "self": return [Ru(e, f, b, d), c]; default: return [Hr("Unsupported: the " + a + " axis"), null] } } var Tu = { dc: "textTest", Vb: "elementTest", $b: "nameTest", ec: "Wildcard", Sb: "anyKindTest" }, Uu = Object.values(Tu); function Vu(a) { return [Z(a, function (b) { return Y(b.code + ".nodeType === /*TEXT_NODE*/ 3", { type: 0 }, []) }), "type-3"] } function Wu(a, b) { if (null === a.namespaceURI && "*" !== a.prefix) { b = b.aa(a.prefix || "") || null; if (!b && a.prefix) throw Error("XPST0081: The prefix " + a.prefix + " could not be resolved."); a.namespaceURI = b; } }
  			function Xu(a, b, c, d) {
  				Wu(a, d); var e = a.prefix, f = a.namespaceURI, g = a.localName; return Ir(c, function (k) {
  					var l = b ? Y(k.code + ".nodeType\n\t\t\t\t\t\t&& (" + k.code + ".nodeType === /*ELEMENT_NODE*/ 1\n\t\t\t\t\t\t|| " + k.code + ".nodeType === /*ATTRIBUTE_NODE*/ 2)", { type: 0 }, []) : Y(k.code + ".nodeType\n\t\t\t\t\t\t&& " + k.code + ".nodeType === /*ELEMENT_NODE*/ 1", { type: 0 }, []); if ("*" === e) return "*" === g ? [l, b ? "type-1-or-type-2" : "type-1"] : [Qr(l, Y(k.code + ".localName === " + xu(g), { type: 0 }, [])), "name-" + g]; l = "*" === g ? l : Qr(l, Y(k.code +
  						".localName === " + xu(g), { type: 0 }, [])); var m = Y(xu(f), { type: 0 }, []); m = "" === e && b ? Z(m, function (q) { return Y(k.code + ".nodeType === /*ELEMENT_NODE*/ 1 ? " + q.code + " : null", { type: 0 }, q.H) }) : m; m = Z(m, function (q) { return Y("(" + k.code + ".namespaceURI || null) === ((" + q.code + ") || null)", { type: 0 }, q.H) }); return [Qr(l, m), "name-" + g]
  				})
  			}
  			function Yu(a, b, c) { var d = (a = L(a, "elementName")) && L(a, "star"); if (null === a || d) return [Z(b, function (e) { return Y(e.code + ".nodeType === /*ELEMENT_NODE*/ 1", { type: 0 }, []) }), "type-1"]; a = Ug(L(a, "QName")); return Xu(a, !1, b, c) } function Zu(a) { return [Z(a, function (b) { return Y("!!" + b.code + ".nodeType", { type: 0 }, []) }), null] }
  			function $u(a, b, c, d) {
  				var e = a[0]; switch (e) {
  					case Tu.Vb: return Yu(a, c, d); case Tu.dc: return Vu(c); case Tu.$b: return Xu(Ug(a), b, c, d); case Tu.ec: return L(a, "star") ? (e = L(a, "uri"), null !== e ? a = Xu({ localName: "*", namespaceURI: Tg(e), prefix: "" }, b, c, d) : (e = L(a, "NCName"), a = "star" === L(a, "*")[0] ? Xu({ localName: Tg(e), namespaceURI: null, prefix: "*" }, b, c, d) : Xu({ localName: "*", namespaceURI: null, prefix: Tg(e) }, b, c, d))) : a = Xu({ localName: "*", namespaceURI: null, prefix: "*" }, b, c, d), a; case Tu.Sb: return Zu(c); default: return [Hr("Test not implemented: '" +
  						e), null]
  				}
  			} function av(a, b, c) { var d = p(c.h(a, b, c)), e = d.next().value; d = d.next().value; return [Lr(e, M(a, "type"), b, c), d] }
  			function bv(a, b, c) {
  				a = a ? O(a, "*") : []; var d = p(a.reduce(function (e, f) { e = p(e); var g = e.next().value, k = e.next().value; if (!g) return av(f, b, c); var l = k; return Ir(g, function (m) { var q = p(av(f, b, c)), u = q.next().value; q = q.next().value; l = Qh(k, q); return [Z(u, function (z) { return Y(m.code + " && " + z.code, { type: 0 }, [].concat(t(m.H), t(z.H))) }), l] }) }, [null, null])); a = d.next().value; d = d.next().value; return [a ? Z(a, function (e) {
  					return Y("(function () {\n\t\t\t\t\t\t\t" + e.H.join("\n") + "\n\t\t\t\t\t\t\treturn " + e.code + ";\n\t\t\t\t\t\t})()",
  						{ type: 0 }, [])
  				}) : null, d]
  			}
  			function cv(a, b, c, d) {
  				if (0 === a.length) return [Z(c, function (q) { return Y("yield " + q.code + ";", { type: 2 }, q.H) }), null]; a = p(a); var e = a.next().value, f = ja(a); if (0 < O(e, "lookup").length) return [Hr("Unsupported: lookups"), null]; var g = vu(d, wu(d, "contextItem")); a = L(e, "predicates"); a = p(bv(a, g, d)); var k = a.next().value, l = a.next().value; if (a = L(e, "xpathAxis")) {
  					e = L(e, Uu); if (!e) return [Hr("Unsupported test in step"), null]; var m = Tg(a); b = "attribute" === m || "self" === m && b; m = p($u(e, b, g, d)); e = m.next().value; m = m.next().value; e = null ===
  						k ? e : Qr(e, k); l = Qh(m, l); b = p(cv(f, b, g, d)); m = b.next().value; b.next(); return Su(a, e, l, m, g, c)
  				} a = N(e, ["filterExpr", "*"]); if (!a) return [Hr("Unsupported: unknown step type"), null]; l = p(d.h(a, c, d)); a = l.next().value; l = l.next().value; return [Z(a, function (q) {
  					var u = 0 === f.length ? Y("", { type: 2 }, []) : Y("if (" + g.code + " !== null && !" + g.code + ".nodeType) {\n\t\t\t\t\t\t\t\t\tthrow new Error('XPTY0019: The result of E1 in a path expression E1/E2 should evaluate to a sequence of nodes.');\n\t\t\t\t\t\t\t\t}", { type: 2 }, []), z =
  						p(cv(f, !0, g, d)), A = z.next().value; z.next(); z = null === k ? A : Z(k, function (D) { return Z(A, function (F) { return Y("if (" + D.code + ") {\n\t\t\t\t\t\t\t\t\t" + F.H.join("\n") + "\n\t\t\t\t\t\t\t\t\t" + F.code + "\n\t\t\t\t\t\t\t\t}", { type: 2 }, D.H) }) }); return Z(z, function (D) {
  							switch (q.va.type) {
  								case 1: return Z(c, function (F) { return Y("for (const " + g.code + " of " + q.code + "(" + F.code + ")) {\n\t\t\t\t\t\t\t\t\t" + D.H.join("\n") + "\n\t\t\t\t\t\t\t\t\t" + D.code + "\n\t\t\t\t\t\t\t\t}", { type: 2 }, [].concat(t(g.H), t(q.H), t(u.H))) }); case 0: return Y("const " +
  									g.code + " = " + q.code + ";\n\t\t\t\t\t\t\t" + u.code + "\n\t\t\t\t\t\t\tif (" + g.code + " !== null) {\n\t\t\t\t\t\t\t\t" + D.H.join("\n") + "\n\t\t\t\t\t\t\t\t" + D.code + "\n\t\t\t\t\t\t\t}", { type: 2 }, [].concat(t(g.H), t(q.H), t(u.H))); default: return Hr("Unsupported generated code type for filterExpr")
  							}
  						})
  				}), l]
  			}
  			function dv(a) { return Z(a, function (b) { return Y("(function () {\n\t\t\t\tlet n = " + b.code + ";\n\t\t\t\twhile (n.nodeType !== /*DOCUMENT_NODE*/9) {\n\t\t\t\t\tn = domFacade.getParentNode(n);\n\t\t\t\t\tif (n === null) {\n\t\t\t\t\t\tthrow new Error('XPDY0050: the root node of the context node is not a document node.');\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t\treturn n;\n\t\t\t})()", { type: 0 }, b.H) }) }
  			function ev(a, b, c) { return Ir(b, function (d) { if (0 < O(a, "lookup").length) return [Hr("Unsupported: lookups"), null]; var e = L(a, "predicates"), f = p(bv(e, d, c)); e = f.next().value; f = f.next().value; var g = L(a, Uu); if (!g) return [Hr("Unsupported test in step"), null]; var k = p($u(g, !0, d, c)); g = k.next().value; k = k.next().value; e = null === e ? g : Qr(g, e); f = Qh(k, f); return [Z(e, function (l) { return Y("((" + l.code + ") ? " + d.code + " : null)", { type: 0 }, [].concat(t(d.H), t(l.H))) }), f] }) }
  			function fv(a, b, c) { var d = O(a, "stepExpr"); if (1 === d.length) { var e = L(d[0], "xpathAxis"); if (e && "self" === Tg(e)) return ev(d[0], b, c) } var f = vu(c, wu(c, "contextItem")); b = (a = L(a, "rootExpr")) ? Kr(c, dv(f), "root") : f; d = p(cv(d, !a, b, c)); c = d.next().value; d = d.next().value; return [Z(c, function (g) { return Y("(function* (" + f.code + ") {\n\t\t\t" + g.H.join("\n") + "\n\t\t\t" + g.code + "\n\t\t})", { type: 1 }, []) }), d] } function gv(a, b, c) {
  				var d = a[0]; switch (d) {
  					case "contextItemExpr": return [b, null]; case "pathExpr": return fv(a, b, c); case "andOp": return Mu(a, "&&", b, c); case "orOp": return Mu(a, "||", b, c); case "stringConstantExpr": return a = L(a, "value")[1] || "", a = xu(a), [Y(a, { type: 0 }, []), null]; case "equalOp": case "notEqualOp": case "lessThanOrEqualOp": case "lessThanOp": case "greaterThanOrEqualOp": case "greaterThanOp": case "eqOp": case "neOp": case "ltOp": case "leOp": case "gtOp": case "geOp": case "isOp": case "nodeBeforeOp": case "nodeAfterOp": a: switch (d) {
  						case "eqOp": case "neOp": case "ltOp": case "leOp": case "gtOp": case "geOp": case "isOp": a =
  							Wr(a, d, b, c); break a; case "equalOp": case "notEqualOp": case "lessThanOrEqualOp": case "lessThanOp": case "greaterThanOrEqualOp": case "greaterThanOp": var e = M(N(a, ["firstOperand", "*"]), "type"), f = M(N(a, ["secondOperand", "*"]), "type"); a = e && f ? 3 === e.g && 3 === f.g ? Wr(a, Tr[d], b, c) : 3 === e.g ? Xr(a, "firstOperand", "secondOperand", Tr[d], b, c) : 3 === f.g ? Xr(a, "secondOperand", "firstOperand", Vr[Tr[d]], b, c) : Hr("General comparison for sequences is not implemented") : Hr("types of compare are not known"); break a; default: a = Hr("Unsupported compare type: " +
  								d);
  					}return [a, null]; case "functionCallExpr": return [Ju(a, b, c), null]; default: return [Hr("Unsupported: the base expression '" + d + "'."), null]
  				}
  			} function hv(a, b) { this.o = new Map; this.v = new Map; this.aa = a; this.B = b; this.h = gv; } function Kr(a, b, c) { return Z(b, function (d) { var e = a.o.get(d); e || (e = wu(a, c), e = Y(e, d.va, [].concat(t(d.H), ["const " + e + " = " + d.code + ";"])), a.o.set(d, e), a.o.set(e, e)); return e }) } function vu(a, b) { b = Y(b, { type: 0 }, []); a.o.set(b, b); return b } function wu(a, b) { b = void 0 === b ? "v" : b; var c = a.v.get(b) || 0; a.v.set(b, c + 1); return b + c } function iv(a) { var b = O(a, "*"); if ("pathExpr" === a[0]) return !0; a = p(b); for (b = a.next(); !b.done; b = a.next())if (iv(b.value)) return !0; return !1 } function jv(a, b, c) {
  				c = c || {}; b = b || 0; if ("string" === typeof a) { a = hl(a); var d = { $: "XQuery3.1" === c.language || "XQueryUpdate3.1" === c.language, debug: !1 }; try { var e = Jq(a, d); } catch (k) { pg(a, k); } } else e = Rk(a); a = L(e, "mainModule"); if (!a) return Hr("Unsupported: XQuery Library modules are not supported."); if (L(a, "prolog")) return Hr("Unsupported: XQuery Prologs are not supported."); d = void 0 === c.defaultFunctionNamespaceURI ? "http://www.w3.org/2005/xpath-functions" : c.defaultFunctionNamespaceURI; a = new hv(c.namespaceResolver ||
  					er(null), d); c = new Jh(new Lg(new zg(a.aa, {}, d, c.functionNameResolver || fr("http://www.w3.org/2005/xpath-functions")))); oh(e, c); if (c = L(e, "mainModule")) if (L(c, "prolog")) a = Hr("Unsupported: XQuery."); else {
  						var f = N(c, ["queryBody", "*"]); c = vu(a, "contextItem"); var g = p(a.h(f, c, a)); d = g.next().value; g.next(); b: switch (f = M(f, "type"), b) { case 9: b = Jr(d, c, a); a = Or(b, f, a); break b; case 7: a = Pr(d, f, c, a); break b; case 3: a = Lr(d, f, c, a); break b; case 2: a = Nr(d, f, c, a); break b; default: a = Hr("Unsupported: the return type '" + b + "'."); }a.isAstAccepted &&
  							(a = "\n\t\t" + a.H.join("\n") + "\n\t\treturn " + a.code + ";", b = "\n\treturn (contextItem, domFacade, runtimeLib, options) => {\n\t\tconst {\n\t\t\terrXPDY0002,\n\t\t} = runtimeLib;", iv(e) && (b += '\n\t\tif (!contextItem) {\n\t\t\tthrow errXPDY0002("Context is needed to evaluate the given path expression.");\n\t\t}\n\n\t\tif (!contextItem.nodeType) {\n\t\t\tthrow new Error("Context item must be subtype of node().");\n\t\t}\n\t\t'), a = { code: b + (a + "}\n//# sourceURL=generated.js"), isAstAccepted: !0 });
  					} else a = Hr("Unsupported: Can not execute a library module.");
  				return a
  			} function kv(a, b, c) { var d = a.stack; d && (d.includes(a.message) && (d = d.substr(d.indexOf(a.message) + a.message.length).trim()), d = d.split("\n"), d.splice(10), d = d.map(function (e) { return e.startsWith("    ") || e.startsWith("\t") ? e : "    " + e }), d = d.join("\n")); a = Error.call(this, "Custom XPath function Q{" + c + "}" + b + " raised:\n" + a.message + "\n" + d); this.message = a.message; "stack" in a && (this.stack = a.stack); } v(kv, Error);
  			function lv(a, b, c) { return 0 === b.g ? a.G() ? null : ir(a.first(), c).next(0).value : 2 === b.g || 1 === b.g ? a.O().map(function (d) { if (B(d.type, 47)) throw Error("Cannot pass attribute nodes to custom functions"); return ir(d, c).next(0).value }) : ir(a.first(), c).next(0).value }
  			function mv(a) { if ("object" === typeof a) return a; a = a.split(":"); if (2 !== a.length) throw Error("Do not register custom functions in the default function namespace"); var b = p(a); a = b.next().value; b = b.next().value; var c = yg[a]; if (!c) { c = "generated_namespace_uri_" + a; if (yg[a]) throw Error("Prefix already registered: Do not register the same prefix twice."); yg[a] = c; } return { localName: b, namespaceURI: c } }
  			function nv(a, b, c, d) { a = mv(a); var e = a.namespaceURI, f = a.localName; if (!e) throw Eg(); var g = b.map(function (l) { return qb(l) }), k = qb(c); wg(e, f, g, k, function (l, m, q) { var u = Array.from(arguments); u.splice(0, 3); u = u.map(function (D, F) { return lv(D, g[F], m) }); var z = {}; z = (z.currentContext = m.l, z.domFacade = m.h.h, z); try { var A = d.apply(void 0, [z].concat(t(u))); } catch (D) { throw new kv(D, f, e); } return A && "object" === typeof A && Object.getOwnPropertySymbols(A).includes(cr) ? C.create(A.Hb) : Ec(m.h, A, k) }); } var ov = { callFunction: function (a, b, c, d, e) { var f = vg(b, c, d.length); if (!f) throw Error("function not found for codegen function call"); b = new Hc({ N: null, Ka: 0, Da: C.empty(), Aa: {} }); var g = new Mb(a); a = new Oc(!1, !1, g, null, null, e ? e.currentContext : null, null); d = f.callFunction.apply(f, [b, a, null].concat(t(d.map(function (k, l) { return Ec(g, k, f.j[l]) })))); return lv(d, { type: 59, g: 0 }, a) }, errXPDY0002: Rc }; function pv(a, b, c, d) { c = c ? c : new Gb; return a()(null !== b && void 0 !== b ? b : null, c, ov, d) } var qv = {}, rv = (qv["http://www.w3.org/2005/XQueryX"] = "xqx", qv["http://www.w3.org/2007/xquery-update-10"] = "xquf", qv["http://fontoxml.com/fontoxpath"] = "x", qv);
  			function sv(a, b) {
  				switch (a) {
  					case "copySource": case "insertAfter": case "insertAsFirst": case "insertAsLast": case "insertBefore": case "insertInto": case "modifyExpr": case "newNameExpr": case "replacementExpr": case "replaceValue": case "returnExpr": case "sourceExpr": case "targetExpr": case "transformCopies": case "transformCopy": return { localName: a, ub: b || "http://www.w3.org/2005/XQueryX" }; case "deleteExpr": case "insertExpr": case "renameExpr": case "replaceExpr": case "transformExpr": return { localName: a, ub: "http://www.w3.org/2007/xquery-update-10" };
  					case "x:stackTrace": return { localName: "stackTrace", ub: "http://fontoxml.com/fontoxpath" }; default: return { localName: a, ub: "http://www.w3.org/2005/XQueryX" }
  				}
  			}
  			function tv(a, b, c, d, e) {
  				if ("string" === typeof c) return 0 === c.length ? null : b.createTextNode(c); if (!Array.isArray(c)) throw new TypeError("JsonML element should be an array or string"); d = sv(c[0], d); var f = d.localName; d = d.ub; var g = b.createElementNS(d, rv[d] + ":" + f), k = c[1], l = 1; if ("object" === typeof k && !Array.isArray(k)) {
  					if (null !== k) {
  						l = p(Object.keys(k)); for (var m = l.next(); !m.done; m = l.next()) {
  							m = m.value; var q = k[m]; null !== q && ("type" === m ? void 0 !== q && a.setAttributeNS(g, d, "fontoxpath:" + m, ob(q)) : ("start" !== m && "end" !==
  								m || "stackTrace" !== f || (q = JSON.stringify(q)), e && "prefix" === m && "" === q || a.setAttributeNS(g, d, rv[d] + ":" + m, q)));
  						}
  					} l = 2;
  				} f = l; for (k = c.length; f < k; ++f)l = tv(a, b, c[f], d, e), null !== l && a.insertBefore(g, l, null); return g
  			}
  			function uv(a, b, c, d) {
  				d = void 0 === d ? Ib : d; a = hl(a); try { var e = Jq(a, { $: "XQuery3.1" === b.language || "XQueryUpdate3.1" === b.language, debug: b.debug }); } catch (m) { pg(a, m); } var f = new zg(b.namespaceResolver || function () { return null }, {}, void 0 === b.defaultFunctionNamespaceURI ? "http://www.w3.org/2005/xpath-functions" : b.defaultFunctionNamespaceURI, b.functionNameResolver || function () { return null }); f = new Lg(f); var g = L(e, ["mainModule", "libraryModule"]), k = L(g, "moduleDecl"); if (k) {
  					var l = Tg(L(k, "prefix")); k = Tg(L(k, "uri")); Pg(f,
  						l, k);
  				} (g = L(g, "prolog")) && Mq(g, f, !1, a); !1 !== b.annotateAst && Bh(e, new Jh(f)); f = new Gb; b = tv(d, c, e, null, !1 === b.wc); d.insertBefore(b, c.createComment(a), f.getFirstChild(b)); return b
  			} function vv(a) { return Promise.resolve(a) } function wv(a, b) {
  				b = void 0 === b ? { debug: !1 } : b; b = Jq(a, { $: !0, debug: b.debug }); Bh(b, new Jh(void 0)); b = L(b, "libraryModule"); if (!b) throw Error("XQuery module must be declared in a library module."); var c = L(b, "moduleDecl"), d = L(c, "uri"), e = Tg(d); c = L(c, "prefix"); d = Tg(c); c = new Lg(new zg(function () { return null }, Object.create(null), "http://www.w3.org/2005/xpath-functions", fr("http://www.w3.org/2005/xpath-functions"))); Pg(c, d, e); b = L(b, "prolog"); if (null !== b) {
  					try { var f = Mq(b, c, !0, a); } catch (g) { pg(a, g); } f.Ma.forEach(function (g) {
  						if (e !==
  							g.namespaceURI) throw Error("XQST0048: Functions and variables declared in a module must reside in the module target namespace.");
  					}); Tk(e, f);
  				} else Tk(e, { Ma: [], Va: [], ra: null, source: a }); return e
  			} var xv = new Map; function yv(a) { var b; a: { if (b = Nk.get(a)) for (var c = p(Object.keys(b)), d = c.next(); !d.done; d = c.next())if (d = d.value, b[d] && b[d].length) { b = b[d][0].h; break a } b = null; } if (b) return b; if (xv.has(a)) return xv.get(a); b = "string" === typeof a ? Jq(a, { $: !1 }) : Rk(a); b = N(b, ["mainModule", "queryBody", "*"]); if (null === b) throw Error("Library modules do not have a specificity"); b = Mk(b, { ua: !1, $: !1 }); xv.set(a, b); return b } function zv(a) { return yv(a).B() } function Av(a, b) { return If(yv(a).o, yv(b).o) } var Bv = new Gb;
  			"undefined" !== typeof fontoxpathGlobal && (fontoxpathGlobal.compareSpecificity = Av, fontoxpathGlobal.compileXPathToJavaScript = jv, fontoxpathGlobal.domFacade = Bv, fontoxpathGlobal.evaluateXPath = rr, fontoxpathGlobal.evaluateXPathToArray = xr, fontoxpathGlobal.evaluateXPathToAsyncIterator = tr, fontoxpathGlobal.evaluateXPathToBoolean = yr, fontoxpathGlobal.evaluateXPathToFirstNode = zr, fontoxpathGlobal.evaluateXPathToMap = Ar, fontoxpathGlobal.evaluateXPathToNodes = Br, fontoxpathGlobal.evaluateXPathToNumber = Cr, fontoxpathGlobal.evaluateXPathToNumbers =
  				Dr, fontoxpathGlobal.evaluateXPathToString = Er, fontoxpathGlobal.evaluateXPathToStrings = Fr, fontoxpathGlobal.evaluateUpdatingExpression = vr, fontoxpathGlobal.evaluateUpdatingExpressionSync = wr, fontoxpathGlobal.executeJavaScriptCompiledXPath = pv, fontoxpathGlobal.executePendingUpdateList = Gr, fontoxpathGlobal.getBucketForSelector = zv, fontoxpathGlobal.getBucketsForNode = Fb, fontoxpathGlobal.precompileXPath = vv, fontoxpathGlobal.registerXQueryModule = wv, fontoxpathGlobal.registerCustomXPathFunction = nv, fontoxpathGlobal.parseScript =
  				uv, fontoxpathGlobal.profiler = or, fontoxpathGlobal.createTypedValueFactory = dr, fontoxpathGlobal.finalizeModuleRegistration = Vk, fontoxpathGlobal.Language = qr, fontoxpathGlobal.ReturnType = kr);
  			return fontoxpathGlobal;
  		})(xspattern, prsc);
  	});

  }

  const dependencies = {
    "hide-if-matches-xpath3": hideIfMatchesXPath3Dependency
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