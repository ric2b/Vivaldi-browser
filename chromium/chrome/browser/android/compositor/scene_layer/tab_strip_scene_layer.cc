// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/tab_strip_scene_layer.h"

#include "base/android/jni_android.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/android/compositor/layer/group_indicator_layer.h"
#include "chrome/browser/android/compositor/layer/tab_handle_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabStripSceneLayer_jni.h"

// Vivaldi
#include "app/vivaldi_apptools.h"
#include "ui/android/color_utils_android.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

TabStripSceneLayer::TabStripSceneLayer(JNIEnv* env,
                                       const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      background_layer_(cc::slim::SolidColorLayer::Create()),
      tab_strip_layer_(cc::slim::SolidColorLayer::Create()),
      group_ui_parent_layer_(cc::slim::Layer::Create()),
      tab_ui_parent_layer_(cc::slim::Layer::Create()),
      foreground_layer_(cc::slim::Layer::Create()),
      foreground_tabs_(cc::slim::Layer::Create()),
      foreground_group_titles_(cc::slim::Layer::Create()),
      new_tab_button_(cc::slim::UIResourceLayer::Create()),
      new_tab_button_background_(cc::slim::UIResourceLayer::Create()),
      left_fade_(cc::slim::UIResourceLayer::Create()),
      right_fade_(cc::slim::UIResourceLayer::Create()),
      left_padding_layer_(cc::slim::SolidColorLayer::Create()),
      right_padding_layer_(cc::slim::SolidColorLayer::Create()),
      model_selector_button_(cc::slim::UIResourceLayer::Create()),
      model_selector_button_background_(cc::slim::UIResourceLayer::Create()),
      scrim_layer_(cc::slim::SolidColorLayer::Create()),
      content_tree_(nullptr) {
  new_tab_button_->SetIsDrawable(true);
  new_tab_button_background_->SetIsDrawable(true);
  model_selector_button_->SetIsDrawable(true);
  model_selector_button_background_->SetIsDrawable(true);

  left_fade_->SetIsDrawable(true);
  right_fade_->SetIsDrawable(true);
  scrim_layer_->SetIsDrawable(true);
  left_padding_layer_->SetIsDrawable(true);
  right_padding_layer_->SetIsDrawable(true);

  // When the ScrollingStripStacker is used, the new tab button and tabs scroll,
  // while the incognito button and left/right fade stay fixed. Put the new tab
  // button and tabs in a separate layer placed visually below the others. Put
  // tab group indicators in a separate layer placed visually below the tabs.
  group_ui_parent_layer_->SetIsDrawable(true);
  tab_ui_parent_layer_->SetIsDrawable(true);
  foreground_layer_->SetIsDrawable(true);
  background_layer_->SetIsDrawable(true);
  foreground_tabs_->SetIsDrawable(true);
  foreground_group_titles_->SetIsDrawable(true);
  tab_strip_layer_->SetIsDrawable(true);

  background_layer_->AddChild(tab_strip_layer_);
  background_layer_->AddChild(scrim_layer_);

  tab_strip_layer_->AddChild(group_ui_parent_layer_);
  tab_strip_layer_->AddChild(tab_ui_parent_layer_);
  tab_strip_layer_->AddChild(foreground_layer_);
  foreground_layer_->AddChild(foreground_group_titles_);
  foreground_layer_->AddChild(foreground_tabs_);

  tab_strip_layer_->AddChild(left_fade_);
  tab_strip_layer_->AddChild(right_fade_);
  tab_strip_layer_->AddChild(left_padding_layer_);
  tab_strip_layer_->AddChild(right_padding_layer_);
  tab_strip_layer_->AddChild(model_selector_button_background_);
  tab_strip_layer_->AddChild(new_tab_button_background_);
  tab_strip_layer_->AddChild(model_selector_button_);
  tab_strip_layer_->AddChild(new_tab_button_);

  // Note(david@vivaldi.com): The correct layer child assignment will be done
  // in |SetContentTree()| as we are dealing with a pair of
  // |TabStripSceneLayer|s.
  if (!vivaldi::IsVivaldiRunning())
  layer()->AddChild(background_layer_);

  // Vivaldi
  use_light_foreground_on_background = false;
  loading_text_ = cc::slim::UIResourceLayer::Create();
  loading_text_->SetIsDrawable(true);
  tab_strip_layer_->AddChild(loading_text_);
}

TabStripSceneLayer::~TabStripSceneLayer() = default;

void TabStripSceneLayer::SetConstants(JNIEnv* env,
                                      jint reorder_background_top_margin,
                                      jint reorder_background_bottom_margin,
                                      jint reorder_background_padding_start,
                                      jint reorder_background_padding_end,
                                      jint reorder_background_corner_radius) {
  GroupIndicatorLayer::SetConstants(
      reorder_background_top_margin, reorder_background_bottom_margin,
      reorder_background_padding_start, reorder_background_padding_end,
      reorder_background_corner_radius);
}

void TabStripSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (content_tree_ &&
      (!content_tree_->layer()->parent() ||
       content_tree_->layer()->parent()->id() != layer()->id()))
    content_tree_ = nullptr;

  if (content_tree != content_tree_) {
    if (content_tree_)
      content_tree_->layer()->RemoveFromParent();
    content_tree_ = content_tree;
    if (content_tree) {
      layer()->InsertChild(content_tree->layer(), 0);
      content_tree->layer()->SetPosition(
          gfx::PointF(0, -layer()->position().y()));
      // Note(david@vivaldi.com): Add the stacking strip to the main strip scene
      // layer in order to achieve the correct scrolling behaviour. The main
      // strip will be normally added to the current layer.
      if (is_stack_strip_)
        content_tree->layer()->AddChild(background_layer_);
      else
        layer()->AddChild(tab_strip_layer_);
    }
  }
}

