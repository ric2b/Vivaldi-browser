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

/** @module */

"use strict";

const {Cache} = require("./caching");

const publicSuffixes = require("../data/publicSuffixList.json");

/**
 * Map of public suffixes to their offsets.
 * @type {Map.<string,number>}
 */
let publicSuffixMap = buildPublicSuffixMap();

/**
 * Cache of domain maps.
 *
 * The domains part of filter text (e.g. `example.com,~mail.example.com`) is
 * often repeated across filters. This cache enables deduplication of the
 * `Map` objects that specify on which domains the filter does and does not
 * apply, which reduces memory usage and improves performance.
 *
 * @type {Map.<string, Map.<string, boolean>>}
 */
let domainsCache = new Cache(1000);

/**
 * Builds a map of public suffixes to their offsets.
 * @returns {Map.<string,number>}
 */
function buildPublicSuffixMap()
{
  let map = new Map();

  for (let key in publicSuffixes)
    map.set(key, publicSuffixes[key]);

  return map;
}

/**
 * A `URLInfo` object represents information about a URL.
 *
 * It is returned by `{@link module:url.parseURL parseURL()}`.
 */
class URLInfo
{
  /**
   * Creates a `URLInfo` object.
   *
   * @param {string} href The entire URL.
   * @param {string} protocol The protocol scheme of the URL, including the
   *   final `:`.
   * @param {string} [hostname] The hostname of the URL.
   *
   * @private
   */
  constructor(href, protocol, hostname = "")
  {
    this._href = href;
    this._protocol = protocol;
    this._hostname = hostname;
  }

  /**
   * The entire URL.
   * @type {string}
   */
  get href()
  {
    return this._href;
  }

  /**
   * The protocol scheme of the URL, including the final `:`.
   * @type {string}
   */
  get protocol()
  {
    return this._protocol;
  }

  /**
   * The hostname of the URL.
   * @type {string}
   */
  get hostname()
  {
    return this._hostname;
  }

  /**
   * Returns the entire URL.
   * @returns {string} The entire URL.
   */
  toString()
  {
    return this._href;
  }
}

let parseURL =
/**
 * Parses a URL to extract the protocol and the hostname.
 *
 * __This is a lightweight alternative to the native `URL` object intended for
 * use primarily within Adblock Plus. Unlike the `URL` object, the `parseURL()`
 * function is not robust and will give incorrect results for invalid URLs. Use
 * this function with valid,
 * [normalized](https://url.spec.whatwg.org/#concept-basic-url-parser),
 * properly encoded (IDNA, etc.) URLs only.__
 *
 * @param {string} url The URL.
 *
 * @returns {module:url~URLInfo} Information about the URL.
 *
 * @public
 */
exports.parseURL = function parseURL(url)
{
  let match = /^([^:]+:)(?:\/\/(?:[^/]*@)?(\[[^\]]*\]|[^:/]+))?/.exec(url);
  return new URLInfo(url, match[1], match[2]);
};

let domainSuffixes =
/**
 * Yields all suffixes for a domain.
 *
 * For example, given the domain `www.example.com`, this function yields
 * `www.example.com`, `example.com`, and `com`, in that order.
 *
 * If the domain ends with a dot, the dot is ignored.
 *
 * @param {string} domain The domain.
 * @param {boolean} [includeBlank] Whether to include the blank suffix at the
 *   end.
 *
 * @generator
 * @yields {string} The next suffix for the domain.
 *
 * @package
 */
exports.domainSuffixes = function* domainSuffixes(domain, includeBlank = false)
{
  // Since any IP address is already expected to be normalized, there's no need
  // to validate it.
  if (isIPAddress(domain))
  {
    yield domain;
  }
  else
  {
    if (domain[domain.length - 1] == ".")
      domain = domain.substring(0, domain.length - 1);

    while (domain != "")
    {
      yield domain;

      let dotIndex = domain.indexOf(".");
      domain = dotIndex == -1 ? "" : domain.substr(dotIndex + 1);
    }
  }

  if (includeBlank)
    yield "";
};

