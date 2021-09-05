// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/content_injection/content_injection_types.h"

namespace content_injection {
StaticInjectionItem::StaticInjectionItem() = default;
StaticInjectionItem::~StaticInjectionItem() = default;
StaticInjectionItem::StaticInjectionItem(const StaticInjectionItem& other) =
    default;
StaticInjectionItem& StaticInjectionItem::operator=(
    const StaticInjectionItem& other) = default;
}  // namespace content_injection