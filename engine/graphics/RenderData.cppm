export module Graphics.RenderData;

import Core.Types;
import Math.Matrix;
import Math.Vector;

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

export struct AtlasConstants {
    U32 tilesX;
    U32 tileSize;
    U32 pad;
    U32 _pad0;
    U32 atlasW;
    U32 atlasH;
};
