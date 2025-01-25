/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_ShaderCodeDictionary_DEFINED
#define skgpu_graphite_ShaderCodeDictionary_DEFINED

#include "include/core/SkSpan.h"
#include "include/private/base/SkTo.h"
#include "src/base/SkArenaAlloc.h"
#include "src/base/SkEnumBitMask.h"
#include "src/base/SkSpinlock.h"
#include "src/core/SkKnownRuntimeEffects.h"
#include "src/core/SkTHash.h"
#include "src/gpu/graphite/BuiltInCodeSnippetID.h"
#include "src/gpu/graphite/PaintParamsKey.h"
#include "src/gpu/graphite/ResourceTypes.h"
#include "src/gpu/graphite/Uniform.h"
#include "src/gpu/graphite/UniquePaintParamsID.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

class SkRuntimeEffect;

namespace skgpu::graphite {

// TODO: How to represent the type (e.g., 2D) of texture being sampled?
class TextureAndSampler {
public:
    constexpr TextureAndSampler(const char* name) : fName(name) {}

    const char* name() const { return fName; }

private:
    const char* fName;
};

enum class SnippetRequirementFlags : uint32_t {
    kNone             = 0x0,
    // Signature of the ShaderNode
    kLocalCoords      = 0x1,
    kPriorStageOutput = 0x2,  // AKA the "input" color, or the "src" argument for a blender
    kBlenderDstColor  = 0x4,  // The "dst" argument for a blender
    // Special values and/or behaviors required for the snippet
    kPrimitiveColor   = 0x8,
    kGradientBuffer   = 0x10,
    kStoresData       = 0x20, // Indicates that the node stores numerical data
};
SK_MAKE_BITMASK_OPS(SnippetRequirementFlags)

class ShaderInfo;
class ShaderNode;

// ShaderSnippets define the "ABI" of a SkSL module function and its required uniform data, as
// well as functions for generating the invoking SkSL. Snippets are composed into an effect tree
// using ShaderNodes.
struct ShaderSnippet {
    using GeneratePreambleForSnippetFn = std::string (*)(const ShaderInfo& shaderInfo,
                                                         const ShaderNode*);
    struct Args {
        std::string fPriorStageOutput;
        std::string fBlenderDstColor;
        std::string fFragCoord;
    };

    static const Args kDefaultArgs;

    ShaderSnippet() = default;

    ShaderSnippet(const char* name,
                  const char* staticFn,
                  SkEnumBitMask<SnippetRequirementFlags> snippetRequirementFlags,
                  SkSpan<const Uniform> uniforms,
                  SkSpan<const TextureAndSampler> texturesAndSamplers = {},
                  GeneratePreambleForSnippetFn preambleGenerator = nullptr,
                  int numChildren = 0)
            : fName(name)
            , fStaticFunctionName(staticFn)
            , fSnippetRequirementFlags(snippetRequirementFlags)
            , fUniforms(uniforms)
            , fTexturesAndSamplers(texturesAndSamplers)
            , fNumChildren(numChildren)
            , fPreambleGenerator(preambleGenerator) {
        // Must always provide a name; static function is not optional if using the default (null)
        // generation logic.
        SkASSERT(name);
        SkASSERT(staticFn || preambleGenerator);
    }

    bool needsLocalCoords() const {
        return SkToBool(fSnippetRequirementFlags & SnippetRequirementFlags::kLocalCoords);
    }
    bool needsPriorStageOutput() const {
        return SkToBool(fSnippetRequirementFlags & SnippetRequirementFlags::kPriorStageOutput);
    }
    bool needsBlenderDstColor() const {
        return SkToBool(fSnippetRequirementFlags & SnippetRequirementFlags::kBlenderDstColor);
    }
    bool storesData() const {
        return SkToBool(fSnippetRequirementFlags & SnippetRequirementFlags::kStoresData);
    }

    const char* fName = nullptr;
    const char* fStaticFunctionName = nullptr;

    // The features and args that this shader snippet requires in order to be invoked
    SkEnumBitMask<SnippetRequirementFlags> fSnippetRequirementFlags{SnippetRequirementFlags::kNone};

