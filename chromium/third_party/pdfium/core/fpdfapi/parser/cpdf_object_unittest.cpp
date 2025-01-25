// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/cpdf_object.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "constants/stream_dict_common.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_indirect_object_holder.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_null.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fxcrt/compiler_specific.h"
#include "core/fxcrt/data_vector.h"
#include "core/fxcrt/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void TestArrayAccessors(const CPDF_Array* arr,
                        size_t index,
                        const char* str_val,
                        const char* const_str_val,
                        int int_val,
                        float float_val,
                        CPDF_Array* arr_val,
                        CPDF_Dictionary* dict_val,
                        CPDF_Stream* stream_val) {
  EXPECT_EQ(str_val, arr->GetByteStringAt(index));
  EXPECT_EQ(int_val, arr->GetIntegerAt(index));
  EXPECT_EQ(float_val, arr->GetFloatAt(index));
  EXPECT_EQ(arr_val, arr->GetArrayAt(index));
  EXPECT_EQ(dict_val, arr->GetDictAt(index));
  EXPECT_EQ(stream_val, arr->GetStreamAt(index));
}

}  // namespace

class PDFObjectsTest : public testing::Test {
 public:
  void SetUp() override {
    // Initialize different kinds of objects.
    // Boolean objects.
    auto boolean_false_obj = pdfium::MakeRetain<CPDF_Boolean>(false);
    auto boolean_true_obj = pdfium::MakeRetain<CPDF_Boolean>(true);
    // Number objects.
    auto number_int_obj = pdfium::MakeRetain<CPDF_Number>(1245);
    auto number_float_obj = pdfium::MakeRetain<CPDF_Number>(9.00345f);
    // String objects.
    auto str_reg_obj =
        pdfium::MakeRetain<CPDF_String>(nullptr, L"A simple test");
    auto str_spec_obj = pdfium::MakeRetain<CPDF_String>(nullptr, L"\t\n");
    // Name object.
    auto name_obj = pdfium::MakeRetain<CPDF_Name>(nullptr, "space");
    // Array object.
    m_ArrayObj = pdfium::MakeRetain<CPDF_Array>();
    m_ArrayObj->InsertNewAt<CPDF_Number>(0, 8902);
    m_ArrayObj->InsertNewAt<CPDF_Name>(1, "address");
    // Dictionary object.
    m_DictObj = pdfium::MakeRetain<CPDF_Dictionary>();
    m_DictObj->SetNewFor<CPDF_Boolean>("bool", false);
    m_DictObj->SetNewFor<CPDF_Number>("num", 0.23f);
    // Stream object.
    static constexpr char kContents[] = "abcdefghijklmnopqrstuvwxyz";
    auto pNewDict = pdfium::MakeRetain<CPDF_Dictionary>();
    m_StreamDictObj = pNewDict;
    m_StreamDictObj->SetNewFor<CPDF_String>("key1", L" test dict");
    m_StreamDictObj->SetNewFor<CPDF_Number>("key2", -1);
    auto stream_obj = pdfium::MakeRetain<CPDF_Stream>(
        DataVector<uint8_t>(std::begin(kContents),
                            UNSAFE_TODO(std::end(kContents) - 1)),
        std::move(pNewDict));
    // Null Object.
    auto null_obj = pdfium::MakeRetain<CPDF_Null>();
    // All direct objects.
    CPDF_Object* objs[] = {
        boolean_false_obj.Get(), boolean_true_obj.Get(), number_int_obj.Get(),
        number_float_obj.Get(),  str_reg_obj.Get(),      str_spec_obj.Get(),
        name_obj.Get(),          m_ArrayObj.Get(),       m_DictObj.Get(),
        stream_obj.Get(),        null_obj.Get()};
    m_DirectObjTypes = {
        CPDF_Object::kBoolean, CPDF_Object::kBoolean, CPDF_Object::kNumber,
        CPDF_Object::kNumber,  CPDF_Object::kString,  CPDF_Object::kString,
        CPDF_Object::kName,    CPDF_Object::kArray,   CPDF_Object::kDictionary,
        CPDF_Object::kStream,  CPDF_Object::kNullobj};
    for (auto* obj : objs) {
      m_DirectObjs.emplace_back(obj);
    }
    // Indirect references to indirect objects.
    m_ObjHolder = std::make_unique<CPDF_IndirectObjectHolder>();
    m_IndirectObjNums = {
        m_ObjHolder->AddIndirectObject(boolean_true_obj->Clone()),
        m_ObjHolder->AddIndirectObject(number_int_obj->Clone()),
        m_ObjHolder->AddIndirectObject(str_spec_obj->Clone()),
        m_ObjHolder->AddIndirectObject(name_obj->Clone()),
        m_ObjHolder->AddIndirectObject(m_ArrayObj->Clone()),
        m_ObjHolder->AddIndirectObject(m_DictObj->Clone()),
        m_ObjHolder->AddIndirectObject(stream_obj->Clone())};
    for (uint32_t objnum : m_IndirectObjNums) {
      m_RefObjs.emplace_back(
          pdfium::MakeRetain<CPDF_Reference>(m_ObjHolder.get(), objnum));
    }
  }

