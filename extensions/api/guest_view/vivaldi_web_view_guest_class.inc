
// included by the class definition in
// "extensions/browser/guest_view/web_view/web_view_guest.h"

// namespace extensions {

// class WebViewGuest .... {
// other declarations

 public:
  void ShowPageInfo(gfx::Point pos);

  void ExtendedLoadProgressChanged(double progress,
                                   double loaded_bytes,
                                   int loaded_elements,
                                   int total_elements) override;

  bool EmbedsFullscreenWidget() override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  void CreateSearch(const base::ListValue& search) override;
  void PasteAndGo(const base::ListValue& search) override;
  void SimpleAction(const base::ListValue& search);
  void ToggleFullscreenModeForTab(content::WebContents* web_contents,
                                  bool enter_fullscreen,
                                  bool skip_window_state = false);
  void BeforeUnloadFired(content::WebContents* web_contents, bool proceed,
                         bool* proceed_to_fire_unload) override;
  // From WebContentsObserver, avoid compile error on the above override.
  void BeforeUnloadFired(bool proceed, const base::TimeTicks& proceed_time) override {}

  // Vivaldi customs below
  bool IsVivaldiMail();
  bool IsVivaldiWebPanel();

  void SetIsFullscreen(bool is_fullscreen, bool skip_window_state);

  // contentblocking handling
  void OnContentAllowed(ContentSettingsType settings_type);
  void OnContentBlocked(ContentSettingsType settings_type);

  void AllowRunningInsecureContent();

  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;

  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

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
      content::WebContents* web_contents,
      const viz::SurfaceId&,
      const gfx::Size& natural_size) override;
  void ExitPictureInPicture() override;

  const std::string& sourceSpec() { return src_.spec(); }

  blink::SecurityStyle GetSecurityStyle(
      content::WebContents* web_contents,
      content::SecurityStyleExplanations* security_style_explanations) override;

  // Vivaldi WebContentsObserver implementation.
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Used to expose the view to extensions, only popups now.
  std::unique_ptr<::vivaldi::VivaldiExtensionHost> extension_host_;

 protected:

  WebContentsDelegate* GetDevToolsConnector() override;

 private:
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
                               bool activePage = true);

  // Starts loading in the current WebContents if this is needed.
  void LoadTabContentsIfNecessary();

  content::WebContents::CreateParams GetWebContentsCreateParams(
      content::BrowserContext* context,
      const GURL site);

  // vivaldi addition:
  bool IsMouseGesturesEnabled() const;

  bool is_fullscreen_;
  ui::WindowShowState window_state_prior_to_fullscreen_ = ui::SHOW_STATE_NORMAL;

  // NOTE(david@vivaldi.com): |connector_item_| is a proxy delegate which is
  // required by Vivaldi.
  scoped_refptr<extensions::DevtoolsConnectorItem> connector_item_;

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

    // Reset and stop the hide timer.
    void Reset() { hide_timer_.Reset(); }
    void Stop() { hide_timer_.Stop(); }

   private:
    aura::client::CursorClient* cursor_client_;

    // A timer to delay hiding the mouse.
    base::RepeatingTimer hide_timer_;

    DISALLOW_COPY_AND_ASSIGN(CursorHider);
  };

  // The object to hide cursor when going into fullscreen mode.
  std::unique_ptr<CursorHider> cursor_hider_;
#endif  // USE_AURA

 private:
  int inspecting_tab_id_ = 0;

// };
