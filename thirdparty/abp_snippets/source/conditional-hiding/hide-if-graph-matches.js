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
import {caller} from "proxy-pants/function";

import {profile} from "../introspection/profile.js";
import {getDebugger} from "../introspection/log.js";
import {randomId} from "../utils/general.js";
import {$$, hideElement} from "../utils/dom.js";
import {raceWinner} from "../introspection/race.js";

let {
  chrome,
  getComputedStyle,
  isExtensionContext,
  parseFloat,
  Array,
  MutationObserver,
  Object,
  WeakSet
} = $(window);


// The logic in this file requires best possible performance. Accordingly,
// instead of promoting every Array as secured Array, methods trapped as caller
// from the Array.prototype are used instead, which is faster than using a
// setPrototypeOf(eachReference) multiple times.
// This is hence an exception to the usual `$([])` pattern.
let {
  filter,
  map,
  push,
  reduce,
  some
} = caller(window.Array.prototype);

/**
 * Hides any HTML element if its structure (graph) is classified as an ad
 * by a built-in machine learning model.
 * @alias module:content/snippets.hide-if-graph-matches
 *
 * @param {string} selector A selector that produces a list of targets to
 * classify.
 * @param {string} tagName An HTML tag name to filter mutations.
 *
 * @since Adblock Plus 3.11.3
 */
export function hideIfGraphMatches(selector, tagName) {
  let scheduled = false;
  let seenMlTargets = new WeakSet();
  let relevantTagName = tagName;
  let callback = mutations => {
    for (let mutation of $(mutations)) {
      let {target} = $(mutation, "MutationRecord");
      if ($(target).tagName == relevantTagName) {
        if (!scheduled) {
          scheduled = true;
          requestAnimationFrame(() => {
            scheduled = false;
            predictAds(win, selector, seenMlTargets);
          });
        }
        break;
      }
    }
  };

  let mo = new MutationObserver(callback);
  let win = raceWinner(
    "hide-if-graph-matches",
    () => mo.disconnect()
  );
  mo.observe(document, {childList: true, subtree: true});
  predictAds(win, selector, seenMlTargets);
}

const GRAPH_CUT_OFF = 50;
const THRESHOLD = 0.5;

// See https://github.com/mozilla/webextension-polyfill/issues/130
const CHROME_SEND_MESSAGE_CALLBACK_NO_RESPONSE_MESSAGE =
        "The message port closed before a response was received.";

const sendMessage = message => new Promise((resolve, reject) => {
  chrome.runtime.sendMessage(message, response => {
    if (chrome.runtime.lastError) {
      if (
        chrome.runtime.lastError.message ===
        CHROME_SEND_MESSAGE_CALLBACK_NO_RESPONSE_MESSAGE
      )
        resolve();
      else
        reject(new Error(chrome.runtime.lastError.message));
    }
    else {
      resolve(response);
    }
  });
});


/**
 * Builds an adjecency matrix and a feature matrix based on an input element.
 * @param {Element} target Input element to convert.
 * @returns {Tuple} (adjMatrix, elementTags) - a 2D array that represent an
 * adjacency matrix of an element. HTML elements are undirected trees, so the
 * adjacency matrix is symmetric.
 * elementTags - a 1D feature matrix, where each element is represents a type
 * of a node.
 * @private
 */
function processElement(target) {
  let {adjacencyMatrix, features} = addEdgesFeatures(target, GRAPH_CUT_OFF);
  return {adjacencyMatrix, features};
}

/**
 * Runs a ML prediction on each element that matches a selector.
 * @param {function} win A callback used to flag a matched case/element.
 * @param {string} selector A selector to use for finding candidates.
 * @param {WeakSet} seenMlTargets Matched elements to ignore.
 *
 * @private
 */
