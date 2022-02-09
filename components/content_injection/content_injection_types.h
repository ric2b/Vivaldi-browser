// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_TYPES_H_
#define COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_TYPES_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "components/content_injection/mojom/content_injection.mojom.h"
#include "url/gurl.h"

namespace content_injection {
struct StaticInjectionItem {
  mojom::InjectionItemMetadata metadata;

  StaticInjectionItem();
  ~StaticInjectionItem();
  StaticInjectionItem(const StaticInjectionItem& other);
  StaticInjectionItem& operator=(const StaticInjectionItem& other);

  base::StringPiece content;
};
}  // namespace content_injection

#endif  // COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_TYPES_H_