  bool Equal(const CPDF_Object* obj1, const CPDF_Object* obj2) {
    if (obj1 == obj2)
      return true;
    if (!obj1 || !obj2 || obj1->GetType() != obj2->GetType())
      return false;
    switch (obj1->GetType()) {
      case CPDF_Object::kBoolean:
        return obj1->GetInteger() == obj2->GetInteger();
      case CPDF_Object::kNumber:
        return obj1->AsNumber()->IsInteger() == obj2->AsNumber()->IsInteger() &&
               obj1->GetInteger() == obj2->GetInteger();
      case CPDF_Object::kString:
      case CPDF_Object::kName:
        return obj1->GetString() == obj2->GetString();
      case CPDF_Object::kArray: {
        const CPDF_Array* array1 = obj1->AsArray();
        const CPDF_Array* array2 = obj2->AsArray();
        if (array1->size() != array2->size())
          return false;
        for (size_t i = 0; i < array1->size(); ++i) {
          if (!Equal(array1->GetObjectAt(i).Get(),
                     array2->GetObjectAt(i).Get())) {
            return false;
          }
        }
        return true;
      }
      case CPDF_Object::kDictionary: {
        const CPDF_Dictionary* dict1 = obj1->AsDictionary();
        const CPDF_Dictionary* dict2 = obj2->AsDictionary();
        if (dict1->size() != dict2->size())
          return false;
        CPDF_DictionaryLocker locker1(dict1);
        for (const auto& item : locker1) {
          if (!Equal(item.second.Get(), dict2->GetObjectFor(item.first).Get()))
            return false;
        }
        return true;
      }
      case CPDF_Object::kNullobj:
        return true;
      case CPDF_Object::kStream: {
        RetainPtr<const CPDF_Stream> stream1(obj1->AsStream());
        RetainPtr<const CPDF_Stream> stream2(obj2->AsStream());
        // Compare dictionaries.
        if (!Equal(stream1->GetDict().Get(), stream2->GetDict().Get()))
          return false;

        auto streamAcc1 =
            pdfium::MakeRetain<CPDF_StreamAcc>(std::move(stream1));
        streamAcc1->LoadAllDataRaw();
        auto streamAcc2 =
            pdfium::MakeRetain<CPDF_StreamAcc>(std::move(stream2));
        streamAcc2->LoadAllDataRaw();
        pdfium::span<const uint8_t> span1 = streamAcc1->GetSpan();
        pdfium::span<const uint8_t> span2 = streamAcc2->GetSpan();

        // Compare sizes.
        if (span1.size() != span2.size())
          return false;

        return memcmp(span1.data(), span2.data(), span2.size()) == 0;
      }
      case CPDF_Object::kReference:
        return obj1->AsReference()->GetRefObjNum() ==
               obj2->AsReference()->GetRefObjNum();
    }
    return false;
  }

 protected:
  // m_ObjHolder needs to be declared first and destructed last since it also
  // refers to some objects in m_DirectObjs.
  std::unique_ptr<CPDF_IndirectObjectHolder> m_ObjHolder;
  std::vector<RetainPtr<CPDF_Object>> m_DirectObjs;
  std::vector<int> m_DirectObjTypes;
  std::vector<RetainPtr<CPDF_Object>> m_RefObjs;
  RetainPtr<CPDF_Dictionary> m_DictObj;
  RetainPtr<CPDF_Dictionary> m_StreamDictObj;
  RetainPtr<CPDF_Array> m_ArrayObj;
  std::vector<uint32_t> m_IndirectObjNums;
};

TEST_F(PDFObjectsTest, GetString) {
  constexpr auto direct_obj_results = fxcrt::ToArray<const char*>(
      {"false", "true", "1245", "9.0034504", "A simple test", "\t\n", "space",
       "", "", "", ""});
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i) {
    EXPECT_EQ(direct_obj_results[i], m_DirectObjs[i]->GetString());
  }

  // Check indirect references.
  constexpr auto indirect_obj_results = fxcrt::ToArray<const char*>(
      {"true", "1245", "\t\n", "space", "", "", ""});
  for (size_t i = 0; i < m_RefObjs.size(); ++i) {
    EXPECT_EQ(indirect_obj_results[i], m_RefObjs[i]->GetString());
  }
}

TEST_F(PDFObjectsTest, GetUnicodeText) {
  constexpr auto direct_obj_results = fxcrt::ToArray<const wchar_t*>(
      {L"", L"", L"", L"", L"A simple test", L"\t\n", L"space", L"", L"",
       L"abcdefghijklmnopqrstuvwxyz", L""});
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i) {
    EXPECT_EQ(direct_obj_results[i], m_DirectObjs[i]->GetUnicodeText());
  }

  // Check indirect references.
  for (const auto& it : m_RefObjs) {
    EXPECT_EQ(L"", it->GetUnicodeText());
  }
}

TEST_F(PDFObjectsTest, GetNumber) {
  constexpr auto direct_obj_results =
      fxcrt::ToArray<const float>({0, 0, 1245, 9.00345f, 0, 0, 0, 0, 0, 0, 0});
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i) {
    EXPECT_EQ(direct_obj_results[i], m_DirectObjs[i]->GetNumber());
  }

  // Check indirect references.
  constexpr auto indirect_obj_results =
      fxcrt::ToArray<const float>({0, 1245, 0, 0, 0, 0, 0});
  for (size_t i = 0; i < m_RefObjs.size(); ++i)
    EXPECT_EQ(indirect_obj_results[i], m_RefObjs[i]->GetNumber());
}

TEST_F(PDFObjectsTest, GetInteger) {
  constexpr auto direct_obj_results =
      fxcrt::ToArray<const int>({0, 1, 1245, 9, 0, 0, 0, 0, 0, 0, 0});
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i) {
    EXPECT_EQ(direct_obj_results[i], m_DirectObjs[i]->GetInteger());
  }

  // Check indirect references.
  constexpr auto indirect_obj_results =
      fxcrt::ToArray<const int>({1, 1245, 0, 0, 0, 0, 0});
  for (size_t i = 0; i < m_RefObjs.size(); ++i) {
    EXPECT_EQ(indirect_obj_results[i], m_RefObjs[i]->GetInteger());
  }
}

TEST_F(PDFObjectsTest, GetDict) {
  const auto direct_obj_results = fxcrt::ToArray<const CPDF_Dictionary*>(
      {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       m_DictObj.Get(), m_StreamDictObj.Get(), nullptr});
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i) {
    EXPECT_EQ(direct_obj_results[i], m_DirectObjs[i]->GetDict());
  }

  const auto indirect_obj_results = fxcrt::ToArray<const CPDF_Dictionary*>(
      {nullptr, nullptr, nullptr, nullptr, nullptr, m_DictObj.Get(),
       m_StreamDictObj.Get()});
  // Check indirect references.
  for (size_t i = 0; i < m_RefObjs.size(); ++i) {
    EXPECT_TRUE(Equal(indirect_obj_results[i], m_RefObjs[i]->GetDict().Get()));
  }
}

