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

import {mkdirSync, readdirSync, readFileSync, rmdirSync, statSync, writeFileSync} from "node:fs";
import {join, resolve} from "node:path";
import {argv, exit} from "node:process";

import {withLicense} from "./utils.js";

const options = {force: true, recursive: true};

const copySync = (source, dest) => {
  for (const file of readdirSync(source)) {
    const fs = join(source, file);
    const fd = join(dest, file);
    if (statSync(fs).isDirectory()) {
      mkdirSync(fd, options);
      copySync(fs, fd);
    }
    else {
      const content = readFileSync(fs);
      writeFileSync(fd, withLicense() + "\n" + content);
    }
  }
};

if (argv.length < 3) {
  console.error("🛑 \x1b[1mWARNING\x1b[0m: missing snippets source folder!\n");
  exit(1);
}

const source = resolve(argv[2]);
try {
  if (!statSync(source).isDirectory())
    throw new Error();
}
catch (_) {
  console.error("Invalid snippets source folder");
  exit(1);
}

const dest = resolve("./source");
try { rmdirSync(dest, {force: true}); } catch (_) {}
mkdirSync(dest, options);
copySync(source, dest);
