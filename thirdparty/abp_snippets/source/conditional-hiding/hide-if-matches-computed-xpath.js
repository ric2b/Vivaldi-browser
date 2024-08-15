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

import {hideElement, initQueryAndApply} from "../utils/dom.js";
import {waitUntilEvent} from "../utils/execution.js";
import {profile} from "../introspection/profile.js";
import {raceWinner} from "../introspection/race.js";
import {getDebugger} from "../introspection/log.js";
import {toRegExp} from "../utils/general.js";

let {MutationObserver, WeakSet} = $(window);

const {ELEMENT_NODE} = Node;

/**
 * Hide a specific element through a XPath 1.0 query string.
 * See {@tutorial xpath-filters} to know more.
 * @alias module:content/snippets.hide-if-matches-xpath
 *
 * @param {string} query The template XPath query that targets the
 * element to hide. Use {{}} to dynamically insert into the query.
 * @param {string} searchQuery XPath query that searchs for an element
 * to be used alongside searchRegex
 * @param {string} searchRegex The regex that is used to extract a text from
 * the element that matches searchQuery. The extracted text will be injected
 * to the query.
 * @param {string} waitUntil Optional parameter that can be used to delay
 * the running of the snippet until the given state is reached.
 * Accepts: loading, interactive, complete, load or any event name
 *
 */
export function hideIfMatchesComputedXPath(query, searchQuery, searchRegex,
                                           waitUntil) {
  const {mark, end} = profile("hide-if-matches-computed-xpath");
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
    const seenMap = new WeakSet();
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
        debugLog("success", "Matched: ", node, " for selector: ", query);
      });
      end();
    };
    const mo = new MutationObserver(callback);
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
      const foundTexts = [];
      const seenMap = new WeakSet();
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
              // If there is a matching group in regex use that one,
              // otherwise return everything.
              foundTextArr[1] ? foundText = foundTextArr[1] :
                foundText = foundTextArr[0];

              foundTexts.push(foundText);
              debugLog("info", "Matched search query: ", foundText);
              startHidingMutationObserver(foundText);
            }
          }
        });
      };

      searchMO = new MutationObserver(findMutationSearchNodes);
      searchMO.observe(
        document, {characterData: true, childList: true, subtree: true}
      );
      findMutationSearchNodes();
    }
  };

  waitUntilEvent(debugLog, mainLogic, waitUntil);
}
