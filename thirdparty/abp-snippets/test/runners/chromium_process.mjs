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

import webdriver from "selenium-webdriver";
import chrome from "selenium-webdriver/chrome.js";
import "chromedriver";

import {executeScript} from "./webdriver.mjs";
import {ensureChromium} from "./chromium_download.mjs";

// The Chromium version is a build number, quite obscure.
// Chromium 63.0.3239.x is 508578
// Chromium 65.0.3325.0 is 530368
// We currently want Chromiun 63, as we still support it and that's the
// loweset version that supports WebDriver.
const CHROMIUM_REVISION = 508578;

function runScript(chromiumPath, script, scriptArgs)
{
  const options = new chrome.Options()
        // Disabling sandboxing is needed on some system configurations
        // like Debian 9.
        .addArguments("--no-sandbox")
        .setChromeBinaryPath(chromiumPath);
  // Headless doesn't seem to work on Windows.
  if (process.platform != "win32" &&
      process.env.BROWSER_TEST_HEADLESS != "0")
    options.headless();

  const driver = new webdriver.Builder()
        .forBrowser("chrome")
        .setChromeOptions(options)
        .build();

  return executeScript(driver, "Chromium (WebDriver)", script, scriptArgs);
}

export default function(script, scriptName, ...scriptArgs)
{
  return ensureChromium(CHROMIUM_REVISION)
    .then(chromiumPath => runScript(chromiumPath, script, scriptArgs));
}
