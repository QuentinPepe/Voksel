export module Systems.CameraControllerSystem;

import ECS.SystemScheduler;
import ECS.Query;
import ECS.World;
import Components.Camera;
import Components.Transform;
import Components.CameraController;
import Input.Core;
import Input.Manager;
import Input.Bindings;
import Math.Core;
import Math.Vector;
import Math.Quaternion;
import Core.Types;
import std;

export class CameraControllerSystem : public QuerySystem<CameraControllerSystem,
    Write<Transform>, Write<CameraController>, With<Camera>> {
private:
    InputManager* m_InputManager{nullptr};
    std::shared_ptr<InputContext> m_InputContext;

public:
    void Setup() {
        QuerySystem::Setup();
        SetName("CameraController");
        SetStage(SystemStage::Update);
        SetPriority(SystemPriority::High);
        RunBefore("Camera");
    }

    void SetInputManager(InputManager* inputManager) {
        m_InputManager = inputManager;
        SetupInput();
    }

    void Run(World* world, F32 dt) override {
        if (!m_InputManager || !m_InputContext) return;

        ForEach(world, [this, dt](Transform* transform, CameraController* controller) {
            if (!controller->enableInput) return;

            switch (controller->type) {
                case CameraController::Type::Free:
                    UpdateFreeCamera(transform, controller, dt);
                    break;
                default:
                    break;
            }
        });
    }

private:
    void SetupInput() {
        if (!m_InputManager) return;

        InputMapper mapper;
        m_InputContext = mapper.CreateContext("CameraControl", 100);

        auto* moveForward = m_InputContext->AddAction("CameraMoveForward");
        moveForward->AddBinding(InputBinding::MakeKey(Key::W));

        auto* moveBackward = m_InputContext->AddAction("CameraMoveBackward");
        moveBackward->AddBinding(InputBinding::MakeKey(Key::S));

        auto* moveLeft = m_InputContext->AddAction("CameraMoveLeft");
        moveLeft->AddBinding(InputBinding::MakeKey(Key::A));

        auto* moveRight = m_InputContext->AddAction("CameraMoveRight");
        moveRight->AddBinding(InputBinding::MakeKey(Key::D));

        auto* moveUp = m_InputContext->AddAction("CameraMoveUp");
        moveUp->AddBinding(InputBinding::MakeKey(Key::E));

        auto* moveDown = m_InputContext->AddAction("CameraMoveDown");
        moveDown->AddBinding(InputBinding::MakeKey(Key::Q));

        auto* boost = m_InputContext->AddAction("CameraBoost");
        boost->AddBinding(InputBinding::MakeKey(Key::LeftShift));

        auto* lookX = m_InputContext->AddAction("CameraLookX");
        lookX->AddBinding(InputBinding::MakeMouseAxis(0, 1.0f));

        auto* lookY = m_InputContext->AddAction("CameraLookY");
        lookY->AddBinding(InputBinding::MakeMouseAxis(1, 1.0f));

        m_InputContext->Update(*m_InputManager);
    }

    void UpdateFreeCamera(Transform* transform, CameraController* controller, F32 dt) const {
        m_InputContext->Update(*m_InputManager);

        if (m_InputManager->IsMouseButtonPressed(MouseButton::Right)) {
            auto lookDeltaX = m_InputContext->GetAction("CameraLookX")->GetValue();
            auto lookDeltaY = m_InputContext->GetAction("CameraLookY")->GetValue();

            controller->yaw -= lookDeltaX * controller->lookSpeed;
            controller->pitch -= lookDeltaY * controller->lookSpeed;

            if (controller->constrainPitch) {
                controller->pitch = Math::Clamp(controller->pitch, controller->minPitch, controller->maxPitch);
            }

            controller->yaw = Math::Wrap(controller->yaw, -Math::PI, Math::PI);
        }

        transform->rotation = Math::Quat::FromEuler(controller->pitch, controller->yaw, 0.0f);

        Math::Vec3 moveInput{};

        if (m_InputContext->GetAction("CameraMoveForward")->IsPressed()) {
            moveInput += transform->Forward();
        }
        if (m_InputContext->GetAction("CameraMoveBackward")->IsPressed()) {
            moveInput -= transform->Forward();
        }
        if (m_InputContext->GetAction("CameraMoveLeft")->IsPressed()) {
            moveInput -= transform->Right();
        }
        if (m_InputContext->GetAction("CameraMoveRight")->IsPressed()) {
            moveInput += transform->Right();
        }
        if (m_InputContext->GetAction("CameraMoveUp")->IsPressed()) {
            moveInput += Math::Vec3::Up;
        }
        if (m_InputContext->GetAction("CameraMoveDown")->IsPressed()) {
            moveInput -= Math::Vec3::Up;
        }

        if (moveInput.LengthSquared() > 0.0f) {
            moveInput = moveInput.Normalized();

            F32 speed = controller->moveSpeed;
            if (m_InputContext->GetAction("CameraBoost")->IsPressed()) {
                speed *= controller->boostMultiplier;
            }

            controller->velocity = moveInput * speed;
        } else {
            controller->velocity = Math::Vec3::Zero;
        }

        transform->position += controller->velocity * dt;
        transform->localDirty = true;
        transform->worldDirty = true;
    }
};