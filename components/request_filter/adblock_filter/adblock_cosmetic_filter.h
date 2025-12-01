// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_COSMETIC_FILTER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_COSMETIC_FILTER_H_

#include "base/memory/weak_ptr.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/request_filter/adblock_filter/mojom/adblock_cosmetic_filter.mojom.h"
#include "content/public/browser/child_process_id.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class RenderFrameHost;
}

namespace adblock_filter {
class RuleServiceImpl;

class CosmeticFilter : public mojom::CosmeticFilter {
 public:
  CosmeticFilter(base::WeakPtr<RuleServiceImpl> rule_service,
                 content::ChildProcessId process_id,
                 int frame_id);
  CosmeticFilter(const CosmeticFilter&) = delete;
  CosmeticFilter& operator=(const CosmeticFilter&) = delete;

  ~CosmeticFilter() override;

  void ShouldAllowWebRTC(const ::GURL& document_url,
                         const std::vector<::GURL>& ice_servers,
                         ShouldAllowWebRTCCallback callback) override;

 private:
  base::WeakPtr<RuleServiceImpl> rule_service_;
  content::ChildProcessId process_id_;
  int frame_id_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_COSMETIC_FILTER_H_
