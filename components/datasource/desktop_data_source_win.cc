// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/desktop_data_source_win.h"

#include <Shobjidl.h>
#include <wrl/client.h>

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_co_mem.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/skia_utils_win.h"

DesktopWallpaperDataClassHandlerWin::DesktopWallpaperDataClassHandlerWin() {}

DesktopWallpaperDataClassHandlerWin::~DesktopWallpaperDataClassHandlerWin() {}

bool DesktopWallpaperDataClassHandlerWin::GetData(
    Profile* profile,
    const std::string& data_id,
    const content::URLDataSource::GotDataCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  Microsoft::WRL::ComPtr<IDesktopWallpaper> desktop_w;
  HRESULT hr = CoCreateInstance(__uuidof(DesktopWallpaper), nullptr,
                                CLSCTX_ALL, IID_PPV_ARGS(&desktop_w));
  if (FAILED(hr)) {
    return false;
  }
  UINT count;
  hr = desktop_w->GetMonitorDevicePathCount(&count);
  if (FAILED(hr)) {
    return false;
  }
  base::win::ScopedCoMem<wchar_t> file_path;
  base::win::ScopedCoMem<wchar_t> monitor_id;

  for (UINT n = 0; n < count; n++) {
    hr = desktop_w->GetMonitorDevicePathAt(
        n, reinterpret_cast<LPWSTR*>(&monitor_id));
    if (SUCCEEDED(hr)) {
      // Try first without monitor id, this will work if the user
      // has the same image on all monitors.
      hr = desktop_w->GetWallpaper(nullptr,
                                   reinterpret_cast<LPWSTR*>(&file_path));
      if (hr == S_FALSE) {
        file_path.Reset(nullptr);
        hr = desktop_w->GetWallpaper(monitor_id,
                                     reinterpret_cast<LPWSTR*>(&file_path));
      }
      if (SUCCEEDED(hr)) {
        if (file_path.get() == previous_path_) {
          // Path has not changed, serve cached data
          callback.Run(cached_image_data_);
          return true;
        } else {
          // TODO(pettern): Offload to FILE thread.
          base::ThreadRestrictions::ScopedAllowIO allow_io;
          base::FilePath f(file_path.get());
          base::File file(f, base::File::FLAG_READ | base::File::FLAG_OPEN);
          int64_t len = file.GetLength();
          if (len > 0) {
            std::unique_ptr<char[]> buffer(new char[len]);
            int read_len = file.Read(0, buffer.get(), len);
            if (read_len == len) {
              scoped_refptr<base::RefCountedMemory> image_data(
                new base::RefCountedBytes(
                  reinterpret_cast<unsigned char*>(buffer.get()),
                  (size_t)len));
              cached_image_data_ = image_data;
              previous_path_ = file_path.get();
              callback.Run(image_data);
              return true;
            }
          }
        }
        break;
      }
    }
  }
  return false;
}
