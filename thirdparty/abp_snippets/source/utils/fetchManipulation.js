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
import {apply, proxy} from "proxy-pants/function";

let {fetch} = $(window);

let hasFetchBeenProxied = false;

/**
 * Callbacks that will be executed for strip-fetch-query-parameter
 */
const preFetchCallbacks = [];

/**
 * Callbacks that will be executed for replace-fetch-response
 */
const postFetchCallbacks = [];


/**
 * Proxies the fetch API: since we have more than 1 snippet manipulating fetch,
 * here we proxy it only once, allowing multiple snippets to add their callbacks
 * without interfering with each other.
 */
const proxyFetch = () => {
  // override the `window.fetch` only once
  if (!hasFetchBeenProxied) {
    window.fetch = proxy(fetch, (...args) => {
      let [source] = args;
      if (preFetchCallbacks.length > 0 && typeof source === "string") {
        let url;
        try {
          url = new URL(source);
        }
        catch (e) {
          if (e instanceof TypeError)
            url = new URL(source, $(document).location);
          else
            throw e;
        }
        preFetchCallbacks.forEach(fn => fn(url));
        args[0] = url.href;
      }

      const promise = apply(fetch, self, args).then(origResponse => {
        let transformedResponse = origResponse;
        postFetchCallbacks.forEach(fn => {
          transformedResponse = fn(transformedResponse);
        });
        return transformedResponse;
      });
      return promise;
    });
    hasFetchBeenProxied = true;
  }
};

export const addPreFetchCallback = callback => {
  preFetchCallbacks.push(callback);
  proxyFetch();
};

export const addPostFetchCallback = callback => {
  postFetchCallbacks.push(callback);
  proxyFetch();
};

