/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"

#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLDataListElement::HTMLDataListElement(Document& document)
    : HTMLElement(html_names::kDatalistTag, document) {
  UseCounter::Count(document, WebFeature::kDataListElement);
  document.IncrementDataListCount();
}

HTMLDataListOptionsCollection* HTMLDataListElement::options() {
  return EnsureCachedCollection<HTMLDataListOptionsCollection>(
      kDataListOptions);
}

void HTMLDataListElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (!change.ByParser()) {
    GetTreeScope().GetIdTargetObserverRegistry().NotifyObservers(
        GetIdAttribute());
  }
}

void HTMLDataListElement::FinishParsingChildren() {
  HTMLElement::FinishParsingChildren();
  GetTreeScope().GetIdTargetObserverRegistry().NotifyObservers(
      GetIdAttribute());
}

void HTMLDataListElement::OptionElementChildrenChanged() {
  GetTreeScope().GetIdTargetObserverRegistry().NotifyObservers(
      GetIdAttribute());
}

void HTMLDataListElement::DidMoveToNewDocument(Document& old_doc) {
  HTMLElement::DidMoveToNewDocument(old_doc);
  old_doc.DecrementDataListCount();
  GetDocument().IncrementDataListCount();
}

void HTMLDataListElement::Prefinalize() {
  GetDocument().DecrementDataListCount();
}

HTMLSelectElement* HTMLDataListElement::ParentSelect() const {
  if (!RuntimeEnabledFeatures::StylableSelectEnabled()) {
    return nullptr;
  }
  if (auto* select = DynamicTo<HTMLSelectElement>(parentNode())) {
    return select;
  }
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (auto* select = DynamicTo<HTMLSelectElement>(root->host())) {
      return select;
    }
  }
  return nullptr;
}

Node::InsertionNotificationRequest HTMLDataListElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (RuntimeEnabledFeatures::StylableSelectEnabled()) {
    HTMLSelectElement* parent_select = nullptr;
    if (parentNode() == insertion_point &&
        IsA<HTMLSelectElement>(insertion_point)) {
      parent_select = To<HTMLSelectElement>(&insertion_point);
    } else if (ShadowRoot* root = ContainingShadowRoot()) {
      parent_select = DynamicTo<HTMLSelectElement>(root->host());
    }
    if (parent_select) {
      EnsurePopoverData()->setType(PopoverValueType::kAuto);
      parent_select->IncrementImplicitlyAnchoredElementCount();
    }
  }
  return HTMLElement::InsertedInto(insertion_point);
}

void HTMLDataListElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);

  if (!parentNode() && RuntimeEnabledFeatures::StylableSelectEnabled()) {
    if (auto* select = DynamicTo<HTMLSelectElement>(insertion_point)) {
      // Clean up the popover data we set in InsertedInto. If this datalist is
      // still considered select-associated, then UpdatePopoverAttribute will
      // early out.
      UpdatePopoverAttribute(FastGetAttribute(html_names::kPopoverAttr));
      select->DecrementImplicitlyAnchoredElementCount();
    }
  }
}

void HTMLDataListElement::ShowPopoverInternal(Element* invoker,
                                              ExceptionState* exception_state) {
  HTMLElement::ShowPopoverInternal(invoker, exception_state);
  if (exception_state && exception_state->HadException()) {
    return;
  }

  if (auto* select = ParentSelect()) {
    if (select->IsAppearanceBaseSelect()) {
      CHECK(RuntimeEnabledFeatures::StylableSelectEnabled());
      // MenuListSelectType::ManuallyAssignSlots changes behavior based on
      // whether the popover is opened or closed.
      select->GetShadowRoot()->SetNeedsAssignmentRecalc();
      // This is a StylableSelect popup. When it is shown, we should focus the
      // selected option.
      if (auto* option = select->SelectedOption()) {
        option->Focus(FocusParams(FocusTrigger::kScript));
      }
      select->PseudoStateChanged(CSSSelector::kPseudoOpen);
    }
  }
}

void HTMLDataListElement::HidePopoverInternal(
    HidePopoverFocusBehavior focus_behavior,
    HidePopoverTransitionBehavior event_firing,
    ExceptionState* exception_state) {
  HTMLElement::HidePopoverInternal(focus_behavior, event_firing,
                                   exception_state);
  if (auto* select = ParentSelect()) {
    if (select->IsAppearanceBaseSelect()) {
      CHECK(RuntimeEnabledFeatures::StylableSelectEnabled());
      // MenuListSelectType::ManuallyAssignSlots changes behavior based on
      // whether the popover is opened or closed.
      select->GetShadowRoot()->SetNeedsAssignmentRecalc();
      select->PseudoStateChanged(CSSSelector::kPseudoOpen);
    }
  }
}

void HTMLDataListElement::ShowPopoverForSelectElement() {
  CHECK(ParentSelect());
  ShowPopoverInternal(/*invoker=*/nullptr, /*exception_state=*/nullptr);
}

void HTMLDataListElement::HidePopoverForSelectElement() {
  CHECK(ParentSelect());
  HidePopoverInternal(
      HidePopoverFocusBehavior::kFocusPreviousElement,
      HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
      /*exception_state=*/nullptr);
}

}  // namespace blink
