// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/viz_layer_context.h"

#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "services/viz/public/mojom/compositing/layer.mojom.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"

namespace cc::mojo_embedder {

namespace {

void ComputePropertyTreeNodeUpdate(
    const TransformNode* old_node,
    const TransformNode& new_node,
    std::vector<viz::mojom::TransformNodePtr>& container) {
  // TODO(https://crbug.com/40902503): This is a subset of the properties we
  // need to sync.
  if (old_node && old_node->id == new_node.id &&
      old_node->parent_id == new_node.parent_id &&
      old_node->parent_frame_id == new_node.parent_frame_id &&
      old_node->element_id == new_node.element_id &&
      old_node->local == new_node.local &&
      old_node->origin == new_node.origin &&
      old_node->scroll_offset == new_node.scroll_offset &&
      old_node->visible_frame_element_id == new_node.visible_frame_element_id) {
    return;
  }

  auto wire = viz::mojom::TransformNode::New();
  wire->id = new_node.id;
  wire->parent_id = new_node.parent_id;
  wire->parent_frame_id = new_node.parent_frame_id;
  wire->element_id = new_node.element_id;
  wire->local = new_node.local;
  wire->origin = new_node.origin;
  wire->scroll_offset = new_node.scroll_offset;
  wire->visible_frame_element_id = new_node.visible_frame_element_id;
  container.push_back(std::move(wire));
}

void ComputePropertyTreeNodeUpdate(
    const ClipNode* old_node,
    const ClipNode& new_node,
    std::vector<viz::mojom::ClipNodePtr>& container) {
  if (old_node && old_node->id == new_node.id &&
      old_node->parent_id == new_node.parent_id &&
      old_node->transform_id == new_node.transform_id &&
      old_node->clip == new_node.clip) {
    return;
  }

  auto wire = viz::mojom::ClipNode::New();
  wire->id = new_node.id;
  wire->parent_id = new_node.parent_id;
  wire->transform_id = new_node.transform_id;
  wire->clip = new_node.clip;
  container.push_back(std::move(wire));
}

void ComputePropertyTreeNodeUpdate(
    const EffectNode* old_node,
    const EffectNode& new_node,
    std::vector<viz::mojom::EffectNodePtr>& container) {
  if (old_node && old_node->id == new_node.id &&
      old_node->parent_id == new_node.parent_id &&
      old_node->transform_id == new_node.transform_id &&
      old_node->clip_id == new_node.clip_id &&
      old_node->element_id == new_node.element_id &&
      old_node->opacity == new_node.opacity &&
      old_node->render_surface_reason == new_node.render_surface_reason) {
    return;
  }

  auto wire = viz::mojom::EffectNode::New();
  wire->id = new_node.id;
  wire->parent_id = new_node.parent_id;
  wire->transform_id = new_node.transform_id;
  wire->clip_id = new_node.clip_id;
  wire->element_id = new_node.element_id;
  wire->opacity = new_node.opacity;
  wire->has_render_surface =
      new_node.render_surface_reason != RenderSurfaceReason::kNone;
  container.push_back(std::move(wire));
}

void ComputePropertyTreeNodeUpdate(
    const ScrollNode* old_node,
    const ScrollNode& new_node,
    std::vector<viz::mojom::ScrollNodePtr>& container) {
  if (old_node && old_node->id == new_node.id &&
      old_node->parent_id == new_node.parent_id &&
      old_node->transform_id == new_node.transform_id &&
      old_node->container_bounds == new_node.container_bounds &&
      old_node->bounds == new_node.bounds &&
      old_node->element_id == new_node.element_id &&
      old_node->scrolls_inner_viewport == new_node.scrolls_inner_viewport &&
      old_node->scrolls_outer_viewport == new_node.scrolls_outer_viewport &&
      old_node->user_scrollable_horizontal ==
          new_node.user_scrollable_horizontal &&
      old_node->user_scrollable_vertical == new_node.user_scrollable_vertical) {
    return;
  }

  auto wire = viz::mojom::ScrollNode::New();
  wire->id = new_node.id;
  wire->parent_id = new_node.parent_id;
  wire->transform_id = new_node.transform_id;
  wire->container_bounds = new_node.container_bounds;
  wire->bounds = new_node.bounds;
  wire->element_id = new_node.element_id;
  wire->scrolls_inner_viewport = new_node.scrolls_inner_viewport;
  wire->scrolls_outer_viewport = new_node.scrolls_outer_viewport;
  wire->user_scrollable_horizontal = new_node.user_scrollable_horizontal;
  wire->user_scrollable_vertical = new_node.user_scrollable_vertical;
  container.push_back(std::move(wire));
}

template <typename TreeType, typename ContainerType>
void ComputePropertyTreeUpdate(const TreeType& old_tree,
                               const TreeType& new_tree,
                               ContainerType& updates,
                               uint32_t& new_num_nodes) {
  using NodeType = typename TreeType::NodeType;
  new_num_nodes = base::checked_cast<uint32_t>(new_tree.size());
  for (size_t i = 0; i < new_tree.size(); ++i) {
    const NodeType* old_node = old_tree.size() > i ? old_tree.Node(i) : nullptr;
    ComputePropertyTreeNodeUpdate(old_node, *new_tree.Node(i), updates);
  }
}

}  // namespace

VizLayerContext::VizLayerContext(viz::mojom::CompositorFrameSink& frame_sink,
                                 LayerTreeHostImpl& host_impl)
    : host_impl_(host_impl) {
  auto context = viz::mojom::PendingLayerContext::New();
  context->receiver = service_.BindNewEndpointAndPassReceiver();
  context->client = client_receiver_.BindNewEndpointAndPassRemote();
  frame_sink.BindLayerContext(std::move(context));
}

VizLayerContext::~VizLayerContext() = default;

void VizLayerContext::SetVisible(bool visible) {
  service_->SetVisible(visible);
}

void VizLayerContext::UpdateDisplayTreeFrom(LayerTreeImpl& tree) {
  auto& property_trees = *tree.property_trees();
  auto update = viz::mojom::LayerTreeUpdate::New();
  update->source_frame_number = tree.source_frame_number();
  update->trace_id = tree.trace_id();
  update->device_viewport = tree.GetDeviceViewport();
  update->device_scale_factor = tree.painted_device_scale_factor();
  if (tree.local_surface_id_from_parent().is_valid()) {
    update->local_surface_id_from_parent = tree.local_surface_id_from_parent();
  }
  update->background_color = tree.background_color();

  const ViewportPropertyIds& property_ids = tree.viewport_property_ids();
  update->overscroll_elasticity_transform =
      property_ids.overscroll_elasticity_transform;
  update->page_scale_transform = property_ids.page_scale_transform;
  update->inner_scroll = property_ids.inner_scroll;
  update->outer_clip = property_ids.outer_clip;
  update->outer_scroll = property_ids.outer_scroll;

  LayerImpl* const root = tree.root_layer();
  std::unordered_set<LayerImpl*> updated_layers = tree.TakeUpdatedLayers();
  for (LayerImpl* layer : updated_layers) {
    auto wire = viz::mojom::Layer::New();
    wire->id = layer->id();
    wire->type = layer->GetLayerType();
    wire->bounds = layer->bounds();
    wire->is_drawable = layer->draws_content();
    wire->contents_opaque = layer->contents_opaque();
    wire->contents_opaque_for_text = layer->contents_opaque_for_text();
    wire->background_color = layer->background_color();
    wire->safe_opaque_background_color = layer->safe_opaque_background_color();
    wire->update_rect = layer->update_rect();
    wire->offset_to_transform_parent = layer->offset_to_transform_parent();
    wire->transform_tree_index = layer->transform_tree_index();
    wire->clip_tree_index = layer->clip_tree_index();
    wire->effect_tree_index = layer->effect_tree_index();
    wire->scroll_tree_index = layer->scroll_tree_index();
    if (layer == root) {
      DCHECK(!update->root_layer);
      update->root_layer = std::move(wire);
    } else {
      update->layers.push_back(std::move(wire));
    }
  }
  update->removed_layers = tree.TakeUnregisteredLayers();

  // TODO(rockot): Granular change tracking for property trees, so we aren't
  // diffing every time.
  PropertyTrees& old_trees = last_committed_property_trees_;
  ComputePropertyTreeUpdate(
      old_trees.transform_tree(), property_trees.transform_tree(),
      update->transform_nodes, update->num_transform_nodes);
  ComputePropertyTreeUpdate(old_trees.clip_tree(), property_trees.clip_tree(),
                            update->clip_nodes, update->num_clip_nodes);
  ComputePropertyTreeUpdate(old_trees.effect_tree(),
                            property_trees.effect_tree(), update->effect_nodes,
                            update->num_effect_nodes);
  ComputePropertyTreeUpdate(old_trees.scroll_tree(),
                            property_trees.scroll_tree(), update->scroll_nodes,
                            update->num_scroll_nodes);

  last_committed_property_trees_ = property_trees;

  service_->UpdateDisplayTree(std::move(update));
}

void VizLayerContext::OnRequestCommitForFrame(const viz::BeginFrameArgs& args) {
}

}  // namespace cc::mojo_embedder