    // If not null, the list of uniforms in `fUniforms` describes an existing struct type declared
    // in the Graphite modules with the given name. Instead of inlining the each uniform in the
    // top-level interface block or aggregate struct, there will be a single member of this struct's
    // type.
    const char* fUniformStructName = nullptr;
    // If the uniforms are being embedded as a sub-struct, this is the required starting alignment.
    int fRequiredAlignment = -1;

    skia_private::TArray<Uniform> fUniforms;
    skia_private::TArray<TextureAndSampler> fTexturesAndSamplers;

    int fNumChildren = 0;
    GeneratePreambleForSnippetFn fPreambleGenerator = nullptr;
};

// ShaderNodes organize snippets into an effect tree, and provide random access to the dynamically
// bound child snippets. Each node has a fixed number of children defined by its code ID
// (either a BuiltInCodeSnippetID or a runtime effect's assigned ID). All children are non-null.
// A ShaderNode tree represents a decompressed PaintParamsKey.
class ShaderNode {
public:
    // ShaderNodes should be created in conjunction with an SkArenaAlloc that owns all nodes.
    ShaderNode(const ShaderSnippet* snippet,
               SkSpan<const ShaderNode*> children,
               int codeID,
               int keyIndex,
               SkSpan<const uint32_t> data)
            : fEntry(snippet)
            , fChildren(children)
            , fCodeID(codeID)
            , fKeyIndex(keyIndex)
            , fRequiredFlags(snippet->fSnippetRequirementFlags)
            , fData(data) {
        SkASSERT(children.size() == (size_t) fEntry->fNumChildren);

        const bool isCompose = codeID == (int) BuiltInCodeSnippetID::kCompose ||
                               codeID == (int) BuiltInCodeSnippetID::kBlendCompose;
        for (const ShaderNode* child : children) {
            // Runtime effects invoke children with explicit parameters so those requirements never
            // need to propagate to the root. Similarly, compose only needs to propagate the
            // variable parameters for the inner children.
            SkEnumBitMask<SnippetRequirementFlags> mask = SnippetRequirementFlags::kNone;
            if (codeID >= kBuiltInCodeSnippetIDCount || (isCompose && child == children.back())) {
                // Only mask off the variable arguments; any special behaviors always propagate.
                mask = SnippetRequirementFlags::kLocalCoords |
                       SnippetRequirementFlags::kPriorStageOutput |
                       SnippetRequirementFlags::kBlenderDstColor;
            }

            fRequiredFlags |= (child->requiredFlags() & ~mask);
        }
        // Data should only be provided if the snippet has the kStoresData flag.
        SkASSERT(fData.empty() || snippet->storesData());
    }

    std::string generateDefaultPreamble(const ShaderInfo& shaderInfo) const;
    std::string invokeAndAssign(const ShaderInfo& shaderInfo,
                                const ShaderSnippet::Args& args,
                                std::string* funcBody) const;

    int32_t codeSnippetId() const { return fCodeID; }
    int32_t keyIndex() const { return fKeyIndex; }
    const ShaderSnippet* entry() const { return fEntry; }

    SkEnumBitMask<SnippetRequirementFlags> requiredFlags() const { return fRequiredFlags; }

    int numChildren() const { return fEntry->fNumChildren; }
    SkSpan<const ShaderNode*> children() const { return fChildren; }
    const ShaderNode* child(int childIndex) const { return fChildren[childIndex]; }

    SkSpan<const uint32_t> data() const { return fData; }

private:
    const ShaderSnippet* fEntry; // Owned by the ShaderCodeDictionary
    SkSpan<const ShaderNode*> fChildren; // Owned by the ShaderInfo's arena

    int32_t fCodeID;
    int32_t fKeyIndex; // index back to PaintParamsKey, unique across nodes within a ShaderInfo

    SkEnumBitMask<SnippetRequirementFlags> fRequiredFlags;
    SkSpan<const uint32_t> fData; // Subspan of PaintParamsKey's fData; shares same owner
};

// ShaderCodeDictionary is a thread-safe dictionary of ShaderSnippets to code IDs for use with
// creating PaintParamKeys, as well as assigning unique IDs to each encountered PaintParamKey.
// It defines ShaderSnippets for every BuiltInCodeSnippetID and maintains records for IDs per
// SkRuntimeEffect, including de-duplicating equivalent SkRuntimeEffect objects.
class ShaderCodeDictionary {
public:
    ShaderCodeDictionary(Layout layout);

