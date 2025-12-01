bool IsProfileDirectoryScheduledForDeletion(
    const base::FilePath& profile_path) {
  const auto it = ProfilesToDelete().find(profile_path);
  if (it == ProfilesToDelete().end()) {
    return false;
  }
  if (it->second == ProfileDeletionStage::SCHEDULING ||
      it->second == ProfileDeletionStage::MARKED) {
    return true;
  }
  return false;
}
