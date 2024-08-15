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

/* global browser, checkElement:readonly */

import $ from "../$.js";
import {apply, bind} from "proxy-pants/function";
import {debug} from "../introspection/debug.js";

import {libEnvironment} from "../environment.js";

let {
  console,
  document,
  getComputedStyle,
  isExtensionContext,
  variables,
  Array,
  MutationObserver,
  Object,
  XPathEvaluator,
  XPathExpression,
  XPathResult
} = $(window);

// Ensures that $$ is bound only in environments where document exists.
const {querySelectorAll} = document;
const document$$ = querySelectorAll && bind(querySelectorAll, document);

/**
 * Gets the open or closed shadow root hosted by the specified element.
 *
 * @param {Element} element - The HTML element for which the shadow root
 *  is to be retrieved.
 * @param {boolean} [failSilently=false] - If true, we don't log errors
 *  because it would clutter the console and add no value for the FLAs.
 * @returns {?ShadowRoot} Returns the shadow root of the element,
 *  or null if access fails.
 */
function $openOrClosedShadowRoot(element, failSilently = false) {
  try {
    const shadowRoot = (navigator.userAgent.includes("Firefox")) ?
      element.openOrClosedShadowRoot :
      browser.dom.openOrClosedShadowRoot(element);
    if (shadowRoot === null && ((debug() && !failSilently)))
      console.log("Shadow root not found or not added in element yet", element);
    return shadowRoot;
  }
  catch (error) {
    if (debug() && !failSilently)
      console.log("Error while accessing shadow root", element, error);
    return null;
  }
}

/**
 * Entry point for querying elements with support
 * for shadow roots and SVG use patterns.
 *
 * @param {string} selector - The selector to be parsed.
 * @param {boolean} [returnRoots=false] - If true, the function
 *  returns an array of objects containing selected elements
 *  and their corresponding shadow root parents.
 * @returns
 *  {Array<Element>|Array<{element: Element, rootParents: Element}>|null} -
 *  An array of selected elements or an array of objects containing selected
 *  elements and their parents. Returns null if no elements are found.
 */

export function $$(selector, returnRoots = false) {
  /** This starts the recursion to handle (possible nested) shadow roots
   *  or svg-use patterns.
   */
  return $$recursion(
    selector,
    document$$.bind(document),
    document,
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
      console.log("No elements found matching", xlinkHref);
      return false;
    }

    // If we don't need to use additional queries on this, this is the
    // found element
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
  // Not a shadow root
  return false;
}

/**
  This function serves as a wrapper around document$$ (querySelectorAll).
  It takes a selector and parses it.
   - When encountering the ^^sh^^ special demarcator, it splits the query there.
  It executes the 1st part of the query. If the result is null, it returns null;
  otherwise it accesses the closed shadow root with
  browser.dom.openOrClosedShadowRoot to the second part.
  The function then recursively invokes itself, to access nested shadow roots.
   - When encountering the ^^svg^^ special demarcator it splits the query there.
  It extracts the value of the xlink:href or href attribute from the element
  it found and uses the attribute value to find the corresponding element.
  The function then recursively invokes itself to handle potential nested
  shadow roots until no more demarcators are found in the selector.
   - When no special commands are found in the selector, it invokes document$$.

  @param {string} selector - the selector to be parsed.
  @param {function} bound$$ - the function to be invoked: the first call will be
    invoked with document.querySelectorAll, the recursive calls will be invoked
    with the shadowRoot.querySelectorAll.
  @param {Element} boundElement - the element that bound$$ is bound to
  @param {boolean} returnRoots - If true, the function returns an array of
    objects containing selected elements and their corresponding shadow root
    parents. This is important when we want to hide a shadow root ancenstor,
    since we wouldn't be able to cross the shadow root boundary without keeping
    track of the parents that host the shadow root.
  @param {Array<Element>} [rootParents] - All root parent elements
    that surround this current root.
  @returns {Array<Element>|Array<{element: Element, rootParents: Element}>|null}
    An array of selected elements or an array of objects containing selected
    elements and their parents.
    Returns null if no elements are found.
 */
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

    // We need to decide which command to execute and fail if it's unknown
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
          console.log(
            currentCommand,
            " is not supported. Supported commands are: \n^^sh^^\n^^svg^^"
          );
        }
        return [];
      }
    }

    // If we don't have a selector, we're supposed to take the current element
    // as new root parent - and not one of it's descendants.
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

