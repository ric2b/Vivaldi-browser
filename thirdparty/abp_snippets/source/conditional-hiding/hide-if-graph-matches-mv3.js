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
import {hideElement} from "../utils/dom.js";
import {raceWinner} from "../introspection/race.js";

/* global globalThis */

let {
  requestAnimationFrame,
  MutationObserver,
  WeakSet,
  console
} = $(window);

/**
 * Hides any HTML element that is classified as an ad by a machine learning
 * graph model.
 * @alias module:content/snippets.hide-if-graph-matches
 *
 * @param {string} selectors The CSS selector of an HTML element that is to
 *   be evaluated.
 *   Can be prefixed with `mldebug:` to trigger debugging output. E.g.:
 *     `*$#$hideIfGraphMatches mldebug:div.someclass`
 *   Can contain `..` to select the selectors parent. E.g.:
 *     `*$#$hideIfGraphMatches div.someclass..span`
 *       Which will search for `span` within the parent of `div.someclass`.
 *     `*$#$hideIfGraphMatches div.someclass....:scope>div`
 *       Which will search for divs as direct ancestors of two parents up
 *       from `div.someclass`.
 * @param {?string} [subtarget] The CSS selector of an ancestor of the
 *   HTML element which will be used for inference. Defaults to the parent
 *   element if not specified. E.g.:
 *     `*$#$hideIfGraphMatches div.someclass div>span`
 *       Which will run inference on the second selector and hide the
 *       element selected via the first selector.
 *     `*$#$hideIfGraphMatches div.someclass..:scope>.headline div>span`
 *       Which will run inference on a `span` as a direct ancestor of a
 *       `div` found as a child of an element with the class `.headline`
 *       as a direct ancestor of the parent of `div.someclass`. In case of
 *       a match, `div.someclass` will be hidden.
 */
function hideIfGraphMatches(selectors, subtarget) {
  let ml = globalThis.ml;
  let seenMlTargets = new WeakSet();
  let scheduled = false;
  let mlDebug = false;
  let mlDebugCSV = "AD,PRED,DTG,DFS,INF,EV,TOTAL\n";
  let mlDebugCount = 0;

  if (selectors.startsWith("mldebug:")) {
    mlDebug = true;
    selectors = selectors.slice(8);
  }
  selectors = selectors.split("..");

  /**
   * Called on any dom mutation and initiates inference again if the
   * provided `selector` matches new elements.
   * @private
   */
  function onDomMutation() {
    if (!scheduled) {
      scheduled = true;
      requestAnimationFrame(() => {
        scheduled = false;
        predictAds();
      });
    }
  }
  let mo = new MutationObserver(onDomMutation);
  let win = raceWinner("hide-if-graph-matches", () => mo.disconnect());

  /**
   * Initiates ML inference on each element that matches `selector`.
   * @private
   */
  async function predictAds() {
    let selectorsCopy = [...selectors];
    let targetElement = document;
    let targetContainers = [];

    // Parent traversal in case `..` is present in the selector
    // `div.foo..:scope>.bar`
    // translates to
    // `*:has(:scope>div.foo)>.bar`
    while (selectorsCopy.length > 1) {
      let subselector = selectorsCopy.shift();
      let subelement;
      try {
        subelement = targetElement.querySelector(subselector);
        if (subselector && subelement)
          targetElement = subelement.parentElement;
      }
      catch (e) {
        targetElement = document;
        break;
      }
    }

    try {
      targetContainers = targetElement.querySelectorAll(selectorsCopy.shift());
    }
    catch (e) {
      /* no-op */
    }

    for (let targetContainer of targetContainers) {
      if (!$(targetContainer).innerHTML)
        continue;
      let subTargets = $(targetContainer).querySelectorAll(subtarget);
      if (subTargets.length === 0)
        subTargets = [targetContainer];
      for (let target of subTargets) {
        if (seenMlTargets.has(targetContainer))
          continue;

        seenMlTargets.add(targetContainer);

        if (!$(target).innerHTML)
          continue;

        if (mlDebug) {
          let stats = {
            dtNow: Date.now(),
            dtDomToGraph: 0,
            dtPreprocessGraph: 0,
            dtPredict: 0,
            dtDigestPrediction: 0,
            predictions: [],
            isAd: null,
            target
          };
          let graph = await ml.util.domToGraph(
            ml.data.hideIfGraphMatchesConfig.config, target, ml.model
          );
          stats.dtDomToGraph = Date.now();
          let preprocessedGraph = await ml.util.preprocessGraph(
            ml.data.hideIfGraphMatchesConfig.config,
            graph,
            window.location.hostname
          );
          stats.dtPreprocessGraph = Date.now();
          stats.predictions = await ml.util.predict(
            ml.tfjs,
            ml.model,
            ml.data.hideIfGraphMatchesConfig.config,
            preprocessedGraph
          );
          stats.dtPredict = Date.now();
          stats.isAd = await ml.util.digestPrediction(stats.predictions);
          stats.dtDigestPrediction = Date.now();

          console.log(
            "%cMLDB \u2665 %c| PRED: %s, DTG: %sms, DFS: %sms, " +
            "INF: %sms, EV: %sms, TOTAL: %sms | %cAd: %s%c |",
            "color:cyan",
            "color:inherit",
            JSON.stringify(stats.predictions),
            stats.dtDomToGraph - stats.dtNow,
            stats.dtPreprocessGraph - stats.dtDomToGraph,
            stats.dtPredict - stats.dtPreprocessGraph,
            stats.dtDigestPrediction - stats.dtPredict,
            stats.dtDigestPrediction - stats.dtNow,
            stats.isAd ? "color:red" : "color:green",
            stats.isAd,
            "color:inherit",
            target
          );

          mlDebugCount += 1;
          mlDebugCSV += `\
            ${stats.isAd},\
            ${JSON.stringify(stats.predictions)},\
            ${stats.dtDomToGraph - stats.dtNow},\
            ${stats.dtPreprocessGraph - stats.dtDomToGraph},\
            ${stats.dtPredict - stats.dtPreprocessGraph},\
            ${stats.dtDigestPrediction - stats.dtPredict},\
            ${stats.dtDigestPrediction - stats.dtNow}
          `;

          if (mlDebugCount > 9) {
            mlDebugCount = 0;
            console.log(
              "%cMLDB CSV \u2665%c\n%s",
              "color:cyan",
              "color:inherit",
              mlDebugCSV.replace(/ /g, "")
            );
          }
        }
        else {
          ml.util.inference(
            ml.tfjs,
            ml.model,
            ml.data.hideIfGraphMatchesConfig.config,
            target
          ).then(result => {
            if (result) {
              win();
              hideElement(targetContainer);
            }
          }).catch(e => { /* no-op */ });
        }
      }
    }
  }

  /**
   * Waits for graphMLUtil to be initialized, sets up mutation observer
   * and initiates inference on the first batch of targets
   * @param {double} [delta] A DOMHighResTimeStamp representing delta
   *   in milliseconds since last call.
   * @returns {number} Animation frame request ID
   * @private
   */
  function init(delta = 0) {
    if (!ml)
      throw new Error("ml not available.");
    if (!ml.model) {
      if (delta > 2000)
        throw new Error("ml failed to initialize.");
      return requestAnimationFrame(init);
    }
    mo.observe(document, {childList: true, subtree: true});
    predictAds();
  }

  init();
}

export {hideIfGraphMatches};
