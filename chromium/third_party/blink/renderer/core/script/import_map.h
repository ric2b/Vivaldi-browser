// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_IMPORT_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_IMPORT_MAP_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/import_map_error.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class ConsoleLogger;
class ExecutionContext;
class ImportMapError;
class JSONObject;
class ParsedSpecifier;

// Import maps.
// https://html.spec.whatwg.org/C#import-maps
class CORE_EXPORT ImportMap final : public GarbageCollected<ImportMap> {
 public:
  static ImportMap* Parse(const String& text,
                          const KURL& base_url,
                          ExecutionContext& context,
                          std::optional<ImportMapError>* error_to_rethrow);

  // <spec href="https://html.spec.whatwg.org/C#module-specifier-map">A
  // specifier map is an ordered map from strings to resolution results.</spec>
  //
  // An invalid KURL corresponds to a null resolution result in the spec.
  //
  // In Blink, we actually use an unordered map here, and related algorithms
  // are implemented differently from the spec.
  using SpecifierMap = HashMap<String, KURL>;

  // <spec href="https://html.spec.whatwg.org/C#concept-import-map-scopes">an
  // ordered map of URLs to specifier maps.</spec>
  using ScopeEntryType = std::pair<String, SpecifierMap>;
  using ScopeType = Vector<ScopeEntryType>;

  using IntegrityMap = HashMap<KURL, String>;

  // Empty import map.
  ImportMap();

  ImportMap(SpecifierMap&& imports,
            ScopeType&& scopes,
            IntegrityMap&& integrity);

  // Return values of Resolve(), ResolveImportsMatch() and
  // ResolveImportsMatchInternal():
  // - std::nullopt: corresponds to returning a null in the spec,
  //   i.e. allowing fallback to a less specific scope etc.
  // - An invalid KURL: corresponds to throwing an error in the spec.
  // - A valid KURL: corresponds to returning a valid URL in the spec.
  std::optional<KURL> Resolve(const ParsedSpecifier&,
                              const KURL& base_url,
                              String* debug_message) const;
  String GetIntegrity(const KURL& module_url) const;

  String ToStringForTesting() const;

  void Trace(Visitor*) const {}

 private:
  using MatchResult = SpecifierMap::const_iterator;

  // https://html.spec.whatwg.org/C#resolving-an-imports-match
  std::optional<KURL> ResolveImportsMatch(const ParsedSpecifier&,
                                          const SpecifierMap&,
                                          String* debug_message) const;
  std::optional<MatchResult> MatchPrefix(const ParsedSpecifier&,
                                         const SpecifierMap&) const;
  static SpecifierMap SortAndNormalizeSpecifierMap(const JSONObject* imports,
                                                   const KURL& base_url,
                                                   ConsoleLogger&);

  KURL ResolveImportsMatchInternal(const String& normalizedSpecifier,
                                   const MatchResult&,
                                   String* debug_message) const;

  // https://html.spec.whatwg.org/C#concept-import-map-imports
  SpecifierMap imports_;

  // https://html.spec.whatwg.org/C#concept-import-map-scopes
  ScopeType scopes_;

  // https://html.spec.whatwg.org/C#concept-import-map-integrity
  IntegrityMap integrity_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_IMPORT_MAP_H_
