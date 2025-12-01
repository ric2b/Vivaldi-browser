// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

namespace content {

class RenderFrameHost;

} // namespace content

// Vivaldi: Detects if given render frame host belongs to a vivaldi tab.
bool IsVivaldiRegularTabFrame(content::RenderFrameHost* frame);
