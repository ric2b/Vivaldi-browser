
// included by the class definition in "extensions/browser/guest_view/web_view/web_view_guest.h"


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
  void NavigationStateChanged(content::WebContents *source,
                              content::InvalidateTypes changed_flags) override;
  void UpdateTargetURL(content::WebContents *source, const GURL &url) override;
  void CreateSearch(const base::ListValue & search) override;
  void PasteAndGo(const base::ListValue & search) override;
  void ToggleFullscreenModeForTab(content::WebContents *web_contents,
                                  bool enter_fullscreen);
  void ShowValidationMessage(content::WebContents *web_contents,
                             const gfx::Rect &anchor_in_root_view,
                             const base::string16 &main_text,
                             const base::string16 &sub_text) override;
  void HideValidationMessage(content::WebContents *web_contents) override;
  void MoveValidationMessage(content::WebContents *web_contents,
                             const gfx::Rect &anchor_in_root_view) override;



  // Vivaldi customs below
  bool IsVivaldiWebPanel();

  void SetVisible(bool is_visible);

  bool IsVisible();

#ifdef VIVALDI_BUILD
  void UpdateMediaState(TabAlertState state);
#endif // VIVALDI_BUILD

  void SetIsFullscreen(bool is_fullscreen);
  extensions::AppWindow* GetAppWindow();
  // gisli@vivaldi.com.  Request that page info is displayed, sends event
  // to from the <webview> tag (requestpageinfo).
  bool RequestPageInfo(const GURL& url);

  // Sets the extension id used when creating the |ExtensionHost|
  void SetExtensionHost(const std::string& name);

  // Creates the |ExtensionHost|
  void CreateExtensionHost(const std::string& extension_id);

 private:
  void VisibleSSLStateChanged(const content::WebContents* source) override;

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

  void SetWebContentsWasInitiallyGuest(bool born_guest);

  content::WebContents::CreateParams GetWebContentsCreateParams(
    content::BrowserContext* context, const GURL site);

  //vivaldi addition:
  bool GetMousegesturesEnabled();
  bool IsRockerGesturesEnabled() const;

  // Store mouse down actions until up is registered.
  bool has_left_mousebutton_down_;
  bool has_right_mousebutton_down_;
  bool eat_next_right_mouseup_;

  // Stores the current RenderViewHost, to ensure only one mouse event callback
  // per RenderViewHost
  content::RenderViewHost* current_host_;

  enum GestureDirection {
    GestureDirectionNONE = -1,
    GestureDirectionUp,
    GestureDirectionRight,
    GestureDirectionDown,
    GestureDirectionLeft
  };
  enum GestureState {
    GestureStateNone,
    GestureStateRecording,
    GestureStateBlocked
  };

  GestureState gesture_state_;
  int x_;
  int y_;
  // Current gesture direction
  GestureDirection gesture_direction_;
  // Candidate position for mouse gesture direction change.
  int gesture_direction_candidate_x_;
  int gesture_direction_candidate_y_;
  // Accumulated gesture state.
  int gesture_data_;

#ifdef VIVALDI_BUILD
  TabAlertState media_state_;
#endif //VIVALDI_BUILD
  bool is_visible_;
  bool is_fullscreen_;
  int window_state_prior_to_fullscreen_;

  bool webcontents_was_created_as_guest_;

  base::WeakPtr<ValidationMessageBubble> validation_message_bubble_;

  std::unique_ptr<ExtensionHostForWebContents> extension_host_;

  // Note(andre@vivaldi.com):
  // This is used for pages opened from extensions. The delegate is not set
  // and therefore navigation is not possible. We use this to navigate after
  // attaching is done.
  std::unique_ptr<std::string> delayed_open_url_;

private:

  content::RenderWidgetHost::MouseEventCallback mouseevent_callback_;

// };
