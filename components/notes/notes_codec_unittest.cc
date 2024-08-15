// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notes/notes_codec.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/notes/note_load_details.h"
#include "components/notes/tests/notes_contenthelper.h"
#include "components/notes/notes_model.h"
#include "components/notes/notes_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

using notes_helper::CreateAutoIndexedContent;

namespace vivaldi {
namespace {

const char16_t kUrl1Title[] = u"url1";
const char kUrl1Url[] = "http://www.url1.com";
const char16_t kUrl2Title[] = u"url2";
const char kUrl2Url[] = "http://www.url2.com";
const char16_t kUrl3Title[] = u"url3";
const char kUrl3Url[] = "http://www.url3.com";
const char16_t kUrl4Title[] = u"url4";
const char kUrl4Url[] = "http://www.url4.com";
const char16_t kTrashUrl1Title[] = u"trash1";
const char16_t kTrashUrl1Url[] = u"http://www.trashurl1.com";
const char16_t kTrashUrl2Title[] = u"trash2";
const char16_t kTrashUrl2Url[] = u"http://www.trashurl2.com";
const char16_t kFolder1Title[] = u"folder1";
const char16_t kFolder2Title[] = u"folder2";
const char16_t kTrashFolder1Title[] = u"trashfolder1";

// Helper to get a mutable note node.
NoteNode* AsMutable(const NoteNode* node) {
  return const_cast<NoteNode*>(node);
}

// Helper to verify the two given notes nodes.
void AssertNodesEqual(const NoteNode* expected, const NoteNode* actual) {
  ASSERT_TRUE(expected);
  ASSERT_TRUE(actual);
  EXPECT_EQ(expected->id(), actual->id());
  EXPECT_EQ(expected->GetTitle(), actual->GetTitle());
  EXPECT_EQ(expected->type(), actual->type());
  EXPECT_EQ(expected->GetContent(), actual->GetContent());
  DCHECK(expected->GetCreationTime() == actual->GetCreationTime());
  EXPECT_TRUE(expected->GetCreationTime() == actual->GetCreationTime());
  if (expected->is_note()) {
    EXPECT_EQ(expected->GetURL(), actual->GetURL());
  } else {
    ASSERT_EQ(expected->children().size(), actual->children().size());
    for (size_t i = 0; i < expected->children().size(); ++i)
      AssertNodesEqual(expected->children()[i].get(),
                       actual->children()[i].get());
  }
}

// Verifies that the two given notes models are the same.
void AssertModelsEqual(NotesModel* expected, NotesModel* actual) {
  ASSERT_NO_FATAL_FAILURE(
      AssertNodesEqual(expected->main_node(), actual->main_node()));
  ASSERT_NO_FATAL_FAILURE(
      AssertNodesEqual(expected->other_node(), actual->other_node()));
}

}  // namespace

std::unique_ptr<NotesModel> CreateTestNotesModel() {
  std::unique_ptr<NotesModel> model =
      std::make_unique<NotesModel>(nullptr, nullptr);
  std::unique_ptr<NoteLoadDetails> details =
      std::make_unique<NoteLoadDetails>();
  model->DoneLoading(std::move(details));
  return model;
}

class NotesCodecTest : public testing::Test {
 protected:
  // Helpers to create notes models with different data.
  NotesModel* CreateTestModel1(int index = 0) {
    std::unique_ptr<NotesModel> model(CreateTestNotesModel());
    const NoteNode* note_node = model->main_node();
    model->AddNote(note_node, 0, kUrl1Title, GURL(kUrl1Url),
                   ASCIIToUTF16(CreateAutoIndexedContent(index)));
    const NoteNode* trash_node = model->trash_node();
    model->AddNote(trash_node, 0, kTrashUrl1Title, GURL(kTrashUrl1Url),
                   ASCIIToUTF16(CreateAutoIndexedContent(index)));
    return model.release();
  }
  NotesModel* CreateTestModel2() {
    std::unique_ptr<NotesModel> model(CreateTestNotesModel());
    const NoteNode* note_node = model->main_node();
    model->AddNote(note_node, 0, kUrl1Title, GURL(kUrl1Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    model->AddNote(note_node, 1, kUrl2Title, GURL(kUrl2Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    const NoteNode* trash_node = model->trash_node();
    model->AddNote(trash_node, 0, kTrashUrl1Title, GURL(kTrashUrl1Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    return model.release();
  }
  NotesModel* CreateTestModel3() {
    std::unique_ptr<NotesModel> model(CreateTestNotesModel());
    const NoteNode* note_node = model->main_node();
    model->AddNote(note_node, 0, kUrl1Title, GURL(kUrl1Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    const NoteNode* folder1 = model->AddFolder(note_node, 1, kFolder1Title);
    model->AddNote(folder1, 0, kUrl2Title, GURL(kUrl2Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    const NoteNode* trash_node = model->trash_node();
    model->AddNote(trash_node, 0, kTrashUrl1Title, GURL(kTrashUrl1Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    const NoteNode* trash_folder1 =
        model->AddFolder(trash_node, 1, kTrashFolder1Title);
    model->AddNote(trash_folder1, 0, kTrashUrl2Title, GURL(kTrashUrl2Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    return model.release();
  }

  void GetNotesChildValue(base::Value& value,
                          size_t index,
                          base::Value::Dict*& result) {
    base::Value::Dict* dict = value.GetIfDict();
    ASSERT_TRUE(dict);

    base::Value::List* bb_children = dict->FindList(NotesCodec::kChildrenKey);
    ASSERT_TRUE(bb_children);

    ASSERT_LT(index, bb_children->size());
    base::Value* child_value = &(*bb_children)[index];
    ASSERT_TRUE(child_value);
    ASSERT_TRUE(child_value->is_dict());

    result = &child_value->GetDict();
  }

  base::Value EncodeHelper(NotesModel* model, std::string* checksum) {
    NotesCodec encoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", encoder.computed_checksum());
    EXPECT_EQ("", encoder.stored_checksum());

    base::Value value(encoder.Encode(model, ""));
    const std::string& computed_checksum = encoder.computed_checksum();
    const std::string& stored_checksum = encoder.stored_checksum();

    // Computed and stored checksums should not be empty and should be equal.
    EXPECT_FALSE(computed_checksum.empty());
    EXPECT_FALSE(stored_checksum.empty());
    EXPECT_EQ(computed_checksum, stored_checksum);

    *checksum = computed_checksum;
    return value;
  }

  bool Decode(NotesCodec* codec, NotesModel* model, const base::Value& value) {
    int64_t max_id;
    bool result = codec->Decode(
        AsMutable(model->main_node()), AsMutable(model->other_node()),
        AsMutable(model->trash_node()), &max_id, value, nullptr);
    model->set_next_node_id(max_id);

    return result;
  }

  NotesModel* DecodeHelper(const base::Value& value,
                           const std::string& expected_stored_checksum,
                           std::string* computed_checksum,
                           bool expected_changes) {
    NotesCodec decoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", decoder.computed_checksum());
    EXPECT_EQ("", decoder.stored_checksum());

    std::unique_ptr<NotesModel> model(CreateTestNotesModel());
    EXPECT_TRUE(Decode(&decoder, model.get(), value));

    *computed_checksum = decoder.computed_checksum();
    const std::string& stored_checksum = decoder.stored_checksum();

    // Computed and stored checksums should not be empty.
    EXPECT_FALSE(computed_checksum->empty());
    EXPECT_FALSE(stored_checksum.empty());

    // Stored checksum should be as expected.
    EXPECT_EQ(expected_stored_checksum, stored_checksum);

    // The two checksums should be equal if expected_changes is true; otherwise
    // they should be different.
    if (expected_changes)
      EXPECT_NE(*computed_checksum, stored_checksum);
    else
      EXPECT_EQ(*computed_checksum, stored_checksum);

    return model.release();
  }

  void CheckIDs(const NoteNode* node, std::set<int64_t>* assigned_ids) {
    DCHECK(node);
    int64_t node_id = node->id();
    EXPECT_TRUE(assigned_ids->find(node_id) == assigned_ids->end());
    assigned_ids->insert(node_id);
    for (auto& it : node->children())
      CheckIDs(it.get(), assigned_ids);
  }

  void ExpectIDsUnique(NotesModel* model) {
    std::set<int64_t> assigned_ids;
    CheckIDs(model->root_node(), &assigned_ids);
  }
};

TEST_F(NotesCodecTest, ChecksumEncodeDecodeTest) {
  std::unique_ptr<NotesModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  base::Value value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  std::string dec_checksum;
  std::unique_ptr<NotesModel> decoded_model(
      DecodeHelper(value, enc_checksum, &dec_checksum, false));
}

TEST_F(NotesCodecTest, ChecksumEncodeIdenticalModelsTest) {
  // Encode two identical models and make sure the check-sums are same as long
  // as the data is the same.
  std::unique_ptr<NotesModel> model1(CreateTestModel1(1003));
  std::string enc_checksum1;
  EncodeHelper(model1.get(), &enc_checksum1);

  std::unique_ptr<NotesModel> model2(CreateTestModel1(1003));
  std::string enc_checksum2;
  EncodeHelper(model2.get(), &enc_checksum2);

  ASSERT_EQ(enc_checksum1, enc_checksum2);
}

TEST_F(NotesCodecTest, ChecksumManualEditTest) {
  std::unique_ptr<NotesModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  base::Value value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  // Change something in the encoded value before decoding it.
  base::Value::Dict* child1 = nullptr;
  GetNotesChildValue(value, 0, child1);
  std::string* title = child1->FindString(NotesCodec::kSubjectKey);
  ASSERT_TRUE(title);
  std::string orig_title = std::move(*title);
  child1->Set(NotesCodec::kSubjectKey, orig_title + "1");

  std::string dec_checksum;
  std::unique_ptr<NotesModel> decoded_model1(
      DecodeHelper(value, enc_checksum, &dec_checksum, true));

  // Undo the change and make sure the checksum is same as original.
  child1->Set(NotesCodec::kSubjectKey, std::move(orig_title));
  std::unique_ptr<NotesModel> decoded_model2(
      DecodeHelper(value, enc_checksum, &dec_checksum, false));
}

TEST_F(NotesCodecTest, ChecksumManualEditIDsTest) {
  std::unique_ptr<NotesModel> model_to_encode(CreateTestModel3());

  // The test depends on existence of multiple children under notes main node,
  // so make sure that's the case.
  int notes_child_count = model_to_encode->main_node()->children().size();
  ASSERT_GT(notes_child_count, 1);

  std::string enc_checksum;
  base::Value value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  // Change IDs for all children of notes main node to be 1.
  for (int i = 0; i < notes_child_count; ++i) {
    base::Value::Dict* child = nullptr;
    GetNotesChildValue(value, i, child);
    ASSERT_TRUE(child->FindString(NotesCodec::kIdKey));
    child->Set(NotesCodec::kIdKey, "1");
  }

  std::string dec_checksum;
  std::unique_ptr<NotesModel> decoded_model(
      DecodeHelper(value, enc_checksum, &dec_checksum, true));

  ExpectIDsUnique(decoded_model.get());

  // add a few extra nodes to notes model and make sure IDs are still uniuqe.
  const NoteNode* notes_node = decoded_model->main_node();
  decoded_model->AddNote(notes_node, 0, u"new url1", GURL("http://newurl1.com"),
                         ASCIIToUTF16(CreateAutoIndexedContent()));
  decoded_model->AddNote(notes_node, 0, u"new url2", GURL("http://newurl2.com"),
                         ASCIIToUTF16(CreateAutoIndexedContent()));

  ExpectIDsUnique(decoded_model.get());
}

TEST_F(NotesCodecTest, PersistIDsTest) {
  std::unique_ptr<NotesModel> model_to_encode(CreateTestModel3());
  NotesCodec encoder;
  base::Value model_value(encoder.Encode(model_to_encode.get(), ""));

  std::unique_ptr<NotesModel> decoded_model(CreateTestNotesModel());
  NotesCodec decoder;
  ASSERT_TRUE(Decode(&decoder, decoded_model.get(), model_value));
  ASSERT_NO_FATAL_FAILURE(
      AssertModelsEqual(model_to_encode.get(), decoded_model.get()));

  // Add a couple of more items to the decoded notes model and make sure
  // ID persistence is working properly.
  const NoteNode* notes_node = decoded_model->main_node();
  decoded_model->AddNote(notes_node, notes_node->children().size(), kUrl3Title,
                         GURL(kUrl3Url),
                         ASCIIToUTF16(CreateAutoIndexedContent()));
  const NoteNode* folder2_node = decoded_model->AddFolder(
      notes_node, notes_node->children().size(), kFolder2Title);
  decoded_model->AddNote(folder2_node, 0, kUrl4Title, GURL(kUrl4Url),
                         ASCIIToUTF16(CreateAutoIndexedContent()));

  NotesCodec encoder2;
  base::Value model_value2(encoder2.Encode(decoded_model.get(), ""));

  std::unique_ptr<NotesModel> decoded_model2(CreateTestNotesModel());
  NotesCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, decoded_model2.get(), model_value2));
  ASSERT_NO_FATAL_FAILURE(
      AssertModelsEqual(decoded_model.get(), decoded_model2.get()));
}

}  // namespace vivaldi
