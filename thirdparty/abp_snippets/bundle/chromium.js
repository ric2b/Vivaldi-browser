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

import {readFileSync, writeFile, rmSync} from "node:fs";
import {join, dirname} from "node:path";
import {fileURLToPath} from "node:url";
import process from "node:process";
import {withLicense} from "./utils.js";

const noLicense = src => src.toString().replace(withLicense(), "");
const noModule = src => src.toString().replace(/export.+callback;\s*$/, "");

const __dirname = dirname(fileURLToPath(import.meta.url));
const module = join(__dirname, "..");

const packageJson = JSON.parse(readFileSync(join(module, "package.json")));
const version = packageJson.version;

const createTemplate = (source, buildXPath3) => {
  const debug = source ? ".source" : "";
  const xPath3 = buildXPath3 ? "-all" : "";
  const isolated = noLicense(readFileSync(join(module, "dist", `isolated${xPath3}${debug}.js`)));
  const main = noLicense(noModule(readFileSync(join(module, "webext", `main${debug}.mjs`))));
  const template = `// ! Version: ${version}
(e, ...t) => {
${withLicense().trim()}
  (${isolated})(e, ...t);
  ${main}
  if (t.every(([name]) => !callback.has(name))) return;
  const isTrustedTypesSupported = typeof trustedTypes !== 'undefined';

  let policy;
  if (isTrustedTypesSupported) {
    try {
      const id = Math.floor(Math.random() * 2116316160 + 60466176).toString(36);
      policy = trustedTypes.createPolicy(id, {
        createScript: (code) => code,
        createScriptURL: (url) => url
      });
    } catch (_) {}
  }
  
  const appendWithTrustedTypes = () => {
    const scriptContent = policy.createScript("(" + callback + ")(..." + JSON.stringify([e, ...t]) + ")");
    const blob = new Blob([scriptContent], { type: 'application/javascript' });
    const url = URL.createObjectURL(blob);
    
    const script = document.createElement('script');
    script.async = false;
    script.src = policy.createScriptURL(url);
    
    document.documentElement.appendChild(script);
    
    URL.revokeObjectURL(url);
  };
  
  const appendOriginal = () => {
    URL.revokeObjectURL(
      Object.assign(
        document.documentElement.appendChild(document.createElement("script")),
        {async: false, src: URL.createObjectURL(new Blob([
          "(" + callback + ")(..." + JSON.stringify([e, ...t]) + ")"
        ]))}
      ).src
    );
  };

  const evalScript = () => {
    const scriptContent =  "(" + callback + ")(..." + JSON.stringify([e, ...t]) + ")";
    
    if (policy) {
      const trustedScript = policy.createScript(scriptContent);
      eval(trustedScript);
    } else {
      eval(scriptContent);
    }
  };

  const appendScript = () => {
    if (policy) {
      appendWithTrustedTypes();
    } else {
      appendOriginal();
    }
  };

  const useEval = !(t.every(([name]) => name !== "use-eval"));
  const executeScript = useEval ? evalScript : appendScript;

  try { 
    executeScript(); }
  catch (_) {
    document.addEventListener("readystatechange", executeScript, {once:true});
  }
}`;

  writeFile(
    join(module, "dist", `isolated-first${xPath3}${debug}.jst`),
    template,
    error => {
      if (error)
        process.exit(1);
    }
  );

  if (buildXPath3) {
    try {
      rmSync(join(module, "dist", `isolated-all${debug}.js`), {force: true});
    }
    catch (_) {}
  }
};

createTemplate(false, false);
createTemplate(true, false);
createTemplate(false, true);
createTemplate(true, true);