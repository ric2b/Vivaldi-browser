// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CORE_PARSE_UTILS_H_
#define COMPONENTS_AD_BLOCKER_CORE_PARSE_UTILS_H_

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"

namespace adblock_filter {

inline constexpr char kAbpSnippetsMainScriptletName[] = "abp-main.js";
inline constexpr char kAbpSnippetsIsolatedScriptletName[] = "abp-isolated.js";

inline constexpr auto kTypeStringMap =
    base::MakeFixedFlatMap<std::string_view, ResourceType>(
        {{"script", ResourceType::kScript},
         {"image", ResourceType::kImage},
         {"background",
          ResourceType::kImage},  // Compat with older filter formats
         {"stylesheet", ResourceType::kStylesheet},
         {"css", ResourceType::kStylesheet},
         {"object", ResourceType::kObject},
         {"xmlhttprequest", ResourceType::kXmlHttpRequest},
         {"subdocument", ResourceType::kSubDocument},
         {"ping", ResourceType::kPing},
         {"websocket", ResourceType::kWebSocket},
         {"webrtc", ResourceType::kWebRTC},
         {"font", ResourceType::kFont},
         {"webtransport", ResourceType::kWebTransport},
         {"webbundle", ResourceType::kWebBundle},
         {"media", ResourceType::kMedia},
         {"other", ResourceType::kOther},
         {"xbl", ResourceType::kOther},  // Compat with older filter formats
         {"dtd", ResourceType::kOther}});

std::string BuildNgramSearchString(const std::string_view& pattern);
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CORE_PARSE_UTILS_H_
