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

export async function executeScript(driver, name, script, scriptArgs)
{
  let realScript = `let f = ${script}
                    let callback = arguments[arguments.length - 1];
                    return f(...arguments).then(() => callback());`;
  try
  {
    await driver.manage().setTimeouts({script: 100000});
    await driver.executeAsyncScript(realScript, ...scriptArgs);
    let result = await driver.executeScript("return window._consoleLogs;");

    console.log(`\nBrowser tests in ${name}`);
    for (let item of result.log)
      console.log(...item);

    if (result.failures != 0)
      throw name;
  }
  finally
  {
    await driver.quit();
  }
}
