/*!
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

import $ from "../$.js";
import {accessor} from "proxy-pants/accessor";

import {getDebugger} from "../introspection/log.js";
import {toRegExp} from "../utils/general.js";

let {Error, URL} = $(window);
let {cookie: documentCookies} = accessor(document);

/**
 * Removes a specific cookie by setting it's expiration date in the past.
 * @alias module:content/snippets.cookie-remover
 *
 * @param {string} cookie The name of the cookie that we want removed.
 * If the string begins and ends with a slash (`/`),
 * the text in between is treated as a regular expression.
 * @param {boolean} autoRemoveCookie Optional param with a default value
 * of `false`.
 * When set to `true`, this parameter enables the function to continuously
 * monitor the targeted cookie, checking for its presence every 1000 ms.
 * If the cookie is found, it will be repeatedly removed.
 *
 * @since Adblock Plus 3.11.2
 */
export function cookieRemover(cookie, autoRemoveCookie = false) {
  if (!cookie)
    throw new Error("[cookie-remover snippet]: No cookie to remove.");

  let debugLog = getDebugger("cookie-remover");
  let re = toRegExp(cookie);

  // In some cases, when the snippet is executed, the protocol is about:blank
  // thus preventing us from actually removing the cookie.
  if (!$(/^http|^about/).test(location.protocol)) {
    debugLog("warn", "Snippet only works for http or https and about.");
    return;
  }

  function getCookieMatches() {
    const arr = $(documentCookies()).split(";");
    return arr.filter(str => re.test($(str).split("=")[0]));
  }

  const mainLogic = () => {
    debugLog("info", "Parsing cookies for matches");
    for (const pair of $(getCookieMatches())) {
      let $hostname = $(location.hostname);
      // We need location.hostname to set the cookie's domain attribute.
      // If location.hostname is falsy,
      // we try to get the hostname from Location.ancestorOrigins.
      if (!$hostname &&
        $(location.ancestorOrigins) && $(location.ancestorOrigins[0]))
        $hostname = new URL($(location.ancestorOrigins[0])).hostname;
      const name = $(pair).split("=")[0];
      const expires = "expires=Thu, 01 Jan 1970 00:00:00 GMT";
      const path = "path=/";
      const domainParts = $hostname.split(".");

      // In case location.hostname is a subdomain,
      // we remove the cookie on every higher level as well
      for (let numDomainParts = domainParts.length;
        numDomainParts > 0; numDomainParts--) {
        const domain =
          domainParts.slice(domainParts.length - numDomainParts).join(".");
        documentCookies(`${$(name).trim()}=;${expires};${path};domain=${domain}`);
        documentCookies(`${$(name).trim()}=;${expires};${path};domain=.${domain}`);
        debugLog("success", `Set expiration date on ${name}`);
      }
    }
  };

  mainLogic();

  if (autoRemoveCookie) {
    // Checking if the already removed cookie gets reset.
    // If so we keep removing it.
    let lastCookie = getCookieMatches();
    setInterval(() => {
      let newCookie = getCookieMatches();
      if (newCookie !== lastCookie) {
        try {
          mainLogic();
        }
        finally {
          lastCookie = newCookie;
        }
      }
    }, 1000);
  }
}
