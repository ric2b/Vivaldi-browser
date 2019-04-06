// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/syncable/entry_kernel.h"
#include "components/sync/syncable/parent_child_index.h"
#include "components/sync/syncable/syncable_util.h"
#include "sync/vivaldi_hash_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::GenerateSyncableNotesHash;

namespace syncer {
namespace syncable {

namespace {

static const char kCacheGuid[] = "8HhNIHlEOCGQbIAALr9QEg==";

class NotesParentChildIndexTest : public testing::Test {
 public:
  void TearDown() override {}

  // Unfortunately, we can't use the regular Entry factory methods, because the
  // ParentChildIndex deals in EntryKernels.

  static syncable::Id GetNoteRootId() {
    return syncable::Id::CreateFromServerId("notes_folder");
  }

  static syncable::Id GetNoteId(int n) {
    return syncable::Id::CreateFromServerId("b" + base::IntToString(n));
  }

  static syncable::Id GetClientUniqueId(int n) {
    return syncable::Id::CreateFromServerId("c" + base::IntToString(n));
  }

  EntryKernel* MakeRoot() {
    // Mimics the root node.
    EntryKernel* root = new EntryKernel();
    root->put(META_HANDLE, 1);
    root->put(BASE_VERSION, -1);
    root->put(SERVER_VERSION, 0);
    root->put(IS_DIR, true);
    root->put(ID, syncable::Id::GetRoot());
    root->put(PARENT_ID, syncable::Id::GetRoot());

    owned_entry_kernels_.push_back(base::WrapUnique(root));
    return root;
  }

  EntryKernel* MakeNoteRoot() {
    // Mimics a server-created notes folder.
    EntryKernel* folder = new EntryKernel;
    folder->put(META_HANDLE, 1);
    folder->put(BASE_VERSION, 9);
    folder->put(SERVER_VERSION, 9);
    folder->put(IS_DIR, true);
    folder->put(ID, GetNoteRootId());
    folder->put(PARENT_ID, syncable::Id::GetRoot());
    folder->put(UNIQUE_SERVER_TAG, "vivaldi_notes");

    owned_entry_kernels_.push_back(base::WrapUnique(folder));
    return folder;
  }

  EntryKernel* MakeNote(int n, int pos, bool is_dir) {
    // Mimics a regular notes or folder.
    EntryKernel* bm = new EntryKernel();
    bm->put(META_HANDLE, n);
    bm->put(BASE_VERSION, 10);
    bm->put(SERVER_VERSION, 10);
    bm->put(IS_DIR, is_dir);
    bm->put(ID, GetNoteId(n));
    bm->put(PARENT_ID, GetNoteRootId());

    bm->put(UNIQUE_NOTES_TAG,
            GenerateSyncableNotesHash(kCacheGuid, bm->ref(ID).GetServerId()));

    UniquePosition unique_pos =
        UniquePosition::FromInt64(pos, bm->ref(UNIQUE_NOTES_TAG));
    bm->put(UNIQUE_POSITION, unique_pos);
    bm->put(SERVER_UNIQUE_POSITION, unique_pos);

    owned_entry_kernels_.push_back(base::WrapUnique(bm));
    return bm;
  }

  EntryKernel* MakeUniqueClientItem(int n) {
    EntryKernel* item = new EntryKernel();
    item->put(META_HANDLE, n);
    item->put(BASE_VERSION, 10);
    item->put(SERVER_VERSION, 10);
    item->put(IS_DIR, false);
    item->put(ID, GetClientUniqueId(n));
    item->put(PARENT_ID, syncable::Id());
    item->put(UNIQUE_CLIENT_TAG, base::IntToString(n));

    owned_entry_kernels_.push_back(base::WrapUnique(item));
    return item;
  }

  ParentChildIndex index_;