function predictAds(win, selector, seenMlTargets) {
  let debugLog = getDebugger("hide-if-graph-matches");

  let targets = $$(selector);
  for (let target of targets) {
    if (seenMlTargets.has(target))
      continue;

    if ($(target).innerText == "")
      continue;

    seenMlTargets.add(target);
    let processedElement = processElement(target);
    // as this call is asynchronous, ensure the id is unique
    let {mark, end} = profile(`ml:inference:${randomId()}`);
    mark();
    let message = {
      type: "ewe:ml-inference",
      inputs: [
        {
          data: [processedElement.adjacencyMatrix], preprocess: [
            {funcName: "padAdjacency", args: GRAPH_CUT_OFF},
            {funcName: "unstack"},
            {funcName: "localPooling"},
            {funcName: "stack"}
          ]
        },
        {
          data: [processedElement.features], preprocess: [
            {funcName: "padFeatures", args: GRAPH_CUT_OFF},
            {funcName: "cast", args: "float32"}
          ]
        }
      ],
      model: "hideIfGraphMatches"
    };

    let process = function(rawPrediction) {
      // Some extensions are shipped without models
      // in which case there won't be a response.
      if (!rawPrediction)
        return;

      let predictionValues = Object.values(rawPrediction);

      if (!some(predictionValues, value => value > 0))
        debugLog("Error: ML prediction results are corrupted");

      // normalize prediction values
      let norm = reduce(
        predictionValues,
        (acc, val) => acc + val,
        0
      );
      let normPredictionValues = map(
        predictionValues,
        val => val / norm
      );
      let result = map(
        filter(normPredictionValues, (_, index) => index % 2 == 1),
        element => element > THRESHOLD
      );

      if (result[0]) {
        debugLog("Detected ad: " + $(target).innerText);
        win();
        hideElement(target);
      }
    };

    if (isExtensionContext) {
      sendMessage(message)
      .then(response => {
        if (response) {
          process(response);
          debugLog(response);
        }
        else {
          // Compatibility for legacy adblockpluschrome
          message.type = "ml.inference";
          sendMessage(message)
            .then(process)
            .catch(debugLog);
        }
        end(true);
      })
      .catch(debugLog);
    }
  }
}

/**
 * Defines a structure for individual node in the Graph used for ML module.
 */
class GraphNode {
  /**
   * Initializes a Node with the tag name and attributed related to the node.
   * @param {string} tag - name of the tag which represents the node
   * @param {Object} attributes - attributes related to the node
   * @private
   */
  constructor(tag, attributes) {
    this.tag = tag;
    this.attributes = attributes;
    this.children = [];
    this.elementHidden = false;
    this.elementBlocked = false;
    this.filter = null;
    this.requestType = null;
    this.height = 0;
    this.width = 0;
    this.cssSelectors = null;
  }

  /**
   * Adds a child node to the current GraphNode.
   * @param {GraphNode} newChild - the child node to be added.
   */
  addChild(newChild) {
    push(this.children, newChild);
  }
}

const IMPORTANT_NODE_ATTRIBUTES = ["style"];

/**
 * Returns a new Object of attributes with only the important ones.
 * @param {Object} object - attributes to be filtered
 *
 * @return {Object} - the filtered attributes
 */
function cloneObject(object) {
  let newObject = {};

  for (let attr in object) {
    if (object[attr])
      newObject[attr] = object[attr];
  }

  return newObject;
}

/**
 * Given a DOM tree, returns the adjacency matrix and features array after
 * doing DFS through the graph.
 *
 * @param {Object} target - the node to start the DFS from
 * @param {number} cutoff - the maximum number of nodes to be added.
 *
 * @returns {Object} - the adjacency matrix and features array
 */
