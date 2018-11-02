/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PropertySetCSSStyleDeclaration_h
#define PropertySetCSSStyleDeclaration_h

#include "core/css/AbstractPropertySetCSSStyleDeclaration.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class MutableCSSPropertyValueSet;
class PropertyRegistry;

class PropertySetCSSStyleDeclaration
    : public AbstractPropertySetCSSStyleDeclaration {
 public:
  PropertySetCSSStyleDeclaration(MutableCSSPropertyValueSet& property_set)
      : property_set_(&property_set) {}

  virtual void Trace(blink::Visitor*);

 protected:
  MutableCSSPropertyValueSet& PropertySet() const final {
    DCHECK(property_set_);
    return *property_set_;
  }

  PropertyRegistry* GetPropertyRegistry() const override { return nullptr; }

  Member<MutableCSSPropertyValueSet> property_set_;  // Cannot be null
};

}  // namespace blink

#endif
