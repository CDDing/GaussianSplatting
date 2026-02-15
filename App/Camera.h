#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdint>

// Must match proj.comp CameraUBO (std140 layout)
struct CameraUBOData {
    glm::mat4 viewMatrix;   // offset 0
    glm::mat4 projMatrix;   // offset 64
    glm::vec4 camPos;       // offset 128, xyz = position
    glm::uvec2 screenSize;  // offset 144
    float fovX;             // offset 152, tan(halfFovX)
    float fovY;             // offset 156, tan(halfFovY)
    float zNear;            // offset 160
    float zFar;             // offset 164
};
static_assert(sizeof(CameraUBOData) == 168, "CameraUBOData must match std140 layout");

class Camera {
public:
    Camera(float fovYRadians, float aspect, float zNear, float zFar);

    // Input
    void Rotate(float deltaYaw, float deltaPitch);
    void Zoom(float delta);
    void Pan(float deltaX, float deltaY);

    // Window resize
    void SetAspect(float aspect);
    void SetScreenSize(uint32_t width, uint32_t height);

    // UBO data (pure math, no Vulkan dependency)
    CameraUBOData GetUBOData() const;

    // Getters
    glm::vec3 GetPosition() const;
    glm::vec3 GetTarget() const { return target_; }

private:
    glm::vec3 target_{0.0f, 0.0f, 0.0f};
    float distance_ = 5.0f;
    float yaw_      = 0.0f;          // radians
    float pitch_    = 0.3f;          // radians, clamped to (-pi/2, pi/2)
    float fovY_;                     // radians
    float aspect_;
    float zNear_;
    float zFar_;
    uint32_t screenWidth_  = 1600;
    uint32_t screenHeight_ = 900;

    glm::vec3 computeEyePosition() const;
};