void TabStripSceneLayer::BeginBuildingFrame(JNIEnv* env,
                                            const JavaParamRef<jobject>& jobj,
                                            jboolean visible) {
  write_index_ = 0;
  group_write_index_ = 0;
  background_layer_->SetHideLayerAndSubtree(!visible);
}

void TabStripSceneLayer::FinishBuildingFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  if (background_layer_->hide_layer_and_subtree()) {
    return;
  }

  for (unsigned i = write_index_; i < tab_handle_layers_.size(); ++i) {
    tab_handle_layers_[i]->layer()->RemoveFromParent();
  }
  for (unsigned i = group_write_index_; i < group_title_layers_.size(); ++i) {
    group_title_layers_[i]->layer()->RemoveFromParent();
  }

  tab_handle_layers_.erase(tab_handle_layers_.begin() + write_index_,
                           tab_handle_layers_.end());
  group_title_layers_.erase(group_title_layers_.begin() + group_write_index_,
                            group_title_layers_.end());
}

void TabStripSceneLayer::UpdateOffsetTag(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& joffset_tag) {
  viz::OffsetTag tag = cc::android::FromJavaOffsetTag(env, joffset_tag);
  layer()->SetOffsetTag(tag);
}

void TabStripSceneLayer::UpdateTabStripLayer(JNIEnv* env,
                                             const JavaParamRef<jobject>& jobj,
                                             jint width,
                                             jint height,
                                             jfloat y_offset,
                                             jint background_color,
                                             jint scrim_color,
                                             jfloat scrim_opacity,
                                             jfloat left_padding,
                                             jfloat right_padding,
                                             jfloat top_padding) {
  gfx::RectF content(0, y_offset, width, height);
  // Note(david@vivaldi.com): We apply a fixed height for the stack strip. The
  // |y_offset| however is only applied to the main strip of which the stacking
  // strip is a child of.
  if (is_stack_strip_)
    background_layer_->SetPosition(gfx::PointF(0, y_offset));
  else
  layer()->SetPosition(gfx::PointF(0, y_offset));
  background_layer_->SetBounds(gfx::Size(width, height));
  if (!vivaldi::IsVivaldiRunning()) {
  background_layer_->SetBackgroundColor(SkColor4f::FromColor(background_color));
  } // End Vivaldi

  float scrollable_strip_height = height - top_padding;
  tab_strip_layer_->SetBounds(gfx::Size(width, scrollable_strip_height));
  tab_strip_layer_->SetPosition(gfx::PointF(0, top_padding));

  // Content tree should not be affected by tab strip scene layer visibility.
  if (content_tree_ && !is_stack_strip_) // Vivaldi
    content_tree_->layer()->SetPosition(gfx::PointF(0, -y_offset));

  // Update left and right padding layers as required.
  if (left_padding == 0) {
    left_padding_layer_->SetHideLayerAndSubtree(true);
  } else {
    left_padding_layer_->SetHideLayerAndSubtree(false);
    left_padding_layer_->SetBounds(gfx::Size(left_padding, height));
    left_padding_layer_->SetBackgroundColor(
        SkColor4f::FromColor(background_color));
  }

  if (right_padding == 0) {
    right_padding_layer_->SetHideLayerAndSubtree(true);
  } else {
    right_padding_layer_->SetHideLayerAndSubtree(false);
    right_padding_layer_->SetBounds(gfx::Size(right_padding, height));
    right_padding_layer_->SetPosition(gfx::PointF(width - right_padding, 0));
    right_padding_layer_->SetBackgroundColor(
        SkColor4f::FromColor(background_color));
  }

  // Hide scrim layer if it's not visible.
  if (scrim_opacity == 0.f) {
    scrim_layer_->SetHideLayerAndSubtree(true);
    return;
  }

  // Set opacity and color
  scrim_layer_->SetOpacity(scrim_opacity);
  scrim_layer_->SetBounds(background_layer_->bounds());
  scrim_layer_->SetBackgroundColor(SkColor4f::FromColor(scrim_color));

  // Ensure layer is visible.
  scrim_layer_->SetHideLayerAndSubtree(false);
}

