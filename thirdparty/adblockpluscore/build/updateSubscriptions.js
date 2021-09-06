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

const {promises: {writeFile}} = require("fs");
const path = require("path");
const {promisify} = require("util");
const readline = require("readline");

const got = require("got");
const untarToMemory = require("untar-memory");

const root = "/subscriptionlist-default";
const listUrl = "https://hg.adblockplus.org/subscriptionlist/archive/" +
                "default.tar.gz";

const filename = "data/subscriptions.json";
const resultingKeys = new Set([
  "title",
  "url",
  "homepage",
  "languages",
  "type"
]);

function parseSubscriptionFile(tarfs, file, validLanguages)
{
  // Bypass parsing remaining lines in the ReadStream's buffer
  let continuing = true;

  return new Promise(resolve =>
  {
    let parsed = {
      name: path.basename(file).replace(/\.\w+$/, "")
    };

    let reader = readline.createInterface({
      input: tarfs.createReadStream(file, {encoding: "utf8"})
    });

    reader.on("line", line =>
    {
      if (!line.match(/\S/g) || !continuing)
        return;

      let [key, value] = line.split("=", 2);
      key = key.trim();

      if (key == "unavailable" || key == "deprecated")
      {
        parsed = null;
        reader.close();
        continuing = false;
        return;
      }

      if (value)
        value = value.trim();

      if (value == "")
        console.warn(`Empty value given for attribute ${key} in ${file}`);

      if (key != "name" && key in parsed)
        console.warn(`Value for attribute ${key} is duplicated in ${file}`);

      if (key == "supplements")
      {
        if (!("supplements" in parsed))
          parsed["supplements"] = [];
        parsed["supplements"].push(value);
      }
      else if (key == "list" || key == "variant")
      {
        let name = parsed["name"];
        let url = null;
        let keywords = {
          recommendation: false,
          complete: false
        };

        let keywordsRegex = /\s*\[((?:\w+,)*\w+)\]$/;
        let variantRegex = /(.+?)\s+(\S+)$/;

        let keywordsMatch = value.match(keywordsRegex);
        if (keywordsMatch)
        {
          value = value.replace(keywordsRegex, "");
          url = value;

          for (let keyword of keywordsMatch[1].split(","))
          {
            keyword = keyword.toLowerCase();
            if (keyword in keywords)
              keywords[keyword] = true;
          }
        }

        if (key == "variant")
        {
          let variantMatch = value.match(variantRegex);
          if (variantMatch)
          {
            name = variantMatch[1];
            url = variantMatch[2];
          }
          else
          {
            console.warn(`Invalid variant format in ${file}, no name` +
                         " given?");
          }
        }
        if (!("variants" in parsed))
          parsed["variants"] = [];
        parsed["variants"].push([name, url, keywords["complete"]]);

        if (keywords["recommendation"])
        {
          parsed["title"] = name;
          parsed["url"] = url;
        }
      }
      else if (key == "languages")
      {
        parsed["languages"] = value.split(",");
        for (let language of parsed["languages"])
        {
          if (!validLanguages.has(language))
            console.warn(`Unknown language code ${language} in ${file}`);
        }
      }
      else
      {
        parsed[key] = value;
      }
    });

    reader.on("close", () =>
    {
      if (!parsed)
      {
        resolve(parsed);
        return;
      }

      if (parsed["variants"].length == 0)
        console.warn(`No list locations given in ${file}`);

      if ("title" in parsed && parsed["type"] == "ads" &&
          parsed["languages"] == null)
        console.warn(`Recommendation without languages in ${file}`);

      if (!("supplements" in parsed))
      {
        for (let variant of parsed["variants"])
        {
          if (variant[2])
          {
            console.warn("Variant marked as complete for non-supplemental " +
                         `subscription in ${file}`);
          }
        }
      }

      resolve(parsed);
    });
  });
}

function parseValidLanguages(tarfs)
{
  return new Promise(resolve =>
  {
    let languageRegex = /(\S{2})=(.*)/;
    let languages = new Set();

    let reader = readline.createInterface({
      input: tarfs.createReadStream(root + "/settings", {encoding: "utf8"})
    });

    reader.on("line", line =>
    {
      let match = line.match(languageRegex);
      if (match)
        languages.add(match[1]);
    });

    reader.on("close", () => resolve(languages));
  });
}

function postProcessSubscription(subscription)
{
  subscription["homepage"] = subscription["homepage"] ||
                             subscription["forum"] ||
                             subscription["blog"] ||
                             subscription["faq"] ||
                             subscription["contact"];

  for (let key in subscription)
  {
    if (!resultingKeys.has(key))
      delete subscription[key];
  }
}

async function main()
{
  let tarfs = await untarToMemory(got.stream(listUrl));
  let languages = await parseValidLanguages(tarfs);
  let tarfiles = await promisify(tarfs.readdir.bind(tarfs))(root);

  let parsed = await Promise.all(
    tarfiles.filter(file => file.match(".subscription")).map(
      file => parseSubscriptionFile(tarfs, root + "/" + file, languages)
    )
  );

  parsed = parsed.filter(subscription =>
    subscription != null && "title" in subscription
  );

  for (let subscription of parsed)
    postProcessSubscription(subscription);

  parsed.sort((a, b) =>
    a["type"].toLowerCase().localeCompare(b["type"]) ||
    a["title"].localeCompare(b["title"])
  );
  await writeFile(filename, JSON.stringify(parsed, null, 2), "utf8");
}

if (require.main == module)
  main();