/**
 * This function serves as a wrapper around `element.closest()` to handle
 * CSS selectors with the special `^^sh^^` or `^^svg^^` demarcator
 * for targeting the content of shadow roots.
 *
 * @param {Element} element - The element on which `closest` is called.
 * @param {string} selector - String containing the CSS selector, with optional
 *   `^^sh^^` or `^^svg^^` demarcator for targeting the content of shadow roots.
 * @param {Element[]} [shadowRootParents=[]] - Array of shadow roots to use
 *   when encountering the `^^sh^^` demarcator. Each shadow root is used as a
 *   context for searching ancestors.
 * @returns {Element | null} - The closest ancestor of the element that matches
 *   the selector, or null if no match is found.
 */
export function $closest(element, selector, shadowRootParents = []) {
  if (selector.includes("^^svg^^"))
    selector = selector.split("^^svg^^")[0];

  if (selector.includes("^^sh^^")) {
    /* The number of ^^sh^^ informs us about the number
    of shadow roots to traverse */
    const splitSelector = selector.split("^^sh^^");
    const numShadowRootsToCross = splitSelector.length - 1;
    selector = `:host ${splitSelector[numShadowRootsToCross]}`;
    /* We check the number of shadow roots specified in the selector and compare
    it with the number of shadow root parents in the shadowRootParents array. */
    if (numShadowRootsToCross === shadowRootParents.length) {
      /* If true it means the selector wants to find
      an element within the current shadow root */
      return element.closest(selector);
    }
    /* If false it picks the corresponding shadow root parent
    from the shadowRootParents array */
    const shadowRootParent = shadowRootParents[numShadowRootsToCross];
    return shadowRootParent.closest(selector);
  }
  if (shadowRootParents[0])
    return shadowRootParents[0].closest(selector);
  return element.closest(selector);
}

/**
 * This function serves as a wrapper around `element.childNodes` and
 * allow us to access the child nodes within shadow root boundaries.
 *
 * @param {Element} element The element for which child nodes are to be
 *   retrieved.
 * @param {boolean} [failSilently=true] - If true, we don't log errors
 *   because it would clutter the console and add no value for the FLAs:
 *   since this is an internal function, the logs coming from here wouldn't
 *   help filter devs in filter writing.
 * @returns {NodeList} NodeList of child nodes of the given element.
 */
export function $childNodes(element, failSilently = true) {
  const shadowRoot = $openOrClosedShadowRoot(element, failSilently);
  if (shadowRoot)
    return shadowRoot.childNodes;

  return $(element).childNodes;
}

// make `new XPathExpression()` operations safe
const {assign, setPrototypeOf} = Object;

class $XPathExpression extends XPathExpression {
  evaluate(...args) {
    return setPrototypeOf(
      apply(super.evaluate, this, args),
      XPathResult.prototype
    );
  }
}

class $XPathEvaluator extends XPathEvaluator {
  createExpression(...args) {
    return setPrototypeOf(
      apply(super.createExpression, this, args),
      $XPathExpression.prototype
    );
  }
}

/**
 * Hides an HTML element by setting its `style` attribute to
 * `display: none !important`.
 *
 * @param {HTMLElement} element The HTML element to hide.
 * @private
 * @returns {bool} true if element has been newly hidden,
 * false if the element was already hidden and no action was taken.
 */