TEST_F(PDFObjectsTest, GetNameFor) {
  m_DictObj->SetNewFor<CPDF_String>("string", "ium");
  m_DictObj->SetNewFor<CPDF_Name>("name", "Pdf");

  EXPECT_EQ("", m_DictObj->GetNameFor("invalid"));
  EXPECT_EQ("", m_DictObj->GetNameFor("bool"));
  EXPECT_EQ("", m_DictObj->GetNameFor("num"));
  EXPECT_EQ("", m_DictObj->GetNameFor("string"));
  EXPECT_EQ("Pdf", m_DictObj->GetNameFor("name"));

  EXPECT_EQ("", m_DictObj->GetByteStringFor("invalid"));
  EXPECT_EQ("false", m_DictObj->GetByteStringFor("bool"));
  EXPECT_EQ(".23", m_DictObj->GetByteStringFor("num"));
  EXPECT_EQ("ium", m_DictObj->GetByteStringFor("string"));
  EXPECT_EQ("Pdf", m_DictObj->GetByteStringFor("name"));
}

TEST_F(PDFObjectsTest, GetArray) {
  const auto direct_obj_results = fxcrt::ToArray<const CPDF_Array*>(
      {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       m_ArrayObj.Get(), nullptr, nullptr, nullptr});
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i) {
    EXPECT_EQ(direct_obj_results[i], m_DirectObjs[i]->AsArray());
  }

  // Check indirect references.
  for (const auto& it : m_RefObjs) {
    EXPECT_FALSE(it->AsArray());
  }
}

TEST_F(PDFObjectsTest, Clone) {
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i) {
    RetainPtr<CPDF_Object> obj = m_DirectObjs[i]->Clone();
    EXPECT_TRUE(Equal(m_DirectObjs[i].Get(), obj.Get()));
  }

  // Check indirect references.
  for (const auto& it : m_RefObjs) {
    RetainPtr<CPDF_Object> obj = it->Clone();
    EXPECT_TRUE(Equal(it.Get(), obj.Get()));
  }
}

TEST_F(PDFObjectsTest, GetType) {
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i)
    EXPECT_EQ(m_DirectObjTypes[i], m_DirectObjs[i]->GetType());

  // Check indirect references.
  for (const auto& it : m_RefObjs)
    EXPECT_EQ(CPDF_Object::kReference, it->GetType());
}

TEST_F(PDFObjectsTest, GetDirect) {
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i)
    EXPECT_EQ(m_DirectObjs[i].Get(), m_DirectObjs[i]->GetDirect());

  // Check indirect references.
  for (size_t i = 0; i < m_RefObjs.size(); ++i)
    EXPECT_EQ(m_IndirectObjNums[i], m_RefObjs[i]->GetDirect()->GetObjNum());
}

TEST_F(PDFObjectsTest, SetString) {
  // Check for direct objects.
  constexpr auto set_values = fxcrt::ToArray<const char*>(
      {"true", "fake", "3.125f", "097", "changed", "", "NewName"});
  constexpr auto expected = fxcrt::ToArray<const char*>(
      {"true", "false", "3.125", "97", "changed", "", "NewName"});
  for (size_t i = 0; i < std::size(set_values); ++i) {
    m_DirectObjs[i]->SetString(set_values[i]);
    EXPECT_EQ(expected[i], m_DirectObjs[i]->GetString());
  }
}

TEST_F(PDFObjectsTest, IsTypeAndAsType) {
  // Check for direct objects.
  for (size_t i = 0; i < m_DirectObjs.size(); ++i) {
    if (m_DirectObjTypes[i] == CPDF_Object::kArray) {
      EXPECT_TRUE(m_DirectObjs[i]->IsArray());
      EXPECT_EQ(m_DirectObjs[i].Get(), m_DirectObjs[i]->AsArray());
    } else {
      EXPECT_FALSE(m_DirectObjs[i]->IsArray());
      EXPECT_FALSE(m_DirectObjs[i]->AsArray());
    }

    if (m_DirectObjTypes[i] == CPDF_Object::kBoolean) {
      EXPECT_TRUE(m_DirectObjs[i]->IsBoolean());
      EXPECT_EQ(m_DirectObjs[i].Get(), m_DirectObjs[i]->AsBoolean());
    } else {
      EXPECT_FALSE(m_DirectObjs[i]->IsBoolean());
      EXPECT_FALSE(m_DirectObjs[i]->AsBoolean());
    }

    if (m_DirectObjTypes[i] == CPDF_Object::kName) {
      EXPECT_TRUE(m_DirectObjs[i]->IsName());
      EXPECT_EQ(m_DirectObjs[i].Get(), m_DirectObjs[i]->AsName());
    } else {
      EXPECT_FALSE(m_DirectObjs[i]->IsName());
      EXPECT_FALSE(m_DirectObjs[i]->AsName());
    }

    if (m_DirectObjTypes[i] == CPDF_Object::kNumber) {
      EXPECT_TRUE(m_DirectObjs[i]->IsNumber());
      EXPECT_EQ(m_DirectObjs[i].Get(), m_DirectObjs[i]->AsNumber());
    } else {
      EXPECT_FALSE(m_DirectObjs[i]->IsNumber());
      EXPECT_FALSE(m_DirectObjs[i]->AsNumber());
    }

    if (m_DirectObjTypes[i] == CPDF_Object::kString) {
      EXPECT_TRUE(m_DirectObjs[i]->IsString());
      EXPECT_EQ(m_DirectObjs[i].Get(), m_DirectObjs[i]->AsString());
    } else {
      EXPECT_FALSE(m_DirectObjs[i]->IsString());
      EXPECT_FALSE(m_DirectObjs[i]->AsString());
    }

    if (m_DirectObjTypes[i] == CPDF_Object::kDictionary) {
      EXPECT_TRUE(m_DirectObjs[i]->IsDictionary());
      EXPECT_EQ(m_DirectObjs[i].Get(), m_DirectObjs[i]->AsDictionary());
    } else {
      EXPECT_FALSE(m_DirectObjs[i]->IsDictionary());
      EXPECT_FALSE(m_DirectObjs[i]->AsDictionary());
    }

    if (m_DirectObjTypes[i] == CPDF_Object::kStream) {
      EXPECT_TRUE(m_DirectObjs[i]->IsStream());
      EXPECT_EQ(m_DirectObjs[i].Get(), m_DirectObjs[i]->AsStream());
    } else {
      EXPECT_FALSE(m_DirectObjs[i]->IsStream());
      EXPECT_FALSE(m_DirectObjs[i]->AsStream());
    }

    EXPECT_FALSE(m_DirectObjs[i]->IsReference());
    EXPECT_FALSE(m_DirectObjs[i]->AsReference());
  }
  // Check indirect references.
  for (size_t i = 0; i < m_RefObjs.size(); ++i) {
    EXPECT_TRUE(m_RefObjs[i]->IsReference());
    EXPECT_EQ(m_RefObjs[i].Get(), m_RefObjs[i]->AsReference());
  }
}

