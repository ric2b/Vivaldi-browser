// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_controller.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {

inline constexpr size_t kYouTubeMusicPlaylistBucketCount = 3;

// This class handles requests from `FocusModeSoundsDelegate` interface. It
// talks to YouTube Music API backend asynchronously, and returns results via
// given callbacks. It handles one request of a kind at a time, which means
// consecutive requests of the same kind would overwrite the previous one. It
// also invokes callbacks strictly, i.e. when successful, failed, or
// overwritten, it would run the given callbacks with valid/empty data.
class ASH_EXPORT FocusModeYouTubeMusicDelegate
    : public FocusModeSoundsDelegate {
 public:
  FocusModeYouTubeMusicDelegate();
  ~FocusModeYouTubeMusicDelegate() override;

  // FocusModeSoundsDelegate:
  bool GetNextTrack(const std::string& playlist_id,
                    FocusModeSoundsDelegate::TrackCallback callback) override;
  bool GetPlaylists(
      FocusModeSoundsDelegate::PlaylistsCallback callback) override;

  void SetFailureCallback(base::RepeatingClosure callback);

  // Reserves a playlist for the returned playlists.
  void ReservePlaylistForGetPlaylists(const std::string& playlist_id);

 private:
  // Struct that keeps track of ongoing `GetPlaylists` request. It contains
  // enough information about how the current request should be done.
  struct GetPlaylistsRequestState {
    GetPlaylistsRequestState();
    ~GetPlaylistsRequestState();

    void Reset();

    void ResetDoneCallback();

    std::vector<FocusModeSoundsDelegate::Playlist> GetTopPlaylists();

    // Data structure that holds data from multiple API requests. It's organized
    // in buckets so that the returned list is ordered.
    std::array<std::vector<Playlist>, kYouTubeMusicPlaylistBucketCount>
        playlist_buckets;

    // Playlist ID to bucket map. It contains all specific playlists to query
    // for the request.
    base::flat_map<std::string, size_t> playlists_to_query;

    // Reserved playlist to query if set.
    std::optional<std::string> reserved_playlist_id;

    // Target number of API requests.
    int target_count = 0;

    // Count of current done API requests.
    int count = 0;

    // Callback to run when this request is successful, failed, or overwritten.
    FocusModeSoundsDelegate::PlaylistsCallback done_callback;
  };

  // Struct that keeps track of ongoing `GetNextTrack` request. It contains
  // enough information about how the current request should be done.
  struct GetNextTrackRequestState {
    GetNextTrackRequestState();
    ~GetNextTrackRequestState();

    void Reset();

    void ResetDoneCallback();

    std::string last_playlist_id;
    std::string last_queue_id;
    FocusModeSoundsDelegate::TrackCallback done_callback;
  };

  // Called when get playlists request is done.
  void OnGetPlaylistDone(size_t bucket,
                         google_apis::ApiErrorCode http_error_code,
                         std::optional<youtube_music::Playlist> playlist);

  // Called when get music section request is done.
  void OnGetMusicSectionDone(
      size_t bucket,
      google_apis::ApiErrorCode http_error_code,
      std::optional<const std::vector<youtube_music::Playlist>> playlists);

  // Called when switching to next track is done.
  void OnNextTrackDone(
      const std::string& playlist_id,
      google_apis::ApiErrorCode http_error_code,
      std::optional<const youtube_music::PlaybackContext> playback_context);

  // Playlists request state for `GetPlaylists`.
  GetPlaylistsRequestState get_playlists_state_;

  // Next track request state for `GetPlaylists`.
  GetNextTrackRequestState next_track_state_;

  // Callback to run when the request fails.
  base::RepeatingClosure failure_callback_;

  base::WeakPtrFactory<FocusModeYouTubeMusicDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_
