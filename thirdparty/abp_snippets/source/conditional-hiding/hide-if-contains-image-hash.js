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

import {raceWinner} from "../introspection/race.js";
import {hideElement} from "../utils/dom.js";

let {
  isNaN,
  parseInt,
  Array,
  Error,
  Image,
  ImageData,
  Math,
  MutationObserver,
  Object,
  Set
} = $(window);

/**
 * Hides any HTML element or one of its ancestors matching a CSS selector if
 * the perceptual hash of the image src or background image of the element
 * matches the given perceptual hash.
 * @alias module:content/snippets.hide-if-contains-image-hash
 *
 * @param {string} hashes List of comma seperated  perceptual hashes of the
 *  images that should be blocked, see also `maxDistance`.
 * @param {?string} [selector] The CSS selector that an HTML element
 *   containing the given pattern must match. If empty or omitted, defaults
 *   to the image element itself.
 * @param {?string} [maxDistance] The maximum hamming distance between `hash`
 *   and the perceptual hash of the image to be considered a match. Defaults
 *   to 0.
 * @param {?number} [blockBits] The block width used to generate the perceptual
 *   image hash, a number of 4 will split the image into 16 blocks
 *   (width/4 * height/4). Defaults to 8. The maximum value allowed is 64.
 * @param {?string} [selection] A string with image coordinates in the format
 *   XxYxWIDTHxHEIGHT for which a perceptual hash should be computated. If
 *   ommitted the entire image will be hashed. The X and Y values can be
 *   negative, in this case they will be relative to the right/bottom corner.
 *
 * @since Adblock Plus 3.6.2
 */
export function hideIfContainsImageHash(hashes,
                                        selector,
                                        maxDistance,
                                        blockBits,
                                        selection) {
  if (selector == null || selector === "")
    selector = "img";

  if (maxDistance == null)
    maxDistance = 0;

  if (blockBits == null)
    blockBits = 8;

  if (isNaN(maxDistance) || isNaN(blockBits))
    return;

  blockBits |= 0;

  if (blockBits < 1 || blockBits > 64)
    return;

  selection = $(selection || "").split("x");

  let seenImages = new Set();

  let callback = images => {
    for (let image of images) {
      let $image = $(image, "HTMLImageElement");
      seenImages.add($image.src);

      let imageElement = new Image();
      imageElement.crossOrigin = "anonymous";
      imageElement.onload = () => {
        let canvas = $(document).createElement("canvas");
        let $canvas = $(canvas, "HTMLCanvasElement");
        let context = $canvas.getContext("2d");
        let $context = $(context, "CanvasRenderingContext2D");

        let {width, height} = imageElement;

        // If a selection is present we are only going to look at that
        // part of the image
        let sX = parseInt(selection[0], 10) || 0;
        let sY = parseInt(selection[1], 10) || 0;
        let sWidth = parseInt(selection[2], 10) || width;
        let sHeight = parseInt(selection[3], 10) || height;

        if (sWidth == 0 || sHeight == 0)
          return;

        // if sX or sY is negative start from the right/bottom respectively
        if (sX < 0)
          sX = width + sX;
        if (sY < 0)
          sY = height + sY;

        if (sX < 0)
          sX = 0;
        if (sY < 0)
          sY = 0;
        if (sWidth > width)
          sWidth = width;
        if (sHeight > height)
          sHeight = height;

        $canvas.width = sWidth;
        $canvas.height = sHeight;

        $context.drawImage(
          imageElement, sX, sY, sWidth, sHeight, 0, 0, sWidth, sHeight
        );

        let imageData = Object.setPrototypeOf(
          $context.getImageData(0, 0, sWidth, sHeight),
          ImageData.prototype
        );

        let result = hashImage(imageData, blockBits);

        for (let hash of $(hashes).split(",")) {
          if (result.length == hash.length) {
            if (hammingDistance(result, hash) <= maxDistance) {
              let closest = $image.closest(selector);
              if (closest) {
                win();
                hideElement(closest);
                return;
              }
            }
          }
        }
      };
      imageElement.src = $image.src;
    }
  };

  let mo = new MutationObserver(() => {
    let images = new Set();
    for (let img of $(document).images) {
      if (!seenImages.has($(img, "HTMLImageElement").src))
        images.add(img);
    }

    if (images.size)
      callback(images);
  });

  let win = raceWinner(
    "hide-if-contains-image-hash",
    () => mo.disconnect()
  );
  mo.observe(document, {childList: true, subtree: true, attributes: true});
  callback($(document).images);
}


