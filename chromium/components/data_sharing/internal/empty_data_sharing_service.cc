// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/empty_data_sharing_service.h"

#include "base/functional/callback.h"

namespace data_sharing {

EmptyDataSharingService::EmptyDataSharingService() = default;

EmptyDataSharingService::~EmptyDataSharingService() = default;

bool EmptyDataSharingService::IsEmptyService() {
  return true;
}

DataSharingNetworkLoader*
EmptyDataSharingService::GetDataSharingNetworkLoader() {
  return nullptr;
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
EmptyDataSharingService::GetCollaborationGroupControllerDelegate() {
  return nullptr;
}

void EmptyDataSharingService::AddObserver(Observer* observer) {}

void EmptyDataSharingService::RemoveObserver(Observer* observer) {}

void EmptyDataSharingService::ReadAllGroups(
    base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::ReadGroup(
    const GroupId& group_id,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::CreateGroup(
    const std::string& group_name,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::DeleteGroup(
    const GroupId& group_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

void EmptyDataSharingService::InviteMember(
    const GroupId& group_id,
    const std::string& invitee_email,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

void EmptyDataSharingService::RemoveMember(
    const GroupId& group_id,
    const std::string& member_email,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

bool EmptyDataSharingService::ShouldInterceptNavigationForShareURL(
    const GURL& url) {
  return false;
}

void EmptyDataSharingService::HandleShareURLNavigationIntercepted(
    const GURL& url) {}

}  // namespace data_sharing
