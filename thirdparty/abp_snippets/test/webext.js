/**
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

import {setup, teardown} from "./browser.js";

import main from "../webext/main.mjs";
import isolated from "../webext/ml.mjs";
import {assert} from "./utils.js";

const {log} = console;

const invokes = [];

console.log = (...args) => {
  invokes.push(args);
};

setup();
main({}, ["log", "LOG"], ["trace", "TRACE"]);
teardown();

assert(
  invokes.length === 1 &&
  invokes[0].length === 1 &&
  invokes[0][0] === "TRACE",
  "unexpected MAIN world behavior"
);

setup();
isolated({}, ["log", "LOG"], ["trace", "TRACE"]);
teardown();

assert(
  typeof isolated.get("hide-if-matches-xpath3") === "function",
  "XPath3 should have dependencies"
);

assert(
  invokes.length === 2 &&
  invokes[1].length === 1 &&
  invokes[1][0] === "LOG",
  "unexpected ISOLATED world behavior"
);

console.log = log;
