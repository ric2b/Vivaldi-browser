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

"use strict";

const fs = require("fs");
const {Module} = require("module");
const path = require("path");

// Load sandboxed-module dynamically instead of using require() so it does not
// have a module.parent
// https://gitlab.com/eyeo/adblockplus/adblockpluscore/-/issues/192
const SandboxedModule = dynamicRequire("sandboxed-module");

const {MILLIS_IN_HOUR} = require("../lib/time");

let globals = {
  atob: data => Buffer.from(data, "base64").toString("binary"),
  btoa: data => Buffer.from(data, "binary").toString("base64"),
  console: {
    log() {},
    warn() {},
    error() {}
  },
  navigator: {
  },
  URL
};

let knownModules = new Map();
for (let dir of [path.join(__dirname, "stub-modules"),
                 path.join(__dirname, "..", "lib")])
{
  for (let file of fs.readdirSync(path.resolve(dir)))
  {
    if (path.extname(file) == ".js")
      knownModules.set(path.basename(file, ".js"), path.resolve(dir, file));
  }
}

function dynamicRequire(id)
{
  let module = new Module(id);
  module.load(require.resolve(id));
  return module.exports;
}

function addExports(exports)
{
  return function(source)
  {
    let extraExports = exports[path.basename(this.filename, ".js")];
    if (extraExports)
    {
      for (let name of extraExports)
      {
        source += `
          Object.defineProperty(exports, "${name}", {get: () => ${name}});`;
      }
    }
    return source;
  };
}

function rewriteRequires(source)
{
  function escapeString(str)
  {
    return str.replace(/(["'\\])/g, "\\$1");
  }

  return source.replace(/(\brequire\(["'])([^"']+)/g, (match, prefix, request) =>
  {
    if (knownModules.has(request))
      return prefix + escapeString(knownModules.get(request));
    return match;
  });
}

exports.createSandbox = function(options)
{
  if (!options)
    options = {};

  let sourceTransformers = [rewriteRequires];
  if (options.extraExports)
    sourceTransformers.push(addExports(options.extraExports));

  // This module loads itself into a sandbox, keeping track of the require
  // function which can be used to load further modules into the sandbox.
  return SandboxedModule.require("./_sandbox.js", {
    globals: Object.assign({}, globals, options.globals),
    sourceTransformers
  }).require;
};

exports.setupTimerAndFetch = function()
{
  let currentTime = 100000 * MILLIS_IN_HOUR;
  let startTime = currentTime;

  let fakeTimer = {
    callback: null,
    delay: -1,
    nextExecution: 0,

    setTimeout(callback, delay)
    {
      // The fake timer implementation is a holdover from the legacy extension.
      // Due to the way it works, it is safer to allow only one callback at a
      // time.
      // https://gitlab.com/eyeo/adblockplus/adblockpluscore/issues/43
      if (this.callback && callback != this.callback)
        throw new Error("Only one timer callback supported");

      this.callback = callback;
      this.delay = delay;
      this.nextExecution = currentTime + delay;
    },

    trigger()
    {
      if (currentTime < this.nextExecution)
        currentTime = this.nextExecution;
      try
      {
        this.callback();
      }
      finally
      {
        this.nextExecution = currentTime + this.delay;
      }
    }
  };

  let requests = [];

  async function fetch(url)
  {
    // Add a dummy resolved promise.
    requests.push(Promise.resolve());

    let urlObj = new URL(url, "https://example.com");

    let result = [404, ""];

    if (urlObj.protocol == "data:")
    {
      let data = decodeURIComponent(urlObj.pathname.replace(/^[^,]+,/, ""));
      result = [200, data];
    }
    else if (urlObj.pathname in fetch.requestHandlers)
    {
      result = fetch.requestHandlers[urlObj.pathname]({
        method: "GET",
        path: urlObj.pathname,
        queryString: urlObj.search.substring(1)
      });
    }

    let [status, text = "", headers = {}] = result;

    if (status == 0)
      throw new Error("Fetch error");

    if (status == 301)
      return fetch(headers["Location"]);

    return {status, url: urlObj.href, text: async() => text};
  }

  fetch.requestHandlers = {};
  this.registerHandler = (requestPath, handler) =>
  {
    fetch.requestHandlers[requestPath] = handler;
  };

  async function waitForRequests()
  {
    if (requests.length == 0)
      return;

    let result = Promise.all(requests);
    requests = [];

    try
    {
      await result;
    }
    catch (error)
    {
      console.error(error);
    }

    await waitForRequests();
  }

  async function runScheduledTasks(maxMillis)
  {
    let endTime = currentTime + maxMillis;

    if (fakeTimer.nextExecution < 0 || fakeTimer.nextExecution > endTime)
    {
      currentTime = endTime;
      return;
    }

    fakeTimer.trigger();

    await waitForRequests();
    await runScheduledTasks(endTime - currentTime);
  }

  this.runScheduledTasks = async(maxHours, initial = 0, skip = 0) =>
  {
    if (typeof maxHours != "number")
      throw new Error("Numerical parameter expected");

    startTime = currentTime;

    if (initial >= 0)
    {
      maxHours -= initial;
      await runScheduledTasks(initial * MILLIS_IN_HOUR);
    }

    if (skip >= 0)
    {
      maxHours -= skip;
      currentTime += skip * MILLIS_IN_HOUR;
    }

    await runScheduledTasks(maxHours * MILLIS_IN_HOUR);
  };

  this.getTimeOffset = () => (currentTime - startTime) / MILLIS_IN_HOUR;
  Object.defineProperty(this, "currentTime", {
    get: () => currentTime
  });

  return {
    setTimeout: fakeTimer.setTimeout.bind(fakeTimer),
    fetch,
    Date: Object.assign(
      function(...args) // eslint-disable-line prefer-arrow-callback
      {
        return new Date(...args);
      },
      {
        now: () => currentTime
      }
    )
  };
};

console.warn = console.log;

exports.setupRandomResult = function()
{
  let randomResult = 0.5;
  Object.defineProperty(this, "randomResult", {
    get()
    {
      return randomResult;
    },
    set(value)
    {
      randomResult = value;
    }
  });

  return {
    Math: Object.create(Math, {
      random: {
        value: () => randomResult
      }
    })
  };
};

exports.unexpectedError = function(error)
{
  console.error(error);
  this.ifError(error);
};
