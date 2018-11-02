// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notes/notes_codec.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "sync/test/integration/notes_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

using notes_helper::CreateAutoIndexedContent;

namespace vivaldi {
namespace {

const char kUrl1Title[] = "url1";
const char kUrl1Url[] = "http://www.url1.com";
const char kUrl2Title[] = "url2";
const char kUrl2Url[] = "http://www.url2.com";
const char kUrl3Title[] = "url3";
const char kUrl3Url[] = "http://www.url3.com";
const char kUrl4Title[] = "url4";
const char kUrl4Url[] = "http://www.url4.com";
const char kFolder1Title[] = "folder1";
const char kFolder2Title[] = "folder2";

// Helper to verify the two given notes nodes.
void AssertNodesEqual(const Notes_Node* expected, const Notes_Node* actual) {
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
    ASSERT_EQ(expected->child_count(), actual->child_count());
    for (int i = 0; i < expected->child_count(); ++i)
      AssertNodesEqual(expected->GetChild(i), actual->GetChild(i));
  }
}

// Verifies that the two given notes models are the same.
void AssertModelsEqual(Notes_Model* expected, Notes_Model* actual) {
  ASSERT_NO_FATAL_FAILURE(
      AssertNodesEqual(expected->main_node(), actual->main_node()));
  ASSERT_NO_FATAL_FAILURE(
      AssertNodesEqual(expected->other_node(), actual->other_node()));
}

}  // namespace

