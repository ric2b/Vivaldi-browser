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
import {apply, call} from "proxy-pants/function";

import {log} from "../introspection/log.js";
import {debug} from "../introspection/debug.js";
import {hideElement, initQueryAll} from "../utils/dom.js";
import {wrapPropertyAccess} from "../utils/execution.js";
import {randomId, toRegExp} from "../utils/general.js";

// These classes cannot be secured in a meaningful way because new nodes
// would not use the secured prototype and the secured environment is obtained
// though the chained counterpart of these classes. Because of this, it's OK to
// just trap any value or prototype on the top of this file or in any other
// file that needs global DOM classes constants or prototype. The `$(element)`
// operation uses indeed chained version of these classes so that it's
// transparent for the node and no prototype upgrade ever leaks in the wild.
let {ELEMENT_NODE, TEXT_NODE, prototype: NodeProto} = Node;
let {prototype: ElementProto} = Element;
let {prototype: HTMLElementProto} = HTMLElement;

let {
  console,
  variables,
  DOMParser,
  Error,
  MutationObserver,
  Object,
  ReferenceError
} = $(window);

let {getOwnPropertyDescriptor} = Object;

/**
 * Freezes a DOM element so it prevents adding new nodes inside it.
 * @alias module:content/snippets.freeze-element
 *
 * @param {string} selector The CSS selector for the parent element that
 *   we want to freeze
 * @param {string?} [options] A single parameter for snippet's options.
 *   A string containing all the options we want to pass, each of them
 *   separated by a plus character (`+`). Empty single quotes if none (`''`).
 *   Available options:
 *   **subtree** (if we want to freeze all the element's children as well);
 *   **abort** (throw an error every time an child element gets added);
 * @param {string?} [exceptions] An array of regex/selectors used to specify
 *   the nodes we don't want to prevent being added.
 *   Each array item can be:
 *   **selector** (targeting Element nodes);
 *   **regex** (targeting Text nodes, identified by slash);
 *
 * @since Adblock Plus 3.9.5
 */
export function freezeElement(selector, options = "", ...exceptions) {
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
  observer = new MutationObserver(searchAndAttach);
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
          throw new Error("[freeze] Unknown option passed to the snippet." +
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
      ElementProto, "append", isFrozen, getSnippetData
    );
    wrapPropertyAccess(ElementProto, "append", descriptor);

    descriptor = getAppendDescriptor(
      ElementProto, "prepend", isFrozen, getSnippetData
    );
    wrapPropertyAccess(ElementProto, "prepend", descriptor);

    descriptor = getAppendDescriptor(
      ElementProto,
      "replaceWith",
      isFrozenOrHasFrozenParent,
      getSnippetDataFromNodeOrParent
    );
    wrapPropertyAccess(ElementProto, "replaceWith", descriptor);

    descriptor = getAppendDescriptor(
      ElementProto,
      "after",
      isFrozenOrHasFrozenParent,
      getSnippetDataFromNodeOrParent
    );
    wrapPropertyAccess(ElementProto, "after", descriptor);

    descriptor = getAppendDescriptor(
      ElementProto,
      "before",
      isFrozenOrHasFrozenParent,
      getSnippetDataFromNodeOrParent
    );
    wrapPropertyAccess(ElementProto, "before", descriptor);

    descriptor = getInsertAdjacentDescriptor(
      ElementProto,
      "insertAdjacentElement",
      isFrozenAndInsideTarget,
      getSnippetDataBasedOnTarget
    );
    wrapPropertyAccess(ElementProto, "insertAdjacentElement", descriptor);

    descriptor = getInsertAdjacentDescriptor(
      ElementProto,
      "insertAdjacentHTML",
      isFrozenAndInsideTarget,
      getSnippetDataBasedOnTarget
    );
    wrapPropertyAccess(ElementProto, "insertAdjacentHTML", descriptor);

    descriptor = getInsertAdjacentDescriptor(
      ElementProto,
      "insertAdjacentText",
      isFrozenAndInsideTarget,
      getSnippetDataBasedOnTarget
    );
    wrapPropertyAccess(ElementProto, "insertAdjacentText", descriptor);

    descriptor = getInnerHTMLDescriptor(
      ElementProto, "innerHTML", isFrozen, getSnippetData
    );
    wrapPropertyAccess(ElementProto, "innerHTML", descriptor);

    descriptor = getInnerHTMLDescriptor(
      ElementProto,
      "outerHTML",
      isFrozenOrHasFrozenParent,
      getSnippetDataFromNodeOrParent
    );
    wrapPropertyAccess(ElementProto, "outerHTML", descriptor);

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
          new MutationObserver(mutationsList => {
            for (let mutation of $(mutationsList))
              markNodes($(mutation, "MutationRecord").addedNodes);
          }).observe(node, {childList: true, subtree: true});
        }
        if (subtree && $(node).nodeType === ELEMENT_NODE)
          markNodes($(node).childNodes);
      }
    }
  }

  // utilities
  function logPrefixed(id, ...args) {
    log(`[freeze][${id}] `, ...args);
  }

  function logChange(nodeOrDOMString, target, property, snippetData) {
    let targetSelector = snippetData.selector;
    let chgId = snippetData.changeId;
    let isDOMString = typeof nodeOrDOMString == "string";
    let action = snippetData.shouldAbort ? "aborting" : "watching";
    console.groupCollapsed(`[freeze][${chgId}] ${action}: ${targetSelector}`);
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
      default:
        break;
    }
    logPrefixed(chgId, `using the function "${property}"`);
    console.groupEnd();
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

  // check inserted content
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

  // descriptors
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
          return apply(origin, this, args);
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
            return apply(origin, this, nodesOrDOMStrings);

          let snippetData = getSnippetData(this);
          if (!snippetData)
            return apply(origin, this, nodesOrDOMStrings);

          let accepted = checkMultiple(
            nodesOrDOMStrings, this, property, snippetData
          );
          if (accepted.length > 0)
            return apply(origin, this, accepted);
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

                default:
                  break;
              }
            }
          }
          return apply(origin, this, args);
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
