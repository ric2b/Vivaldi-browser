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

import {$$, $closest, hideElement} from "../utils/dom.js";
import {raceWinner} from "../introspection/race.js";
import {formatArguments, toRegExp} from "../utils/general.js";
import {getDebugger} from "../introspection/log.js";
import {profile} from "../introspection/profile.js";
import {fetchContent} from "../utils/execution.js";

let {
  getComputedStyle,
  MutationObserver,
  DOMParser,
  Math,
  Node,
  Map
} = $(window);

/**
 * Hides any HTML element or one of its ancestors matching a CSS selector if
 * a child element's background SVG contains a specific, visible text string
 * and/or specific attributes.
 * @alias module:content/snippets.hide-if-svg-contains
 *
 * @param {string} search The text that needs to exist in the SVG image
 * for it to be hidden. If the string begins and ends with a slash (`/`),
 * the text in between is treated as a regular expression.
 * @param {string} selector The CSS selector that an HTML element must match
 * for it to be hidden.
 * @param {?string} [searchSelector] The CSS selector that an HTML element
 * containing the SVG with the given text must match. Defaults to
 * the value of the `selector` argument.
 * @param {?Array.<string>} attributes The CSS attributes a computed style
 * map of a node should have in order to consider that node hidden.
 * Syntax: <key>:<value>, where <value> can be a string or a regex (if it
 * starts and ends with a `/`).
 */

export function hideIfSvgContains(
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
  const defaultOptionalParameters = new Map([
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
  let attributesMap = new Map(defaultCSSEntries.concat(entries));

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

  let mo = new MutationObserver(callback);
  let win = raceWinner(
    "hide-if-svg-contains",
    () => mo.disconnect()
  );
  mo.observe(document, {childList: true, subtree: true});
  callback();

  /**
   * Manually checks a text element in a virtual SVG DOM for visibility,
   * considering inline styles, presentation attributes, and custom attributes.
   * @param {Element} element The text element to check(from virtual SVG DOM):
   * @param {RegExp} searchRegExp The regex to test against text content.
   * @returns {boolean} True if the element is visible and
   * its text matches, false otherwise.
   */
  function isElementVisibleAndTextMatchesInSvg(
    element,
    searchRegExp
  ) {
    if (!searchRegExp.test(element.textContent))
      return false;

    const {ELEMENT_NODE} = Node;
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

        if (Math.abs(finalX) > positionThresh ||
        Math.abs(finalY) > positionThresh)
          return false;
      }

      currentElement = currentElement.parentElement;
    }

    return true;
  }
}

