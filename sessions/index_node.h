// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef SESSIONS_INDEX_NODE_H_
#define SESSIONS_INDEX_NODE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "base/synchronization/waitable_event.h"
#include "ui/base/models/tree_node_model.h"

class Profile;

namespace base {
class Value;
class SequencedTaskRunner;
}  // namespace base

namespace sessions {

class IndexStorage;
class IndexLoadDetails;

class Index_Node : public ui::TreeNode<Index_Node> {
 public:
  enum Type {kNode=0, kFolder=1};

  Index_Node(const std::string& guid, int64_t id, Type type = kNode);
  ~Index_Node() override;
  Index_Node(const Index_Node&) = delete;
  Index_Node& operator=(const Index_Node&) = delete;

  void DumpTree(int indent = 0);

  int64_t id() const { return id_; }

  // SetGuid() should only be used if we have to change guid as a result of
  // resolving a guid duplication.
  void SetGuid(std::string guid) { guid_ = guid; }
  const std::string& guid() const { return guid_; }

  // Returns the node in the tree of nodes that match the id.
  Index_Node* GetById(int64_t id);
  // Returns the node in the tree of nodes that match the guid.
  Index_Node* GetByGuid(const std::string& guid);

  // Copies content except id and guid of 'from'.
  void Copy(const Index_Node* from);

  static std::string root_node_guid() { return kRootNodeGuid; }
  static int64_t root_node_id() { return kRootNodeId; }
  static std::string items_node_guid() { return kItemsNodeGuid; }
  static int64_t items_node_id() { return kItemsNodeId; }
  static std::string trash_node_guid() { return kTrashNodeGuid; }
  static int64_t trash_node_id() { return kTrashNodeId; }
  static std::string autosave_node_guid() { return kAutosaveNodeGuid; }
  static int64_t autosave_node_id() { return kAutosaveNodeId; }
  static std::string backup_node_guid() { return kBackupNodeGuid; }
  static int64_t backup_node_id() { return kBackupNodeId; }
  static std::string persistent_node_guid() { return kPersistentNodeGuid; }
  static int64_t persistent_node_id() { return kPersistentNodeId; }

  static int64_t GetNewId() { return id_counter++; }

  Type type() const { return type_; }
  bool is_folder() const { return type_ == kFolder; }
  bool is_trash_folder() const { return id_ == kTrashNodeId; }
  bool is_autosave_node() const { return id_ == kAutosaveNodeId; }
  bool is_backup_node() const { return id_ == kBackupNodeId; }
  bool is_persistent_node() const { return id_ == kPersistentNodeId; }
  bool is_container() const { return type_ == kNode && children().size() > 0; }

  void SetFilename(std::string filename) { filename_ = filename; }
  std::string filename() const { return filename_; }
  void SetContainerGuid(std::string container_guid) {
    container_guid_ = container_guid;
  }
  std::string container_guid() const { return container_guid_; }
  void SetCreateTime(double time) { create_time_ = time; }
  double create_time() const { return create_time_; }
  void SetModifyTime(double time) { modify_time_ = time; }
  double modify_time() const { return modify_time_; }
  void SetWindowsCount(int count) { windows_count_ = count; }
  int windows_count() const { return windows_count_; }
  void SetTabsCount(int count) { tabs_count_ = count; }
  int tabs_count() const { return tabs_count_; }
  void SetQuarantineCount(int count) { quarantine_count_ = count; }
  int quarantine_count() const { return quarantine_count_; }
  void SetWorkspaces(base::Value::List workspaces) {
    workspaces_ = std::move(workspaces);
  }
  const base::Value::List &workspaces() const { return workspaces_; }
  void SetGroupNames(base::Value::Dict group_names) {
    group_names_ = std::move(group_names);
  }
  const base::Value::Dict &group_names() const { return group_names_; }

 private:
  friend class Index_Model;
  // Runtime ids for items. Not saved to disk as guids are.
  enum FixedId {
    // Top node
    kRootNodeId = 1,
    // All items exposed to the user are inside this node.
    kItemsNodeId,
    // Deleted items are inside this node.
    kTrashNodeId,
    // Save on exit items are listed here.
    kAutosaveNodeId,
    // This node is not visible to the user. Holds a backup that is updated
    // regularlly. Removed on exit, but if not (because of a crash or similar)
    // moved to the list managed by kAutosaveNodeId on next startup.
    kBackupNodeId,
    // This node is not visible to the user. Holds persistent nodes (pinned and
    // workspace nodes) when all windows are closed.
    kPersistentNodeId,
    // Regular items get ids starting with this.
    kFirstDynamicNodeId
  };
  static const char kRootNodeGuid[];
  static const char kItemsNodeGuid[];
  static const char kTrashNodeGuid[];
  static const char kAutosaveNodeGuid[];
  static const char kBackupNodeGuid[];
  static const char kPersistentNodeGuid[];
  std::string filename_;
  double create_time_ = 0;
  double modify_time_ = 0;
  int windows_count_ = 0;
  int tabs_count_ = 0;
  int quarantine_count_ = 0;
  std::string guid_;
  // A key to look up what container (if any) a trashed node came from.
  std::string container_guid_;
  int64_t id_;
  Type type_;
  base::Value::List workspaces_;
  base::Value::Dict group_names_;

  // Next available id is copied from this value.
  static int64_t id_counter;
};

}  // namespace sessions

#endif  //  SESSIONS_INDEX_NODE_H_