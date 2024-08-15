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

import {readFileSync} from "node:fs";
import {createRequire} from "node:module";
import {join, dirname} from "node:path";
import {fileURLToPath} from "node:url";

import {setup, teardown} from "./browser.js";
import {assert} from "./utils.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
const dist = join(__dirname, "..", "dist");

// for a nice looking result in console
const pad = (message, file) => message.padEnd(40, " ") + "\x1b[0m" + file;

const test = file => {
  const callback = Function(`return ${readFileSync(join(dist, file))}`)();
  const invokes = [];
  const {log} = console;
  console.log = (...args) => {
    invokes.push(args);
  };
  setup();
  callback({}, ["log", "LOG"], ["trace", "TRACE"]);
  assert(invokes.length === 2, pad("unexpected invokes length", file));
  assert(
    invokes[0].length === 1 &&
    invokes[0][0] === "LOG",
    pad("unexpected ISOLATED world behavior", file)
  );
  assert(
    invokes[1].length === 1 &&
    invokes[1][0] === "TRACE",
    pad("unexpected MAIN world behavior", file)
  );
  teardown();
  console.log = log;
};

test("snippets.js");
test("isolated-first.jst");