function addEdgesFeatures(target, cutoff) {
  // Create adjacency matrix and initialize it with '0's
  let emptyMatrix = new Array(cutoff);
  for (let i = 0; i < cutoff; i++)
    emptyMatrix[i] = new Array(cutoff).fill(0);
  let adjacencyMatrix = emptyMatrix;
  let features = [];

  let numOfElements = 0;

  /**
   * DFS through the DOM tree and creates GraphNode, which is used for
   * generating features and adjacency matrix.
   *
   * @param {Object} element - the node to process in one step of DFS
   * @param {GraphNode} parentNode - the parent node of the current nodeId
   */
  function domToGraph(element, parentNode) {
    if (numOfElements >= cutoff)
      return;

    let attributes = {};

    // Get all the attributes
    for (let attr of $(element).attributes)
      attributes[$(attr).name] = $(attr).value;

    for (let attr of IMPORTANT_NODE_ATTRIBUTES)
      attributes[attr] = cloneObject(element[attr]);

    // Create the graph object
    let node = new GraphNode(element.tagName, attributes);

    // Add CSS selectors and size attributes
    node.cssSelectors = getComputedStyle(element).cssText;
    node.height = element.clientHeight;
    node.width = element.clientWidth;

    // Add nodeId
    node.nodeId = numOfElements;
    numOfElements += 1;

    if (parentNode !== null) {
      adjacencyMatrix[parentNode.nodeId][node.nodeId] = 1;
      adjacencyMatrix[node.nodeId][parentNode.nodeId] = 1;
    }

    // Add node level, parent node id, siblings
    if (parentNode == null) {
      node.nodeLevel = 0;
      node.parentNodeId = 0;
      node.siblings = 0;
    }
    else {
      node.nodeLevel = parentNode.nodeLevel + 1;
      node.parentNodeId = parentNode.nodeId;
      node.siblings = parentNode.children.length;
    }

    // Add children
    node.numChildren = element.children.length;

    // add src and reqtype
    // eslint-disable-next-line no-prototype-builtins
    if (node.attributes.hasOwnProperty("src") &&
    // eslint-disable-next-line no-undefined
      node.attributes["src"] !== undefined) {
      node.attributes["src_level"] = 0;
    }
    else if (parentNode !== null &&
        Object.values(parentNode.attributes).length !== 0 &&
        // eslint-disable-next-line no-prototype-builtins
        parentNode.attributes.hasOwnProperty("src") &&
        // eslint-disable-next-line no-undefined
        parentNode.attributes["src"] !== undefined) {
      node.attributes["src"] = parentNode.attributes["src"];
      node.attributes["src_level"] = parentNode.attributes["src_level"] + 1;
      node["requestType"] = parentNode["requestType"];
    }

    // call feature generator
    node.features = new featureGenerator().
    getNodeFeatures(node, location.href, true, true, true, true);

    push(features, node.features);

    // DFS through the children
    for (let child of element.children)
      domToGraph(child, node);
  }

  domToGraph(target, null);

  return {adjacencyMatrix, features};
}

/**
 * Function to generate an eye matrix
 * @param {number} n - size of the matrix
 * @returns {Array} - the eye matrix
 */
let eye = function(n) {
  let matrix = [];
  for (let i = 0; i < n; i++) {
    let content = [];
    for (let k = 0; k < n; k++)
      push(content, i === k ? 1 : 0);
    push(matrix, content);
  }
  return matrix;
};

const DISPLAY_ATTR_MAP = {"none": 1, "inline": 2,
                          "block": 3, "inline-block": 4, "inherit": 5};

const ONE_HOT_DISPLAY_LEN = 5;

const NUM_CSS_SELECTORS = 55;

const CSS_COLORS = ["background-color", "border-bottom-color",
                    "border-left-color", "border-right-color",
                    "border-top-color", "color", "outline-color",
                    "text-decoration", "text-decoration-color",
                    "column-rule-color", "-webkit-text-emphasis-color",
                    "-webkit-text-fill-color", "-webkit-text-stroke-color",
                    "caret-color"];

const CSS_PIXELS = ["border-bottom-width", "height", "min-height",
                    "min-width", "padding-bottom", "padding-left",
                    "padding-right", "padding-top", "width"];

const CSS_ORIGINS = ["perspective-origin", "transform-origin"];

const REQUEST_TYPES = ["script", "subdocument", "image", "xmlhttprequest",
                       "font", "document", "stylesheet", "other", "ping",
                       "websocket", "media", "object"];

let requestTypesDict = {};
const reqtypeVEC = eye(REQUEST_TYPES.length);

for (let ob in REQUEST_TYPES)
  requestTypesDict[REQUEST_TYPES[ob]] = reqtypeVEC[ob];