void TabStripSceneLayer::UpdateNewTabButton(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint resource_id,
    jint bg_resource_id,
    jfloat x,
    jfloat y,
    jfloat touch_target_offset,
    jboolean visible,
    jboolean should_apply_hover_highlight,
    jint tint,
    jint background_tint,
    jfloat button_alpha,
    const JavaParamRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* button_resource =
      resource_manager->GetStaticResourceWithTint(resource_id, tint);

  // Vivaldi
  if (!use_light_foreground_on_background) {
    button_resource =
        resource_manager->GetStaticResourceWithTint(resource_id, SK_ColorBLACK);
  }

  ui::Resource* background_resource =
      resource_manager->GetStaticResourceWithTint(bg_resource_id,
                                                  background_tint, true);

  x += touch_target_offset;

  UpdateCompositorButton(new_tab_button_, new_tab_button_background_,
                         button_resource, background_resource, x, y, visible,
                         should_apply_hover_highlight, button_alpha);
}

void TabStripSceneLayer::UpdateModelSelectorButton(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint resource_id,
    jint bg_resource_id,
    jfloat x,
    jfloat y,
    jboolean visible,
    jboolean should_apply_hover_highlight,
    jint tint,
    jint background_tint,
    jfloat button_alpha,
    const JavaParamRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* button_resource =
      resource_manager->GetStaticResourceWithTint(resource_id, tint);
  ui::Resource* background_resource =
      resource_manager->GetStaticResourceWithTint(bg_resource_id,
                                                  background_tint, true);

  // Vivaldi
  if (!use_light_foreground_on_background) {
    button_resource =
        resource_manager->GetStaticResourceWithTint(resource_id, SK_ColorBLACK);
  }

  UpdateCompositorButton(model_selector_button_,
                         model_selector_button_background_, button_resource,
                         background_resource, x, y, visible,
                         should_apply_hover_highlight, button_alpha);
}

