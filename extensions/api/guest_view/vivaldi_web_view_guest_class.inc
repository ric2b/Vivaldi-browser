
// included by the class definition in
// "extensions/browser/guest_view/web_view/web_view_guest.h"

// namespace extensions {

// class WebViewGuest .... {
// other declarations
#if BUILDFLAG(ENABLE_EXTENSIONS)

 public:
  void ShowPageInfo(gfx::Point pos);

  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  void CreateSearch(const base::Value::List& search) override;
  void PasteAndGo(const base::Value::List& search) override;
  void ToggleFullscreenModeForTab(content::WebContents* web_contents,
                                  bool enter_fullscreen);
  void BeforeUnloadFired(content::WebContents* web_contents, bool proceed,
                         bool* proceed_to_fire_unload) override;
  // From WebContentsObserver, avoid compile error on the above override.
  void BeforeUnloadFired(bool proceed) override {}
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;

  // Vivaldi customs below
  bool IsVivaldiMail();
  bool IsVivaldiWebPanel();
  bool IsVivaldiWebPageWidget();

  void SetIsFullscreen(bool is_fullscreen);

  // contentblocking handling
  void OnContentAllowed(ContentSettingsType settings_type);
  void OnContentBlocked(ContentSettingsType settings_type);

  void AllowRunningInsecureContent();

  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;

  void OnWindowBlocked(const GURL& window_target_url,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features);

  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;

  // Note(andre@vivaldi.com):
  // This is used for pages opened from extensions. The delegate is not set
  // and therefore navigation is not possible. We use this to navigate after
  // attaching is done.
  std::unique_ptr<std::string> delayed_open_url_;

  // Called when the mouse cursor enters/leaves the appwindow the guest is
  // inside.
  void OnMouseEnter();
  void OnMouseLeave();

  // WebContentsDelegate
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;
  void CapturePaintPreviewOfSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      const base::UnguessableToken& guid,
      content::RenderFrameHost* render_frame_host) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;

  // Vivaldi WebContentsObserver implementation.
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Used to expose the view to extensions, only popups now.
  std::unique_ptr<::vivaldi::VivaldiExtensionHost> extension_host_;

  bool IsNavigatingAwayFromVivaldiUI() {
    return is_navigating_away_from_vivaldi_ui_;
  }

 protected:

  WebContentsDelegate* GetDevToolsConnector() override;

 private:
  // Called from WebViewGuest::CanDownload and blocks a download following the
  // same criteria as InsecureDownloadData. True on block, false on download.
  void VivaldiCanDownload(const GURL& url,
                          const std::string& request_method,
                          base::OnceCallback<void(bool)> callback);

  void VisibleSecurityStateChanged(content::WebContents* source) override;

  // Parses the user input string for new window creation.
  void ParseNewWindowUserInput(const std::string& user_input,
                               int& window_id,
                               bool& foreground,
                               bool& incognito);

  // Add a WebContents object to the browsers tab-strip. The tabstrip will take
  // over responsibilty of the webcontents after the guest is destroyed.
  void AddGuestToTabStripModel(WebViewGuest* guest,
                               int windowId,
                               bool activePage = true,
                               bool inherit_opener = true);

  // Starts loading in the current WebContents if this is needed.
  void LoadTabContentsIfNecessary();

  void VivaldiCreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                                const base::Value::Dict& create_params,
                                GuestPageCreatedCallback callback);

  // vivaldi addition:
  bool IsMouseGesturesEnabled() const;

  void VivaldiSetLoadProgressEventExtraArgs(base::Value::Dict& dictionary);

  bool is_fullscreen_ = false;
  //ui::mojom::WindowShowState window_state_prior_to_fullscreen_ = ui::mojom::WindowShowState::kNormal;

  // Used to reset webkitprefs that is overridden in
  // extension_webkit_preferences::SetPreferences when navigating away from
  // Vivaldi ui.
  void SetIsNavigatingAwayFromVivaldiUI(bool away);
  bool is_navigating_away_from_vivaldi_ui_ = false;

  // NOTE(david@vivaldi.com): |connector_item_| is a proxy delegate which is
  // required by Vivaldi. connector_item_ is owned by the DevtoolsConnectorAPI.
  raw_ptr<extensions::DevtoolsConnectorItem> connector_item_ = nullptr;

#if defined(USE_AURA)

#define TIME_BEFORE_HIDING_MS 3000

  // This is a object that will hide the cursor for the webviewguests window
  // based on a timer. The cursor will be shown by the CompoundEventFilter on
  // mouse events, but this class will hide the cursor on a beat in fullscreen.
  class CursorHider {
   public:
    static std::unique_ptr<CursorHider> Create(aura::Window* window);

    void Hide();

    CursorHider(aura::Window* window);
    ~CursorHider();
    CursorHider(const CursorHider&) = delete;
    CursorHider& operator=(const CursorHider&) = delete;

    // Reset and stop the hide timer.
    void Reset() { hide_timer_.Reset(); }
    void Stop() { hide_timer_.Stop(); }

   private:
    const raw_ptr<aura::client::CursorClient> cursor_client_;

    // A timer to delay hiding the mouse.
    base::RepeatingTimer hide_timer_;
  };

  // The object to hide cursor when going into fullscreen mode.
  std::unique_ptr<CursorHider> cursor_hider_;
#endif  // USE_AURA

 private:
  int inspecting_tab_id_ = 0;
  // Used for SetContentsBounds while being unattached.
  std::unique_ptr<gfx::Rect> last_set_bounds_;

#endif  // Extensions

// };
