// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/core/ir/transform/builtin_polyfill.h"

#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/core/type/sampled_texture.h"

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

namespace tint::core::ir::transform {

namespace {

/// Constant value used to polyfill the radians() builtin.
static constexpr double kDegToRad = 0.017453292519943295474;

/// Constant value used to polyfill the degrees() builtin.
static constexpr double kRadToDeg = 57.295779513082322865;

/// PIMPL state for the transform.
struct State {
    /// The polyfill config.
    const BuiltinPolyfillConfig& config;

    /// The IR module.
    Module& ir;

    /// The IR builder.
    Builder b{ir};

    /// The type manager.
    core::type::Manager& ty{ir.Types()};

    /// The symbol table.
    SymbolTable& sym{ir.symbols};

    /// Process the module.
    void Process() {
        // Find the builtin call instructions that may need to be polyfilled.
        Vector<ir::CoreBuiltinCall*, 4> worklist;
        for (auto* inst : ir.Instructions()) {
            if (auto* builtin = inst->As<ir::CoreBuiltinCall>()) {
                switch (builtin->Func()) {
                    case core::BuiltinFn::kClamp:
                        if (config.clamp_int &&
                            builtin->Result(0)->Type()->is_integer_scalar_or_vector()) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kCountLeadingZeros:
                        if (config.count_leading_zeros) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kCountTrailingZeros:
                        if (config.count_trailing_zeros) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kDegrees:
                        if (config.degrees) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kExtractBits:
                        if (config.extract_bits != BuiltinPolyfillLevel::kNone) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kFirstLeadingBit:
                        if (config.first_leading_bit) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kFirstTrailingBit:
                        if (config.first_trailing_bit) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kInsertBits:
                        if (config.insert_bits != BuiltinPolyfillLevel::kNone) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kRadians:
                        if (config.radians) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kSaturate:
                        if (config.saturate) {
                            worklist.Push(builtin);
                        }
                        break;
                    case core::BuiltinFn::kTextureSampleBaseClampToEdge:
                        if (config.texture_sample_base_clamp_to_edge_2d_f32) {
                            auto* tex =
                                builtin->Args()[0]->Type()->As<core::type::SampledTexture>();
                            if (tex && tex->dim() == core::type::TextureDimension::k2d &&
                                tex->type()->Is<core::type::F32>()) {
                                worklist.Push(builtin);
                            }
                        }
                        break;
                    case core::BuiltinFn::kDot4U8Packed:
                    case core::BuiltinFn::kDot4I8Packed: {
                        if (config.dot_4x8_packed) {
                            worklist.Push(builtin);
                        }
                        break;
                    }
                    case core::BuiltinFn::kPack4XI8:
                    case core::BuiltinFn::kPack4XU8:
                    case core::BuiltinFn::kPack4XI8Clamp:
                    case core::BuiltinFn::kUnpack4XI8:
                    case core::BuiltinFn::kUnpack4XU8: {
                        if (config.pack_unpack_4x8) {
                            worklist.Push(builtin);
                        }
                        break;
                    }
                    case core::BuiltinFn::kPack4XU8Clamp: {
                        if (config.pack_4xu8_clamp) {
                            worklist.Push(builtin);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        // Polyfill the builtin call instructions that we found.
        for (auto* builtin : worklist) {
            switch (builtin->Func()) {
                case core::BuiltinFn::kClamp:
                    ClampInt(builtin);
                    break;
                case core::BuiltinFn::kCountLeadingZeros:
                    CountLeadingZeros(builtin);
                    break;
                case core::BuiltinFn::kCountTrailingZeros:
                    CountTrailingZeros(builtin);
                    break;
                case core::BuiltinFn::kDegrees:
                    Degrees(builtin);
                    break;
                case core::BuiltinFn::kExtractBits:
                    ExtractBits(builtin);
                    break;
                case core::BuiltinFn::kFirstLeadingBit:
                    FirstLeadingBit(builtin);
                    break;
                case core::BuiltinFn::kFirstTrailingBit:
                    FirstTrailingBit(builtin);
                    break;
                case core::BuiltinFn::kInsertBits:
                    InsertBits(builtin);
                    break;
                case core::BuiltinFn::kRadians:
                    Radians(builtin);
                    break;
                case core::BuiltinFn::kSaturate:
                    Saturate(builtin);
                    break;
                case core::BuiltinFn::kTextureSampleBaseClampToEdge:
                    TextureSampleBaseClampToEdge_2d_f32(builtin);
                    break;
                case core::BuiltinFn::kDot4I8Packed:
                    Dot4I8Packed(builtin);
                    break;
                case core::BuiltinFn::kDot4U8Packed:
                    Dot4U8Packed(builtin);
                    break;
                case core::BuiltinFn::kPack4XI8:
                    Pack4xI8(builtin);
                    break;
                case core::BuiltinFn::kPack4XU8:
                    Pack4xU8(builtin);
                    break;
                case core::BuiltinFn::kPack4XI8Clamp:
                    Pack4xI8Clamp(builtin);
                    break;
                case core::BuiltinFn::kPack4XU8Clamp:
                    Pack4xU8Clamp(builtin);
                    break;
                case core::BuiltinFn::kUnpack4XI8:
                    Unpack4xI8(builtin);
                    break;
                case core::BuiltinFn::kUnpack4XU8:
                    Unpack4xU8(builtin);
                    break;
                default:
                    break;
            }
        }
    }

    /// Polyfill a `clamp()` builtin call for integers.
    /// @param call the builtin call instruction
    void ClampInt(ir::CoreBuiltinCall* call) {
        auto* type = call->Result(0)->Type();
        auto* e = call->Args()[0];
        auto* low = call->Args()[1];
        auto* high = call->Args()[2];

        b.InsertBefore(call, [&] {
            auto* max = b.Call(type, core::BuiltinFn::kMax, e, low);
            b.CallWithResult(call->DetachResult(), core::BuiltinFn::kMin, max, high);
        });
        call->Destroy();
    }

    /// Polyfill a `countLeadingZeros()` builtin call.
    /// @param call the builtin call instruction
    void CountLeadingZeros(ir::CoreBuiltinCall* call) {
        auto* input = call->Args()[0];
        auto* result_ty = input->Type();
        auto* uint_ty = ty.match_width(ty.u32(), result_ty);
        auto* bool_ty = ty.match_width(ty.bool_(), result_ty);

        // Make an u32 constant with the same component count as result_ty.
        auto V = [&](uint32_t u) { return b.MatchWidth(u32(u), result_ty); };

        b.InsertBefore(call, [&] {
            // %x = %input;
            // if (%x is signed) {
            //   %x = bitcast<u32>(%x)
            // }
            // %b16 = select(0, 16, %x <= 0x0000ffff);
            // %x <<= %b16;
            // %b8  = select(0, 8, %x <= 0x00ffffff);
            // %x <<= %b8;
            // %b4  = select(0, 4, %x <= 0x0fffffff);
            // %x <<= %b4;
            // %b2  = select(0, 2, %x <= 0x3fffffff);
            // %x <<= %b2;
            // %b1  = select(0, 1, %x <= 0x7fffffff);
            // %b0  = select(0, 1, %x == 0);
            // %result = (%b16 | %b8 | %b4 | %b2 | %b1) + %b0;

            auto* x = input;
            if (result_ty->is_signed_integer_scalar_or_vector()) {
                x = b.Bitcast(uint_ty, x)->Result(0);
            }
            auto* b16 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(16),
                               b.LessThanEqual(bool_ty, x, V(0x0000ffff)));
            x = b.ShiftLeft(uint_ty, x, b16)->Result(0);
            auto* b8 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(8),
                              b.LessThanEqual(bool_ty, x, V(0x00ffffff)));
            x = b.ShiftLeft(uint_ty, x, b8)->Result(0);
            auto* b4 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(4),
                              b.LessThanEqual(bool_ty, x, V(0x0fffffff)));
            x = b.ShiftLeft(uint_ty, x, b4)->Result(0);
            auto* b2 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(2),
                              b.LessThanEqual(bool_ty, x, V(0x3fffffff)));
            x = b.ShiftLeft(uint_ty, x, b2)->Result(0);
            auto* b1 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(1),
                              b.LessThanEqual(bool_ty, x, V(0x7fffffff)));
            auto* b0 =
                b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(1), b.Equal(bool_ty, x, V(0)));
            Instruction* result = b.Add(
                uint_ty,
                b.Or(
                    uint_ty, b16,
                    b.Or(uint_ty, b8, b.Or(uint_ty, b4, b.Or(uint_ty, b2, b.Or(uint_ty, b1, b0))))),
                b0);
            if (result_ty->is_signed_integer_scalar_or_vector()) {
                result = b.Bitcast(result_ty, result);
            }
            result->SetResults(Vector{call->DetachResult()});
        });
        call->Destroy();
    }

