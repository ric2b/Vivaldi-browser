/**
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


import {snippets} from "./isolated.js";

import {hideIfMatchesXPath3} from
"../source/conditional-hiding/hide-if-matches-xpath3.js";
import {hideIfClassifies} from "@eyeo/mlaf";

snippets["hide-if-matches-xpath3"] = hideIfMatchesXPath3;
snippets["hide-if-classifies"] = hideIfClassifies;

// {"snippet-name": {...}} dependencies to optionally injext before snippets
// get executed, particularly useful for machine learning or snippets with
// huge external dependencies that should not be evaluated each time.
// The value should be an object with fields wable as serializable functions.
// https://developer.chrome.com/docs/extensions/reference/scripting/#injected-code

export {snippets};