const URL_CHARS =
// eslint-disable-next-line no-useless-escape
  " abcdefghijklmnopqrstuvwxyz1234567890:;/?!=+.,()[]-`*_\|~".split("");

let wordIndex = {};

let charEncoderIndex = 1;
URL_CHARS.forEach(char => {
  wordIndex[char] = charEncoderIndex;
  charEncoderIndex += 1;
});

/**
 * featureGenerator to generate features for the nodes
 */
let featureGenerator = function() {
  let appendFeatures = function(features, arr) {
    map(arr, el => push(features, el));
  };

  /**
   * Generates a sequence for a given string based on a wordIndex
   *
   * @param {string} st - the string to generate the sequence for
   * @param {number} maxLen - the maximum length of the sequences
   * @returns {Array} - the generated sequence
   */
  let getStringSeq = function(st, maxLen) {
    let seq = [];
    st = st.split("");
    st.forEach(char => {
      // eslint-disable-next-line no-prototype-builtins
      push(seq, wordIndex.hasOwnProperty(char) ?
        wordIndex[char] : -1);
    });
    if (seq.length > maxLen)
      return seq.slice(-maxLen);

    // padding
    appendFeatures(seq, new Array(maxLen - seq.length).fill(0));
    return seq;
  };

  /**
   * Gets the vector for a given tag name.
   *
   * @param {Object} node - The node that contains the tag
   * @param {number} maxLen - The maximum length of the sequence
   * @returns {Array} - The vector for the tag
   */
  let getTagVec = function(node, maxLen = 8) {
    let tag = node["tag"].toLowerCase();
    let tagVec = getStringSeq(tag, maxLen);
    return tagVec;
  };
  /**
   * Returns the meta features for the domains
   *
   * @param {Object} node - The node that contains the domain
   * @param {string} domain - The domain
   * @param {string} target - target field
   * @returns {Array} - The meta features for the domain
   */
  let getMetaFeatures = function(node, domain, target) {
    // Checking for source attribute
    let sourceIsUrl = 0.0;
    let hasSource = 0.0;
    let sourceIsThirdParty = 0.0;

    let attrs = node["attributes"];
    // eslint-disable-next-line no-undefined
    if (attrs !== undefined &&
      // eslint-disable-next-line no-prototype-builtins
      attrs.hasOwnProperty(target)) {
      let src = attrs[target].toLowerCase();
      hasSource = 1.0;
      if (src.startsWith("http") == true)
        sourceIsUrl = 1.0;

      let pathArray = src.split("/");
      let netloc = pathArray[2];
      if (netloc !== domain)
        sourceIsThirdParty = 1.0;
    }
    return [hasSource, sourceIsUrl, sourceIsThirdParty];
  };

  /**
   * Gets rgb features for a given rgb_string
   *
   * @param {String} rgbString - The rgb string
   * @returns {Array} - The rgb features
   */
  let getRgb = function(rgbString) {
    // Extract and normalize RGB colors from a CSS string
    let regex = /rgba?\((\d+), (\d+), (\d+)\)/g;
    let rgbColors = regex.exec(rgbString);

    if (rgbColors) {
      return [parseFloat(rgbColors[1]),
              parseFloat(rgbColors[2]),
              parseFloat(rgbColors[3])];
    }
    return [0.0, 0.0, 0.0];
  };

  /**
   * Gets pixels from a pixel strings
   *
   * @param {String} pxString - The pixel string
   * @returns {Array} - The pixels
   */
  let getPixels = function(pxString) {
    let regex = /(\d*?\.?\d*)px/g;
    let pixel = regex.exec(pxString);

    if (pixel)
      return [parseFloat(pixel[1])];

    return [0.0];
  };

  /**
   * Gets origins for a css coordinate string
   *
   * @param {String} originString - The css string
   * @returns {Array} - The origins
   */
  let getOrigins = function(originString) {
    let regex = /(\d*?\.?\d*)px (\d*?\.?\d*)px/g;
    let origin = regex.exec(originString);

    if (origin)
      return [parseFloat(origin[1]), parseFloat(origin[2])];

    return [0.0, 0.0];
  };

  /**
   * Returns the css features for a given node
   *
   * @param {Object} node - The node that contains the css
   * @returns {Array} - The css features
   */
  let getCssFeatures = function(node) {
    let css = node["cssSelectors"];

    if (!css)
      return new Array(NUM_CSS_SELECTORS).fill(0.0);

    let cssAttrs = {};
    let spl = css.split("; ");

    for (let element in spl) {
      let el = spl[element].split(":");

      let x = el[0];
      let y = el[1];
      // eslint-disable-next-line no-undefined
      if (x !== undefined && y !== undefined)
        cssAttrs[x.trim()] = y.trim();
    }


    let cssFeatures = [];

    for (let color in CSS_COLORS) {
      // eslint-disable-next-line no-prototype-builtins
      let cssColors = cssAttrs.hasOwnProperty(CSS_COLORS[color]) ?
           getRgb(cssAttrs[CSS_COLORS[color]]) : [0.0, 0.0, 0.0];
      appendFeatures(cssFeatures, cssColors);
    }
    for (let px in CSS_PIXELS) {
      // eslint-disable-next-line no-prototype-builtins
      let cssPixels = cssAttrs.hasOwnProperty(CSS_PIXELS[px]) ?
          getPixels(cssAttrs[CSS_PIXELS[px]]) : [0.0];
      appendFeatures(cssFeatures, cssPixels);
    }
    for (let origin in CSS_ORIGINS) {
      // eslint-disable-next-line no-prototype-builtins
      let cssOrigins = cssAttrs.hasOwnProperty(CSS_ORIGINS[origin]) ?
          getOrigins(cssAttrs[CSS_ORIGINS[origin]]) : [0.0, 0.0];
      appendFeatures(cssFeatures, cssOrigins);
    }

    return cssFeatures;
  };
  /**
   * Returns all style features including css features
   *
   * @param {Object} node - The node that contains the features to be extracted
   * @returns {Array} - The style features
   */
  let getStyleFeatures = function(node) {
    // eslint-disable-next-line no-prototype-builtins
    let attrs = node.hasOwnProperty("attributes") ?
      node["attributes"] : {};
    // eslint-disable-next-line no-prototype-builtins
    let styleAttrs = attrs.hasOwnProperty("style") ?
      attrs["style"] : {};

    // eslint-disable-next-line no-prototype-builtins
    let offsetHeight = styleAttrs.hasOwnProperty("offsetHeight") ?
      styleAttrs["offsetHeight"] : "0";

    if (offsetHeight)
      offsetHeight = parseFloat(offsetHeight.trim());

    // eslint-disable-next-line no-prototype-builtins
    let offsetWidth = styleAttrs.hasOwnProperty("offsetWidth") ?
      styleAttrs["offsetWidth"] : "0";

    if (offsetWidth)
      offsetWidth = parseFloat(offsetWidth.trim());

    // eslint-disable-next-line no-prototype-builtins
    let displayAttr = styleAttrs.hasOwnProperty("display") ?
      styleAttrs["display"] : "-1";

    if (displayAttr)
      displayAttr.trim().toLowerCase();

    // eslint-disable-next-line no-prototype-builtins
    displayAttr = DISPLAY_ATTR_MAP.hasOwnProperty(displayAttr) ?
      DISPLAY_ATTR_MAP[displayAttr] : 0;

    let displayVec = new Array(ONE_HOT_DISPLAY_LEN).fill(0);

    if (displayAttr > 0)
      displayVec[displayAttr - 1] = 1;

    let styleFeatures = [offsetHeight, offsetWidth];
    appendFeatures(styleFeatures, displayVec);

    // eslint-disable-next-line no-prototype-builtins
    let fontSize = styleAttrs.hasOwnProperty("font-size") ?
      styleAttrs["font-size"] : 0;

    if (fontSize)
      fontSize = parseFloat(fontSize.trim().toLowerCase().replace("px", ""));

    push(styleFeatures, fontSize);
    return styleFeatures;
  };

  /**
   * Given a node, generates all features related to it.
   *
   * @param {Object} node - The node that contains the features to be extracted
   * @param {string} domain - The domain of the node
   * @param {boolean} topLevel - Whether to include top_level features
   * @param {boolean} attributes - Whether to include attributes features
   * @param {boolean} style - Whether to include style features
   * @param {boolean} css - Whether to include css features
   *
   * @returns {Array} - The features
   */
  this.getNodeFeatures = function(node, domain,
                                  topLevel = true, attributes = false,
                                  style = false, css = false) {
    let features = [];

    if (topLevel == true) {
      // Add node ids
      push(features, node.nodeId);

      // Add parent ids
      push(features, node.parentNodeId);

      // Add siblings
      push(features, node.siblings);

      // Get the tag ID as an one hot vector
      appendFeatures(features, getTagVec(node));

      // level
      push(features, node.nodeLevel);

      // children
      push(features, node.numChildren);

      // eslint-disable-next-line no-prototype-builtins
      let reqType = node.hasOwnProperty("requestType") ?
        node["requestType"] : null;

      if (reqType !== null)
        reqType = reqType.toLowerCase();

      let reqTypeVec = new Array(REQUEST_TYPES.length).fill(0);
      // eslint-disable-next-line no-prototype-builtins
      if (reqType !== null && requestTypesDict.hasOwnProperty(reqType))
        reqTypeVec = requestTypesDict[reqType];
      appendFeatures(features, reqTypeVec);
    }

    if (attributes == true) {
      // Get source sequence
      // eslint-disable-next-line no-prototype-builtins
      let attrs = node.hasOwnProperty("attributes") ?
        // eslint-disable-next-line no-undefined
         node["attributes"] : undefined;

      let src = "";
      // eslint-disable-next-line no-undefined
      if (attrs != undefined &&
        // eslint-disable-next-line no-prototype-builtins
        attrs.hasOwnProperty("src")) {
        src = attrs["src"];
        src = src.trim().toLowerCase();
      }
      appendFeatures(features, getStringSeq(src, 256));

      // Get source meta attributes
      appendFeatures(features, getMetaFeatures(node, domain, "src"));

      // Get href sequence
      let href = "";
      // eslint-disable-next-line no-undefined
      if (attrs != undefined &&
        // eslint-disable-next-line no-prototype-builtins
        attrs.hasOwnProperty("href")) {
        href = attrs["href"];
        href = href.trim().toLowerCase();
      }
      appendFeatures(features, getStringSeq(href, 256));

      // Get href meta attributes
      appendFeatures(features, getMetaFeatures(node, domain, "href"));

      // Get id sequence
      let id = "";
      // eslint-disable-next-line no-undefined
      if (attrs != undefined &&
          // eslint-disable-next-line no-prototype-builtins
          attrs.hasOwnProperty("id")) {
        id = attrs["id"];
        id = id.trim().toLowerCase();
        // replace punct and collapse whitespace
        // eslint-disable-next-line no-useless-escape, no-irregular-whitespace
        id = id.replace(/[!"'\(\)\*,\-\.\/:;\?\[\\\]\^_`\{\|\}~“” ]/g, " ").replace(/\s+/g, " ");
      }
      appendFeatures(features, getStringSeq(id, 16));

      // Get class sequence
      let clas = "";
      // eslint-disable-next-line no-undefined
      if (attrs != undefined &&
          // eslint-disable-next-line no-prototype-builtins
          attrs.hasOwnProperty("class")) {
        clas = attrs["class"];
        clas = clas.trim().toLowerCase();
        // replace punct and collapse whitespace
        // eslint-disable-next-line no-useless-escape, no-irregular-whitespace
        clas = clas.replace(/[!"'\(\)\*,\-\.\/:;\?\[\\\]\^_`\{\|\}~“” ]/g, " ").replace(/\s+/g, " ");
      }
      appendFeatures(features, getStringSeq(clas, 16));
    }
    if (style == true)
      appendFeatures(features, getStyleFeatures(node));

    // Get CSS style features
    if (css == true)
      appendFeatures(features, getCssFeatures(node));

    return features;
  };
};