/**
 * Checks whether the given hostname refers to
 * {@link https://en.wikipedia.org/wiki/Localhost localhost}.
 *
 * If the hostname is `localhost`, `127.0.0.1`, or `[::1]`, the function
 * returns `true`; otherwise, it returns `false`.
 *
 * @param {string} hostname The hostname.
 *
 * @returns {boolean} Whether the hostname refers to localhost.
 *
 * @package
 */
exports.isLocalhost = function isLocalhost(hostname)
{
  return hostname == "localhost" || hostname == "127.0.0.1" ||
         hostname == "[::1]";
};

/**
 * Checks whether the given hostname is an IP address.
 *
 * Unlike `{@link module:url~isValidIPv4Address isValidIPv4Address()}`, this
 * function only checks whether the hostname _looks like_ an IP address. Use
 * this function with valid, normalized, properly encoded (IDNA) hostnames
 * only.
 *
 * @param {string} hostname An IDNA-encoded hostname.
 *
 * @returns {boolean} Whether the hostname is an IP address.
 */
function isIPAddress(hostname)
{
  // Does it look like an IPv6 address?
  if (hostname[0] == "[" && hostname[hostname.length - 1] == "]")
    return true;

  // Note: The first condition helps us avoid the more expensive regular
  // expression match for most hostnames.
  return hostname[hostname.length - 1] >= 0 &&
         /^\d+\.\d+\.\d+\.\d+$/.test(hostname);
}

/**
 * Checks whether a given address is a valid IPv4 address.
 *
 * Only a normalized IPv4 address is considered valid. e.g. `0x7f.0x0.0x0.0x1`
 * is invalid, whereas `127.0.0.1` is valid.
 *
 * @param {string} address The address to check.
 *
 * @returns {boolean} Whether the address is a valid IPv4 address.
 */
function isValidIPv4Address(address)
{
  return /^(((2[0-4]|1[0-9]|[1-9])?[0-9]|25[0-5])\.){4}$/.test(address + ".");
}

/**
 * Checks whether a given hostname is valid.
 *
 * This function is used for filter validation.
 *
 * A hostname occurring in a filter must be normalized. For example,
 * <code>&#x1f642;</code> (slightly smiling face) should be normalized to
 * `xn--938h`; otherwise this function returns `false` for such a hostname.
 * Similarly, IP addresses should be normalized.
 *
 * @param {string} hostname The hostname to check.
 *
 * @returns {boolean} Whether the hostname is valid.
 */
exports.isValidHostname = function isValidHostname(hostname)
{
  if (isValidIPv4Address(hostname))
    return true;

  // This does not in fact validate the IPv6 address but it's alright for now.
  if (hostname[0] == "[" && hostname[hostname.length - 1] == "]")
    return true;

  // Based on
  // https://en.wikipedia.org/wiki/Hostname#Restrictions_on_valid_hostnames
  if (hostname[hostname.length - 1] == ".")
    hostname = hostname.substring(0, hostname.length - 1);

  if (hostname.length > 253)
    return false;

  let labels = hostname.split(".");

  for (let label of labels)
  {
    if (!/^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$/i.test(label))
      return false;
  }

  // Based on https://tools.ietf.org/html/rfc3696#section-2
  if (!/\D/.test(labels[labels.length - 1]))
    return false;

  return true;
};

let getBaseDomain =
/**
 * Gets the base domain for the given hostname.
 *
 * @param {string} hostname
 *
 * @returns {string}
 *
 * @package
 */
exports.getBaseDomain = function getBaseDomain(hostname)
{
  let slices = [];
  let cutoff = NaN;

  for (let suffix of domainSuffixes(hostname))
  {
    slices.push(suffix);

    let offset = publicSuffixMap.get(suffix);

    if (typeof offset != "undefined")
    {
      cutoff = slices.length - 1 - offset;
      break;
    }
  }

  if (isNaN(cutoff))
    return slices.length > 2 ? slices[slices.length - 2] : hostname;

  if (cutoff <= 0)
    return hostname;

  return slices[cutoff];
};

/**
 * Checks whether a request's origin is different from its document's origin.
 *
 * @param {string} requestHostname The IDNA-encoded hostname of the request.
 * @param {string} documentHostname The IDNA-encoded hostname of the document.
 *
 * @returns {boolean}
 */
