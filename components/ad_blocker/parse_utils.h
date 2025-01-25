// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_PARSE_UTILS_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_PARSE_UTILS_H_

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "components/ad_blocker/adblock_request_filter_rule.h"

namespace adblock_filter {

constexpr char kAbpSnippetsMainScriptletName[] = "abp-main.js";
constexpr char kAbpSnippetsIsolatedScriptletName[] = "abp-isolated.js";

constexpr auto kTypeStringMap = base::MakeFixedFlatMap<std::string_view, int>(
    {{"script", RequestFilterRule::kScript},
     {"image", RequestFilterRule::kImage},
     {"background",
      RequestFilterRule::kImage},  // Compat with older filter formats
     {"stylesheet", RequestFilterRule::kStylesheet},
     {"css", RequestFilterRule::kStylesheet},
     {"object", RequestFilterRule::kObject},
     {"xmlhttprequest", RequestFilterRule::kXmlHttpRequest},
     {"subdocument", RequestFilterRule::kSubDocument},
     {"ping", RequestFilterRule::kPing},
     {"websocket", RequestFilterRule::kWebSocket},
     {"webrtc", RequestFilterRule::kWebRTC},
     {"font", RequestFilterRule::kFont},
     {"webtransport", RequestFilterRule::kWebTransport},
     {"webbundle", RequestFilterRule::kWebBundle},
     {"media", RequestFilterRule::kMedia},
     {"other", RequestFilterRule::kOther},
     {"xbl", RequestFilterRule::kOther},  // Compat with older filter formats
     {"dtd", RequestFilterRule::kOther}});

std::string BuildNgramSearchString(const std::string_view& pattern);
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_PARSE_UTILS_H_
