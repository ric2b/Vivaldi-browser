#include "components/history/core/browser/top_sites_impl.h"

namespace history {

// Added by Vivaldi
void TopSitesImpl::UpdateNow() {
  StartQueryForMostVisited();
}

void TopSitesImpl::SetAggressiveUpdate() {
  vivaldi_aggressive_update_ = true;
}

}  // namespace history
