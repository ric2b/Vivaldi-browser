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

import {existsSync, mkdirSync, readdirSync, copyFile} from "node:fs";
import {join, dirname} from "node:path";
import {fileURLToPath} from "node:url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const serviceDist = join(__dirname, "..", "dist", "service");
const serviceSource = join(__dirname, "..", "source", "service");

console.log("Bundling service files.");

if (!existsSync(serviceDist)) {
  console.log(`Creating folder ${serviceDist}`);
  mkdirSync(serviceDist, {recursive: true});
}

for (const file of readdirSync(serviceSource)) {
  const from = join(serviceSource, file);
  const to = join(serviceDist, file);
  console.log(`Copying ${file}`);
  copyFile(from, to, err => {
    if (err) {
      console.log(`Can't write file ${to}`);
      process.exit(1);
    }
  });
}
