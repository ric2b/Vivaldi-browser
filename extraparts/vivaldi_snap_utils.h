#ifndef EXTRAPARTS_VIVALDI_SNAP_UTILS_H
#define EXTRAPARTS_VIVALDI_SNAP_UTILS_H

namespace vivaldi {

// Detects that we're running in a Snapcraft container
bool IsRunningInSnap();

// If under snap, will return true and set path to point to $SNAP_REAL_HOME/.local/share/applications
bool GetSnapDesktopPathOverride(base::FilePath *path);

} // namespace vivaldi

#endif /* EXTRAPARTS_VIVALDI_SNAP_UTILS_H */
