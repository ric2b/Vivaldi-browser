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

import {log} from "../introspection/log.js";
import {apply} from "proxy-pants/function";

/**
 * Similar to `log`, but does the logging in the context of the document rather
 * than the content script.
 *
 * This may be used for testing and debugging, especially to verify that the
 * injection of snippets into the document is working without any errors.
 * @alias module:content/snippets.trace
 *
 * @param {...*} [args] The arguments to log.
 *
 * @since Adblock Plus 3.3
 */
export function trace(...args) {
  // We could simply use console.log here, but the goal is to demonstrate the
  // usage of snippet dependencies.
  apply(log, null, args);
}
