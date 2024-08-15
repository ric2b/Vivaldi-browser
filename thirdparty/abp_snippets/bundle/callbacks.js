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

import {readdirSync, readFileSync, writeFile} from "node:fs";
import {join, dirname} from "node:path";
import {fileURLToPath} from "node:url";
import process from "node:process";

import {getSnippets, withLicense, withoutLicense} from "./utils.js";

const noStrict = str => str.replace(/(['"])use strict\1;/, '');

const __dirname = dirname(fileURLToPath(import.meta.url));

const dist = join(__dirname, "..", "dist");
for (const file of readdirSync(dist)) {
  if (/\.js$/.test(file)) {
    const compressed = !/\.source\.js$/.test(file);
    const content = noStrict(readFileSync(join(dist, file)).toString());
    const {snippets, dependencies = {}} = getSnippets(content);
    if (!snippets) {
      console.warn("This is likely CI failing. If not, please fix this!");
      console.warn(`File: ${join(dist, file)}`);
      continue;
    }

    // This is mostly just a visual confirmation everything is OK
    if (compressed)
      console.log(file, Object.keys(snippets));

    let callback = (
      withLicense() + content
        // make exports constants instead
        .replace(
          /(,|\b)exports\.([^=]+?)=/g,
          (_, $1, $2) => `${$1 === ',' ? ';' : $1}\nconst ${$2}=`
        )
        // avoid re-declaration as in const snippets = snippets;
        .replace(
          /const\s+(\S+?)\s*=\s*\b\1\b\s*[;\r\n]+/g,
          (_, ref) => {
            // guard against unexpected exports
            if (!/^(?:snippets|dependencies)$/.test(ref))
              throw new Error(`unexpected redeclaration ${ref}`);
            return '';
          }
        )
        .trim()
      )
      // better indentation for non minified code
      .replace(/^(.+)/gm, (_, c) => c.trim().length ? ('  ' + c) : '');

    callback = `(environment, ...filters) => {
${callback}
  let context;
  for (const [name, ...args] of filters) {
    if (snippets.hasOwnProperty(name)) {
      try { context = snippets[name].apply(context, args); }
      catch (error) { console.error(error); }
    }
  }
  context = void 0;
}`;

    if (compressed)
      callback = callback.replace(/^\s+(?!\*)/mg, '');

    const graph = [];

    let webextCallback = callback;
    let alreadyRemovedDep = false;
    for (const k of Object.keys(snippets)) {
      let dependency = 'null';
      if (dependencies[k]) {
        dependency = dependencies[k];

        // Following logic is added to prevent the dependency function from being duplicated,
        // reducing the artifact size significantly.

        if (compressed && !alreadyRemovedDep) {
          // We need to remove a portion that looks like this:
          // const xyz={'snippet1':giantDep1, 'snippet2': giantDep2};
          // const dependencies=xyz;

          // The 'xyz' portion changes from build to build so we need to find it first.
          const regexp = /const dependencies=(.{1,5}?);/g;
          const matches = webextCallback.matchAll(regexp);
          const matchesArr = [...matches];
          // This is the variable name we are looking for
          const dependencyVarName = matchesArr.map(m => m[1]).toString();
          if (dependencyVarName.length) {
            // Remove anything that matches:
            // const xyz=--anything--const dependencies=xyz;
            let finalEscapedDepStr = `const ${dependencyVarName}=([\\S\\s]*?)const dependencies=${dependencyVarName};`;
            const finalEscapedDepRegex = new RegExp(finalEscapedDepStr, "g");
            webextCallback = webextCallback.replace(finalEscapedDepRegex, '');
            alreadyRemovedDep = true;
          }
        }
        else {
          // For the non-minified version we need to remove all dependencies one by one.

          // Get the beginning and the end characters of the dependency function.
          const firstSubstr = `${dependency}`.slice(0, 30);
          const lastSubstr = `${dependency}`.slice(-30);

          // Get the first characters of the dependency and escape them to use in a regex
          let firstSubstrEscaped = firstSubstr.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
          firstSubstrEscaped = firstSubstrEscaped.replace(/\s+/g, '\\s\+');

          // Get the last characters of the dependency and escape them to use in a regex
          let lastSubstrEscaped = lastSubstr.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
          lastSubstrEscaped = lastSubstrEscaped.replace(/\s+/g, '\\s\+');

          // Match everything between the first substring and last substring (whole dependency)
          let finalEscapedDepStr = `${firstSubstrEscaped}([\\S\\s]*?)${lastSubstrEscaped}`;
          const finalEscapedDepRegex = new RegExp(finalEscapedDepStr, "g");

          // Remove the dependency from the artifact to avoid duplicating.
          webextCallback = webextCallback.replace(finalEscapedDepRegex, '');
        }
      }
      graph.push(`[${JSON.stringify(k)},${dependency}]`);
    }

    // Remove the dependencies object as it is unnecessary and contains
    // references to the deleted dependencies
    webextCallback = webextCallback.replace(/const dependencies([\S\s]*?)\;/, '');

    let output = withLicense() + `
const callback = ${withoutLicense(webextCallback)};
const graph = new Map([${graph.join(',')}]);
callback.get = snippet => graph.get(snippet);
callback.has = snippet => graph.has(snippet);
export default callback;`;

    writeFile(
      join(dist, file),
      callback,
      (error) => {
        if (error)
          process.exit(1);
      }
    );
    if (!(/all/.test(file))) {
      writeFile(
        join(dist, '..', 'webext', file.replace(/\.js$/, '.mjs')),
        output,
        (error) => {
          if (error)
            process.exit(1);
        }
      );
    }
  }
}
