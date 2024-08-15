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

// this is required by $ so it cannot be "magic" or circular dependency happens
import env from "./env.js";
import transformOnce from "transform-once";

const {Map, WeakMap, WeakSet, setTimeout} = env;

let cleanup = true;
let cleanUpCallback = map => {
  map.clear();
  cleanup = !cleanup;
};

export default transformOnce.bind({
  WeakMap,
  WeakSet,
  // this allows multiple $(primitives) and it cleans references later on
  // basically a WeakRef implementation based on a single, shared, timer
  WeakValue: class extends Map {
    set(key, value) {
      if (cleanup) {
        cleanup = !cleanup;
        setTimeout(cleanUpCallback, 0, this);
      }
      return super.set(key, value);
    }
  }
});