TEST_F(PDFObjectsTest, MakeReferenceGeneric) {
  auto original_obj = pdfium::MakeRetain<CPDF_Null>();
  original_obj->SetObjNum(42);
  ASSERT_FALSE(original_obj->IsInline());

  auto ref_obj = original_obj->MakeReference(m_ObjHolder.get());

  ASSERT_TRUE(ref_obj->IsReference());
  EXPECT_EQ(original_obj->GetObjNum(),
            ToReference(ref_obj.Get())->GetRefObjNum());
}

TEST_F(PDFObjectsTest, KeyForCache) {
  std::set<uint64_t> key_set;

  // Check all direct objects inserted without collision.
  for (const auto& direct : m_DirectObjs) {
    EXPECT_TRUE(key_set.insert(direct->KeyForCache()).second);
  }
  // Check indirect objects inserted without collision.
  for (const auto& pair : *m_ObjHolder) {
    EXPECT_TRUE(key_set.insert(pair.second->KeyForCache()).second);
  }

  // Check all expected objects counted.
  EXPECT_EQ(18u, key_set.size());
}

TEST(PDFArrayTest, GetMatrix) {
  using Row = std::array<float, 6>;
  constexpr auto elems = fxcrt::ToArray<const Row>({
      {{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
      {{1, 2, 3, 4, 5, 6}},
      {{2.3f, 4.05f, 3, -2, -3, 0.0f}},
      {{0.05f, 0.1f, 0.56f, 0.67f, 1.34f, 99.9f}},
  });
  for (const auto& elem : elems) {
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    for (float f : elem) {
      arr->AppendNew<CPDF_Number>(f);
    }
    CFX_Matrix matrix(elem[0], elem[1], elem[2], elem[3], elem[4], elem[5]);
    CFX_Matrix arr_matrix = arr->GetMatrix();
    EXPECT_EQ(matrix, arr_matrix);
  }
}

TEST(PDFArrayTest, GetRect) {
  using Row = std::array<float, 4>;
  constexpr auto elems = fxcrt::ToArray<const Row>({
      {{0.0f, 0.0f, 0.0f, 0.0f}},
      {{1, 2, 5, 6}},
      {{2.3f, 4.05f, -3, 0.0f}},
      {{0.05f, 0.1f, 1.34f, 99.9f}},
  });
  for (const auto& elem : elems) {
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    for (float f : elem) {
      arr->AppendNew<CPDF_Number>(f);
    }
    CFX_FloatRect rect(elem[0], elem[1], elem[2], elem[3]);
    CFX_FloatRect arr_rect = arr->GetRect();
    EXPECT_EQ(rect, arr_rect);
  }
}

TEST(PDFArrayTest, GetTypeAt) {
  {
    // Boolean array.
    constexpr auto vals =
        fxcrt::ToArray<const bool>({true, false, false, true, true});
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    for (size_t i = 0; i < vals.size(); ++i) {
      arr->InsertNewAt<CPDF_Boolean>(i, vals[i]);
    }
    for (size_t i = 0; i < vals.size(); ++i) {
      TestArrayAccessors(arr.Get(), i,                // Array and index.
                         vals[i] ? "true" : "false",  // String value.
                         nullptr,                     // Const string value.
                         vals[i] ? 1 : 0,             // Integer value.
                         0,                           // Float value.
                         nullptr,                     // Array value.
                         nullptr,                     // Dictionary value.
                         nullptr);                    // Stream value.
    }
  }
  {
    // Integer array.
    constexpr auto vals = fxcrt::ToArray<const int>(
        {10, 0, -345, 2089345456, -1000000000, 567, 93658767});
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    for (size_t i = 0; i < vals.size(); ++i) {
      arr->InsertNewAt<CPDF_Number>(i, vals[i]);
    }
    for (size_t i = 0; i < vals.size(); ++i) {
      char buf[33];
      TestArrayAccessors(arr.Get(), i,                  // Array and index.
                         FXSYS_itoa(vals[i], buf, 10),  // String value.
                         nullptr,                       // Const string value.
                         vals[i],                       // Integer value.
                         vals[i],                       // Float value.
                         nullptr,                       // Array value.
                         nullptr,                       // Dictionary value.
                         nullptr);                      // Stream value.
    }
  }
  {
    // Float array.
    constexpr auto vals = fxcrt::ToArray<const float>(
        {0.0f, 0, 10, 10.0f, 0.0345f, 897.34f, -2.5f, -1.0f, -345.0f, -0.0f});
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    for (size_t i = 0; i < vals.size(); ++i) {
      arr->InsertNewAt<CPDF_Number>(i, vals[i]);
    }
    constexpr auto expected_str =
        fxcrt::ToArray<const char*>({"0", "0", "10", "10", ".034499999",
                                     "897.34003", "-2.5", "-1", "-345", "0"});
    for (size_t i = 0; i < vals.size(); ++i) {
      TestArrayAccessors(arr.Get(), i,     // Array and index.
                         expected_str[i],  // String value.
                         nullptr,          // Const string value.
                         vals[i],          // Integer value.
                         vals[i],          // Float value.
                         nullptr,          // Array value.
                         nullptr,          // Dictionary value.
                         nullptr);         // Stream value.
    }
  }
  {
    // String and name array
    constexpr auto vals = fxcrt::ToArray<const char*>(
        {"this", "adsde$%^", "\r\t", "\"012", ".", "EYREW", "It is a joke :)"});
    auto string_array = pdfium::MakeRetain<CPDF_Array>();
    auto name_array = pdfium::MakeRetain<CPDF_Array>();
    for (size_t i = 0; i < vals.size(); ++i) {
      string_array->InsertNewAt<CPDF_String>(i, vals[i]);
      name_array->InsertNewAt<CPDF_Name>(i, vals[i]);
    }
    for (size_t i = 0; i < std::size(vals); ++i) {
      TestArrayAccessors(string_array.Get(), i,  // Array and index.
                         vals[i],                // String value.
                         vals[i],                // Const string value.
                         0,                      // Integer value.
                         0,                      // Float value.
                         nullptr,                // Array value.
                         nullptr,                // Dictionary value.
                         nullptr);               // Stream value.
      TestArrayAccessors(name_array.Get(), i,    // Array and index.
                         vals[i],                // String value.
                         vals[i],                // Const string value.
                         0,                      // Integer value.
                         0,                      // Float value.
                         nullptr,                // Array value.
                         nullptr,                // Dictionary value.
                         nullptr);               // Stream value.
    }
  }
  {
    // Null element array.
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    for (size_t i = 0; i < 3; ++i)
      arr->InsertNewAt<CPDF_Null>(i);
    for (size_t i = 0; i < 3; ++i) {
      TestArrayAccessors(arr.Get(), i,  // Array and index.
                         "",            // String value.
                         nullptr,       // Const string value.
                         0,             // Integer value.
                         0,             // Float value.
                         nullptr,       // Array value.
                         nullptr,       // Dictionary value.
                         nullptr);      // Stream value.
    }
  }
  {
    // Array of array.
    std::array<RetainPtr<CPDF_Array>, 3> vals;
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    for (size_t i = 0; i < 3; ++i) {
      vals[i] = arr->AppendNew<CPDF_Array>();
      for (size_t j = 0; j < 3; ++j) {
        int value = j + 100;
        vals[i]->InsertNewAt<CPDF_Number>(j, value);
      }
    }
    for (size_t i = 0; i < 3; ++i) {
      TestArrayAccessors(arr.Get(), i,   // Array and index.
                         "",             // String value.
                         nullptr,        // Const string value.
                         0,              // Integer value.
                         0,              // Float value.
                         vals[i].Get(),  // Array value.
                         nullptr,        // Dictionary value.
                         nullptr);       // Stream value.
    }
  }
  {
    // Dictionary array.
    std::array<RetainPtr<CPDF_Dictionary>, 3> vals;
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    for (size_t i = 0; i < 3; ++i) {
      vals[i] = arr->AppendNew<CPDF_Dictionary>();
      for (size_t j = 0; j < 3; ++j) {
        std::string key("key");
        char buf[33];
        key.append(FXSYS_itoa(j, buf, 10));
        int value = j + 200;
        vals[i]->SetNewFor<CPDF_Number>(key.c_str(), value);
      }
    }
    for (size_t i = 0; i < 3; ++i) {
      TestArrayAccessors(arr.Get(), i,   // Array and index.
                         "",             // String value.
                         nullptr,        // Const string value.
                         0,              // Integer value.
                         0,              // Float value.
                         nullptr,        // Array value.
                         vals[i].Get(),  // Dictionary value.
                         nullptr);       // Stream value.
    }
  }
  {
    // Stream array.
    CPDF_IndirectObjectHolder object_holder;

    std::array<RetainPtr<CPDF_Dictionary>, 3> vals;
    std::array<RetainPtr<CPDF_Stream>, 3> stream_vals;
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    for (size_t i = 0; i < 3; ++i) {
      vals[i] = pdfium::MakeRetain<CPDF_Dictionary>();
      for (size_t j = 0; j < 3; ++j) {
        std::string key("key");
        char buf[33];
        key.append(FXSYS_itoa(j, buf, 10));
        int value = j + 200;
        vals[i]->SetNewFor<CPDF_Number>(key.c_str(), value);
      }
      static constexpr uint8_t kContents[] = "content: this is a stream";
      stream_vals[i] = object_holder.NewIndirect<CPDF_Stream>(
          DataVector<uint8_t>(std::begin(kContents), std::end(kContents)),
          vals[i]);
      arr->AppendNew<CPDF_Reference>(&object_holder,
                                     stream_vals[i]->GetObjNum());
    }
    for (size_t i = 0; i < 3; ++i) {
      TestArrayAccessors(arr.Get(), i,           // Array and index.
                         "",                     // String value.
                         nullptr,                // Const string value.
                         0,                      // Integer value.
                         0,                      // Float value.
                         nullptr,                // Array value.
                         vals[i].Get(),          // Dictionary value.
                         stream_vals[i].Get());  // Stream value.
    }
  }
  {
    // Mixed array.

    CPDF_IndirectObjectHolder object_holder;
    auto arr = pdfium::MakeRetain<CPDF_Array>();
    arr->InsertNewAt<CPDF_Boolean>(0, true);
    arr->InsertNewAt<CPDF_Boolean>(1, false);
    arr->InsertNewAt<CPDF_Number>(2, 0);
    arr->InsertNewAt<CPDF_Number>(3, -1234);
    arr->InsertNewAt<CPDF_Number>(4, 2345.0f);
    arr->InsertNewAt<CPDF_Number>(5, 0.05f);
    arr->InsertNewAt<CPDF_String>(6, "");
    arr->InsertNewAt<CPDF_String>(7, "It is a test!");
    arr->InsertNewAt<CPDF_Name>(8, "NAME");
    arr->InsertNewAt<CPDF_Name>(9, "test");
    arr->InsertNewAt<CPDF_Null>(10);

    auto arr_val = arr->InsertNewAt<CPDF_Array>(11);
    arr_val->AppendNew<CPDF_Number>(1);
    arr_val->AppendNew<CPDF_Number>(2);

    auto dict_val = arr->InsertNewAt<CPDF_Dictionary>(12);
    dict_val->SetNewFor<CPDF_String>("key1", "Linda");
    dict_val->SetNewFor<CPDF_String>("key2", "Zoe");

    auto stream_dict = pdfium::MakeRetain<CPDF_Dictionary>();
    stream_dict->SetNewFor<CPDF_String>("key1", "John");
    stream_dict->SetNewFor<CPDF_String>("key2", "King");
    static constexpr uint8_t kData[] = "A stream for test";
    // The data buffer will be owned by stream object, so it needs to be
    // dynamically allocated.
    auto stream_val = object_holder.NewIndirect<CPDF_Stream>(
        DataVector<uint8_t>(std::begin(kData), std::end(kData)), stream_dict);
    arr->InsertNewAt<CPDF_Reference>(13, &object_holder,
                                     stream_val->GetObjNum());
    constexpr auto expected_str = fxcrt::ToArray<const char*>(
        {"true", "false", "0", "-1234", "2345", ".050000001", "",
         "It is a test!", "NAME", "test", "", "", "", ""});
    constexpr auto expected_int = fxcrt::ToArray<const int>(
        {1, 0, 0, -1234, 2345, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    constexpr auto expected_float = fxcrt::ToArray<const float>(
        {0, 0, 0, -1234, 2345, 0.05f, 0, 0, 0, 0, 0, 0, 0, 0});
    for (size_t i = 0; i < arr->size(); ++i) {
      EXPECT_EQ(expected_str[i], arr->GetByteStringAt(i));
      EXPECT_EQ(expected_int[i], arr->GetIntegerAt(i));
      EXPECT_EQ(expected_float[i], arr->GetFloatAt(i));
      if (i == 11) {
        EXPECT_EQ(arr_val, arr->GetArrayAt(i));
      } else {
        EXPECT_FALSE(arr->GetArrayAt(i));
      }
      if (i == 13) {
        EXPECT_EQ(stream_dict, arr->GetDictAt(i));
        EXPECT_EQ(stream_val, arr->GetStreamAt(i));
      } else {
        EXPECT_FALSE(arr->GetStreamAt(i));
        if (i == 12)
          EXPECT_EQ(dict_val, arr->GetDictAt(i));
        else
          EXPECT_FALSE(arr->GetDictAt(i));
      }
    }
  }
}

TEST(PDFArrayTest, AddNumber) {
  constexpr auto vals = fxcrt::ToArray<const float>(
      {1.0f, -1.0f, 0, 0.456734f, 12345.54321f, 0.5f, 1000, 0.000045f});
  auto arr = pdfium::MakeRetain<CPDF_Array>();
  for (size_t i = 0; i < vals.size(); ++i) {
    arr->AppendNew<CPDF_Number>(vals[i]);
  }
  for (size_t i = 0; i < vals.size(); ++i) {
    EXPECT_EQ(CPDF_Object::kNumber, arr->GetObjectAt(i)->GetType());
    EXPECT_EQ(vals[i], arr->GetObjectAt(i)->GetNumber());
  }
}

TEST(PDFArrayTest, AddInteger) {
  constexpr auto vals = fxcrt::ToArray<const int>(
      {0, 1, 934435456, 876, 10000, -1, -24354656, -100});
  auto arr = pdfium::MakeRetain<CPDF_Array>();
  for (size_t i = 0; i < vals.size(); ++i) {
    arr->AppendNew<CPDF_Number>(vals[i]);
  }
  for (size_t i = 0; i < vals.size(); ++i) {
    EXPECT_EQ(CPDF_Object::kNumber, arr->GetObjectAt(i)->GetType());
    EXPECT_EQ(vals[i], arr->GetObjectAt(i)->GetNumber());
  }
}

TEST(PDFArrayTest, AddStringAndName) {
  static constexpr auto kVals =
      fxcrt::ToArray<const char*>({"", "a", "ehjhRIOYTTFdfcdnv", "122323",
                                   "$#%^&**", " ", "This is a test.\r\n"});
  auto string_array = pdfium::MakeRetain<CPDF_Array>();
  auto name_array = pdfium::MakeRetain<CPDF_Array>();
  for (const char* val : kVals) {
    string_array->AppendNew<CPDF_String>(val);
    name_array->AppendNew<CPDF_Name>(val);
  }
  for (size_t i = 0; i < std::size(kVals); ++i) {
    EXPECT_EQ(CPDF_Object::kString, string_array->GetObjectAt(i)->GetType());
    EXPECT_EQ(kVals[i], string_array->GetObjectAt(i)->GetString());
    EXPECT_EQ(CPDF_Object::kName, name_array->GetObjectAt(i)->GetType());
    EXPECT_EQ(kVals[i], name_array->GetObjectAt(i)->GetString());
  }
}

TEST(PDFArrayTest, AddReferenceAndGetObjectAt) {
  auto holder = std::make_unique<CPDF_IndirectObjectHolder>();
  auto boolean_obj = pdfium::MakeRetain<CPDF_Boolean>(true);
  auto int_obj = pdfium::MakeRetain<CPDF_Number>(-1234);
  auto float_obj = pdfium::MakeRetain<CPDF_Number>(2345.089f);
  auto str_obj =
      pdfium::MakeRetain<CPDF_String>(nullptr, "Adsfdsf 343434 %&&*\n");
  auto name_obj = pdfium::MakeRetain<CPDF_Name>(nullptr, "Title:");
  auto null_obj = pdfium::MakeRetain<CPDF_Null>();
  auto indirect_objs = fxcrt::ToArray<RetainPtr<CPDF_Object>>(
      {boolean_obj, int_obj, float_obj, str_obj, name_obj, null_obj});
  auto obj_nums = fxcrt::ToArray<int>({2, 4, 7, 2345, 799887, 1});
  auto arr = pdfium::MakeRetain<CPDF_Array>();
  auto arr1 = pdfium::MakeRetain<CPDF_Array>();
  // Create two arrays of references by different AddReference() APIs.
  for (size_t i = 0; i < std::size(indirect_objs); ++i) {
    holder->ReplaceIndirectObjectIfHigherGeneration(obj_nums[i],
                                                    indirect_objs[i]);
    arr->AppendNew<CPDF_Reference>(holder.get(), obj_nums[i]);
    arr1->AppendNew<CPDF_Reference>(holder.get(),
                                    indirect_objs[i]->GetObjNum());
  }
  // Check indirect objects.
  for (size_t i = 0; i < std::size(obj_nums); ++i)
    EXPECT_EQ(indirect_objs[i], holder->GetOrParseIndirectObject(obj_nums[i]));
  // Check arrays.
  EXPECT_EQ(arr->size(), arr1->size());
  for (size_t i = 0; i < arr->size(); ++i) {
    EXPECT_EQ(CPDF_Object::kReference, arr->GetObjectAt(i)->GetType());
    EXPECT_EQ(indirect_objs[i], arr->GetObjectAt(i)->GetDirect());
    EXPECT_EQ(indirect_objs[i], arr->GetDirectObjectAt(i));
    EXPECT_EQ(CPDF_Object::kReference, arr1->GetObjectAt(i)->GetType());
    EXPECT_EQ(indirect_objs[i], arr1->GetObjectAt(i)->GetDirect());
    EXPECT_EQ(indirect_objs[i], arr1->GetDirectObjectAt(i));
  }
}

TEST(PDFArrayTest, CloneDirectObject) {
  CPDF_IndirectObjectHolder objects_holder;
  auto array = pdfium::MakeRetain<CPDF_Array>();
  array->AppendNew<CPDF_Reference>(&objects_holder, 1234);
  ASSERT_EQ(1U, array->size());
  RetainPtr<const CPDF_Object> obj = array->GetObjectAt(0);
  ASSERT_TRUE(obj);
  EXPECT_TRUE(obj->IsReference());

  RetainPtr<CPDF_Object> cloned_array_object = array->CloneDirectObject();
  ASSERT_TRUE(cloned_array_object);
  ASSERT_TRUE(cloned_array_object->IsArray());

  RetainPtr<CPDF_Array> cloned_array = ToArray(std::move(cloned_array_object));
  ASSERT_EQ(0U, cloned_array->size());
  RetainPtr<const CPDF_Object> cloned_obj = cloned_array->GetObjectAt(0);
  EXPECT_FALSE(cloned_obj);
}

TEST(PDFArrayTest, ConvertIndirect) {
  CPDF_IndirectObjectHolder objects_holder;
  auto array = pdfium::MakeRetain<CPDF_Array>();
  auto pObj = array->AppendNew<CPDF_Number>(42);
  array->ConvertToIndirectObjectAt(0, &objects_holder);
  RetainPtr<const CPDF_Object> pRef = array->GetObjectAt(0);
  RetainPtr<const CPDF_Object> pNum = array->GetDirectObjectAt(0);
  EXPECT_TRUE(pRef->IsReference());
  EXPECT_TRUE(pNum->IsNumber());
  EXPECT_NE(pObj, pRef);
  EXPECT_EQ(pObj, pNum);
  EXPECT_EQ(42, array->GetIntegerAt(0));
}

TEST(PDFStreamTest, SetData) {
  DataVector<uint8_t> data(100);
  auto stream = pdfium::MakeRetain<CPDF_Stream>(
      data, pdfium::MakeRetain<CPDF_Dictionary>());
  EXPECT_EQ(static_cast<int>(data.size()),
            stream->GetDict()->GetIntegerFor(pdfium::stream::kLength));

  stream->GetMutableDict()->SetNewFor<CPDF_String>(pdfium::stream::kFilter,
                                                   L"SomeFilter");
  stream->GetMutableDict()->SetNewFor<CPDF_String>(pdfium::stream::kDecodeParms,
                                                   L"SomeParams");

  DataVector<uint8_t> new_data(data.size() * 2);
  stream->SetData(new_data);

  // The "Length" field should be updated for new data size.
  EXPECT_EQ(static_cast<int>(new_data.size()),
            stream->GetDict()->GetIntegerFor(pdfium::stream::kLength));

  // The "Filter" and "DecodeParms" fields should not be changed.
  EXPECT_EQ(stream->GetDict()->GetUnicodeTextFor(pdfium::stream::kFilter),
            L"SomeFilter");
  EXPECT_EQ(stream->GetDict()->GetUnicodeTextFor(pdfium::stream::kDecodeParms),
            L"SomeParams");
}

TEST(PDFStreamTest, SetDataAndRemoveFilter) {
  DataVector<uint8_t> data(100);
  auto stream = pdfium::MakeRetain<CPDF_Stream>(
      data, pdfium::MakeRetain<CPDF_Dictionary>());
  EXPECT_EQ(static_cast<int>(data.size()),
            stream->GetDict()->GetIntegerFor(pdfium::stream::kLength));

  stream->GetMutableDict()->SetNewFor<CPDF_String>(pdfium::stream::kFilter,
                                                   L"SomeFilter");
  stream->GetMutableDict()->SetNewFor<CPDF_String>(pdfium::stream::kDecodeParms,
                                                   L"SomeParams");

  DataVector<uint8_t> new_data(data.size() * 2);
  stream->SetDataAndRemoveFilter(new_data);
  // The "Length" field should be updated for new data size.
  EXPECT_EQ(static_cast<int>(new_data.size()),
            stream->GetDict()->GetIntegerFor(pdfium::stream::kLength));

  // The "Filter" and "DecodeParms" should be removed.
  EXPECT_FALSE(stream->GetDict()->KeyExist(pdfium::stream::kFilter));
  EXPECT_FALSE(stream->GetDict()->KeyExist(pdfium::stream::kDecodeParms));
}

TEST(PDFStreamTest, LengthInDictionaryOnCreate) {
  static constexpr uint32_t kBufSize = 100;
  // The length field should be created on stream create.
  {
    auto stream = pdfium::MakeRetain<CPDF_Stream>(
        DataVector<uint8_t>(kBufSize), pdfium::MakeRetain<CPDF_Dictionary>());
    EXPECT_EQ(static_cast<int>(kBufSize),
              stream->GetDict()->GetIntegerFor(pdfium::stream::kLength));
  }
  // The length field should be corrected on stream create.
  {
    auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
    dict->SetNewFor<CPDF_Number>(pdfium::stream::kLength, 30000);
    auto stream = pdfium::MakeRetain<CPDF_Stream>(DataVector<uint8_t>(kBufSize),
                                                  std::move(dict));
    EXPECT_EQ(static_cast<int>(kBufSize),
              stream->GetDict()->GetIntegerFor(pdfium::stream::kLength));
  }
}

TEST(PDFDictionaryTest, CloneDirectObject) {
  CPDF_IndirectObjectHolder objects_holder;
  auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
  dict->SetNewFor<CPDF_Reference>("foo", &objects_holder, 1234);
  ASSERT_EQ(1U, dict->size());
  RetainPtr<const CPDF_Object> obj = dict->GetObjectFor("foo");
  ASSERT_TRUE(obj);
  EXPECT_TRUE(obj->IsReference());

  RetainPtr<CPDF_Object> cloned_dict_object = dict->CloneDirectObject();
  ASSERT_TRUE(cloned_dict_object);
  ASSERT_TRUE(cloned_dict_object->IsDictionary());

  RetainPtr<CPDF_Dictionary> cloned_dict =
      ToDictionary(std::move(cloned_dict_object));
  ASSERT_EQ(0U, cloned_dict->size());
  RetainPtr<const CPDF_Object> cloned_obj = cloned_dict->GetObjectFor("foo");
  EXPECT_FALSE(cloned_obj);
}

TEST(PDFObjectTest, CloneCheckLoop) {
  {
    // Create a dictionary/array pair with a reference loop.
    auto arr_obj = pdfium::MakeRetain<CPDF_Array>();
    auto dict_obj = arr_obj->InsertNewAt<CPDF_Dictionary>(0);
    dict_obj->SetFor("arr", arr_obj);
    // Clone this object to see whether stack overflow will be triggered.
    RetainPtr<CPDF_Array> cloned_array = ToArray(arr_obj->Clone());
    // Cloned object should be the same as the original.
    ASSERT_TRUE(cloned_array);
    EXPECT_EQ(1u, cloned_array->size());
    RetainPtr<const CPDF_Object> cloned_dict = cloned_array->GetObjectAt(0);
    ASSERT_TRUE(cloned_dict);
    ASSERT_TRUE(cloned_dict->IsDictionary());
    // Recursively referenced object is not cloned.
    EXPECT_FALSE(cloned_dict->AsDictionary()->GetObjectFor("arr"));
    dict_obj->RemoveFor("arr");  // Break deliberate cycle for cleanup.
  }
  {
    CPDF_IndirectObjectHolder objects_holder;
    // Create an object with a reference loop.
    auto dict_obj = objects_holder.NewIndirect<CPDF_Dictionary>();
    auto arr_obj = pdfium::MakeRetain<CPDF_Array>();
    arr_obj->InsertNewAt<CPDF_Reference>(0, &objects_holder,
                                         dict_obj->GetObjNum());
    RetainPtr<const CPDF_Object> elem0 = arr_obj->GetObjectAt(0);
    dict_obj->SetFor("arr", std::move(arr_obj));
    EXPECT_EQ(1u, dict_obj->GetObjNum());
    ASSERT_TRUE(elem0);
    ASSERT_TRUE(elem0->IsReference());
    EXPECT_EQ(1u, elem0->AsReference()->GetRefObjNum());
    EXPECT_EQ(dict_obj, elem0->AsReference()->GetDirect());

    // Clone this object to see whether stack overflow will be triggered.
    RetainPtr<CPDF_Dictionary> cloned_dict =
        ToDictionary(dict_obj->CloneDirectObject());
    // Cloned object should be the same as the original.
    ASSERT_TRUE(cloned_dict);
    RetainPtr<const CPDF_Object> cloned_arr = cloned_dict->GetObjectFor("arr");
    ASSERT_TRUE(cloned_arr);
    ASSERT_TRUE(cloned_arr->IsArray());
    EXPECT_EQ(0U, cloned_arr->AsArray()->size());
    // Recursively referenced object is not cloned.
    EXPECT_FALSE(cloned_arr->AsArray()->GetObjectAt(0));
    dict_obj->RemoveFor("arr");  // Break deliberate cycle for cleanup.
  }
}

TEST(PDFDictionaryTest, ConvertIndirect) {
  CPDF_IndirectObjectHolder objects_holder;
  auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
  auto pObj = dict->SetNewFor<CPDF_Number>("clams", 42);
  dict->ConvertToIndirectObjectFor("clams", &objects_holder);
  RetainPtr<const CPDF_Object> pRef = dict->GetObjectFor("clams");
  RetainPtr<const CPDF_Object> pNum = dict->GetDirectObjectFor("clams");
  EXPECT_TRUE(pRef->IsReference());
  EXPECT_TRUE(pNum->IsNumber());
  EXPECT_NE(pObj, pRef);
  EXPECT_EQ(pObj, pNum);
  EXPECT_EQ(42, dict->GetIntegerFor("clams"));
}

TEST(PDFDictionaryTest, ExtractObjectOnRemove) {
  auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
  auto pObj = dict->SetNewFor<CPDF_Number>("child", 42);
  auto extracted_object = dict->RemoveFor("child");
  EXPECT_EQ(pObj, extracted_object.Get());

  extracted_object = dict->RemoveFor("non_exists_object");
  EXPECT_FALSE(extracted_object);
}

TEST(PDFRefernceTest, MakeReferenceToReference) {
  auto obj_holder = std::make_unique<CPDF_IndirectObjectHolder>();
  auto original_ref = pdfium::MakeRetain<CPDF_Reference>(obj_holder.get(), 42);
  original_ref->SetObjNum(1952);
  ASSERT_FALSE(original_ref->IsInline());

  auto ref_obj = original_ref->MakeReference(obj_holder.get());

  ASSERT_TRUE(ref_obj->IsReference());
  // We do not allow reference to reference.
  // New reference should have same RefObjNum.
  EXPECT_EQ(original_ref->GetRefObjNum(),
            ToReference(ref_obj.Get())->GetRefObjNum());
}
