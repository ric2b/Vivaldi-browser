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
import {bind} from "proxy-pants/function";

import {debug} from "./debug.js";

const {console} = $(window);

export const noop = () => {};

/**
 * Logs its arguments to the console.
 *
 * This may be used for testing and debugging.
 *
 * @alias module:content/snippets.log
 *
 * @param {...*} [args] The arguments to log.
 *
 * @since Adblock Plus 3.3
 */
export function log(...args) {
  if (debug()) {
    const logArgs = ["%c DEBUG", "font-weight: bold;"];

    const isErrorIndex = args.indexOf("error");
    const isWarnIndex = args.indexOf("warn");
    const isSuccessIndex = args.indexOf("success");
    const isInfoIndex = args.indexOf("info");

    if (isErrorIndex !== -1) {
      logArgs[0] += " - ERROR";
      logArgs[1] += "color: red; border:2px solid red";
      $(args).splice(isErrorIndex, 1);
    }
    else if (isWarnIndex !== -1) {
      logArgs[0] += " - WARNING";
      logArgs[1] += "color: orange; border:2px solid orange ";
      $(args).splice(isWarnIndex, 1);
    }
    else if (isSuccessIndex !== -1) {
      logArgs[0] += " - SUCCESS";
      logArgs[1] += "color: green; border:2px solid green";
      $(args).splice(isSuccessIndex, 1);
    }
    else if (isInfoIndex !== -1) {
      logArgs[1] += "color: black;";
      $(args).splice(isInfoIndex, 1);
    }

    $(args).unshift(...logArgs);
  }
  console.log(...args);
}

/**
 * Returns a no-op if debugging mode is off, returns a bound log otherwise.
 * @param {string} name the debugger name (first logged value)
 * @returns {function} either a no-op function or the logger one
 */
export function getDebugger(name) {
  return bind(debug() ? log : noop, null, name);
}
