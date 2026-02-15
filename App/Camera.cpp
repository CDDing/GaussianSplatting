#include "Camera.h"

Camera::Camera(float fovYRadians, float aspect, float zNear, float zFar)
    : fovY_(fovYRadians), aspect_(aspect), zNear_(zNear), zFar_(zFar)
{
}

glm::vec3 Camera::computeEyePosition() const {
    float x = distance_ * std::cos(pitch_) * std::sin(yaw_);
    float y = distance_ * std::sin(pitch_);
    float z = distance_ * std::cos(pitch_) * std::cos(yaw_);
    return target_ + glm::vec3(x, y, z);
}

glm::vec3 Camera::GetPosition() const {
    return computeEyePosition();
}

void Camera::Rotate(float deltaYaw, float deltaPitch) {
    yaw_ += deltaYaw;
    pitch_ += deltaPitch;

    // Clamp pitch to avoid gimbal lock
    constexpr float limit = glm::radians(89.0f);
    pitch_ = glm::clamp(pitch_, -limit, limit);
}

void Camera::Zoom(float delta) {
    distance_ -= delta;
    distance_ = glm::max(distance_, 0.1f);
}

void Camera::Pan(float deltaX, float deltaY) {
    glm::vec3 eye = computeEyePosition();
    glm::vec3 forward = glm::normalize(target_ - eye);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::cross(right, forward);

    target_ += right * deltaX + up * deltaY;
}

void Camera::SetAspect(float aspect) {
    aspect_ = aspect;
}

void Camera::SetScreenSize(uint32_t width, uint32_t height) {
    screenWidth_ = width;
    screenHeight_ = height;
    if (height > 0) {
        aspect_ = static_cast<float>(width) / static_cast<float>(height);
    }
}

CameraUBOData Camera::GetUBOData() const {
    CameraUBOData data{};

    glm::vec3 eye = computeEyePosition();

    // View matrix
    data.viewMatrix = glm::lookAt(eye, target_, glm::vec3(0.0f, 1.0f, 0.0f));

    // Projection matrix (Vulkan: depth [0,1], Y-flip)
    data.projMatrix = glm::perspectiveRH_ZO(fovY_, aspect_, zNear_, zFar_);
    data.projMatrix[1][1] *= -1.0f; // Vulkan Y-flip

    // Camera position
    data.camPos = glm::vec4(eye, 1.0f);

    // Screen size
    data.screenSize = glm::uvec2(screenWidth_, screenHeight_);

    // FOV as half-tangent (matches proj.comp usage)
    float halfFovY = fovY_ * 0.5f;
    data.fovY = std::tan(halfFovY);
    data.fovX = data.fovY * aspect_;

    data.zNear = zNear_;
    data.zFar = zFar_;

    return data;
}