    /// Polyfill a `countTrailingZeros()` builtin call.
    /// @param call the builtin call instruction
    void CountTrailingZeros(ir::CoreBuiltinCall* call) {
        auto* input = call->Args()[0];
        auto* result_ty = input->Type();
        auto* uint_ty = ty.match_width(ty.u32(), result_ty);
        auto* bool_ty = ty.match_width(ty.bool_(), result_ty);

        // Make an u32 constant with the same component count as result_ty.
        auto V = [&](uint32_t u) { return b.MatchWidth(u32(u), result_ty); };

        b.InsertBefore(call, [&] {
            // %x = %input;
            // if (%x is signed) {
            //   %x = bitcast<u32>(%x)
            // }
            // %b16 = select(0, 16, (%x & 0x0000ffff) == 0);
            // %x >>= %b16;
            // %b8  = select(0, 8,  (%x & 0x000000ff) == 0);
            // %x >>= %b8;
            // %b4  = select(0, 4,  (%x & 0x0000000f) == 0);
            // %x >>= %b4;
            // %b2  = select(0, 2,  (%x & 0x00000003) == 0);
            // %x >>= %b2;
            // %b1  = select(0, 1,  (%x & 0x00000001) == 0);
            // %b0  = select(0, 1,  (%x & 0x00000001) == 0);
            // %result = (%b16 | %b8 | %b4 | %b2 | %b1) + %b0;

            auto* x = input;
            if (result_ty->is_signed_integer_scalar_or_vector()) {
                x = b.Bitcast(uint_ty, x)->Result(0);
            }
            auto* b16 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(16),
                               b.Equal(bool_ty, b.And(uint_ty, x, V(0x0000ffff)), V(0)));
            x = b.ShiftRight(uint_ty, x, b16)->Result(0);
            auto* b8 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(8),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x000000ff)), V(0)));
            x = b.ShiftRight(uint_ty, x, b8)->Result(0);
            auto* b4 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(4),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x0000000f)), V(0)));
            x = b.ShiftRight(uint_ty, x, b4)->Result(0);
            auto* b2 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(2),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x00000003)), V(0)));
            x = b.ShiftRight(uint_ty, x, b2)->Result(0);
            auto* b1 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(1),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x00000001)), V(0)));
            auto* b0 =
                b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(1), b.Equal(bool_ty, x, V(0)));
            Instruction* result = b.Add(
                uint_ty,
                b.Or(uint_ty, b16, b.Or(uint_ty, b8, b.Or(uint_ty, b4, b.Or(uint_ty, b2, b1)))),
                b0);
            if (result_ty->is_signed_integer_scalar_or_vector()) {
                result = b.Bitcast(result_ty, result);
            }
            result->SetResults(Vector{call->DetachResult()});
        });
        call->Destroy();
    }

    /// Polyfill an `degrees()` builtin call.
    /// @param call the builtin call instruction
    void Degrees(ir::CoreBuiltinCall* call) {
        auto* arg = call->Args()[0];
        auto* type = arg->Type()->DeepestElement();
        ir::Value* value = nullptr;
        if (type->Is<core::type::F16>()) {
            value = b.Constant(f16(kRadToDeg));
        } else if (type->Is<core::type::F32>()) {
            value = b.Constant(f32(kRadToDeg));
        }
        b.InsertBefore(call, [&] {
            auto* mul = b.Multiply(arg->Type(), arg, value);
            mul->SetResults(Vector{call->DetachResult()});
        });
        call->Destroy();
    }

    /// Polyfill an `extractBits()` builtin call.
    /// @param call the builtin call instruction
    void ExtractBits(ir::CoreBuiltinCall* call) {
        auto* offset = call->Args()[1];
        auto* count = call->Args()[2];

        switch (config.extract_bits) {
            case BuiltinPolyfillLevel::kClampOrRangeCheck: {
                b.InsertBefore(call, [&] {
                    // Replace:
                    //    extractBits(e, offset, count)
                    // With:
                    //    let o = min(offset, 32);
                    //    let c = min(count, w - o);
                    //    extractBits(e, o, c);
                    auto* o = b.Call(ty.u32(), core::BuiltinFn::kMin, offset, 32_u);
                    auto* c = b.Call(ty.u32(), core::BuiltinFn::kMin, count,
                                     b.Subtract(ty.u32(), 32_u, o));
                    call->SetOperand(ir::CoreBuiltinCall::kArgsOperandOffset + 1, o->Result(0));
                    call->SetOperand(ir::CoreBuiltinCall::kArgsOperandOffset + 2, c->Result(0));
                });
                break;
            }
            default:
                TINT_UNIMPLEMENTED() << "extractBits polyfill level";
        }
    }

    /// Polyfill a `firstLeadingBit()` builtin call.
    /// @param call the builtin call instruction
    void FirstLeadingBit(ir::CoreBuiltinCall* call) {
        auto* input = call->Args()[0];
        auto* result_ty = input->Type();
        auto* uint_ty = ty.match_width(ty.u32(), result_ty);
        auto* bool_ty = ty.match_width(ty.bool_(), result_ty);

        // Make an u32 constant with the same component count as result_ty.
        auto V = [&](uint32_t u) { return b.MatchWidth(u32(u), result_ty); };

        b.InsertBefore(call, [&] {
            // %x = %input;
            // if (%x is signed) {
            //   %x = select(u32(%x), ~u32(%x), x > 0x80000000);
            // }
            // %b16 = select(16, 0, (%x & 0xffff0000) == 0);
            // %x >>= %b16;
            // %b8  = select(8, 0,  (%x & 0x0000ff00) == 0);
            // %x >>= %b8;
            // %b4  = select(4, 0,  (%x & 0x000000f0) == 0);
            // %x >>= %b4;
            // %b2  = select(2, 0,  (%x & 0x0000000c) == 0);
            // %x >>= %b2;
            // %b1  = select(1, 0,  (%x & 0x00000002) == 0);
            // %result = %b16 | %b8 | %b4 | %b2 | %b1;
            // %result = select(%result, 0xffffffff, %x == 0);

            auto* x = input;
            if (result_ty->is_signed_integer_scalar_or_vector()) {
                x = b.Bitcast(uint_ty, x)->Result(0);
                auto* inverted = b.Complement(uint_ty, x);
                x = b.Call(uint_ty, core::BuiltinFn::kSelect, inverted, x,
                           b.LessThan(bool_ty, x, V(0x80000000)))
                        ->Result(0);
            }
            auto* b16 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(16), V(0),
                               b.Equal(bool_ty, b.And(uint_ty, x, V(0xffff0000)), V(0)));
            x = b.ShiftRight(uint_ty, x, b16)->Result(0);
            auto* b8 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(8), V(0),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x0000ff00)), V(0)));
            x = b.ShiftRight(uint_ty, x, b8)->Result(0);
            auto* b4 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(4), V(0),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x000000f0)), V(0)));
            x = b.ShiftRight(uint_ty, x, b4)->Result(0);
            auto* b2 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(2), V(0),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x0000000c)), V(0)));
            x = b.ShiftRight(uint_ty, x, b2)->Result(0);
            auto* b1 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(1), V(0),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x00000002)), V(0)));
            Instruction* result =
                b.Or(uint_ty, b16, b.Or(uint_ty, b8, b.Or(uint_ty, b4, b.Or(uint_ty, b2, b1))));
            result = b.Call(uint_ty, core::BuiltinFn::kSelect, result, V(0xffffffff),
                            b.Equal(bool_ty, x, V(0)));
            if (result_ty->is_signed_integer_scalar_or_vector()) {
                result = b.Bitcast(result_ty, result);
            }
            result->SetResults(Vector{call->DetachResult()});
        });
        call->Destroy();
    }

    /// Polyfill a `firstTrailingBit()` builtin call.
    /// @param call the builtin call instruction
    void FirstTrailingBit(ir::CoreBuiltinCall* call) {
        auto* input = call->Args()[0];
        auto* result_ty = input->Type();
        auto* uint_ty = ty.match_width(ty.u32(), result_ty);
        auto* bool_ty = ty.match_width(ty.bool_(), result_ty);

        // Make an u32 constant with the same component count as result_ty.
        auto V = [&](uint32_t u) { return b.MatchWidth(u32(u), result_ty); };

        b.InsertBefore(call, [&] {
            // %x = %input;
            // if (%x is signed) {
            //   %x = bitcast<u32>(%x)
            // }
            // %b16 = select(0, 16, (%x & 0x0000ffff) == 0);
            // %x >>= %b16;
            // %b8  = select(0, 8,  (%x & 0x000000ff) == 0);
            // %x >>= %b8;
            // %b4  = select(0, 4,  (%x & 0x0000000f) == 0);
            // %x >>= %b4;
            // %b2  = select(0, 2,  (%x & 0x00000003) == 0);
            // %x >>= %b2;
            // %b1  = select(0, 1,  (%x & 0x00000001) == 0);
            // %result = %b16 | %b8 | %b4 | %b2 | %b1;
            // %result = select(%result, 0xffffffff, %x == 0);

            auto* x = input;
            if (result_ty->is_signed_integer_scalar_or_vector()) {
                x = b.Bitcast(uint_ty, x)->Result(0);
            }
            auto* b16 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(16),
                               b.Equal(bool_ty, b.And(uint_ty, x, V(0x0000ffff)), V(0)));
            x = b.ShiftRight(uint_ty, x, b16)->Result(0);
            auto* b8 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(8),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x000000ff)), V(0)));
            x = b.ShiftRight(uint_ty, x, b8)->Result(0);
            auto* b4 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(4),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x0000000f)), V(0)));
            x = b.ShiftRight(uint_ty, x, b4)->Result(0);
            auto* b2 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(2),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x00000003)), V(0)));
            x = b.ShiftRight(uint_ty, x, b2)->Result(0);
            auto* b1 = b.Call(uint_ty, core::BuiltinFn::kSelect, V(0), V(1),
                              b.Equal(bool_ty, b.And(uint_ty, x, V(0x00000001)), V(0)));
            Instruction* result =
                b.Or(uint_ty, b16, b.Or(uint_ty, b8, b.Or(uint_ty, b4, b.Or(uint_ty, b2, b1))));
            result = b.Call(uint_ty, core::BuiltinFn::kSelect, result, V(0xffffffff),
                            b.Equal(bool_ty, x, V(0)));
            if (result_ty->is_signed_integer_scalar_or_vector()) {
                result = b.Bitcast(result_ty, result);
            }
            result->SetResults(Vector{call->DetachResult()});
        });
        call->Destroy();
    }

    /// Polyfill an `insertBits()` builtin call.
    /// @param call the builtin call instruction
    void InsertBits(ir::CoreBuiltinCall* call) {
        auto* offset = call->Args()[2];
        auto* count = call->Args()[3];

        switch (config.insert_bits) {
            case BuiltinPolyfillLevel::kClampOrRangeCheck: {
                b.InsertBefore(call, [&] {
                    // Replace:
                    //    insertBits(e, offset, count)
                    // With:
                    //    let o = min(offset, 32);
                    //    let c = min(count, w - o);
                    //    insertBits(e, o, c);
                    auto* o = b.Call(ty.u32(), core::BuiltinFn::kMin, offset, 32_u);
                    auto* c = b.Call(ty.u32(), core::BuiltinFn::kMin, count,
                                     b.Subtract(ty.u32(), 32_u, o));
                    call->SetOperand(ir::CoreBuiltinCall::kArgsOperandOffset + 2, o->Result(0));
                    call->SetOperand(ir::CoreBuiltinCall::kArgsOperandOffset + 3, c->Result(0));
                });
                break;
            }
            default:
                TINT_UNIMPLEMENTED() << "insertBits polyfill level";
        }
    }

    /// Polyfill an `radians()` builtin call.
    /// @param call the builtin call instruction
    void Radians(ir::CoreBuiltinCall* call) {
        auto* arg = call->Args()[0];
        auto* type = arg->Type()->DeepestElement();
        ir::Value* value = nullptr;
        if (type->Is<core::type::F16>()) {
            value = b.Constant(f16(kDegToRad));
        } else if (type->Is<core::type::F32>()) {
            value = b.Constant(f32(kDegToRad));
        }
        b.InsertBefore(call, [&] {
            auto* mul = b.Multiply(arg->Type(), arg, value);
            mul->SetResults(Vector{call->DetachResult()});
        });
        call->Destroy();
    }

    /// Polyfill a `saturate()` builtin call.
    /// @param call the builtin call instruction
    void Saturate(ir::CoreBuiltinCall* call) {
        // Replace `saturate(x)` with `clamp(x, 0., 1.)`.
        auto* type = call->Result(0)->Type();
        ir::Constant* zero = nullptr;
        ir::Constant* one = nullptr;
        if (type->DeepestElement()->Is<core::type::F32>()) {
            zero = b.MatchWidth(0_f, type);
            one = b.MatchWidth(1_f, type);
        } else if (type->DeepestElement()->Is<core::type::F16>()) {
            zero = b.MatchWidth(0_h, type);
            one = b.MatchWidth(1_h, type);
        }
        auto* clamp = b.CallWithResult(call->DetachResult(), core::BuiltinFn::kClamp,
                                       Vector{call->Args()[0], zero, one});
        clamp->InsertBefore(call);
        call->Destroy();
    }

    /// Polyfill a `textureSampleBaseClampToEdge()` builtin call for 2D F32 textures.
    /// @param call the builtin call instruction
    void TextureSampleBaseClampToEdge_2d_f32(ir::CoreBuiltinCall* call) {
        // Replace `textureSampleBaseClampToEdge(%texture, %sample, %coords)` with:
        //   %dims       = vec2f(textureDimensions(%texture));
        //   %half_texel = vec2f(0.5) / dims;
        //   %clamped    = clamp(%coord, %half_texel, 1.0 - %half_texel);
        //   %result     = textureSampleLevel(%texture, %sampler, %clamped, 0);
        auto* texture = call->Args()[0];
        auto* sampler = call->Args()[1];
        auto* coords = call->Args()[2];
        b.InsertBefore(call, [&] {
            auto* dims = b.Call<vec2<u32>>(core::BuiltinFn::kTextureDimensions, texture);
            auto* fdims = b.Convert<vec2<f32>>(dims);
            auto* half_texel = b.Divide<vec2<f32>>(b.Splat<vec2<f32>>(0.5_f), fdims);
            auto* one_minus_half_texel = b.Subtract<vec2<f32>>(b.Splat<vec2<f32>>(1_f), half_texel);
            auto* clamped = b.Call<vec2<f32>>(core::BuiltinFn::kClamp, coords, half_texel,
                                              one_minus_half_texel);
            b.CallWithResult(call->DetachResult(), core::BuiltinFn::kTextureSampleLevel, texture,
                             sampler, clamped, 0_f);
        });
        call->Destroy();
    }

    /// Polyfill a `dot4I8Packed()` builtin call
    /// @param call the builtin call instruction
    void Dot4I8Packed(ir::CoreBuiltinCall* call) {
        // Replace `dot4I8Packed(%x,%y)` with:
        //   %unpacked_x = unpack4xI8(%x);
        //   %unpacked_y = unpack4xI8(%y);
        //   %result = dot(%unpacked_x, %unpacked_y);
        auto* x = call->Args()[0];
        auto* y = call->Args()[1];
        auto* unpacked_x = Unpack4xI8OnValue(call, x);
        auto* unpacked_y = Unpack4xI8OnValue(call, y);
        b.InsertBefore(call, [&] {
            b.CallWithResult(call->DetachResult(), core::BuiltinFn::kDot, unpacked_x, unpacked_y);
        });
        call->Destroy();
    }

    /// Polyfill a `dot4U8Packed()` builtin call
    /// @param call the builtin call instruction
    void Dot4U8Packed(ir::CoreBuiltinCall* call) {
        // Replace `dot4U8Packed(%x,%y)` with:
        //   %unpacked_x = unpack4xU8(%x);
        //   %unpacked_y = unpack4xU8(%y);
        //   %result = dot(%unpacked_x, %unpacked_y);
        auto* x = call->Args()[0];
        auto* y = call->Args()[1];
        auto* unpacked_x = Unpack4xU8OnValue(call, x);
        auto* unpacked_y = Unpack4xU8OnValue(call, y);
        b.InsertBefore(call, [&] {
            b.CallWithResult(call->DetachResult(), core::BuiltinFn::kDot, unpacked_x, unpacked_y);
        });
        call->Destroy();
    }

    /// Polyfill a `pack4xI8()` builtin call
    /// @param call the builtin call instruction
    void Pack4xI8(ir::CoreBuiltinCall* call) {
        // Replace `pack4xI8(%x)` with:
        //   %n      = vec4u(0, 8, 16, 24);
        //   %x_u32  = bitcast<vec4u>(%x)
        //   %x_u8   = (%x_u32 & vec4u(0xff)) << n;
        //   %result = dot(%x_u8, vec4u(1));
        auto* x = call->Args()[0];
        b.InsertBefore(call, [&] {
            auto* vec4u = ty.vec4<u32>();

            auto* n = b.Construct(vec4u, b.Constant(u32(0)), b.Constant(u32(8)),
                                  b.Constant(u32(16)), b.Constant(u32(24)));
            auto* x_u32 = b.Bitcast(vec4u, x);
            auto* x_u8 = b.ShiftLeft(
                vec4u, b.And(vec4u, x_u32, b.Construct(vec4u, b.Constant(u32(0xff)))), n);
            b.CallWithResult(call->DetachResult(), core::BuiltinFn::kDot, x_u8,
                             b.Construct(vec4u, (b.Constant(u32(1)))));
        });
        call->Destroy();
    }

    /// Polyfill a `pack4xU8()` builtin call
    /// @param call the builtin call instruction
    void Pack4xU8(ir::CoreBuiltinCall* call) {
        // Replace `pack4xU8(%x)` with:
        //   %n      = vec4u(0, 8, 16, 24);
        //   %x_i8   = (%x & vec4u(0xff)) << %n;
        //   %result = dot(%x_i8, vec4u(1));
        auto* x = call->Args()[0];
        b.InsertBefore(call, [&] {
            auto* vec4u = ty.vec4<u32>();

            auto* n = b.Construct(vec4u, b.Constant(u32(0)), b.Constant(u32(8)),
                                  b.Constant(u32(16)), b.Constant(u32(24)));
            auto* x_u8 =
                b.ShiftLeft(vec4u, b.And(vec4u, x, b.Construct(vec4u, b.Constant(u32(0xff)))), n);
            b.CallWithResult(call->DetachResult(), core::BuiltinFn::kDot, x_u8,
                             b.Construct(vec4u, (b.Constant(u32(1)))));
        });
        call->Destroy();
    }

    /// Polyfill a `pack4xI8Clamp()` builtin call
    /// @param call the builtin call instruction
    void Pack4xI8Clamp(ir::CoreBuiltinCall* call) {
        // Replace `pack4xI8Clamp(%x)` with:
        //   %n           = vec4u(0, 8, 16, 24);
        //   %min_i8_vec4 = vec4i(-128);
        //   %max_i8_vec4 = vec4i(127);
        //   %x_clamp     = clamp(%x, %min_i8_vec4, %max_i8_vec4);
        //   %x_u32       = bitcast<vec4u>(%x_clamp);
        //   %x_u8        = (%x_u32 & vec4u(0xff)) << n;
        //   %result      = dot(%x_u8, vec4u(1));
        auto* x = call->Args()[0];
        b.InsertBefore(call, [&] {
            auto* vec4i = ty.vec4<i32>();
            auto* vec4u = ty.vec4<u32>();

            auto* n = b.Construct(vec4u, b.Constant(u32(0)), b.Constant(u32(8)),
                                  b.Constant(u32(16)), b.Constant(u32(24)));
            auto* min_i8_vec4 = b.Construct(vec4i, b.Constant(i32(-128)));
            auto* max_i8_vec4 = b.Construct(vec4i, b.Constant(i32(127)));
            auto* x_clamp = b.Call(vec4i, core::BuiltinFn::kClamp, x, min_i8_vec4, max_i8_vec4);
            auto* x_u32 = b.Bitcast(vec4u, x_clamp);
            auto* x_u8 = b.ShiftLeft(
                vec4u, b.And(vec4u, x_u32, b.Construct(vec4u, b.Constant(u32(0xff)))), n);
            b.CallWithResult(call->DetachResult(), core::BuiltinFn::kDot, x_u8,
                             b.Construct(vec4u, (b.Constant(u32(1)))));
        });
        call->Destroy();
    }

    /// Polyfill a `pack4xU8Clamp()` builtin call
    /// @param call the builtin call instruction
    void Pack4xU8Clamp(ir::CoreBuiltinCall* call) {
        // Replace `pack4xU8Clamp(%x)` with:
        //   %n       = vec4u(0, 8, 16, 24);
        //   %min_u8_vec4 = vec4u(0);
        //   %max_u8_vec4 = vec4u(255);
        //   %x_clamp = clamp(%x, vec4u(0), vec4u(255));
        //   %x_u8    = %x_clamp << n;
        //   %result  = dot(%x_u8, vec4u(1));
        auto* x = call->Args()[0];
        b.InsertBefore(call, [&] {
            auto* vec4u = ty.vec4<u32>();

            auto* n = b.Construct(vec4u, b.Constant(u32(0)), b.Constant(u32(8)),
                                  b.Constant(u32(16)), b.Constant(u32(24)));
            auto* min_u8_vec4 = b.Construct(vec4u, b.Constant(u32(0)));
            auto* max_u8_vec4 = b.Construct(vec4u, b.Constant(u32(255)));
            auto* x_clamp = b.Call(vec4u, core::BuiltinFn::kClamp, x, min_u8_vec4, max_u8_vec4);
            auto* x_u8 = b.ShiftLeft(vec4u, x_clamp, n);
            b.CallWithResult(call->DetachResult(), core::BuiltinFn::kDot, x_u8,
                             b.Construct(vec4u, (b.Constant(u32(1)))));
        });
        call->Destroy();
    }

    /// Emit code for `unpack4xI8` on u32 value `x`, before the given call.
    /// @param call the instruction that should follow the emitted code
    /// @param x the u32 value to be unpacked
    ir::Instruction* Unpack4xI8OnValue(ir::CoreBuiltinCall* call, ir::Value* x) {
        // Replace `unpack4xI8(%x)` with:
        //   %n       = vec4u(24, 16, 8, 0);
        //   %x_splat = vec4u(%x); // splat the scalar to a vector
        //   %x_vec4i = bitcast<vec4i>(%x_splat << n);
        //   %result  = %x_vec4i >> vec4u(24);
        ir::Instruction* result = nullptr;
        b.InsertBefore(call, [&] {
            auto* vec4i = ty.vec4<i32>();
            auto* vec4u = ty.vec4<u32>();

            auto* n = b.Construct(vec4u, b.Constant(u32(24)), b.Constant(u32(16)),
                                  b.Constant(u32(8)), b.Constant(u32(0)));
            auto* x_splat = b.Construct(vec4u, x);
            auto* x_vec4i = b.Bitcast(vec4i, b.ShiftLeft(vec4u, x_splat, n));
            result = b.ShiftRight(vec4i, x_vec4i, b.Construct(vec4u, b.Constant(u32(24))));
        });
        return result;
    }

    /// Polyfill a `unpack4xI8()` builtin call
    /// @param call the builtin call instruction
    void Unpack4xI8(ir::CoreBuiltinCall* call) {
        auto* result = Unpack4xI8OnValue(call, call->Args()[0]);
        result->SetResults(Vector{call->DetachResult()});
        call->Destroy();
    }

    /// Emit code for `unpack4xU8` on u32 value `x`, before the given call.
    /// @param call the instruction that should follow the emitted code
    /// @param x the u32 value to be unpacked
    Instruction* Unpack4xU8OnValue(ir::CoreBuiltinCall* call, ir::Value* x) {
        // Replace `unpack4xU8(%x)` with:
        //   %n       = vec4u(0, 8, 16, 24);
        //   %x_splat = vec4u(%x); // splat the scalar to a vector
        //   %x_vec4u = %x_splat >> n;
        //   %result  = %x_vec4u & vec4u(0xff);
        ir::Instruction* result = nullptr;
        b.InsertBefore(call, [&] {
            auto* vec4u = ty.vec4<u32>();

            auto* n = b.Construct(vec4u, b.Constant(u32(0)), b.Constant(u32(8)),
                                  b.Constant(u32(16)), b.Constant(u32(24)));
            auto* x_splat = b.Construct(vec4u, x);
            auto* x_vec4u = b.ShiftRight(vec4u, x_splat, n);
            result = b.And(vec4u, x_vec4u, b.Construct(vec4u, b.Constant(u32(0xff))));
        });
        return result;
    }

    /// Polyfill a `unpack4xU8()` builtin call
    /// @param call the builtin call instruction
    void Unpack4xU8(ir::CoreBuiltinCall* call) {
        auto* result = Unpack4xU8OnValue(call, call->Args()[0]);
        result->SetResults(Vector{call->DetachResult()});
        call->Destroy();
    }
};

}  // namespace

Result<SuccessType> BuiltinPolyfill(Module& ir, const BuiltinPolyfillConfig& config) {
    auto result = ValidateAndDumpIfNeeded(ir, "BuiltinPolyfill transform");
    if (result != Success) {
        return result;
    }

    State{config, ir}.Process();

    return Success;
}

}  // namespace tint::core::ir::transform