    UniquePaintParamsID findOrCreate(PaintParamsKeyBuilder*) SK_EXCLUDES(fSpinLock);

    PaintParamsKey lookup(UniquePaintParamsID) const SK_EXCLUDES(fSpinLock);

    SkString idToString(UniquePaintParamsID id) const {
        return this->lookup(id).toString(this, /*includeData=*/false);
    }

#if defined(SK_DEBUG)
    bool isValidID(int snippetID) const SK_EXCLUDES(fSpinLock);

    void dump(UniquePaintParamsID) const;
#endif

    // This method can return nullptr
    const ShaderSnippet* getEntry(int codeSnippetID) const SK_EXCLUDES(fSpinLock);
    const ShaderSnippet* getEntry(BuiltInCodeSnippetID codeSnippetID) const {
        // Built-in code snippets are initialized once so there is no need to take a lock
        return &fBuiltInCodeSnippets[SkTo<int>(codeSnippetID)];
    }

    int findOrCreateRuntimeEffectSnippet(const SkRuntimeEffect* effect) SK_EXCLUDES(fSpinLock);

private:
    const char* addTextToArena(std::string_view text);

    SkSpan<const Uniform> convertUniforms(const SkRuntimeEffect* effect);
    ShaderSnippet convertRuntimeEffect(const SkRuntimeEffect* effect, const char* name);

    const Layout fLayout;

    std::array<ShaderSnippet, kBuiltInCodeSnippetIDCount> fBuiltInCodeSnippets;

    using KnownRuntimeEffectArray = std::array<ShaderSnippet, SkKnownRuntimeEffects::kStableKeyCnt>;
    KnownRuntimeEffectArray fKnownRuntimeEffectCodeSnippets SK_GUARDED_BY(fSpinLock);

    // The value returned from 'getEntry' must be stable so, hold the user-defined code snippet
    // entries as pointers.
    using RuntimeEffectArray = skia_private::TArray<ShaderSnippet>;
    RuntimeEffectArray fUserDefinedCodeSnippets SK_GUARDED_BY(fSpinLock);

    // TODO: can we do something better given this should have write-seldom/read-often behavior?
    mutable SkSpinlock fSpinLock;

    using PaintIDMap = skia_private::THashMap<PaintParamsKey,
                                              UniquePaintParamsID,
                                              PaintParamsKey::Hash>;

    PaintIDMap fPaintKeyToID SK_GUARDED_BY(fSpinLock);
    skia_private::TArray<PaintParamsKey> fIDToPaintKey SK_GUARDED_BY(fSpinLock);

    SK_BEGIN_REQUIRE_DENSE
    struct RuntimeEffectKey {
        uint32_t fHash;
        uint32_t fUniformSize;

        bool operator==(RuntimeEffectKey rhs) const {
            return fHash == rhs.fHash && fUniformSize == rhs.fUniformSize;
        }
    };
    SK_END_REQUIRE_DENSE

    // A map from RuntimeEffectKeys (hash plus uniforms) to code-snippet IDs. RuntimeEffectKeys
    // don't track the lifetime of a runtime effect at all; they live forever, and a newly-
    // instantiated runtime effect with the same program as a previously-discarded effect will reuse
    // an existing ID. Entries in the runtime-effect map are never removed; they only disappear when
    // the context is discarded, which takes the ShaderCodeDictionary along with it. However, they
    // are extremely small (< 20 bytes) so the memory footprint should be unnoticeable.
    using RuntimeEffectMap = skia_private::THashMap<RuntimeEffectKey, int32_t>;
    RuntimeEffectMap fRuntimeEffectMap SK_GUARDED_BY(fSpinLock);

    // This arena holds:
    //   - the backing data for PaintParamsKeys in `fPaintKeyToID` and `fIDToPaintKey`
    //   - Uniform data created by `findOrCreateRuntimeEffectSnippet`
    // and in all cases is guarded by `fSpinLock`
    SkArenaAlloc fArena{256};
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_ShaderCodeDictionary_DEFINED