void TabStripSceneLayer::UpdateCompositorButton(
    scoped_refptr<cc::slim::UIResourceLayer> button,
    scoped_refptr<cc::slim::UIResourceLayer> background,
    ui::Resource* button_resource,
    ui::Resource* background_resource,
    float x,
    float y,
    bool visible,
    bool should_apply_hover_highlight,
    float button_alpha) {
  button->SetUIResourceId(button_resource->ui_resource()->id());
  button->SetBounds(button_resource->size());
  button->SetHideLayerAndSubtree(!visible);
  button->SetOpacity(button_alpha);

  gfx::Size background_size = background_resource->size();
  gfx::Size button_size = button_resource->size();
  float x_offset = (background_size.width() - button_size.width()) / 2;
  float y_offset = (background_size.height() - button_size.height()) / 2;
  button->SetPosition(gfx::PointF(x + x_offset, y + y_offset));

  if (!should_apply_hover_highlight) {
    background->SetHideLayerAndSubtree(true);
  } else {
    background->SetUIResourceId(background_resource->ui_resource()->id());
    background->SetPosition(gfx::PointF(x, y));
    background->SetBounds(background_resource->size());
    background->SetHideLayerAndSubtree(!visible);
    background->SetOpacity(button_alpha);
  }
}

void TabStripSceneLayer::UpdateTabStripLeftFade(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint resource_id,
    jfloat opacity,
    const JavaParamRef<jobject>& jresource_manager,
    jint left_fade_color,
    jfloat left_padding) {
  // Hide layer if it's not visible.
  if (opacity == 0.f) {
    left_fade_->SetHideLayerAndSubtree(true);
    return;
  }

  // Set UI resource.
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* fade_resource = resource_manager->GetStaticResourceWithTint(
        resource_id, left_fade_color);
  // Note (david@vivaldi.com): In Vivaldi we tint the fade resource.
  if (vivaldi::IsVivaldiRunning()) {
    fade_resource = resource_manager->GetStaticResourceWithTint(
        resource_id, tab_strip_layer_->background_color().toSkColor());
  }
  left_fade_->SetUIResourceId(fade_resource->ui_resource()->id());

  // The same resource is used for both left and right fade, so the
  // resource must be mirrored for the left fade.
  gfx::Transform fade_transform = gfx::Transform::MakeScale(-1.0f, 1.0f);
  left_fade_->SetTransform(fade_transform);

  // Set opacity.
  left_fade_->SetOpacity(opacity);

  // Set bounds. Use the parent layer height so the 1px fade resource is
  // stretched vertically.
  float height = tab_strip_layer_->bounds().height();
  left_fade_->SetBounds(gfx::Size(fade_resource->size().width(), height));

  // Set position. The rotation set above requires the layer to be offset
  // by its width in order to display on the left edge.
  left_fade_->SetPosition(
      gfx::PointF(fade_resource->size().width() + left_padding, 0));

  // Ensure layer is visible.
  left_fade_->SetHideLayerAndSubtree(false);
}

void TabStripSceneLayer::UpdateTabStripRightFade(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint resource_id,
    jfloat opacity,
    const JavaParamRef<jobject>& jresource_manager,
    jint right_fade_color,
    jfloat right_padding) {
  // Hide layer if it's not visible.
  if (opacity == 0.f) {
    right_fade_->SetHideLayerAndSubtree(true);
    return;
  }

  // Set UI resource.
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* fade_resource = resource_manager->GetStaticResourceWithTint(
        resource_id, right_fade_color);
  // Note (david@vivaldi.com): In Vivaldi we tint the fade resource.
  if (vivaldi::IsVivaldiRunning()) {
    fade_resource = resource_manager->GetStaticResourceWithTint(
        resource_id, tab_strip_layer_->background_color().toSkColor());
  }
  right_fade_->SetUIResourceId(fade_resource->ui_resource()->id());

  // Set opacity.
  right_fade_->SetOpacity(opacity);

  // Set bounds. Use the parent layer height so the 1px fade resource is
  // stretched vertically.
  float height = tab_strip_layer_->bounds().height();
  right_fade_->SetBounds(gfx::Size(fade_resource->size().width(), height));

  // Set position. The right fade is positioned at the end of the tab strip.
  float x = tab_strip_layer_->bounds().width() - fade_resource->size().width() -
            right_padding;
  right_fade_->SetPosition(gfx::PointF(x, 0));

  // Ensure layer is visible.
  right_fade_->SetHideLayerAndSubtree(false);
}