function isThirdParty(requestHostname, documentHostname)
{
  if (requestHostname[requestHostname.length - 1] == ".")
    requestHostname = requestHostname.substring(0, requestHostname.length - 1);

  if (documentHostname[documentHostname.length - 1] == ".")
  {
    documentHostname =
      documentHostname.substring(0, documentHostname.length - 1);
  }

  if (requestHostname == documentHostname)
    return false;

  if (!requestHostname || !documentHostname)
    return true;

  if (isIPAddress(requestHostname) || isIPAddress(documentHostname))
    return true;

  return getBaseDomain(requestHostname) != getBaseDomain(documentHostname);
}

let URLRequest =
/**
 * The `URLRequest` class represents a URL request.
 */
exports.URLRequest = class URLRequest
{
  /**
   * @private
   */
  constructor() {}

  /**
   * The URL of the request.
   * @type {string}
   */
  get href()
  {
    return this._href;
  }

  /**
   * Information about the URL of the request.
   * @type {module:url~URLInfo}
   * @package
   */
  get urlInfo()
  {
    if (!this._urlInfo)
      this._urlInfo = parseURL(this._href);

    return this._urlInfo;
  }

  /**
   * The hostname of the document making the request.
   * @type {?string}
   */
  get documentHostname()
  {
    return this._documentHostname == null ? null : this._documentHostname;
  }

  /**
   * Whether this is a third-party request.
   * @type {boolean}
   */
  get thirdParty()
  {
    if (typeof this._thirdParty == "undefined")
    {
      this._thirdParty = this._documentHostname == null ? false :
                           isThirdParty(this.urlInfo.hostname,
                                        this._documentHostname);
    }

    return this._thirdParty;
  }

  /**
   * Returns the URL of the request.
   * @returns {string}
   */
  toString()
  {
    return this._href;
  }

  /**
   * The lower-case version of the URL of the request.
   * @type {string}
   * @package
   */
  get lowerCaseHref()
  {
    if (this._lowerCaseHref == null)
      this._lowerCaseHref = this._href.toLowerCase();

    return this._lowerCaseHref;
  }
};

/**
 * Returns a `{@link module:url.URLRequest URLRequest}` object for the given
 * URL.
 *
 * @param {string|module:url~URLInfo|URL} url The URL. If this is a `string`,
 *   it must be a canonicalized URL (see
 *   `{@link module:url.parseURL parseURL()}`).
 * @param {?string} [documentHostname] The IDNA-encoded hostname of the
 *   document making the request.
 *
 * @returns {module:url.URLRequest}
 */
exports.URLRequest.from = function(url, documentHostname = null)
{
  let request = new URLRequest();

  if (typeof url == "string")
  {
    request._href = url;
  }
  else
  {
    request._urlInfo = url instanceof URLInfo ? url :
                         new URLInfo(url.href, url.protocol, url.hostname);
    request._href = url.href;
  }

  if (documentHostname != null)
    request._documentHostname = documentHostname;

  return request;
};

/**
 * Parses the domains part of a filter text
 * (e.g. `example.com,~mail.example.com`) into a `Map` object.
 *
 * @param {string} source The domains part of a filter text.
 * @param {string} separator The string used to separate two or more domains in
 *   the domains part of a filter text.
 *
 * @returns {?Map.<string, boolean>}
 *
 * @package
 */
exports.parseDomains = function parseDomains(source, separator)
{
  let domains = domainsCache.get(source);
  if (typeof domains != "undefined")
    return domains;

  if (source[0] != "~" && !source.includes(separator))
  {
    // Fast track for the common one-domain scenario.
    domains = new Map([["", false], [source, true]]);
  }
  else
  {
    domains = null;

    let hasIncludes = false;
    for (let domain of source.split(separator))
    {
      if (domain == "")
        continue;

      let include;
      if (domain[0] == "~")
      {
        include = false;
        domain = domain.substring(1);
      }
      else
      {
        include = true;
        hasIncludes = true;
      }

      if (!domains)
        domains = new Map();

      domains.set(domain, include);
    }

    if (domains)
      domains.set("", !hasIncludes);
  }

  domainsCache.set(source, domains);

  return domains;
};
