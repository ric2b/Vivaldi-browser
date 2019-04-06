
// included by the class definition in
// "extensions/browser/guest_view/web_view/web_view_guest.h"

// namespace extensions {

// class WebViewGuest .... {
// other declarations

 public:
  void InitListeners();
  bool OnMouseEvent(const blink::WebMouseEvent& mouse_event);

  void ShowPageInfo(gfx::Point pos);

  void ExtendedLoadProgressChanged(content::WebContents* source,
                                   double progress,
                                   double loaded_bytes,
                                   int loaded_elements,
                                   int total_elements) override;

  bool EmbedsFullscreenWidget() const override;
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
  void BeforeUnloadFired(const base::TimeTicks& proceed_time) override {}

  // Vivaldi customs below
  bool IsVivaldiMail();
  bool IsVivaldiWebPanel();

  void SetVisible(bool is_visible);

  bool IsVisible();

  void SetIsFullscreen(bool is_fullscreen, bool skip_window_state);

  // contentblocking handling
  void OnContentBlocked(ContentSettingsType settings_type,
                        const base::string16& details);
  void AllowRunningInsecureContent();

  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;

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
  bool HasOwnerShipOfContents() override;

  const std::string& sourceSpec() { return src_.spec(); }

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
  bool IsRockerGesturesEnabled() const;

  // Store mouse down actions until up is registered.
  bool has_left_mousebutton_down_;
  bool has_right_mousebutton_down_;
  bool eat_next_right_mouseup_;

  // Stores the current RenderViewHost, to ensure only one mouse event callback
  // per RenderViewHost
  content::RenderViewHost* current_host_;

  int mousedown_x_;
  int mousedown_y_;
  static bool gesture_recording_;

  bool is_visible_;
  bool is_fullscreen_;
  ui::WindowShowState window_state_prior_to_fullscreen_ = ui::SHOW_STATE_NORMAL;

#if defined(USE_AURA)

#define TIME_BEFORE_HIDING_MS 3000

  // This is a object that will hide the cursor for the webviewguests window
  // based on a timer. The cursor will be shown by the CompoundEventFilter on
  // mouse events, but this class will hide the cursor on a beat in fullscreen.
  class CursorHider {
   public:
    static std::unique_ptr<CursorHider> Create(aura::Window* window) {
      return std::unique_ptr<CursorHider>(
          base::WrapUnique(new CursorHider(window)));
    }

    void Hide() { cursor_client_->HideCursor(); }

    CursorHider(aura::Window* window) {
      cursor_client_ = aura::client::GetCursorClient(window);
      hide_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(TIME_BEFORE_HIDING_MS),
                        this, &CursorHider::Hide);
    }

    ~CursorHider() { cursor_client_->ShowCursor(); }

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
  content::RenderWidgetHost::MouseEventCallback mouseevent_callback_;

  int inspecting_tab_id_ = 0;

  scoped_refptr<extensions::DevtoolsConnectorItem> connector_item_;

// };