void TabStripSceneLayer::PutStripTabLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint id,
    jint close_resource_id,
    jint close_hover_bg_resource_id,
    jint divider_resource_id,
    jint handle_resource_id,
    jint handle_outline_resource_id,
    jint close_tint,
    jint close_hover_bg_tint,
    jint divider_tint,
    jint handle_tint,
    jint handle_outline_tint,
    jboolean foreground,
    jboolean shouldShowTabOutline,
    jboolean close_pressed,
    jfloat toolbar_width,
    jfloat x,
    jfloat y,
    jfloat width,
    jfloat height,
    jfloat content_offset_y,
    jfloat divider_offset_x,
    jfloat bottom_margin,
    jfloat top_margin,
    jfloat close_button_padding,
    jfloat close_button_alpha,
    jboolean is_start_divider_visible,
    jboolean is_end_divider_visible,
    jboolean is_loading,
    jfloat spinner_rotation,
    jfloat opacity,
    const JavaParamRef<jobject>& jlayer_title_cache,
    const JavaParamRef<jobject>& jresource_manager,
    jfloat tab_alpha, // Vivaldi
    jboolean is_shown_as_favicon, // Vivaldi
    jfloat title_offset) { // Vivaldi
  LayerTitleCache* layer_title_cache =
      LayerTitleCache::FromJavaObject(jlayer_title_cache);
  scoped_refptr<TabHandleLayer> layer = GetNextTabLayer(layer_title_cache);

  if (foreground != layer->foreground()) {
    if (foreground) {
      foreground_tabs_->AddChild(layer->layer());
    } else {
      tab_ui_parent_layer_->AddChild(layer->layer());
    }
  }

  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::NinePatchResource* tab_handle_resource =
      ui::NinePatchResource::From(resource_manager->GetStaticResourceWithTint(
          handle_resource_id, handle_tint, true));
  ui::NinePatchResource* tab_handle_outline_resource =
      ui::NinePatchResource::From(resource_manager->GetStaticResourceWithTint(
          handle_outline_resource_id, handle_outline_tint));
  ui::Resource* close_button_resource =
      resource_manager->GetStaticResourceWithTint(close_resource_id,
                                                  close_tint);
  ui::Resource* close_button_hover_resource =
      resource_manager->GetStaticResourceWithTint(close_hover_bg_resource_id,
                                                  close_hover_bg_tint, true);
  ui::Resource* divider_resource = resource_manager->GetStaticResourceWithTint(
      divider_resource_id, divider_tint, true);

  layer->SetProperties(
      id, close_button_resource, close_button_hover_resource, divider_resource,
      tab_handle_resource, tab_handle_outline_resource, foreground,
      shouldShowTabOutline, close_pressed, toolbar_width, x, y, width, height,
      content_offset_y, divider_offset_x, bottom_margin, top_margin,
      close_button_padding, close_button_alpha, is_start_divider_visible,
      is_end_divider_visible, is_loading, spinner_rotation, opacity,
      tab_alpha, is_shown_as_favicon, title_offset); // Vivaldi
}