/**
 * Calculates and returns the perceptual hash of the supplied image.
 *
 * The following lines are based off the blockhash-js library which is
 * licensed under the MIT licence
 * {@link https://github.com/commonsmachinery/blockhash-js/tree/2084417e40005e37f4ad957dbd2bca08ddc222bc Blockhash.js}
 *
 * @param {object} imageData ImageData object containing the image data of the
 *  image for which a hash should be calculated
 * @param {?number} [blockBits] The block width used to generate the perceptual
 *   image hash, a number of 4 will split the image into 16 blocks
 *   (width/4 * height/4). Defaults to 8.
 * @returns {string} The resulting hash
 * @private
 */
function hashImage(imageData, blockBits) {
  function median(mdarr) {
    $(mdarr).sort((a, b) => a - b);
    let {length} = mdarr;
    if (length % 2 === 0)
      return (mdarr[length / 2 - 1] + mdarr[length / 2]) / 2.0;
    return mdarr[(length / 2) | 0];
  }

  function translateBlocksToBits(blocks, pixelsPerBlock) {
    let halfBlockValue = pixelsPerBlock * 256 * 3 / 2;
    let bandsize = blocks.length / 4;

    // Compare medians across four horizontal bands
    for (let i = 0; i < 4; i++) {
      let index = i * bandsize;
      let length = (i + 1) * bandsize;
      let m = median(blocks.slice(index, length));
      for (let j = index; j < length; j++) {
        let v = blocks[j];

        // Output a 1 if the block is brighter than the median.
        // With images dominated by black or white, the median may
        // end up being 0 or the max value, and thus having a lot
        // of blocks of value equal to the median.  To avoid
        // generating hashes of all zeros or ones, in that case output
        // 0 if the median is in the lower value space, 1 otherwise
        blocks[j] = (v > m || (m - v < 1 && m > halfBlockValue)) ? 1 : 0;
      }
    }
  }

  function bitsToHexhash(bitsArray) {
    let hex = $([]);
    let {length} = bitsArray;
    for (let i = 0; i < length; i += 4) {
      let nibble = bitsArray.slice(i, i + 4);
      hex.push($(parseInt($(nibble).join(""), 2)).toString(16));
    }

    return hex.join("");
  }

  function bmvbhashEven(data, bits) {
    let {width, height, data: imgData} = data;
    let blocksizeX = (width / bits) | 0;
    let blocksizeY = (height / bits) | 0;

    let result = new Array(bits * bits);

    for (let y = 0; y < bits; y++) {
      for (let x = 0; x < bits; x++) {
        let total = 0;

        for (let iy = 0; iy < blocksizeY; iy++) {
          let ii = ((y * blocksizeY + iy) * width + x * blocksizeX) * 4;

          for (let ix = 0; ix < blocksizeX; ix++) {
            let alpha = imgData[ii + 3];
            if (alpha === 0)
              total += 765;
            else
              total += imgData[ii] + imgData[ii + 1] + imgData[ii + 2];

            ii += 4;
          }
        }

        result[y * bits + x] = total;
      }
    }

    translateBlocksToBits(result, blocksizeX * blocksizeY);
    return bitsToHexhash(result);
  }

  function bmvbhash(data, bits) {
    let x; let y;
    let blockWidth; let blockHeight;
    let weightTop; let weightBottom; let weightLeft; let weightRight;
    let blockTop; let blockBottom; let blockLeft; let blockRight;
    let yMult; let yFrac; let yInt;
    let xMult; let xFrac; let xInt;
    let {width, height, data: imgData} = data;

    let evenX = width % bits === 0;
    let evenY = height % bits === 0;

    if (evenX && evenY)
      return bmvbhashEven(data, bits);

    // initialize blocks array with 0s
    let result = new Array(bits * bits).fill(0);

    blockWidth = width / bits;
    blockHeight = height / bits;

    yInt = 1;
    yFrac = 0;

    yMult = blockHeight;

    weightTop = 1;
    weightBottom = 0;

    let ii = 0;
    for (y = 0; y < height; y++) {
      if (evenY) {
        blockTop = blockBottom = (y / blockHeight) | 0;
      }
      else {
        if (y + 1 >= yMult) {
          let mod = (y + 1) % blockHeight;
          yInt = mod | 0;
          yFrac = mod - yInt;

          if (blockHeight > 1)
            yMult = Math.ceil((y + 1) / blockHeight) * blockHeight;

          weightTop = (1 - yFrac);
          weightBottom = (yFrac);
        }

        // yInt will be 0 on bottom/right borders and on block boundaries
        if (yInt > 0 || (y + 1) === height) {
          blockTop = blockBottom = (y / blockHeight) | 0;
        }
        else {
          let div = y / blockHeight;
          blockTop = div | 0;
          blockBottom = blockTop === div ? blockTop : blockTop + 1;
        }
      }

      xInt = 1;
      xFrac = 0;

      xMult = blockWidth;

      weightLeft = 1;
      weightRight = 0;

      for (x = 0; x < width; x++) {
        let avgvalue = 765;
        let alpha = imgData[ii + 3];
        if (alpha !== 0)
          avgvalue = imgData[ii] + imgData[ii + 1] + imgData[ii + 2];

        if (evenX) {
          blockLeft = blockRight = (x / blockWidth) | 0;
        }
        else {
          if (x + 1 >= xMult) {
            let mod = (x + 1) % blockWidth;
            xInt = mod | 0;
            xFrac = mod - xInt;

            if (blockWidth > 1)
              xMult = Math.ceil((x + 1) / blockWidth) * blockWidth;

            weightLeft = 1 - xFrac;
            weightRight = xFrac;
          }

          // xInt will be 0 on bottom/right borders and on block boundaries
          if (xInt > 0 || (x + 1) === width) {
            blockLeft = blockRight = (x / blockWidth) | 0;
          }
          else {
            let div = x / blockWidth;
            blockLeft = div | 0;
            blockRight = blockLeft === div ? blockLeft : blockLeft + 1;
          }
        }

        // add weighted pixel value to relevant blocks
        result[blockTop * bits + blockLeft] +=
          avgvalue * weightTop * weightLeft;
        result[blockTop * bits + blockRight] +=
          avgvalue * weightTop * weightRight;
        result[blockBottom * bits + blockLeft] +=
          avgvalue * weightBottom * weightLeft;
        result[blockBottom * bits + blockRight] +=
          avgvalue * weightBottom * weightRight;

        ii += 4;

        xInt++;
      }

      yInt++;
    }

    translateBlocksToBits(result, blockWidth * blockHeight);
    return bitsToHexhash(result);
  }

  return bmvbhash(imageData, blockBits);
}

/**
 * Calculate the hamming distance for two hashes in hex format
 *
 * The following lines are based off the blockhash-js library which is
 * licensed under the MIT licence
 * {@link https://github.com/commonsmachinery/blockhash-js/tree/2084417e40005e37f4ad957dbd2bca08ddc222bc Blockhash.js}
 *
 * @param {string} hash1 the first hash of the comparison
 * @param {string} hash2 the second hash of the comparison
 * @returns {number} The resulting hamming distance between hash1 and hash2
 * @private
 */
function hammingDistance(hash1, hash2) {
  let oneBits = [0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4];

  let d = 0;
  let i;

  if (hash1.length !== hash2.length)
    throw new Error("Can't compare hashes with different length");

  for (i = 0; i < hash1.length; i++) {
    let n1 = parseInt(hash1[i], 16);
    let n2 = parseInt(hash2[i], 16);
    d += oneBits[n1 ^ n2];
  }
  return d;
}
