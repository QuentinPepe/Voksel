module Graphics;

import Graphics.DX12.GraphicsContext;

std::unique_ptr<IGraphicsContext> CreateGraphicsContext(Window& window, const GraphicsConfig& config) {
    return CreateDX12GraphicsContext(window, config);
}