void TabStripSceneLayer::PutGroupIndicatorLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jboolean incognito,
    jboolean foreground,
    jboolean show_reorder_background,
    jboolean show_bubble,
    jint id,
    jint tint,
    jint reorder_background_tint,
    jint bubble_tint,
    jfloat x,
    jfloat y,
    jfloat width,
    jfloat height,
    jfloat title_start_padding,
    jfloat title_end_padding,
    jfloat corner_radius,
    jfloat bottom_indicator_width,
    jfloat bottom_indicator_height,
    jfloat bubble_padding,
    jfloat bubble_size,
    const JavaParamRef<jobject>& jlayer_title_cache) {
  LayerTitleCache* layer_title_cache =
      LayerTitleCache::FromJavaObject(jlayer_title_cache);

  // Reuse existing layer if it exists.
  scoped_refptr<GroupIndicatorLayer> layer =
      GetNextGroupIndicatorLayer(layer_title_cache);

  // Foreground if needed.
  if (foreground != layer->foreground()) {
    if (foreground) {
      foreground_group_titles_->AddChild(layer->layer());
    } else {
      group_ui_parent_layer_->AddChild(layer->layer());
    }
  }

  layer->SetProperties(
      id, tint, reorder_background_tint, bubble_tint, incognito, foreground,
      show_bubble, show_reorder_background, x, y, width, height,
      title_start_padding, title_end_padding, corner_radius,
      bottom_indicator_width, bottom_indicator_height, bubble_padding,
      bubble_size, tab_strip_layer_->bounds().height());
}

scoped_refptr<TabHandleLayer> TabStripSceneLayer::GetNextTabLayer(
    LayerTitleCache* layer_title_cache) {
  if (write_index_ < tab_handle_layers_.size())
    return tab_handle_layers_[write_index_++];

  scoped_refptr<TabHandleLayer> layer_tree =
      TabHandleLayer::Create(layer_title_cache);
  tab_handle_layers_.push_back(layer_tree);
  tab_ui_parent_layer_->AddChild(layer_tree->layer());
  write_index_++;
  return layer_tree;
}

scoped_refptr<GroupIndicatorLayer>
TabStripSceneLayer::GetNextGroupIndicatorLayer(
    LayerTitleCache* layer_title_cache) {
  if (group_write_index_ < group_title_layers_.size()) {
    return group_title_layers_[group_write_index_++];
  }

  scoped_refptr<GroupIndicatorLayer> layer_tree =
      GroupIndicatorLayer::Create(layer_title_cache);
  group_title_layers_.push_back(layer_tree);
  group_ui_parent_layer_->AddChild(layer_tree->layer());
  group_write_index_++;
  return layer_tree;
}

bool TabStripSceneLayer::ShouldShowBackground() {
  if (content_tree_)
    return content_tree_->ShouldShowBackground();
  return SceneLayer::ShouldShowBackground();
}

SkColor TabStripSceneLayer::GetBackgroundColor() {
  if (content_tree_)
    return content_tree_->GetBackgroundColor();
  return SceneLayer::GetBackgroundColor();
}

// Vivaldi
void TabStripSceneLayer::SetTabStripBackgroundColor(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint java_color,
    jboolean use_light) {
  SkColor4f color =
      SkColor4f::FromColor(ui::JavaColorToOptionalSkColor(java_color).value());
  tab_strip_layer_->SetBackgroundColor(color);
  use_light_foreground_on_background = use_light;
}

// Vivaldi
void TabStripSceneLayer::SetIsStackStrip(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jboolean jis_stack_strip){
  is_stack_strip_ = jis_stack_strip;
}

// Vivaldi
void TabStripSceneLayer::UpdateLoadingState(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint loading_text_resource_id,
    const base::android::JavaParamRef<jobject>& jresource_manager,
    jboolean should_show_loading) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* title_resource = resource_manager->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP, loading_text_resource_id);
  if (title_resource) {
    loading_text_->SetUIResourceId(title_resource->ui_resource()->id());
    loading_text_->SetBounds(title_resource->size());
    int pos_x = tab_strip_layer_->bounds().width() / 2;
    int pos_y = tab_strip_layer_->bounds().height() / 2;
    loading_text_->SetPosition(
        gfx::PointF(pos_x - (loading_text_->bounds().width() / 2),
                    pos_y - loading_text_->bounds().height() / 2));
    loading_text_->SetHideLayerAndSubtree(!should_show_loading);
  }
}

static jlong JNI_TabStripSceneLayer_Init(JNIEnv* env,
                                         const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  TabStripSceneLayer* scene_layer = new TabStripSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
