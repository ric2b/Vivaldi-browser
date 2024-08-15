// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_COSMETIC_FILTER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_COSMETIC_FILTER_H_

#include "base/memory/weak_ptr.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/request_filter/adblock_filter/mojom/adblock_cosmetic_filter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class RenderFrameHost;
}

namespace adblock_filter {
class RulesIndexManager;

class CosmeticFilter : public mojom::CosmeticFilter {
 public:
  static void Create(content::RenderFrameHost* frame,
                     mojo::PendingReceiver<mojom::CosmeticFilter> receiver);
  ~CosmeticFilter() override;

  void Initialize(std::array<base::WeakPtr<RulesIndexManager>, kRuleGroupCount>
                      index_managers);

  void ShouldAllowWebRTC(const ::GURL& document_url,
                         const std::vector<::GURL>& ice_servers,
                         ShouldAllowWebRTCCallback callback) override;

 private:
  CosmeticFilter(int process_id, int frame_id);
  CosmeticFilter(const CosmeticFilter&) = delete;
  CosmeticFilter& operator=(const CosmeticFilter&) = delete;

  int process_id_;
  int frame_id_;
  std::array<base::WeakPtr<RulesIndexManager>, kRuleGroupCount> index_managers_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_COSMETIC_FILTER_H_
