---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides > Views
page_name: views-intro
title: Views (Chromium UI Framework)
---

[Views](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/;bpv=0;bpt=0)
is Chromium's UI framework. This is a quick primer to help you get started
hacking together UIs with Views. Views are lightweight and cross-platform, but
they can be difficult to learn and debug because documentation is sparse.

UIs in Chromium are generally implemented using either Views/C++ or
HTML/JS/Polymer. To get an idea of where these are used respectively:

-   The system shell (named Ash for "Aura Shell") uses Views and implements all
    of the basic desktop interfaces for ChromeOS, like the taskbar, the system
    tray, the app menu, the wallpaper, window decorations, and the login
    screens.
-   The Chrome browser uses Views to implement the address bar and menus.
-   System applications (Settings, Files, Calculator, etc.) are web apps with
    system APIs exposed using Mojo as needed. These web apps are hosted in a
    type of View called WebView.
-   The "Out of Box Experience" OOBE, which guides the user through first-time
    setup of a Chromebook, is a web app.
-   Third-party apps are integrated using PWA web apps, ARC++ for Android apps,
    or containerized Linux apps.

## Basics

### `views::View` base class

All Views inherit from `views::View`
([//ui/views/view.h](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h)).
You should take the time to read through that file carefully. It contains lots
of documentation, and the methods expose a lot of functionality that you'll want
sooner or later.

### The View hierarchy and the bounds rectangle

Each View has a bounds rectangle (height, width, position) defined in the
coordinate system of its parent. The bounds define the area where the View is
responsible for painting and handling events. While the bounds can be
manipulated with methods like `SetX()`, `SetY()`, `SetWidth()`, `SetHeight()`,
this is usually not done directly (see [Layout](#layout)).

Each View has a parent and zero or more children, forming a tree. All
interactions with each View propagate up and down the hierarchy. For example,
when you click somewhere in the root View, it checks whether any of its
children's bounds intersect with the click event, and the event passes down the
hierarchy recursively.

### Controls

There are a number of useful *controls* (buttons, labels, textareas, sliders,
images, etc.) in the
[//ui/views/controls](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/controls/)
directory. Unless you need something very specific, most Views you create will
be a composed of these basic Views.

### Example

Let's look at a quick example to get an idea of how to declare a new View and
add some controls:

```cpp
namespace {

// It's common practice to declare all constants in the anonymous namespace. It's
// especially useful to always provide units for numeric quantities. Here, the "-Dp"
// suffix lets the reader know that the units here are "device-independent pixels",
// which are pixels that may be scaled for the output device's resolution.
constexpr int kChildViewSpacingDp = 5;
constexpr int kDefaultTextfieldWidthInChars = 20;

} // namespace

// This is a simple form with a label, text field, and submit button arranged
// horizontally on a line.
class FooView : public views::View {
 public:
  FooView() {
    // It's very common to set up child Views and configuration in the constructor.
    // The code here serves a similar purpose to markup in other UI frameworks.

    // Define a layout. The layout will handle positioning the child Views. See the
    // Layout section for more details. By default, BoxLayout will arrange the child
    // views side-by-side horizontally.
    views::BoxLayout* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
    layout->set_between_child_spacing(kChildViewSpacing);

    // Let's add some child Views:

    // First let's add a label.
    label_ = AddChildView(std::make_unique<views::Label>());
    // Notice that we have to provide a UTF16 string to set the text. Usually we
    // want to use a localized string.
    label_->SetText(l10n_util::GetStringUTF16(IDS_FOO_LABEL));

    // A text field lets the user input some text.
    text_field_ = AddChildView(std::make_unique<views::TextField>());
    text_field_->SetDefaultWidthInChars(kDefaultTextfieldWidthInChars);

    // Add a submit button for the user to submit the text.
    button_ = AddChildView(std::make_unique<views::LabelButton>(
      /*callback=*/base::BindRepeating(&FooView::OnSubmit, base::Unretained(this)),
      /*text=*/l10n_util::GetStringUTF16(IDS_FOO_SUBMIT)
    ));
  }

 private:
  void OnSubmit() {
    std::string text = base::UTF16ToUTF8(text_field_->GetText());
    // Do something with the text.
  }

  // While not necessary, it's often convenient to keep pointers to the child views.
  // This allows us to reference the controls later while e.g. responding to events.
  // Raw pointers are OK here, since the base class owns the views.
  base::raw_ptr<views::Label> label_;
  base::raw_ptr<views::TextField> text_field_;
  base::raw_ptr<views::LabelButton> button_;
};
```

## Layout

*Layout* refers to the process of arranging the children of a View by assigning
their bounds before painting. It's possible to do this manually by setting the
bounds on each child, but more often it's easier to just set a LayoutManager.

### Layout Managers

If you provide a View with a
[LayoutManager](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/layout_manager.h),
the View will use it to automatically layout its children. There are a number of
different types of layout managers in
[`//ui/views/layout`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/):

-   [BoxLayout](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/box_layout.h)
    can be used to arrange child Views in a row or column.
-   [FlexLayout](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/flex_layout.h)
    is similar to CSS
    [Flexbox](https://developer.mozilla.org/en-US/docs/Learn/CSS/CSS_layout/Flexbox)
    and is also used to arrange children in rows/columns. It has more
    complicated layout rules than BoxLayout, and allows the children to wrap to
    multiple rows/columns and expand to fill available space.
-   [FillLayout](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/fill_layout.cc)
    assigns all children to have the same bounds as the parent. This is useful
    for stacking Views on top of each other, or creating a placeholder where you
    expect only one child to be visible at a time.
-   [TableLayout](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/table_layout.h)
    allows you to define rows and columns and create a grid of views.

To assign a layout manager to a View, simply call `SetLayoutManager()`. As a
convenience, that method returns a pointer to the layout manager, which is handy
for setting properties on the layout.

```cpp
class FooView : public views::View {
 public:
  FooView() {
    views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());

    // Configure the layout to make a column.
    layout->SetOrientation(LayoutOrientation::kVertical);
  }
};
```

### Layout Views

Oftentimes, you'll want to nest some child views inside an intermediate view to
help with layout. For example, suppose you want to stack a label on top of a row
of buttons. There are some Views with LayoutManagers already set to cut down on
boilerplate in this situation:

```cpp
class FooView : public views::View {
 public:
  FooView() {
    views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
    layout->SetOrientation(LayoutOrientation::kVertical);

    auto* label = AddChildView(std::make_unique<views::Label>());
    label_->SetText(l10n_util::GetStringUTF16(IDS_FOO_LABEL));

    // We can create an extra container View without defining a new class.
    // Notice that BoxLayoutView exposes the functionality of BoxLayout without
    // having to directly use a layout manager.
    auto* button_row = AddChildView(std::make_unique<views::BoxLayoutView>());
    button_row->SetOrientation(LayoutOrientation::kHorizontal);
    button_row->SetBetweenChildSpacing(5);

    auto* button1 = button_row->AddChildView(std::make_unique<views::Button>());
    auto* button2 = button_row->AddChildView(std::make_unique<views::Button>());
    auto* button3 = button_row->AddChildView(std::make_unique<views::Button>());
  }
};
```

This is a good example of a common pattern with Views: it's usually better to
create complex layouts by combining simple layouts.

### Default Fill Layout

Views also have a built-in FillLayout that you can enable by simply calling
`SetUseDefaultFillLayout(true)`. In the future, this may be turned on by
default, so if you plan to not use a layout manager (see next section), then you
may want to `SetUseDefaultFillLayout(false)`.

### Manual Layout

If you have a particularly difficult layout that requires custom logic, then you
need not use a layout manager at all. Although it should be a last resort, you
can simply override the View's `Layout()` method, and calculate the desired
bounds of each child.

## Animations

Views can be animated in a number of ways. In the simplest case, we might want
to make some property of the View a function of time, e.g. smoothly decrease the
opacity of the View to make it fade out, or briefly increase the scale of the
View to give the user visual feedback on a click event.

### AnimationBuilder

The
[`AnimationBuilder`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/animation/animation_builder.h)
class provides a fluent interface for creating a variety of animations. Example:

```cpp
class BlinkingRedRectangleView : public views::View {
 public:
  FooView() {
    // This is important for opacity animations -- see "layer animations" below.
    SetPaintToLayer();

    // Paint something to this view so that we can see the animation.
    SetBackground(views::Background::CreateSolidBackground(SK_ColorRED));

    // Create an animation with AnimationBuilder. AnimationBuilder will
    // automatically start the animation when it falls out of scope.
    views::AnimationBuilder()
      // Repeatedly() means that this animation will continue looping. You could
      // also use Once() here if you only want the animation to play once, e.g.
      // after a click event.
      .Repeatedly()
      // The next set of calls specify an animation block. Each block has a
      // duration, which is the interval of time over which each property will
      // be animated from its old value to the value provided.
      .SetDuration(base::Milliseconds(500))
      .SetOpacity(weak_ptr_factory_.GetWeakPtr(), 0.0f,
                  gfx::Tween::ACCEL_30_DECEL_20_85)
      // Then() starts a new animation block. Alternatively, At(...) or
      // Offset(...) to specify when the animation block should occur, or
      // set a delay before playing the block.
      .Then()
      .SetDuration(base::Milliseconds(500))
      // Notice the use of a weak pointer here. If the view is destroyed during
      // the animation, this prevents a UAF bug.
      //
      // See "Tween" section below for more info on what Tween means.
      .SetOpacity(weak_ptr_factory_.GetWeakPtr(), 1.0f,
                  gfx::Tween::LINEAR);
  }

 private:
  base::WeakPtrFactory<BlinkingRedRectangleView> weak_ptr_factory_{this};
};
```

Note that `AnimationBuilder` is a convenient shorthand for animations. You might
see some
[older animation code](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:ash/login/ui/auth_icon_view.cc;l=159;drc=b73134cfcce34a13f25202d077c0aa9dc03b662e)
that directly creates a
[LayerAnimationSequence](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:ui/compositor/layer_animation_sequence.h).

### gfx::Tween

The
[`gfx::Tween`](https://source.chromium.org/chromium/chromium/src/+/main:ui/gfx/animation/tween.h)
enum allows you to specify the
[*easing function*](https://www.smashingmagazine.com/2021/04/easing-functions-css-animations-transitions/)
to be used when animating between two values of a property. This lets you give
the appearance of acceleration to your animations, which often looks more
polished than the default `gfx::Tween::LINEAR`.

The UX spec will often specify this using
[cubic-beziers](https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
([example](https://cubic-bezier.com/#.17,.67,.83,.67)). This is similar to
[how it's done in CSS](https://developer.mozilla.org/en-US/docs/Web/CSS/easing-function#cubic_b%C3%A9zier_easing_function).

If you want to find a `gfx::Tween` value that will match a certain cubic-bezier,
then it's easiest to look at the implementation
[here](https://source.chromium.org/chromium/chromium/src/+/main:ui/gfx/animation/tween.cc;l=35;drc=3e1a26c44c024d97dc9a4c09bbc6a2365398ca2c).

### Layer Animations

Typically, a View is rendered by recursively traversing the hierarchy of Views
and painting each child onto the same buffer, similar to how a painter might
paint objects onto a canvas
([Painter's Algorithm](https://en.wikipedia.org/wiki/Painter%27s_algorithm)). It
is possible to instead paint a View and its children to a separate buffer,
called a *layer*. Later in the rendering pipeline, a component called the
*compositor* combines and flattens all the layers into an image to be sent to
the display.

Layers allow us to animate Views without having to repaint for every frame of
the animation. Instead of animating the View itself, we animate the layer the
View was painted onto. This kind of animation can be done very efficiently on
modern graphics hardware.

In order to paint a View onto its own layer, simply call `SetPaintToLayer()` on
the View (e.g. in the constructor). Be sure to remember this step -- if you
attempt to apply a layer animation to a View that doesn't have a layer, then
Chrome will crash.

#### Opacity

Opacity specifies how opaque the layer should be as a number between `0.0f`
(fully transparent) and `1.0f` (fully opaque). Behind the scenes, the compositor
will multiply the
[alpha channel](https://developer.mozilla.org/en-US/docs/Glossary/Alpha) of the
layer by this number before combining it with the other layers.

See the "BlinkingRedRectangleView" example above to see how to use opacity
animations.

#### Transforms

If you've studied linear algebra, then you might know that 3D linear
transformations such as scaling, rotation, reflection, and skew can be
represented using 3x3 matrices. It turns out that by instead using 4x4 matrices,
it's possible to also represent translation (see
[Affine Transformation](https://en.wikipedia.org/wiki/Transformation_matrix#:~:text=of%20transformations%20subsection-,Affine%20transformations,-Perspective%20projection)).

If this sounds unfamiliar, don't worry -- you don't need to know the math to be
able to use these transformations. All you need to know is that if you want to
add a sliding, spinning, or stretching animation to a View, you would achieve
that by animating the layer's
[*transform*](https://source.chromium.org/chromium/chromium/src/+/main:ui/gfx/geometry/transform.h).

##### Example: Translation

```cpp
// Move |this| view 5dp to the right of center, then 5dp to the left of
// center, then return to center.
views::AnimationBuilder()
  .Once()
  .SetDuration(base::Milliseconds(200))
  .SetTransform(this, gfx::Transform::MakeTranslation(/*tx=*/5, /*ty=*/0),
                gfx::Tween::SMOOTH)
  .Then()
  .SetDuration(base::Milliseconds(300))
  .SetTransform(this, gfx::Transform::MakeTranslation(/*tx=*/-5, /*ty=*/0),
                gfx::Tween::SMOOTH)
  .Then()
  .SetDuration(base::Milliseconds(200))
  .SetTransform(this, gfx::Transform::MakeTranslation(/*tx=*/0, /*ty=*/0),
                gfx::Tween::SMOOTH);
```

##### Rotation and scaling about a point

`gfx::Transform`'s `Rotate` and `Scale` methods always perform rotation and
scaling relative to the origin of coordinates, i.e. the top-left corner of the
parent view. If instead you want to rotate around a particular point, then this
can be done by (1) translating the view so that the point is at the origin, (2)
performing the rotation, and (3) translating the view back to its original
location.

Since `gfx::Transform` can represent any combination of transforms, we can
calculate this entire operation as one transform.

```cpp
// Rotate 90 degrees clockwise about (100, 100).
gfx::Vector2df point(100, 100);
gfx::Transform rotation;
rotation.Translate(-point);
rotation.Rotate(-90);
rotation.Translate(point);

// Apply the rotation, then return to the original position.
views::AnimationBuilder()
  .Once()
  .SetDuration(base::Milliseconds(300))
  .SetTransform(this, rotation, gfx::Tween::SMOOTH)
  .Then()
  .SetDuration(base::Milliseconds(300))
  .SetTransform(this, gfx::Transform(), gfx::Tween::SMOOTH);
```

#### Other types of Layer Animations

There are more types of layer animations that are less frequently used. See the
methods
[here](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:ui/views/animation/animation_sequence_block.h;l=58;drc=b73134cfcce34a13f25202d077c0aa9dc03b662e).

### Lottie

For complex animations, especially animated illustrations, it is more convenient
to have the UX designer produce an asset for us to load at runtime that will
produce the desired animation without the need for us to interpret an obscenely
complex spec. A format for vector animations called
[Lottie](https://en.wikipedia.org/wiki/Lottie_\(file_format\)) has recently
become very popular.

Lottie files are JSON files exported from Adobe After Effects. Lottie was
originally developed for showing animations on the web, but there is now an
implementation of Lottie in Skia (Chrome's graphics library) called Skottie.

[`views::AnimatedImageView`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/controls/animated_image_view.h;l=59;drc=b7cf155470c42c9b1b481fcaed6912b73e09ed7c)
can be used to load a Lottie file into a View for playback.

You can upload Lottie files to <https://skottie-internal.skia.org> for easy
viewing/sharing with coworkers.

### Custom Painting

Occasionally, you might need to animate something that is too complicated for
layer animations, but easy to paint. In this case, you can use a timer to
repaint the view repeatedly to simulate a sequence of animation frames.

[`views::Throbber`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:ui/views/controls/throbber.cc)
is a good example of this technique.

