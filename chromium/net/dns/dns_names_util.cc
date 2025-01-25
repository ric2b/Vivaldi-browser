// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_names_util.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "net/base/ip_address.h"
#include "net/base/url_util.h"
#include "net/dns/public/dns_protocol.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace net::dns_names_util {

bool IsValidDnsName(std::string_view dotted_form_name) {
  return DottedNameToNetwork(dotted_form_name,
                             /*require_valid_internet_hostname=*/false)
      .has_value();
}

bool IsValidDnsRecordName(std::string_view dotted_form_name) {
  IPAddress ip_address;
  return IsValidDnsName(dotted_form_name) &&
         !HostStringIsLocalhost(dotted_form_name) &&
         !ip_address.AssignFromIPLiteral(dotted_form_name) &&
         !ParseURLHostnameToAddress(dotted_form_name, &ip_address);
}

// Based on DJB's public domain code.
std::optional<std::vector<uint8_t>> DottedNameToNetwork(
    std::string_view dotted_form_name,
    bool require_valid_internet_hostname) {
  // Use full IsCanonicalizedHostCompliant() validation if not
  // `is_unrestricted`. All subsequent validity checks should not apply unless
  // `is_unrestricted` because IsCanonicalizedHostCompliant() is expected to be
  // more strict than any validation here.
  if (require_valid_internet_hostname &&
      !IsCanonicalizedHostCompliant(dotted_form_name))
    return std::nullopt;

  const char* buf = dotted_form_name.data();
  size_t n = dotted_form_name.size();
  uint8_t label[dns_protocol::kMaxLabelLength];
  size_t labellen = 0; /* <= sizeof label */
  std::vector<uint8_t> name(dns_protocol::kMaxNameLength, 0);
  size_t namelen = 0; /* <= sizeof name */
  char ch;

  for (;;) {
    if (!n)
      break;
    ch = *buf++;
    --n;
    if (ch == '.') {
      // Don't allow empty labels per http://crbug.com/456391.
      if (!labellen) {
        DCHECK(!require_valid_internet_hostname);
        return std::nullopt;
      }
      if (namelen + labellen + 1 > name.size()) {
        DCHECK(!require_valid_internet_hostname);
        return std::nullopt;
      }
      name[namelen++] = static_cast<uint8_t>(labellen);
      memcpy(name.data() + namelen, label, labellen);
      namelen += labellen;
      labellen = 0;
      continue;
    }
    if (labellen >= sizeof(label)) {
      DCHECK(!require_valid_internet_hostname);
      return std::nullopt;
    }
    label[labellen++] = ch;
  }

  // Allow empty label at end of name to disable suffix search.
  if (labellen) {
    if (namelen + labellen + 1 > name.size()) {
      DCHECK(!require_valid_internet_hostname);
      return std::nullopt;
    }
    name[namelen++] = static_cast<uint8_t>(labellen);
    memcpy(name.data() + namelen, label, labellen);
    namelen += labellen;
    labellen = 0;
  }

  if (namelen + 1 > name.size()) {
    DCHECK(!require_valid_internet_hostname);
    return std::nullopt;
  }
  if (namelen == 0) {  // Empty names e.g. "", "." are not valid.
    DCHECK(!require_valid_internet_hostname);
    return std::nullopt;
  }
  name[namelen++] = 0;  // This is the root label (of length 0).

  name.resize(namelen);
  return name;
}

std::optional<std::string> NetworkToDottedName(base::span<const uint8_t> span,
                                               bool require_complete) {
  auto reader = base::SpanReader(span);
  return NetworkToDottedName(reader, require_complete);
}

std::optional<std::string> NetworkToDottedName(
    base::SpanReader<const uint8_t>& reader,
    bool require_complete) {
  std::string ret;
  size_t octets_read = 0u;
  while (reader.remaining() > 0u) {
    // DNS name compression not allowed because it does not make sense without
    // the context of a full DNS message.
    if ((reader.remaining_span()[0u] & dns_protocol::kLabelMask) ==
        dns_protocol::kLabelPointer) {
      return std::nullopt;
    }

    base::span<const uint8_t> label;
    if (!ReadU8LengthPrefixed(reader, &label)) {
      return std::nullopt;
    }

    // Final zero-length label not included in size enforcement.
    if (!label.empty()) {
      octets_read += label.size() + 1u;
    }

    if (label.size() > dns_protocol::kMaxLabelLength) {
      return std::nullopt;
    }
    if (octets_read > dns_protocol::kMaxNameLength) {
      return std::nullopt;
    }

    if (label.empty()) {
      return ret;
    }

    if (!ret.empty()) {
      ret.append(".");
    }

    ret.append(base::as_string_view(label));
  }

  if (require_complete) {
    return std::nullopt;
  }

  // If terminating zero-length label was not included in the input, no need to
  // recheck against max name length because terminating zero-length label does
  // not count against the limit.

  return ret;
}

bool ReadU8LengthPrefixed(base::SpanReader<const uint8_t>& reader,
                          base::span<const uint8_t>* out) {
  base::SpanReader<const uint8_t> inner_reader = reader;
  uint8_t len;
  if (!inner_reader.ReadU8BigEndian(len)) {
    return false;
  }
  std::optional<base::span<const uint8_t>> bytes = inner_reader.Read(len);
  if (!bytes) {
    return false;
  }
  *out = *bytes;
  reader = inner_reader;
  return true;
}

bool ReadU16LengthPrefixed(base::SpanReader<const uint8_t>& reader,
                           base::span<const uint8_t>* out) {
  base::SpanReader<const uint8_t> inner_reader = reader;
  uint16_t len;
  if (!inner_reader.ReadU16BigEndian(len)) {
    return false;
  }
  std::optional<base::span<const uint8_t>> bytes = inner_reader.Read(len);
  if (!bytes) {
    return false;
  }
  *out = *bytes;
  reader = inner_reader;
  return true;
}

std::string UrlCanonicalizeNameIfAble(std::string_view name) {
  std::string canonicalized;
  url::StdStringCanonOutput output(&canonicalized);
  url::CanonHostInfo host_info;
  url::CanonicalizeHostVerbose(name.data(), url::Component(0, name.size()),
                               &output, &host_info);

  if (host_info.family == url::CanonHostInfo::Family::BROKEN) {
    return std::string(name);
  }

  output.Complete();
  return canonicalized;
}

}  // namespace net::dns_names_util
