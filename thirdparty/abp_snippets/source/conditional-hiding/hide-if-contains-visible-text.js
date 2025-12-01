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

import {$childNodes, hideIfMatches, isVisible} from "../utils/dom.js";
import {formatArguments, toRegExp} from "../utils/general.js";
import {log} from "../introspection/log.js";
import {debug} from "../introspection/debug.js";
import {profile} from "../introspection/profile.js";
import {raceWinner} from "../introspection/race.js";

let {getComputedStyle, Map, WeakSet, parseFloat, DOMMatrix, Math} = $(window);

const {ELEMENT_NODE, TEXT_NODE} = Node;

/**
 * Hides any HTML element matching a CSS selector if the visible text content
 * of the element contains a given string.
 * @alias module:content/snippets.hide-if-contains-visible-text
 *
 * @param {string} search The string to match to the visible text. Is considered
 *   visible text that isn't hidden by CSS properties or other means.
 *   If the string begins and ends with a slash (`/`), the text in between is
 *   treated as a regular expression.
 * @param {string} selector The CSS selector that an HTML element must match
 *   for it to be hidden.
 * @param {?string} [searchSelector] The CSS selector that an HTML element
 *   containing the given string must match. Defaults to the value of the
 *   `selector` argument.
 * @param {?Array.<string>} [attributes] The CSS attributes a computed style
 *   map of a node should have in order to consider that node hidden.
 *   Syntax: <key>:<value>, where <value> can be a string or a regex (if it
 *   starts and ends with a `/`).
 *
 * @since Adblock Plus 3.11.4
 */
