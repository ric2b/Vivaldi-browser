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

let {Math, setInterval, chrome, isExtensionContext, performance} = $(window);

/**
 * Default profile("...") returned object when profile mode is disabled.
 * @type {Profiler}
 * @private
 */
const noopProfile = {
  mark() {},
  end() {},
  toString() {
    return "{mark(){},end(){}}";
  }
};

/**
 * Whether profile mode is inactive.
 * @type {boolean}
 * @private
 */
let inactive = true;

/**
 * Tells if the profile is inactive.
 * @memberOf module:content/snippets.profile
 * @returns {boolean}
 */
export function inactiveProfile() {
  return inactive;
}

/**
 * Enables profile mode.
 * @alias module:content/snippets.profile
 * @since Adblock Plus 3.9
 *
 * @example
 * example.com#$#profile; log 'Hello, world!'
 */
export function setProfile() {
  inactive = false;
}

/**
 * @typedef {object} Profiler
 * @property {function} mark Add a `performance.mark(uniqueId)` entry.
 * @property {function} end Measure and clear `uniqueId` related marks. If a
 * `true` value is passed as argument, clear related interval and process all
 * collected samples since the creation of the profiler.
 * @private
 */

/**
 * Create an object with `mark()` and `end()` methods to either keep marking a
 * specific profiled name, or ending it.
 *
 * @example
 * let {mark, end} = profile('console.log');
 * mark();
 * console.log(1, 2, 3);
 * end();
 *
 * @param {string} id the callback name or unique ID to profile.
 * @param {number} [rate] The number of times per minute to process samples.
 * @returns {Profiler} The profiler with `mark()` and `end(clear = false)`
 * methods.
 * @private
 */
export function profile(id, rate = 10) {
  if (inactive || !isExtensionContext)
    return noopProfile;

  function processSamples() {
    let samples = $([]);

    for (let {name, duration} of performance.getEntriesByType("measure"))
      samples.push({name, duration});

    if (samples.length) {
      performance.clearMeasures();

      chrome.runtime.sendMessage({
        type: "ewe:profiler.sample",
        category: "snippets",
        samples
      });
    }
  }

  // avoid creation of N intervals when the same id is used
  // over and over (i.e. within loops or multiple profile calls)
  if (!profile[id]) {
    profile[id] = setInterval(processSamples,
                              Math.round(60000 / Math.min(60, rate)));
  }

  return {
    mark() {
      performance.mark(id);
    },
    end(clear = false) {
      performance.measure(id, id);
      performance.clearMarks(id);
      if (clear) {
        clearInterval(profile[id]);
        delete profile[id];
        processSamples();
      }
    }
  };
}
