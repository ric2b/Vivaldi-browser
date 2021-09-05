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

/* eslint-env node */

import {spawn} from "child_process";
import fs from "fs";
import path from "path";
import {fileURLToPath} from "url";
import {promisify} from "util";

import MemoryFS from "memory-fs";
import webpack from "webpack";

import chromiumRemoteProcess from "./test/runners/chromium_remote_process.mjs";
import chromiumProcess from "./test/runners/chromium_process.mjs";
import firefoxProcess from "./test/runners/firefox_process.mjs";

let dirname = path.dirname(fileURLToPath(import.meta.url));

let unitFiles = [];
let browserFiles = [];

let runnerDefinitions = {
  // Chromium with chrome-remote-interface
  chromium_remote: chromiumRemoteProcess,
  // Chromium with WebDriver (requires Chromium >= 63.0.3239)
  chromium: chromiumProcess,
  firefox: firefoxProcess
};

function configureRunners()
{
  let runners = "BROWSER_TEST_RUNNERS" in process.env ?
                  process.env.BROWSER_TEST_RUNNERS.split(",") :
                  [];

  if (runners.length == 0)
  {
    // We default to not using the Chromium remote interface on Windows,
    // as it fails.
    if (process.platform == "win32")
      return ["chromium", "firefox"];
    return ["chromium_remote", "firefox"];
  }

  return runners.filter(
    runner => Object.prototype.hasOwnProperty.call(runnerDefinitions, runner)
  );
}

let runnerProcesses = configureRunners();

function addTestPaths(testPaths, recurse)
{
  for (let testPath of testPaths)
  {
    let stat = fs.statSync(testPath);
    if (stat.isDirectory())
    {
      if (recurse)
      {
        addTestPaths(fs.readdirSync(testPath).map(
          file => path.join(testPath, file))
        );
      }
      continue;
    }
    if (path.basename(testPath).startsWith("_"))
      continue;
    if (path.extname(testPath) == ".js")
    {
      if (testPath.split(path.sep).includes("browser"))
        browserFiles.push(testPath);
      else
        unitFiles.push(testPath);
    }
  }
}

async function webpackInMemory(bundleFilename, options)
{
  // Based on this example
  // https://webpack.js.org/api/node/#custom-file-systems
  let memoryFS = new MemoryFS();

  options.output = {filename: bundleFilename, path: "/"};
  options.devtool = "cheap-eval-source-map";
  let webpackCompiler = webpack(options);
  webpackCompiler.outputFileSystem = memoryFS;

  let stats = null;

  try
  {
    stats = await promisify(webpackCompiler.run).call(webpackCompiler);
  }
  catch (error)
  {
    // Error handling is based on this example
    // https://webpack.js.org/api/node/#error-handling
    let reason = error.stack || error;
    if (error.details)
      reason += "\n" + error.details;
    throw reason;
  }

  if (stats.hasErrors())
    throw stats.toJson().errors;

  let bundle = memoryFS.readFileSync("/" + bundleFilename, "utf-8");
  memoryFS.unlinkSync("/" + bundleFilename);
  return bundle;
}

async function runBrowserTests(processes)
{
  if (browserFiles.length == 0)
    return;

  let bundleFilename = "bundle.js";
  let mochaPath = path.join(dirname, "node_modules", "mocha",
                            "mocha.js");
  let chaiPath = path.join(dirname, "node_modules", "chai", "chai.js");

  let bundle = await webpackInMemory(bundleFilename, {
    entry: path.join(dirname, "test", "browser", "_bootstrap.js"),
    module: {
      rules: [
        {
          // we use the browser version of mocha
          resource: mochaPath,
          use: ["script-loader"]
        },
        {
          resource: chaiPath,
          use: ["script-loader"]
        }
      ]
    },
    resolve: {
      alias: {
        mocha$: mochaPath,
        chai$: chaiPath
      },
      modules: [path.resolve(dirname, "lib")]
    },
    optimization:
    {
      minimize: false
    }
  });

  let errors = [];

  await Promise.all(
    processes.map(
      currentProcess => runnerDefinitions[currentProcess](
        bundle, bundleFilename,
        browserFiles.map(
          file => path.relative(path.join(dirname, "test", "browser"), file)
                  .replace(/\.js$/, "")
        )
      )
      // We need to convert rejected promise to a resolved one
      // or the test will not let close the webdriver.
      .catch(error =>
      {
        errors.push(error);
      })
    )
  );

  if (errors.length > 0)
    throw `Browser tests failed: ${errors.join(", ")}`;
}

function spawnScript(name, ...args)
{
  return new Promise((resolve, reject) =>
  {
    let script = spawn("npm",
                       ["run", name, ...args],
                       {stdio: ["inherit", "inherit", "inherit"]});
    script.on("error", reject);
    script.on("close", code =>
    {
      if (code == 0)
        resolve();
      else
        reject();
    });
  });
}

(async function()
{
  let paths = process.argv.length > 2 ? process.argv.slice(2) :
                [path.join(dirname, "test"),
                 path.join(dirname, "test", "browser")];
  addTestPaths(paths, true);

  try
  {
    await runBrowserTests(runnerProcesses);

    if (unitFiles.length > 0)
      await spawnScript("unit-tests", ...unitFiles);
  }
  catch (error)
  {
    if (error)
      console.error(error);

    process.exit(1);
  }
})();
