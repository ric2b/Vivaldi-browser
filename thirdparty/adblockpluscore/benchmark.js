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
/* eslint no-console: "off" */

/* global gc */

"use strict";

const https = require("https");

const {filterEngine} = require("./lib/filterEngine");

const EASY_LIST = "https://easylist-downloads.adblockplus.org/easylist.txt";
const AA = "https://easylist-downloads.adblockplus.org/exceptionrules.txt";

function sliceString(str)
{
  // Create a new string in V8 to free up the parent string.
  return JSON.parse(JSON.stringify(str));
}

function download(url)
{
  return new Promise((resolve, reject) =>
  {
    let request = https.request(url);

    request.on("error", reject);
    request.on("response", response =>
    {
      let {statusCode} = response;
      if (statusCode != 200)
      {
        reject(`Download failed for ${url} with status ${statusCode}`);
        return;
      }

      let body = "";

      response.on("data", data => body += data);
      response.on("end", () => resolve(body));
    });

    request.end();
  });
}

function toMiB(numBytes)
{
  return new Intl.NumberFormat().format(numBytes / 1024 / 1024);
}

function printMemory()
{
  gc();

  let {heapUsed, heapTotal} = process.memoryUsage();

  console.log(`Heap (used): ${toMiB(heapUsed)} MiB`);
  console.log(`Heap (total): ${toMiB(heapTotal)} MiB`);

  console.log();
}

async function main()
{
  let lists = [];

  switch (process.argv[2])
  {
    case "EasyList":
      lists.push(EASY_LIST);
      break;
    case "EasyList+AA":
      lists.push(EASY_LIST);
      lists.push(AA);
      break;
  }

  let filters = [];

  if (lists.length > 0)
  {
    for (let list of lists)
    {
      console.debug(`Downloading ${list} ...`);

      let content = await download(list);
      filters = filters.concat(content.split(/\r?\n/).map(sliceString));
    }

    console.debug();
  }

  if (filters.length > 0)
  {
    console.log("# " + process.argv[2]);
    console.log();

    console.time("Initialization");
    await filterEngine.initialize(filters);
    console.timeEnd("Initialization");
    console.log();

    // Call printMemory() asynchronously so GC can clean up any objects from
    // here.
    setTimeout(printMemory, 1000);
  }
}

if (require.main == module)
  main();
