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

import {$$} from "../utils/dom.js";
import $ from "../$.js";

import {formatArguments} from "../utils/general.js";
import {getDebugger} from "../introspection/log.js";
import {profile} from "../introspection/profile.js";
import {initQueryAndApply} from "../utils/dom.js";
import {raceWinner} from "../introspection/race.js";
import {waitUntilEvent} from "../utils/execution.js";

let {isNaN, MutationObserver, parseInt, parseFloat, setTimeout} = $(window);

/**
 * Skips video
 * @alias module:content/snippets.skip-video
 *
 * @param {string} playerSelector The CSS or the XPath selector to the
 * <video> element in the page.
 * @param {string} xpathCondition The XPath selector that will be used to
 * know when to trigger the skipping logic.
 * @param {?Array.<string>} [attributes] Optional parameters that can be used
 * to configure the snippet.
 *
 * Syntax: <key>:<value>.
 *
 * Accepts:
 *
 * -skip-to:-0.1 (default is -0.1)
 * Determines the time of the video to skip to.
 * Skips to the end if value is negative or zero.
 * Fast forwards video with the given value if positive.
 *
 * -wait-until:load (default is waitUntil disabled)
 * Optional parameter that can be used to delay
 * the running of the snippet until the given state is reached.
 * Accepts: loading, interactive, complete, load or any event name
 * Pass empty string to disable.
 *
 * -max-attempts:10 (default is 10)
 * If the video is not fully loaded by the time the
 * xpath condition is met; there is a retry mechanism in the snippet.
 * maxAttempts parameter will determine the maximum number of attemps
 * the snippet should do before giving up.
 *
 * -retry-ms:10 (default is 10)
 * The snippet will try to skip the video
 * once every retryMs interval.
 *
 * -run-once:true (default is false)
 * Used to disable the snippet after it has skipped the video once.
 * Can be improve performance in some cases.
 *
 * -stop-on-video-end:true (default is false)
 * Used to disable the snippet when the video is already near its end.
 * Video is considered near its end when the difference between the
 * video duration and the current time is less than 0.5 seconds.
 *
 * -start-from:1000 (default is 0)
 * Delays running of the snippet until video reaches the given timestamp.
 * Unit is in ms.
 *
 * -mute-video-when-skipping:false (default is true)
 * Mutes the video when skipping is happening.
 */
export function skipVideo(playerSelector, xpathCondition, ...attributes) {
  const formattedArguments = formatArguments(arguments);
  const optionalParameters = new Map([
    ["-max-attempts", "10"],
    ["-retry-ms", "10"],
    ["-run-once", "false"],
    ["-wait-until", ""],
    ["-skip-to", "-0.1"],
    ["-stop-on-video-end", "false"],
    ["-start-from", "0"],
    ["-mute-video-when-skipping", "true"]
  ]);

  for (let attr of attributes) {
    attr = $(attr);
    let markerIndex = attr.indexOf(":");
    if (markerIndex < 0)
      continue;

    let key = attr.slice(0, markerIndex).trim().toString();
    let value = attr.slice(markerIndex + 1).trim().toString();

    if (key && value && optionalParameters.has(key))
      optionalParameters.set(key, value);
  }

  const maxAttemptsStr = optionalParameters.get("-max-attempts");
  const maxAttemptsNum = parseInt(maxAttemptsStr || 10, 10);

  const retryMsStr = optionalParameters.get("-retry-ms");
  const retryMsNum = parseInt(retryMsStr || 10, 10);

  const runOnceStr = optionalParameters.get("-run-once");
  const runOnceFlag = (runOnceStr === "true");

  const skipToStr = optionalParameters.get("-skip-to");
  const skipToNum = parseFloat(skipToStr || -0.1);

  const startFromStr = optionalParameters.get("-start-from");
  const startFrom = parseInt(startFromStr || 0, 10);

  const waitUntil = optionalParameters.get("-wait-until");

  const stopOnVideoEndStr = optionalParameters.get("-stop-on-video-end");
  const stopOnVideoEndFlag = (stopOnVideoEndStr === "true");

  const muteVideoStr = optionalParameters.get("-mute-video-when-skipping");
  const muteVideo = !(muteVideoStr === "false");

  const debugLog = getDebugger("skip-video");
  const {mark, end} = profile("skip-video");
  const queryAndApply = initQueryAndApply(`xpath(${xpathCondition})`);
  let skippedOnce = false;

  const mainLogic = () => {
    mark();
    const seenMap = new WeakSet();
    const callback = (retryCounter = 0) => {
      if (skippedOnce && runOnceFlag) {
        if (mo)
          mo.disconnect();
        return;
      }
      queryAndApply(node => {
        let nodeAlreadySeen = seenMap.has(node);
        let lastSkippedVideoDuration;
        if (!nodeAlreadySeen) {
          debugLog("info", "Matched:", node, " for selector: ", xpathCondition);
          debugLog("info", "Running video skipping logic.");
        }
        const videos = $$(playerSelector);
        let foundValidVideo = false;
        for (const video of videos) {
          if (!video || isNaN(video.duration) || isNaN(video.currentTime))
            continue;
          foundValidVideo = true;
          const videoNearEnd = (video.duration - video.currentTime) < 0.5;
          if ((video.duration > 0) && (video.currentTime < video.duration) &&
              !(stopOnVideoEndFlag && videoNearEnd)) {
            if (muteVideo) {
              video.muted = true;
              if (!nodeAlreadySeen)
                debugLog("success", "Muted video...");
            }
            if (startFrom <= video.currentTime * 1000) {
              // If skipTo is zero or negative, skip to the end of the video
              // If skipTo is positive, skip forward for the given time.
              skipToNum <= 0 ?
                video.currentTime = video.duration + skipToNum :
                video.currentTime += skipToNum;
              if (lastSkippedVideoDuration !== video.duration) {
                debugLog("success",
                         "Skipped video, currentTime: ",
                         video.currentTime,
                         "s.",
                         "\nFILTER: skip-video",
                         formattedArguments);
                seenMap.add(node);
                lastSkippedVideoDuration = video.duration;
              }
              video.paused && video.play();
              skippedOnce = true;
              win();
            }
          }
        }
        if (!foundValidVideo && retryCounter < maxAttemptsNum) {
          setTimeout(() => {
            const attempt = retryCounter + 1;
            debugLog("info",
                     "Running video skipping logic. Attempt: ",
                     attempt);
            callback(attempt);
          }, retryMsNum);
        }
      });
    };
    const mo = new MutationObserver(callback);
    const win = raceWinner(
      "skip-video",
      () => mo.disconnect()
    );
    mo.observe(
      document, {characterData: true, childList: true, subtree: true});
    callback();
    end();
  };

  waitUntilEvent(debugLog, mainLogic, waitUntil);
}
