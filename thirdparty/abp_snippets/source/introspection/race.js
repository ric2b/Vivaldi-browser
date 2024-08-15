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
import {noop} from "./log.js";
import {getDebugger} from "./log.js";

let {Array, Error, Map, parseInt} = $(window);

let stack = null;
let won = null;

// #$#race start; thing1; thing2; race stop;

/**
 * Delimits a race among filters, to be able to disable competing filters when
 * any of them "wins the race". `#$#race start; filter1; filter2; race end;`
 * @param {string} action either `start` or `stop` the race.
 * @param {string} winners the amount of possible race's winners: 1 by default.
 */
export function race(action, winners = "1") {
  switch (action) {
    case "start":
      stack = {
        winners: parseInt(winners, 10) || 1,
        participants: new Map()
      };
      won = new Array();
      break;
    case "end":
    case "finish":
    case "stop":
      stack = null;
      for (let win of won)
        win();
      won = null;
      break;
    default:
      throw new Error(`Invalid action: ${action}`);
  }
}

/**
 * Returns a function that, when a race is happening, can mark a winner,
 * by invoking all callbacks passed for every other snippet that lost the race.
 * @param {string} name the snippet name that is racing.
 * @param {function} lose a callback that, once invoked, will stop the snippet.
 * @returns {function} a callback to invoke whenever a match happens.
 */
export function raceWinner(name, lose) {
  // without races, every invoke is a noop
  if (stack === null)
    return noop;

  // within races though, trap the current stack and winners because more than
  // a race could be defined for the same domains / filters
  let current = stack;
  let {participants} = current;
  participants.set(win, lose);

  // return a function that, once invoked, becomes a noop every other time and
  // also invokes every other functions in the race to stop them as loosers
  return win;

  function win() {
    // make the noop case the fastest one for any further invoke
    if (current.winners < 1)
      return;

    let debugLog = getDebugger("race");
    debugLog("success", `${name} won the race`);

    // in case a snippet wins while the race is still happening, queue them all
    // so that unknown racing snippets get a chance to be disabled later on.
    // i.e. race start 2; winner1; looser2; winner3; looser4; race end;
    if (current === stack) {
      won.push(win);
    }
    else {
      participants.delete(win);
      if (--current.winners < 1) {
        for (let looser of participants.values())
          looser();

        participants.clear();
      }
    }
  }
}
