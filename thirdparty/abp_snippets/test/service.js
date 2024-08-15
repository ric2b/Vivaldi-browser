/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */


import {join, dirname} from "node:path";
import {fileURLToPath} from "node:url";

import {setup, teardown} from "./browser.js";
import {assert} from "./utils.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
const dist = join(__dirname, "..", "dist", "service");

// for a nice looking result in console
const pad = (message, file) => message.padEnd(40, " ") + "\x1b[0m" + file;

const test = async file => {
  setup();
  const {warn} = console;
  console.warn = (...args) => { /* ignore TFJS warnings */ };
  let module = await import(join(dist, file));
  console.warn = warn;
  assert(
    "service" in module &&
    "messageListener" in module.service,
    pad("unexpected service behavior", file)
  );
  teardown();
};

await test("mlaf.mjs");
await test("mlaf.source.mjs");
