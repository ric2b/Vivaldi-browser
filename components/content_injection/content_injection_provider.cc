// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/content_injection/content_injection_provider.h"

#include "components/content_injection/content_injection_service.h"

namespace content_injection {
Provider::Provider() = default;

Provider::~Provider() {
  if (!service_)
    return;
  bool removed = service_->RemoveProvider(this);
  DCHECK(removed);
}

void Provider::OnAddedToService(base::WeakPtr<Service> service) {
  service_ = std::move(service);
}

void Provider::OnRemovedFromService() {
  service_.reset();
}
}  // namespace content_injection