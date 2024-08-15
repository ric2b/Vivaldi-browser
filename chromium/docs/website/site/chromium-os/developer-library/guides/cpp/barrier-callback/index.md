---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: barrier-callback
title: How to wait for a callback be invoked N times
---

## Applying the barrier_callback

**Problem**

We want to run a callback function multiple times, the callback will get
consumed when put into `std::move()` function.

For example, we have several `root_window` stored in `all_window` array, and we
are using an async function `GrabWindowSnapshotAsPNG()` on each
`root_window`. So we will have to call a callback function when each
`GrabWindowSnapshotAsPNG()` is finished:

```
for (aura::Window* root_window : all_windows) {
  gfx::Rect rect = root_window->bounds();
  ui::GrabWindowSnapshotAsPNG(
      root_window, rect,
      base::BindOnce(some_function, callback));
}
```

**Solution**

Use `barrier_callback` when we need to call the callback more than once:

```
auto barrier_callback =
    base::BarrierCallback<T>(
        num_callbacks,
        done_callback);
```

The `num_callbacks` defines how many times the barrier call back can be
called. When the last time it's called, `done_callback` is executed immediately.

So in our case, we need to update the code:

1.  Create the barrier_callback:

    ```
    auto barrier_callback =
        base::BarrierCallback<scoped_refptr<base::RefCountedMemory>>(
            all_windows.size(),
            base::BindOnce(&OsFeedbackScreenshotManager::OnAllScreenshotsTaken,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    ```

    Here we want to handle the final results from `GrabWindowSnapshotAsPNG()`
    all at once using the `OnAllScreenshotsTaken()` function.

2.  Gather each result from `GrabWindowSnapshotAsPNG()`, move it into the
    callback storage:

    ```
    for (aura::Window* root_window : all_windows) {
      gfx::Rect rect = root_window->bounds();
      ui::GrabWindowSnapshotAsPNG(
          root_window, rect,
          base::BindOnce(OnOneScreenshotTaken, barrier_callback));
    }
    ```

    where `OnOneScreenshotTaken` moves the data into the callback:

    ```
    void OnOneScreenshotTaken(
        base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>
            barrier_callback,
        scoped_refptr<base::RefCountedMemory> png_data) {
      std::move(barrier_callback).Run(png_data);
    }
    ```

3.  In the final OnAllScreenshotsTaken()`, go through the collected results and
    handle them all together:

    ```
    void OsFeedbackScreenshotManager::OnAllScreenshotsTaken(
        ScreenshotCallback callback,
        std::vector<scoped_refptr<base::RefCountedMemory>> all_data) {
      std::vector<scoped_refptr<base::RefCountedMemory>> screenshot_data_set;
      for (scoped_refptr<base::RefCountedMemory> data : all_data) {
        if (data && data.get()) {
          screenshot_data_set.push_back(data);
        }
      }
      if (screenshot_data_set.size() > 0) {
        // Some followup treatments.
        screenshot_png_data_ = GetCombinedBitmap(screenshot_data_set);
        // Eventually run callback.
        std::move(callback).Run(true);
      } else {
        LOG(ERROR) << "failed to take screenshot.";
        std::move(callback).Run(false);
      }
    }
    ```

**Example CL**

*   https://crrev.com/c/4009384
