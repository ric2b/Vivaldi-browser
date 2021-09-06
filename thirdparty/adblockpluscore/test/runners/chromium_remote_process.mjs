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

import childProcess from "child_process";
import fs from "fs";
import os from "os";
import path from "path";

import remoteInterface from "chrome-remote-interface";

import {ensureChromium} from "./chromium_download.mjs";

// Chromium 60.0.3082.x
const CHROMIUM_REVISION = 467222;

function rmdir(dirPath)
{
  for (let file of fs.readdirSync(dirPath))
  {
    let filePath = path.join(dirPath, file);
    try
    {
      if (fs.statSync(filePath).isDirectory())
        rmdir(filePath);
      else
        fs.unlinkSync(filePath);
    }
    catch (error)
    {
      console.error(error);
    }
  }

  try
  {
    fs.rmdirSync(dirPath);
  }
  catch (error)
  {
    console.error(error);
  }
}

function startChromium(chromiumPath)
{
  fs.chmodSync(chromiumPath, fs.constants.S_IRWXU);

  let dataDir = fs.mkdtempSync(path.join(os.tmpdir(), "chromium-data"));
  let child = null;
  return {
    kill: () => child && child.kill(),
    done: new Promise((resolve, reject) =>
    {
      child = childProcess.execFile(chromiumPath, [
        "--headless", "--disable-gpu", "--no-sandbox",
        "--allow-file-access-from-files", "--remote-debugging-port=9222",
        "--user-data-dir=" + dataDir
      ], error =>
      {
        rmdir(dataDir);
        if (error)
          reject(error);
        else
          resolve();
      });
    })
  };
}

function throwException(details, url)
{
  let text = details.exception ? details.exception.description : details.text;
  if (!details.stackTrace)
  {
    // ExceptionDetails uses zero-based line and column numbers.
    text += `\n    at ${details.url || url}:` +
            (details.lineNumber + 1) + ":" +
            (details.columnNumber + 1);
  }
  throw text;
}

function reportMessage(text, level)
{
  let method = {
    log: "log",
    warning: "warn",
    error: "error",
    debug: "log",
    info: "info"
  }[level] || "log";
  console[method](text);
}

function connectRemoteInterface(attempt)
{
  // We use a Chrome version that is too old and doesn't support the
  // protocal command, so we  have to pass the option `local`
  // https://www.npmjs.com/package/chrome-remote-interface#chrome-debugging-protocol-versions
  return remoteInterface({local: true}).catch(error =>
  {
    attempt = attempt || 1;
    if (attempt > 50)
    {
      // Stop trying to connect after 10 seconds
      throw error;
    }

    return new Promise((resolve, reject) =>
    {
      setTimeout(() =>
      {
        connectRemoteInterface(attempt + 1).then(resolve).catch(reject);
      }, 200);
    });
  });
}

function runScript(script, scriptName, scriptArgs)
{
  return connectRemoteInterface().then(async client =>
  {
    try
    {
      let {Runtime, Log} = client;

      await Log.enable();
      Log.entryAdded(({entry}) =>
      {
        reportMessage(entry.text, entry.level);
      });

      await Runtime.enable();
      let compileResult = await Runtime.compileScript({
        expression: script,
        sourceURL: scriptName,
        persistScript: true
      });
      if (compileResult.exceptionDetails)
        throwException(compileResult.exceptionDetails, scriptName);

      let runResult = await Runtime.runScript({
        scriptId: compileResult.scriptId
      });
      if (runResult.exceptionDetails)
        throwException(runResult.exceptionDetails, scriptName);

      let callResult = await Runtime.callFunctionOn({
        objectId: runResult.result.objectId,
        functionDeclaration: "function(...args) { return this(...args); }",
        arguments: scriptArgs.map(arg => ({value: arg}))
      });
      if (callResult.exceptionDetails)
        throwException(callResult.exceptionDetails, scriptName);

      let promiseResult = await Runtime.awaitPromise({
        promiseObjectId: callResult.result.objectId
      });
      if (promiseResult.exceptionDetails)
        throwException(promiseResult.exceptionDetails, scriptName);

      let result = await Runtime.evaluate({
        expression: "window._consoleLogs",
        returnByValue: true
      });

      console.log("\nBrowser tests in Chromium (Remote Interface)");
      for (let item of result.result.value.log)
        console.log(...item);

      if (result.result.value.failures != 0)
        throw "Chromium (Remote Interface)";
    }
    finally
    {
      client.close();
    }
  });
}

export default function(script, scriptName, ...scriptArgs)
{
  return ensureChromium(CHROMIUM_REVISION).then(chromiumPath =>
  {
    let child = startChromium(chromiumPath);
    return Promise.race([
      child.done,
      runScript(script, scriptName, scriptArgs)
    ]).then(result =>
    {
      child.kill();
      return result;
    }).catch(error =>
    {
      child.kill();
      throw error;
    });
  });
}