class NotesCodecTest : public testing::Test {
 protected:
  // Helpers to create notes models with different data.
  Notes_Model* CreateTestModel1(int index = 0) {
    std::unique_ptr<Notes_Model> model(Notes_Model::CreateModel());
    const Notes_Node* notes_node = model->main_node();
    model->AddNote(notes_node, 0, ASCIIToUTF16(kUrl1Title), GURL(kUrl1Url),
                   ASCIIToUTF16(CreateAutoIndexedContent(index)));
    return model.release();
  }
  Notes_Model* CreateTestModel2() {
    std::unique_ptr<Notes_Model> model(Notes_Model::CreateModel());
    const Notes_Node* notes_node = model->main_node();
    model->AddNote(notes_node, 0, ASCIIToUTF16(kUrl1Title), GURL(kUrl1Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    model->AddNote(notes_node, 1, ASCIIToUTF16(kUrl2Title), GURL(kUrl2Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    return model.release();
  }
  Notes_Model* CreateTestModel3() {
    std::unique_ptr<Notes_Model> model(Notes_Model::CreateModel());
    const Notes_Node* notes_node = model->main_node();
    model->AddNote(notes_node, 0, ASCIIToUTF16(kUrl1Title), GURL(kUrl1Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    const Notes_Node* folder1 =
        model->AddFolder(notes_node, 1, ASCIIToUTF16(kFolder1Title));
    model->AddNote(folder1, 0, ASCIIToUTF16(kUrl2Title), GURL(kUrl2Url),
                   ASCIIToUTF16(CreateAutoIndexedContent()));
    return model.release();
  }

  void GetNotesChildValue(base::Value* value,
                          size_t index,
                          base::DictionaryValue** result_value) {
    ASSERT_EQ(base::Value::Type::DICTIONARY, value->type());

    base::DictionaryValue* d_value = static_cast<base::DictionaryValue*>(value);

    base::Value* bb_children_value;
    ASSERT_TRUE(d_value->Get(NotesCodec::kChildrenKey, &bb_children_value));
    ASSERT_EQ(base::Value::Type::LIST, bb_children_value->type());

    base::ListValue* bb_children_l_value =
        static_cast<base::ListValue*>(bb_children_value);
    base::Value* child_value;
    ASSERT_TRUE(bb_children_l_value->Get(index, &child_value));
    ASSERT_EQ(base::Value::Type::DICTIONARY, child_value->type());

    *result_value = static_cast<base::DictionaryValue*>(child_value);
  }

  base::Value* EncodeHelper(Notes_Model* model, std::string* checksum) {
    NotesCodec encoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", encoder.computed_checksum());
    EXPECT_EQ("", encoder.stored_checksum());

    std::unique_ptr<base::Value> value(encoder.Encode(model));
    const std::string& computed_checksum = encoder.computed_checksum();
    const std::string& stored_checksum = encoder.stored_checksum();

    // Computed and stored checksums should not be empty and should be equal.
    EXPECT_FALSE(computed_checksum.empty());
    EXPECT_FALSE(stored_checksum.empty());
    EXPECT_EQ(computed_checksum, stored_checksum);

    *checksum = computed_checksum;
    return value.release();
  }

  bool Decode(NotesCodec* codec, Notes_Model* model, const base::Value& value) {
    int64_t max_id;
    bool result = codec->Decode(AsMutable(model->main_node()),
                                AsMutable(model->other_node()),
                                AsMutable(model->trash_node()), &max_id, value);
    model->set_next_index_id(max_id);
    AsMutable(model->root_node())
        ->set_sync_transaction_version(codec->model_sync_transaction_version());

    return result;
  }

  Notes_Model* DecodeHelper(const base::Value& value,
                            const std::string& expected_stored_checksum,
                            std::string* computed_checksum,
                            bool expected_changes) {
    NotesCodec decoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", decoder.computed_checksum());
    EXPECT_EQ("", decoder.stored_checksum());

    std::unique_ptr<Notes_Model> model(Notes_Model::CreateModel());
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

  void CheckIDs(const Notes_Node* node, std::set<int64_t>* assigned_ids) {
    DCHECK(node);
    int64_t node_id = node->id();
    EXPECT_TRUE(assigned_ids->find(node_id) == assigned_ids->end());
    assigned_ids->insert(node_id);
    for (int i = 0; i < node->child_count(); ++i)
      CheckIDs(node->GetChild(i), assigned_ids);
  }

  void ExpectIDsUnique(Notes_Model* model) {
    std::set<int64_t> assigned_ids;
    CheckIDs(model->root_node(), &assigned_ids);
  }
};

TEST_F(NotesCodecTest, ChecksumEncodeDecodeTest) {
  std::unique_ptr<Notes_Model> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  std::unique_ptr<base::Value> value(
      EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  std::string dec_checksum;
  std::unique_ptr<Notes_Model> decoded_model(
      DecodeHelper(*value.get(), enc_checksum, &dec_checksum, false));
}

TEST_F(NotesCodecTest, ChecksumEncodeIdenticalModelsTest) {
  // Encode two identical models and make sure the check-sums are same as long
  // as the data is the same.
  std::unique_ptr<Notes_Model> model1(CreateTestModel1(1003));
  std::string enc_checksum1;
  std::unique_ptr<base::Value> value1(
      EncodeHelper(model1.get(), &enc_checksum1));
  EXPECT_TRUE(value1.get() != NULL);

  std::unique_ptr<Notes_Model> model2(CreateTestModel1(1003));
  std::string enc_checksum2;
  std::unique_ptr<base::Value> value2(
      EncodeHelper(model2.get(), &enc_checksum2));
  EXPECT_TRUE(value2.get() != NULL);

  ASSERT_EQ(enc_checksum1, enc_checksum2);
}

TEST_F(NotesCodecTest, ChecksumManualEditTest) {
  std::unique_ptr<Notes_Model> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  std::unique_ptr<base::Value> value(
      EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  // Change something in the encoded value before decoding it.
  base::DictionaryValue* child1_value;
  GetNotesChildValue(value.get(), 0, &child1_value);
  std::string title;
  ASSERT_TRUE(child1_value->GetString(NotesCodec::kNameKey, &title));
  child1_value->SetString(NotesCodec::kNameKey, title + "1");

  std::string dec_checksum;
  std::unique_ptr<Notes_Model> decoded_model1(
      DecodeHelper(*value.get(), enc_checksum, &dec_checksum, true));

  // Undo the change and make sure the checksum is same as original.
  child1_value->SetString(NotesCodec::kNameKey, title);
  std::unique_ptr<Notes_Model> decoded_model2(
      DecodeHelper(*value.get(), enc_checksum, &dec_checksum, false));
}

TEST_F(NotesCodecTest, ChecksumManualEditIDsTest) {
  std::unique_ptr<Notes_Model> model_to_encode(CreateTestModel3());

  // The test depends on existence of multiple children under notes main node,
  // so make sure that's the case.
  int notes_child_count = model_to_encode->main_node()->child_count();
  ASSERT_GT(notes_child_count, 1);

  std::string enc_checksum;
  std::unique_ptr<base::Value> value(
      EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  // Change IDs for all children of notes main node to be 1.
  base::DictionaryValue* child_value;
  for (int i = 0; i < notes_child_count; ++i) {
    GetNotesChildValue(value.get(), i, &child_value);
    std::string id;
    ASSERT_TRUE(child_value->GetString(NotesCodec::kIdKey, &id));
    child_value->SetString(NotesCodec::kIdKey, "1");
  }

  std::string dec_checksum;
  std::unique_ptr<Notes_Model> decoded_model(
      DecodeHelper(*value.get(), enc_checksum, &dec_checksum, true));

  ExpectIDsUnique(decoded_model.get());

  // add a few extra nodes to notes model and make sure IDs are still uniuqe.
  const Notes_Node* notes_node = decoded_model->main_node();
  decoded_model->AddNote(notes_node, 0, ASCIIToUTF16("new url1"),
                         GURL("http://newurl1.com"),
                         ASCIIToUTF16(CreateAutoIndexedContent()));
  decoded_model->AddNote(notes_node, 0, ASCIIToUTF16("new url2"),
                         GURL("http://newurl2.com"),
                         ASCIIToUTF16(CreateAutoIndexedContent()));

  ExpectIDsUnique(decoded_model.get());
}

TEST_F(NotesCodecTest, PersistIDsTest) {
  std::unique_ptr<Notes_Model> model_to_encode(CreateTestModel3());
  NotesCodec encoder;
  std::unique_ptr<base::Value> model_value(
      encoder.Encode(model_to_encode.get()));

  std::unique_ptr<Notes_Model> decoded_model(Notes_Model::CreateModel());
  NotesCodec decoder;
  ASSERT_TRUE(Decode(&decoder, decoded_model.get(), *model_value.get()));
  ASSERT_NO_FATAL_FAILURE(
      AssertModelsEqual(model_to_encode.get(), decoded_model.get()));

  // Add a couple of more items to the decoded notes model and make sure
  // ID persistence is working properly.
  const Notes_Node* notes_node = decoded_model->main_node();
  decoded_model->AddNote(notes_node, notes_node->child_count(),
                         ASCIIToUTF16(kUrl3Title), GURL(kUrl3Url),
                         ASCIIToUTF16(CreateAutoIndexedContent()));
  const Notes_Node* folder2_node = decoded_model->AddFolder(
      notes_node, notes_node->child_count(), ASCIIToUTF16(kFolder2Title));
  decoded_model->AddNote(folder2_node, 0, ASCIIToUTF16(kUrl4Title),
                         GURL(kUrl4Url),
                         ASCIIToUTF16(CreateAutoIndexedContent()));

  NotesCodec encoder2;
  std::unique_ptr<base::Value> model_value2(
      encoder2.Encode(decoded_model.get()));

  std::unique_ptr<Notes_Model> decoded_model2(Notes_Model::CreateModel());
  NotesCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, decoded_model2.get(), *model_value2.get()));
  ASSERT_NO_FATAL_FAILURE(
      AssertModelsEqual(decoded_model.get(), decoded_model2.get()));
}

TEST_F(NotesCodecTest, EncodeAndDecodeSyncTransactionVersion) {
  // Add sync transaction version and encode.
  std::unique_ptr<Notes_Model> model(CreateTestModel2());
  model->SetNodeSyncTransactionVersion(model->root_node(), 1);
  const Notes_Node* main = model->main_node();
  model->SetNodeSyncTransactionVersion(main->GetChild(1), 42);

  std::string checksum;
  std::unique_ptr<base::Value> value(EncodeHelper(model.get(), &checksum));
  ASSERT_TRUE(value.get() != NULL);

  // Decode and verify.
  model.reset(DecodeHelper(*value, checksum, &checksum, false));
  EXPECT_EQ(1, model->root_node()->sync_transaction_version());
  main = model->main_node();
  EXPECT_EQ(42, main->GetChild(1)->sync_transaction_version());
  EXPECT_EQ(Notes_Node::kInvalidSyncTransactionVersion,
            main->GetChild(0)->sync_transaction_version());
}

}  // namespace vivaldi
