export module Graphics.RenderData;

import Core.Types;
import Math.Matrix;

export struct CameraConstants {
    Math::Mat4 view;
    Math::Mat4 projection;
    Math::Mat4 viewProjection;
    Math::Vec3 cameraPosition;
    F32 padding;
};

export struct ObjectConstants {
    Math::Mat4 world;
};