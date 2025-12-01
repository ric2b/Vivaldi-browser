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
import {arrayOverride} from "../source/behavioral/array-override.js";
import {blobOverride} from "../source/behavioral/blob-override.js";
import {cookieRemover} from "../source/behavioral/cookie-remover.js";
import {setDebug as debug} from "../source/introspection/debug.js";
import {eventOverride} from "../source/behavioral/event-override.js";
import {freezeElement} from "../source/behavioral/freeze-element.js";
import {hideIfCanvasContains} from
  "../source/conditional-hiding/hide-if-canvas-contains.js";
import {hideIfShadowContains} from
  "../source/conditional-hiding/hide-if-shadow-contains.js";
import {jsonOverride} from "../source/behavioral/json-override.js";
import {jsonPrune} from "../source/behavioral/json-prune.js";
import {mapOverride} from "../source/behavioral/map-override.js";
import {overridePropertyRead} from
  "../source/behavioral/override-property-read.js";
import {preventListener} from "../source/behavioral/prevent-listener.js";
import {replaceFetchResponse} from "../source/behavioral/replace-fetch-response.js";
import {replaceOutboundValue} from "../source/behavioral/replace-outbound-value.js";
import {replaceXhrResponse} from "../source/behavioral/replace-xhr-response.js";
import {setProfile} from "../source/introspection/profile.js";
import {stripFetchQueryParameter} from
  "../source/behavioral/strip-fetch-query-parameter.js";
import {trace} from "../source/introspection/trace.js";

export const snippets = {
  "abort-current-inline-script": abortCurrentInlineScript,
  "abort-on-iframe-property-read": abortOnIframePropertyRead,
  "abort-on-iframe-property-write": abortOnIframePropertyWrite,
  "abort-on-property-read": abortOnPropertyRead,
  "abort-on-property-write": abortOnPropertyWrite,
  "array-override": arrayOverride,
  "blob-override": blobOverride,
  "cookie-remover": cookieRemover,
  "profile": setProfile,
  "debug": debug,
  "event-override": eventOverride,
  "freeze-element": freezeElement,
  "hide-if-canvas-contains": hideIfCanvasContains,
  "hide-if-shadow-contains": hideIfShadowContains,
  "json-override": jsonOverride,
  "json-prune": jsonPrune,
  "map-override": mapOverride,
  "override-property-read": overridePropertyRead,
  "prevent-listener": preventListener,
  "replace-fetch-response": replaceFetchResponse,
  "replace-outbound-value": replaceOutboundValue,
  "replace-xhr-response": replaceXhrResponse,
  "strip-fetch-query-parameter": stripFetchQueryParameter,
  "trace": trace
};
