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

import fs from "fs";
import path from "path";
import {fileURLToPath} from "url";

import {download, unzipArchive} from "./download.mjs";

const MAX_VERSION_DECREMENTS = 200;

let dirname = path.dirname(fileURLToPath(import.meta.url));

function getChromiumExecutable(chromiumDir)
{
  switch (process.platform)
  {
    case "win32":
      return path.join(chromiumDir, "chrome-win32", "chrome.exe");
    case "linux":
      return path.join(chromiumDir, "chrome-linux", "chrome");
    case "darwin":
      return path.join(chromiumDir, "chrome-mac", "Chromium.app", "Contents",
                       "MacOS", "Chromium");
    default:
      throw new Error("Unexpected platform");
  }
}

export async function ensureChromium(chromiumRevision, unpack = true)
{
  let revisionInt = parseInt(chromiumRevision, 10);
  let startingRevision = revisionInt;
  let {platform} = process;
  if (platform == "win32")
    platform += "-" + process.arch;
  let buildTypes = {
    "win32-ia32": ["Win", "chrome-win32.zip"],
    "win32-x64": ["Win_x64", "chrome-win32.zip"],
    "linux": ["Linux_x64", "chrome-linux.zip"],
    "darwin": ["Mac", "chrome-mac.zip"]
  };

  if (!Object.prototype.hasOwnProperty.call(buildTypes, platform))
    throw new Error(`Cannot run browser tests, ${platform} is unsupported`);

  let [dir, fileName] = buildTypes[platform];
  let archive = null;
  let chromiumDir = null;
  let snapshotsDir = path.join(dirname, "..", "..", "chromium-snapshots");

  while (true)
  {
    chromiumDir = path.join(snapshotsDir,
                            `chromium-${platform}-${revisionInt}`);
    if (fs.existsSync(chromiumDir))
    {
      console.info(`Reusing cached executable in ${chromiumDir}`);
      return getChromiumExecutable(chromiumDir);
    }

    if (!fs.existsSync(path.dirname(chromiumDir)))
      fs.mkdirSync(path.dirname(chromiumDir));

    archive = path.join(snapshotsDir, "download-cache",
                            `${revisionInt}-${fileName}`);

    try
    {
      if (!fs.existsSync(archive))
      {
        console.info("Downloading Chromium...");
        await download(
          `https://www.googleapis.com/download/storage/v1/b/chromium-browser-snapshots/o/${dir}%2F${revisionInt}%2F${fileName}?alt=media`,
          archive);
      }
      else
      {
        console.info(`Reusing cached archive ${archive}`);
      }
      break;
    }
    catch (e)
    {
      // The Chromium authors advise us to try decrementing
      // the branch_base_position when no matching build was found. See
      // https://www.chromium.org/getting-involved/download-chromium
      console.info(`Base ${revisionInt} not found, trying ${--revisionInt}`);
      if (revisionInt <= startingRevision - MAX_VERSION_DECREMENTS)
        throw new Error(`No Chromium package found for ${startingRevision}`);
    }
  }

  if (!unpack)
    return null;

  await unzipArchive(archive, chromiumDir);
  return getChromiumExecutable(chromiumDir);
}
