export module Systems.CameraManager;

import Core.Types;
import Core.Assert;
import Components.Camera;
import Components.Transform;
import ECS.Component;
import ECS.World;
import Math.Matrix;
import std;

export class CameraManager {
private:
    static inline EntityHandle s_PrimaryCamera{};
    static inline Vector<EntityHandle> s_AllCameras;
    static inline std::mutex s_Mutex;

public:
    static void RegisterCamera(EntityHandle handle) {
        std::lock_guard lock{s_Mutex};
        s_AllCameras.push_back(handle);

        if (!s_PrimaryCamera.valid()) {
            s_PrimaryCamera = handle;
        }
    }

    static void UnregisterCamera(EntityHandle handle) {
        std::lock_guard lock{s_Mutex};
        if (auto it = std::ranges::find(s_AllCameras, handle); it != s_AllCameras.end()) {
            s_AllCameras.erase(it);
        }

        if (s_PrimaryCamera == handle) {
            s_PrimaryCamera = s_AllCameras.empty() ? EntityHandle{} : s_AllCameras[0];
        }
    }

    static void SetPrimaryCamera(EntityHandle handle) {
        std::lock_guard lock{s_Mutex};
        s_PrimaryCamera = handle;
    }

    [[nodiscard]] static EntityHandle GetPrimaryCamera() {
        std::lock_guard lock{s_Mutex};
        return s_PrimaryCamera;
    }

    [[nodiscard]] static const Vector<EntityHandle>& GetAllCameras() {
        return s_AllCameras;
    }

    static void Clear() {
        std::lock_guard lock{s_Mutex};
        s_AllCameras.clear();
        s_PrimaryCamera = EntityHandle{};
    }
};