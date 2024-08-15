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

import {hideIfContains} from
  "../source/conditional-hiding/hide-if-contains.js";
import {hideIfContainsAndMatchesStyle} from
  "../source/conditional-hiding/hide-if-contains-and-matches-style.js";
import {hideIfContainsImage} from
  "../source/conditional-hiding/hide-if-contains-image.js";
import {hideIfContainsSimilarText} from
  "../source/conditional-hiding/hide-if-contains-similar-text.js";
import {hideIfContainsVisibleText} from
  "../source/conditional-hiding/hide-if-contains-visible-text.js";
import {hideIfHasAndMatchesStyle} from
  "../source/conditional-hiding/hide-if-has-and-matches-style.js";
import {hideIfLabelledBy} from
  "../source/conditional-hiding/hide-if-labelled-by.js";
import {hideIfMatchesXPath} from
  "../source/conditional-hiding/hide-if-matches-xpath.js";
import {hideIfMatchesComputedXPath} from
  "../source/conditional-hiding/hide-if-matches-computed-xpath.js";
import {log} from "../source/introspection/log.js";
import {race} from "../source/introspection/race.js";
import {setDebug} from "../source/introspection/debug.js";
import {simulateMouseEvent} from 
  "../source/behavioral/simulate-mouse-event.js";
import {skipVideo} from "../source/behavioral/skip-video.js";

export const snippets = {
  log,
  race,
  "debug": setDebug,
  "hide-if-matches-xpath": hideIfMatchesXPath,
  "hide-if-matches-computed-xpath": hideIfMatchesComputedXPath,
  "hide-if-contains": hideIfContains,
  "hide-if-contains-similar-text": hideIfContainsSimilarText,
  "hide-if-contains-visible-text": hideIfContainsVisibleText,
  "hide-if-contains-and-matches-style": hideIfContainsAndMatchesStyle,
  "hide-if-has-and-matches-style": hideIfHasAndMatchesStyle,
  "hide-if-labelled-by": hideIfLabelledBy,
  "hide-if-contains-image": hideIfContainsImage,
  "simulate-mouse-event": simulateMouseEvent,
  "skip-video": skipVideo
};
