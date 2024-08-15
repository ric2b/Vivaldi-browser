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
import {profile} from "../introspection/profile.js";
import {raceWinner} from "../introspection/race.js";
import {getDebugger} from "../introspection/log.js";

let {MutationObserver, WeakSet} = $(window);

const {ELEMENT_NODE} = Node;

/**
 * Hide a specific element through a XPath 1.0 query string.
 * See {@tutorial xpath-filters} to know more.
 * @alias module:content/snippets.hide-if-matches-xpath
 *
 * @param {string} query The XPath query that targets the element to hide.
 * @param {string} scopeQuery CSS or XPath selector that the filter devs can
 * use to restrict the scope of the MO.
 * It is important that the selector is as specific as possible to avoid to
 * match too many nodes.
 *
 * @since Adblock Plus 3.9.0
 */
export function hideIfMatchesXPath(query, scopeQuery) {
  const {mark, end} = profile("hide-if-matches-xpath");
  const debugLog = getDebugger("hide-if-matches-xpath");

  const startHidingMutationObserver = scopeNode => {
    const queryAndApply = initQueryAndApply(`xpath(${query})`);
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
      "hide-if-matches-xpath",
      () => mo.disconnect()
    );
    mo.observe(
      scopeNode, {characterData: true, childList: true, subtree: true});
    callback();
  };

  if (scopeQuery) {
    // This is a performance optimization: we only observe DOM subtrees
    // instead of the whole document, if a scope query is given.
    let count = 0;
    let scopeMutationObserver;
    const scopeQueryAndApply = initQueryAndApply(`xpath(${scopeQuery})`);
    const findMutationScopeNodes = () => {
      scopeQueryAndApply(scopeNode => {
        // Start a Mutation Observer for each found node
        startHidingMutationObserver(scopeNode);
        count++;
      });
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
    // If no scope query has been specified, observe the document
    startHidingMutationObserver(document);
  }
}
