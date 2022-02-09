// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/simple_feature.h"

namespace extensions {

bool ComplexFeature::IsVivaldiFeature() const {
  return features_[0]->IsVivaldiFeature();
}

void ComplexFeature::set_vivaldi(bool flag) {
  features_[0]->set_vivaldi(flag);
}

bool SimpleFeature::IsVivaldiFeature() const {
  return is_vivaldi_feature_;
}

void SimpleFeature::set_vivaldi(bool flag) {
  is_vivaldi_feature_ = flag;
}

}  // namespace extensions
