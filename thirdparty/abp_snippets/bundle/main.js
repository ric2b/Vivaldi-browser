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

import {abortCurrentInlineScript} from
  "../source/behavioral/abort-current-inline-script.js";
import {abortOnIframePropertyRead} from
  "../source/behavioral/abort-on-iframe-property-read.js";
import {abortOnIframePropertyWrite} from
  "../source/behavioral/abort-on-iframe-property-write.js";
import {abortOnPropertyRead} from
  "../source/behavioral/abort-on-property-read.js";
import {abortOnPropertyWrite} from
  "../source/behavioral/abort-on-property-write.js";
import {cookieRemover} from "../source/behavioral/cookie-remover.js";
import {setDebug as debug} from "../source/introspection/debug.js";
import {freezeElement} from "../source/behavioral/freeze-element.js";
import {hideIfShadowContains} from
  "../source/conditional-hiding/hide-if-shadow-contains.js";
import {jsonOverride} from "../source/behavioral/json-override.js";
import {jsonPrune} from "../source/behavioral/json-prune.js";
import {overridePropertyRead} from
  "../source/behavioral/override-property-read.js";
import {preventListener} from "../source/behavioral/prevent-listener.js";
import {stripFetchQueryParameter} from
  "../source/behavioral/strip-fetch-query-parameter.js";
import {trace} from "../source/introspection/trace.js";

export const snippets = {
  "abort-current-inline-script": abortCurrentInlineScript,
  "abort-on-iframe-property-read": abortOnIframePropertyRead,
  "abort-on-iframe-property-write": abortOnIframePropertyWrite,
  "abort-on-property-read": abortOnPropertyRead,
  "abort-on-property-write": abortOnPropertyWrite,
  "cookie-remover": cookieRemover,
  "debug": debug,
  "freeze-element": freezeElement,
  "hide-if-shadow-contains": hideIfShadowContains,
  "json-override": jsonOverride,
  "json-prune": jsonPrune,
  "override-property-read": overridePropertyRead,
  "prevent-listener": preventListener,
  "strip-fetch-query-parameter": stripFetchQueryParameter,
  "trace": trace
};