 private:
  std::vector<std::unique_ptr<EntryKernel>> owned_entry_kernels_;
};

TEST_F(NotesParentChildIndexTest, TestRootNode) {
  EntryKernel* root = MakeRoot();
  EXPECT_FALSE(ParentChildIndex::ShouldInclude(root));
}

TEST_F(NotesParentChildIndexTest, TestNoteRootFolder) {
  EntryKernel* bm_folder = MakeNoteRoot();
  EXPECT_TRUE(ParentChildIndex::ShouldInclude(bm_folder));
}

// Tests iteration over a set of siblings.
TEST_F(NotesParentChildIndexTest, ChildInsertionAndIteration) {
  EntryKernel* bm_folder = MakeNoteRoot();
  index_.Insert(bm_folder);

  // Make some folder and non-folder entries.
  EntryKernel* b1 = MakeNote(1, 1, false);
  EntryKernel* b2 = MakeNote(2, 2, false);
  EntryKernel* b3 = MakeNote(3, 3, true);
  EntryKernel* b4 = MakeNote(4, 4, false);

  // Insert them out-of-order to test different cases.
  index_.Insert(b3);  // Only child.
  index_.Insert(b4);  // Right-most child.
  index_.Insert(b1);  // Left-most child.
  index_.Insert(b2);  // Between existing items.

  // Double-check they've been added.
  EXPECT_TRUE(index_.Contains(b1));
  EXPECT_TRUE(index_.Contains(b2));
  EXPECT_TRUE(index_.Contains(b3));
  EXPECT_TRUE(index_.Contains(b4));

  // Check the ordering.
  const OrderedChildSet* children = index_.GetChildren(GetNoteRootId());
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 4UL);
  OrderedChildSet::const_iterator it = children->begin();
  EXPECT_EQ(*it, b1);
  it++;
  EXPECT_EQ(*it, b2);
  it++;
  EXPECT_EQ(*it, b3);
  it++;
  EXPECT_EQ(*it, b4);
  it++;
  EXPECT_TRUE(it == children->end());
}

// Tests iteration when hierarchy is involved.
TEST_F(NotesParentChildIndexTest, ChildInsertionAndIterationWithHierarchy) {
  EntryKernel* bm_folder = MakeNoteRoot();
  index_.Insert(bm_folder);

  // Just below the root, we have folders f1 and f2.
  EntryKernel* f1 = MakeNote(1, 1, false);
  EntryKernel* f2 = MakeNote(2, 2, false);
  EntryKernel* f3 = MakeNote(3, 3, false);

  // Under folder f1, we have two notes.
  EntryKernel* f1_b1 = MakeNote(101, 1, false);
  f1_b1->put(PARENT_ID, GetNoteId(1));
  EntryKernel* f1_b2 = MakeNote(102, 2, false);
  f1_b2->put(PARENT_ID, GetNoteId(1));

  // Under folder f2, there is one notes.
  EntryKernel* f2_b1 = MakeNote(201, 1, false);
  f2_b1->put(PARENT_ID, GetNoteId(2));

  // Under folder f3, there is nothing.

  // Insert in a strange order, because we can.
  index_.Insert(f1_b2);
  index_.Insert(f2);
  index_.Insert(f2_b1);
  index_.Insert(f1);
  index_.Insert(f1_b1);
  index_.Insert(f3);

  OrderedChildSet::const_iterator it;

  // Iterate over children of the notes root.
  const OrderedChildSet* top_children = index_.GetChildren(GetNoteRootId());
  ASSERT_TRUE(top_children);
  ASSERT_EQ(top_children->size(), 3UL);
  it = top_children->begin();
  EXPECT_EQ(*it, f1);
  it++;
  EXPECT_EQ(*it, f2);
  it++;
  EXPECT_EQ(*it, f3);
  it++;
  EXPECT_TRUE(it == top_children->end());

  // Iterate over children of the first folder.
  const OrderedChildSet* f1_children = index_.GetChildren(GetNoteId(1));
  ASSERT_TRUE(f1_children);
  ASSERT_EQ(f1_children->size(), 2UL);
  it = f1_children->begin();
  EXPECT_EQ(*it, f1_b1);
  it++;
  EXPECT_EQ(*it, f1_b2);
  it++;
  EXPECT_TRUE(it == f1_children->end());

  // Iterate over children of the second folder.
  const OrderedChildSet* f2_children = index_.GetChildren(GetNoteId(2));
  ASSERT_TRUE(f2_children);
  ASSERT_EQ(f2_children->size(), 1UL);
  it = f2_children->begin();
  EXPECT_EQ(*it, f2_b1);
  it++;
  EXPECT_TRUE(it == f2_children->end());

  // Check for children of the third folder.
  const OrderedChildSet* f3_children = index_.GetChildren(GetNoteId(3));
  EXPECT_FALSE(f3_children);
}

// Tests removing items.
TEST_F(NotesParentChildIndexTest, RemoveWithHierarchy) {
  EntryKernel* bm_folder = MakeNoteRoot();
  index_.Insert(bm_folder);

  // Just below the root, we have folders f1 and f2.
  EntryKernel* f1 = MakeNote(1, 1, false);
  EntryKernel* f2 = MakeNote(2, 2, false);
  EntryKernel* f3 = MakeNote(3, 3, false);

  // Under folder f1, we have two notes.
  EntryKernel* f1_b1 = MakeNote(101, 1, false);
  f1_b1->put(PARENT_ID, GetNoteId(1));
  EntryKernel* f1_b2 = MakeNote(102, 2, false);
  f1_b2->put(PARENT_ID, GetNoteId(1));

  // Under folder f2, there is one notes.
  EntryKernel* f2_b1 = MakeNote(201, 1, false);
  f2_b1->put(PARENT_ID, GetNoteId(2));

  // Under folder f3, there is nothing.

  // Insert in any order.
  index_.Insert(f2_b1);
  index_.Insert(f3);
  index_.Insert(f1_b2);
  index_.Insert(f1);
  index_.Insert(f2);
  index_.Insert(f1_b1);

  // Check that all are in the index.
  EXPECT_TRUE(index_.Contains(f1));
  EXPECT_TRUE(index_.Contains(f2));
  EXPECT_TRUE(index_.Contains(f3));
  EXPECT_TRUE(index_.Contains(f1_b1));
  EXPECT_TRUE(index_.Contains(f1_b2));
  EXPECT_TRUE(index_.Contains(f2_b1));

  // Remove them all in any order.
  index_.Remove(f3);
  EXPECT_FALSE(index_.Contains(f3));
  index_.Remove(f1_b2);
  EXPECT_FALSE(index_.Contains(f1_b2));
  index_.Remove(f2_b1);
  EXPECT_FALSE(index_.Contains(f2_b1));
  index_.Remove(f1);
  EXPECT_FALSE(index_.Contains(f1));
  index_.Remove(f2);
  EXPECT_FALSE(index_.Contains(f2));
  index_.Remove(f1_b1);
  EXPECT_FALSE(index_.Contains(f1_b1));
}

// Test that involves two non-ordered items.
TEST_F(NotesParentChildIndexTest, UnorderedChildren) {
  // Make two unique client tag items under the root node.
  EntryKernel* u1 = MakeUniqueClientItem(1);
  EntryKernel* u2 = MakeUniqueClientItem(2);

  EXPECT_FALSE(u1->ShouldMaintainPosition());
  EXPECT_FALSE(u2->ShouldMaintainPosition());

  index_.Insert(u1);
  index_.Insert(u2);

  const OrderedChildSet* children = index_.GetChildren(syncable::Id());
  EXPECT_EQ(children->count(u1), 1UL);
  EXPECT_EQ(children->count(u2), 1UL);
  EXPECT_EQ(children->size(), 2UL);
}

// Test ordered and non-ordered entries under the same parent.
// TODO(rlarocque): We should not need to support this.
TEST_F(NotesParentChildIndexTest, OrderedAndUnorderedChildren) {
  EntryKernel* bm_folder = MakeNoteRoot();
  index_.Insert(bm_folder);

  EntryKernel* b1 = MakeNote(1, 1, false);
  EntryKernel* b2 = MakeNote(2, 2, false);
  EntryKernel* u1 = MakeUniqueClientItem(1);
  u1->put(PARENT_ID, GetNoteRootId());

  index_.Insert(b1);
  index_.Insert(u1);
  index_.Insert(b2);

  const OrderedChildSet* children = index_.GetChildren(GetNoteRootId());
  ASSERT_TRUE(children);
  EXPECT_EQ(children->size(), 3UL);

  // Ensure that the non-positionable item is moved to the far right.
  OrderedChildSet::const_iterator it = children->begin();
  EXPECT_EQ(*it, b1);
  it++;
  EXPECT_EQ(*it, b2);
  it++;
  EXPECT_EQ(*it, u1);
  it++;
  EXPECT_TRUE(it == children->end());
}

}  // namespace
}  // namespace syncable
}  // namespace syncer