export function hideElement(element) {
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

  // Listen for changes to the style property and if our values are unset
  // then reset them.
  new MutationObserver(() => {
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

/**
 * Notifies the current contentScript that a new element has been hidden.
 * This is done by calling the globally available `checkElement` function
 * and passing the element.
 *
 * @param {HTMLElement} element The HTML element that was hidden.
 * @private
 */
function notifyElementHidden(element) {
  if (isExtensionContext && typeof checkElement === "function")
    checkElement(element);
}

/**
 * A callback function to be applied to a node.
 * @callback queryAndApplyCallback
 * @param {Node} node
 * @private
 */

/**
 * The query function. Accepts a callback function
 * which will be called for every node resulted from querying the document.
 * @callback queryAndApply
 * @param {queryAndApplyCallback} cb
 * @private
 */

/**
 * Given a CSS or Xpath selector, returns a query function.
 * @param {string} selector A CSS selector or a Xpath selector which must be
 * described with the following syntax: `xpath(the_actual_selector)`
 * @returns {queryAndApply} The query function. Accepts a callback function
 * which will be called for every node resulted from querying the document.
 * @private
 */
export function initQueryAndApply(selector) {
  let $selector = selector;
  if ($selector.startsWith("xpath(") &&
      $selector.endsWith(")")) {
    let xpathQuery = $selector.slice(6, -1);
    let evaluator = new $XPathEvaluator();
    let expression = evaluator.createExpression(xpathQuery, null);
    // do not use ORDERED_NODE_ITERATOR_TYPE or the test env will fail
    let flag = XPathResult.ORDERED_NODE_SNAPSHOT_TYPE;

    return cb => {
      if (!cb)
        return;
      let result = expression.evaluate(document, flag, null);
      let {snapshotLength} = result;
      for (let i = 0; i < snapshotLength; i++)
        cb(result.snapshotItem(i));
    };
  }
  return cb => $$(selector).forEach(cb);
}

/**
 * The query function. Retrieves all the nodes in the DOM matching the
 * provided selector.
 * @callback queryAll
 * @returns {Node[]} An array containing all the nodes in the DOM matching
 * the provided selector.
 * @private
 */

/**
 * Given a CSS or Xpath selector, returns a query function.
 * @param {string} selector A CSS selector or a Xpath selector which must be
 * described with the following syntax: `xpath(the_actual_selector)`
 * @returns {queryAll} The query function. Retrieves all the nodes in the DOM
 * matching the provided selector.
 * @private
 */
export function initQueryAll(selector) {
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

/**
 * Hides any HTML element or one of its ancestors matching a CSS selector if
 * it matches the provided condition.
 *
 * @param {function} match The function that provides the matching condition.
 * @param {string} selector The CSS selector that an HTML element must match
 *   for it to be hidden.
 * @param {?string} [searchSelector] The CSS selector that an HTML element
 *   containing the given string must match. Defaults to the value of the
 *   `selector` argument.
 * @param {?string} [onHideCallback] The callback that will be invoked after
 *    a HTML element was matched and hidden. It gets called with this element as
 *    argument.
 * @returns {MutationObserver} Augmented MutationObserver object. It has a new
 *   function mo.race added to it. This can be used by the snippets to
 *   disconnect the MutationObserver with the racing mechanism.
 *   Used like: mo.race(raceWinner(() => {mo.disconnect();}));
 * @private
 */
export function hideIfMatches(match, selector, searchSelector, onHideCallback) {
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
    new MutationObserver(callback),
    {
      race(win) {
        won = win;
        this.observe(document, {childList: true,
                                characterData: true,
                                subtree: true});
        callback();
      }
    }
  );
}

/**
 * Check if an element is visible
 *
 * @param {Element} element The element to check visibility of.
 * @param {CSSStyleDeclaration} style The computed style of element.
 * @param {?Element} closest The closest parent to reach.
 * @param {?Array.<Element>} shadowRootParents Optional list of
 *    shadow root parents.
 * @return {bool} Whether the element is visible.
 * @private
 */
export function isVisible(element, style, closest, shadowRootParents) {
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
    // If there is no parent node, try to get it from next shadow root parent
    if (shadowRootParents && shadowRootParents.length) {
      parent = shadowRootParents[shadowRootParents.length - 1];
      shadowRootParents = shadowRootParents.slice(0, -1);
    }
    else {
      return true;
    }
  }

  return isVisible(
    parent, getComputedStyle(parent), closest, shadowRootParents
  );
}

/**
 * Returns the value of the `cssText` property of the object returned by
 * `getComputedStyle` for the given element.
 *
 * If the value of the `cssText` property is blank, this function computes the
 * value out of the properties available in the object.
 *
 * @param {Element} element The element for which to get the computed CSS text.
 *
 * @returns {string} The computed CSS text.
 * @private
 */
export function getComputedCSSText(element) {
  let style = getComputedStyle(element);
  let {cssText} = style;

  if (cssText)
    return cssText;

  for (let property of style)
    cssText += `${property}: ${style[property]}; `;

  return $(cssText).trim();
}
