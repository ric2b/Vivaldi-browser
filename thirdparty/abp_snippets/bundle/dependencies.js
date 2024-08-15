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

import {readFileSync, writeFile} from "node:fs";
import {join, dirname} from "node:path";
import {fileURLToPath} from "node:url";
import process from "node:process";

import {withLicense} from "./utils.js";

/* eslint-env node */
/* eslint no-new-func: "off" */
/* eslint no-undef: "off" */

const noLicense = src => src.toString().replace(withLicense(), "");
const noModule = src => src.toString().replace(/export/, "");

const __dirname = dirname(fileURLToPath(import.meta.url));
const module = join(__dirname, "..");

const xPath3Dependency = noLicense(noModule(readFileSync(join(module, "source", "conditional-hiding", "hide-if-matches-xpath3-dependency.js"))));
const template = `${withLicense().trim()}
/**! Start hide-if-matches-xpath3 dependency !**/
${xPath3Dependency}
/**! End hide-if-matches-xpath3 dependency !**/`;

writeFile(
  join(module, "dist", `dependencies.jst`),
  template,
  error => {
    if (error)
      process.exit(1);
  }
);