export function hideIfContainsVisibleText(search, selector,
                                          searchSelector = null,
                                          ...attributes) {
  const {mark, end} = profile("hide-if-contains-visible-text");
  const formattedArguments = formatArguments(arguments);
  let entries = $([]);
  const optionalParameters = new Map([
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
    // if color is transparent...
    ["color", "rgba(0, 0, 0, 0)"]
  ]);

  let attributesMap = new Map(defaultEntries.concat(entries));

  /**
   * Determines if the text inside the element is visible.
   *
   * @param {Element} element The element we are checking.
   * @param {?CSSStyleDeclaration} style The computed style of element. If
   *   falsey it will be queried.
   * @returns {bool} Whether the text is visible.
   * @private
   */
  function isTextVisible(element,
                         style,
                         {bgColorCheck = true, pseudoElemCheck = false} = {}) {
    if (!style)
      style = getComputedStyle(element);
    style = $(style);
    for (const [key, value] of attributesMap) {
      let valueAsRegex = toRegExp(value);
      if (valueAsRegex.test(style.getPropertyValue(key)))
        return false;
    }
    const color = style.getPropertyValue("color");
    if (bgColorCheck && style.getPropertyValue("background-color") === color)
      return false;
    // Handle edge cases with ::first-line pseudo-element
    if (!pseudoElemCheck) {
      const firstLineStyle = getComputedStyle(element, "::first-line");
      if (firstLineStyle) {
        return isTextVisible(element,
                             firstLineStyle,
                             {bgColorCheck, pseudoElemCheck: true});
      }
    }
    // Only consider text-shadow if it significantly affects visibility
    const textShadow = style.getPropertyValue("text-shadow");
    if (color.includes("rgba(0, 0, 0, 0)") &&
        (textShadow === "none" ||
        textShadow.includes("rgba(0, 0, 0, 0)"))
    )
      return false;
    return true;
  }

  function getTransformMatrix(element, pseudo = null) {
    const style = getComputedStyle(element, pseudo);
    let transform = style.transform;
    // If transform is none, we need to set it to the identity matrix
    if (transform === "none")
      transform = "matrix(1, 0, 0, 1, 0, 0)";
    return new DOMMatrix(transform);
  }

  /**
   * Check if a pseudo element has visible text via `content`.
   *
   * @param {Element} element The element to check visibility of.
   * @param {string} pseudo The `::before` or `::after` pseudo selector.
   * @param {DOMMatrix} parentMatrix The combined DOMMatrix of the
   * parent elements.
   * @return {string} The pseudo content or an empty string.
   * @private
   */
  function getPseudoContent(element, pseudo, parentMatrix,
                            {bgColorCheck = true, translateThresh = 2} = {}) {
    let style = getComputedStyle(element, pseudo);

    if (!isVisible(element, style) ||
     !isTextVisible(element, style, {bgColorCheck}))
      return "";

    let {content} = $(style);
    if (content && content !== "none") {
      let strings = $([]);

      const domMatrix = getTransformMatrix(element, pseudo);
      const resultMatrix = parentMatrix.multiply(domMatrix);

      const angle = Math.atan2(resultMatrix.b, resultMatrix.a);
      const angleDegrees = angle * (180 / Math.PI);
      const rotated = Math.abs(angleDegrees) > 5;

      if (rotated)
        return "";

      const translated = Math.abs(resultMatrix.e) > translateThresh ||
                         Math.abs(resultMatrix.f) > translateThresh;
      if (translated)
        return "";

      // remove all strings, in quotes, including escaping chars, putting
      // instead `\x01${string-index}` in place, which is not valid CSS,
      // so that it's safe to parse it back at the end of the operation.
      content = $(content).trim().replace(
        /(["'])(?:(?=(\\?))\2.)*?\1/g,
        value => `\x01${strings.push($(value).slice(1, -1)) - 1}`
      );

      // replace attr(...) with the attribute value or an empty string,
      // ignoring units and fallback values, as these do not work, or have,
      // any meaning in the CSS `content` property value.
      content = content.replace(
        /\s*attr\(\s*([^\s,)]+)[^)]*?\)\s*/g,
        (_, name) => $(element).getAttribute(name) || ""
      );

      // replace back all `\x01${string-index}` values with their corresponding
      // strings, so that the outcome is a real, cleaned up, `content` value.
      return content.replace(
        /\x01(\d+)/g,
        (_, index) => strings[index]);
    }
    return "";
  }

  /**
   * Checks if child element is 100% included in the parent element.
   *
   * @param {Element} childNode
   * @param {Element} parentNode
   * @param {Object?} conf
   * @param {Number?} conf.boxMargin
   * @param {boolean?} conf.ignorePadding
   * @returns {boolean}
   */
  function isContained(childNode, parentNode, {
    boxMargin = 2,
    ignorePadding = false
  } = {}) {
    let child = $(childNode).getBoundingClientRect();
    if (ignorePadding) {
      const style = getComputedStyle(childNode);
      const paddingTop = parseFloat(style.paddingTop) || 0;
      const paddingRight = parseFloat(style.paddingRight) || 0;
      const paddingBottom = parseFloat(style.paddingBottom) || 0;
      const paddingLeft = parseFloat(style.paddingLeft) || 0;

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

  /**
   * Returns the visible text content from an element and its descendants.
   *
   * @param {Element} element The element whose visible text we want.
   * @param {Element} closest The closest parent to reach while checking
   *   for visibility.
   * @param {?CSSStyleDeclaration} style The computed style of element. If
   *   falsey it will be queried.
   * @param {Element} parentOverflowNode The closest parent with overflow hidden
   * @param {Element} originalElement The original element
   * @param {?Array.<Element>} shadowRootParents Optional list of
   *    shadow root parents.
   * @param {DOMMatrix} domMatrix The combined transformation matrix
   *  of the parent elements.
   * @param {?Object} conf Configuration object
   * @param {?Number} conf.boxMargin The optional parameter that
   *   can be used to specify how much to stretch the bounding box of the
   *   overflow parent in pixels. Used to counter the hiding methods that
   *   involve pushing decoy elements outside an overflow-y:hidden parent
   *   to make them invisible. Default is 2 pixels.
   * @returns {string} The text that is visible.
   * @private
   */
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
      style = getComputedStyle(element);

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
        case ELEMENT_NODE:
          text += getVisibleContent(node,
                                    element,
                                    getComputedStyle(node),
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
          // If there is a parent with overflow:hidden, it is possible to push
          // elements out of the boundary box of that parent to make them
          // invisible. This clause checks against that. We fallback to the
          // current behaviour if no overflow parent.
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
  const boxMargin = parseFloat(boxMarginStr) || 0;

  const bgColorCheckStr = optionalParameters.get("-disable-bg-color-check");
  const bgColorCheck = !(bgColorCheckStr === "true");

  const checkIsContainedStr = optionalParameters.get("-check-is-contained");
  const checkIsContained = (checkIsContainedStr === "true");

  const ignorePaddingStr = optionalParameters.get("-ignore-padding");
  const ignorePadding = (ignorePaddingStr === "true");

  const translateThreshStr = optionalParameters.get("-pseudo-box-margin");
  const translateThresh = parseFloat(translateThreshStr) || 0;

  let re = toRegExp(search);
  let seen = new WeakSet();

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
        // eslint-disable-next-line max-len
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
