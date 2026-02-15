# Orbit Camera Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add an orbit camera that provides view/projection matrices matching the `CameraUBO` layout in `proj.comp`.

**Architecture:** Camera is a pure math class (GLM only, no Vulkan). Lives in `App/Camera.h/.cpp`. Per-frame UBO buffers (one per frame-in-flight) created and uploaded by App. Buffer class extended with host-visible mapped buffer support.

**Tech Stack:** C++20, GLM (glm::mat4, glm::lookAt, glm::perspectiveRH_ZO), GLFW input callbacks, VMA mapped buffers

**Design doc:** `docs/plans/2026-02-15-orbit-camera-design.md`

---

### Task 1: Update CMakeLists.txt shader references

**Files:**
- Modify: `CMakeLists.txt:22-25`

**Important context:**
- Shader files were renamed: `triangle.vert` → `rast.comp`, `triangle.frag` → `sort.comp`, and `proj.comp` was added.
- CMakeLists.txt still references the old names. Fix first so the build is clean from the start.

**Step 1: Update shader sources list**

Replace lines 22-25:
```cmake
set(SHADER_SOURCES
    ${SHADER_DIR}/proj.comp
    ${SHADER_DIR}/sort.comp
    ${SHADER_DIR}/rast.comp
)
```

**Step 2: Verify shaders compile**

Run: `cmake --build cmake-build-debug --target Shaders`
Expected: all three shaders compile to .spv files

**Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "fix(build): update shader sources to match renamed files"
```

---

### Task 2: Create Camera class header

**Files:**
- Create: `App/Camera.h`

**Important context:**
- `proj.comp` uses `fovX`/`fovY` as **half-tangent values** (not angles). See `proj.comp:97-98`: `focalX = screenSize.x / (2.0 * camera.fovX)` means `fovX = tan(halfFovX)`.
- The CameraUBOData struct must match std140 layout exactly. Current layout packs tightly with GLM types (verified: sizeof == 168).
- Vulkan uses Y-down clip space and [0,1] depth range. Use `glm::perspectiveRH_ZO` explicitly (NOT `glm::perspective` which defaults to OpenGL's [-1,1] depth range).

**Step 1: Write `App/Camera.h`**

```cpp
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
```

**Step 2: Commit**

```bash
git add App/Camera.h
git commit -m "feat(camera): add Camera class header with CameraUBOData struct"
```

---

### Task 3: Implement Camera class

**Files:**
- Create: `App/Camera.cpp`

**Important context:**
- `computeEyePosition()`: spherical coordinates → cartesian. `eye = target + distance * (cos(pitch)*sin(yaw), sin(pitch), cos(pitch)*cos(yaw))`.
- `GetUBOData()`: must compute `fovX = tan(fovY/2) * aspect`, `fovY = tan(fovY/2)` to match `proj.comp`'s usage.
- Projection: use `glm::perspectiveRH_ZO` for Vulkan's [0,1] depth range and right-handed coordinate system. Flip Y by negating `projMatrix[1][1]` for Vulkan.
- Pan: move target along camera's local right and up axes.

**Step 1: Write `App/Camera.cpp`**

```cpp
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
```

**Step 2: Add to CMakeLists.txt**

In `CMakeLists.txt:43`, add `App/Camera.cpp` to the source list:
```cmake
add_executable(GaussianSplatting
    main.cpp
    App/App.cpp
    App/Camera.cpp
    Vulkan/Context.cpp
    ...
```

Run: `cmake --build cmake-build-debug --target GaussianSplatting`
Expected: compiles without errors

**Step 3: Commit**

```bash
git add App/Camera.cpp CMakeLists.txt
git commit -m "feat(camera): implement orbit camera math"
```

---

### Task 4: Extend Buffer class for host-visible mapped buffers

**Files:**
- Modify: `Vulkan/Buffer.h:6-24`
- Modify: `Vulkan/Buffer.cpp:34-130`

**Important context:**
- Current Buffer class only creates device-local buffers (with optional staging upload).
- UBO needs a host-visible, persistently mapped buffer that can be updated every frame via `memcpy`.
- Add a static factory method `Buffer::CreateMapped()` and expose `Upload()`.

**Step 1: Update `Vulkan/Buffer.h`**

Add after `vk::Buffer GetHandle() const` (line 18):
```cpp
    // Host-visible mapped buffer factory (for UBOs updated every frame)
    static Buffer CreateMapped(Context& context, vk::BufferUsageFlags usage,
                               vk::DeviceSize size);

    // Upload data to mapped buffer (only valid for mapped buffers)
    void Upload(const void* data, vk::DeviceSize size);
```

Add to private section after `VmaAllocation allocation_` (line 23):
```cpp
    void* mappedData_       = nullptr;
    vk::DeviceSize size_    = 0;
    Buffer() = default;  // for CreateMapped factory
```

Update move constructor/assignment to also move `mappedData_` and `size_`.

**Step 2: Update `Vulkan/Buffer.cpp`**

Add `size_(size)` to existing constructor's member initializer list (line 36).

Add `CreateMapped` implementation after the existing constructor:
```cpp
Buffer Buffer::CreateMapped(Context& context, vk::BufferUsageFlags usage,
                            vk::DeviceSize size) {
    Buffer buf;
    buf.allocator_ = context.GetAllocator();
    buf.size_ = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = size;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(usage);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage         = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags         = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VmaAllocationInfo allocInfoOut{};
    if (vmaCreateBuffer(buf.allocator_, &bufferInfo, &allocInfo,
                        &buf.buffer_, &buf.allocation_, &allocInfoOut) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create mapped buffer");
    }

    buf.mappedData_ = allocInfoOut.pMappedData;
    return buf;
}
```

Add `Upload` implementation:
```cpp
void Buffer::Upload(const void* data, vk::DeviceSize size) {
    if (!mappedData_) {
        throw std::runtime_error("Upload called on non-mapped buffer");
    }
    memcpy(mappedData_, data, size);
}
```

Update move constructor to transfer `mappedData_` and `size_`:
```cpp
Buffer::Buffer(Buffer&& other) noexcept
    : allocator_(other.allocator_)
    , buffer_(other.buffer_)
    , allocation_(other.allocation_)
    , mappedData_(other.mappedData_)
    , size_(other.size_)
{
    other.allocator_  = nullptr;
    other.buffer_     = VK_NULL_HANDLE;
    other.allocation_ = nullptr;
    other.mappedData_ = nullptr;
    other.size_       = 0;
}
```

Update move assignment similarly (add `mappedData_` and `size_` transfer + reset).

**Step 3: Verify it compiles**

Run: `cmake --build cmake-build-debug --target GaussianSplatting`
Expected: compiles without errors

**Step 4: Commit**

```bash
git add Vulkan/Buffer.h Vulkan/Buffer.cpp
git commit -m "feat(buffer): add host-visible mapped buffer support for UBOs"
```

---

### Task 5: Integrate Camera into App (per-frame-in-flight UBO buffers)

**Files:**
- Modify: `App/App.h:1-43`
- Modify: `App/App.cpp:1-150`

**Important context:**
- App already owns `GLFWwindow*` and has `framebufferResizeCallback`.
- **Must use FRAMES_IN_FLIGHT (2) UBO buffers** to avoid data race: frame N's GPU read and frame N+1's CPU write must not collide. Each frame slot writes to its own buffer.
- Renderer exposes `currentFrame_` index, or App tracks its own frame index. Simplest: Renderer already returns from `DrawFrame`, so App needs the frame index. Two options:
  - (a) Expose `Renderer::GetCurrentFrame()` getter
  - (b) App tracks its own frame counter matching Renderer's
  - Choose (a) — add a one-line getter to Renderer.
- GLFW callbacks: need `mouseButtonCallback`, `cursorPosCallback`, `scrollCallback`.

**Step 1: Add frame index getter to `Vulkan/Renderer.h`**

Add after `void RecreateFramebuffers(...)` (line 23):
```cpp
    uint32_t GetCurrentFrame() const { return currentFrame_; }
```

**Step 2: Update `App/App.h`**

Add include (after `#include "PlyLoader.h"`, line 10):
```cpp
#include "Camera.h"
```

Add members (after `std::unique_ptr<SplatSet> splatSet_;`, line 33):
```cpp
    Camera camera_{glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 1000.0f};
    std::array<std::unique_ptr<Buffer>, CommandManager::FRAMES_IN_FLIGHT> cameraUboBuffers_;

    // Input state
    bool leftMouseDown_  = false;
    bool rightMouseDown_ = false;
    double lastMouseX_   = 0.0;
    double lastMouseY_   = 0.0;
```

Add `#include <array>` if not already in Core.h (it is — `Core.h:7`).

Add static callbacks (after `framebufferResizeCallback`, line 42):
```cpp
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
```

**Step 3: Update `App/App.cpp`**

**3a. Register GLFW callbacks** — in `initWindow()` after `glfwSetFramebufferSizeCallback` (line 50):
```cpp
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
```

**3b. Create per-frame UBO buffers** — in `initVulkan()` after pipeline creation (line 60):
```cpp
    for (auto& buf : cameraUboBuffers_) {
        buf = std::make_unique<Buffer>(
            Buffer::CreateMapped(*context_, vk::BufferUsageFlagBits::eUniformBuffer,
                                 sizeof(CameraUBOData))
        );
    }
    camera_.SetScreenSize(swapchain_->GetExtent().width, swapchain_->GetExtent().height);
```

**3c. Per-frame UBO upload** — in `mainLoop()` before `DrawFrame` call (line 108):
```cpp
        // Update camera UBO for current frame
        uint32_t frameIdx = renderer_->GetCurrentFrame();
        CameraUBOData uboData = camera_.GetUBOData();
        cameraUboBuffers_[frameIdx]->Upload(&uboData, sizeof(uboData));
```

Update `DrawFrame` call to pass current frame's UBO buffer handle:
```cpp
        bool needsRecreation = renderer_->DrawFrame(
            *context_, *swapchain_, *pipeline_, *commandManager_,
            positionBuffer_->GetHandle()
        );
```
(No change needed to DrawFrame signature yet — UBO binding will happen when descriptor sets are wired up in a future task.)

**3d. Update camera on resize** — in `recreateSwapchain()` after swapchain recreate (line 137):
```cpp
    camera_.SetScreenSize(swapchain_->GetExtent().width, swapchain_->GetExtent().height);
```

**3e. Add destruction** — in `~App()` after `renderer_.reset()` (line 19):
```cpp
    for (auto& buf : cameraUboBuffers_) buf.reset();
```

**3f. Implement GLFW callbacks:**

```cpp
void App::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        app->leftMouseDown_ = (action == GLFW_PRESS);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        app->rightMouseDown_ = (action == GLFW_PRESS);
    }

    if (action == GLFW_PRESS) {
        glfwGetCursorPos(window, &app->lastMouseX_, &app->lastMouseY_);
    }
}

void App::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    double dx = xpos - app->lastMouseX_;
    double dy = ypos - app->lastMouseY_;
    app->lastMouseX_ = xpos;
    app->lastMouseY_ = ypos;

    if (app->leftMouseDown_) {
        constexpr float sensitivity = 0.005f;
        app->camera_.Rotate(
            static_cast<float>(-dx) * sensitivity,
            static_cast<float>(-dy) * sensitivity
        );
    }

    if (app->rightMouseDown_) {
        constexpr float panSpeed = 0.01f;
        app->camera_.Pan(
            static_cast<float>(-dx) * panSpeed,
            static_cast<float>(dy) * panSpeed
        );
    }
}

void App::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    app->camera_.Zoom(static_cast<float>(yoffset) * 0.5f);
}
```

**Step 4: Verify it compiles and runs**

Run: `cmake --build cmake-build-debug --target GaussianSplatting`
Expected: compiles. Run the app — window should open. Mouse interactions won't produce visible changes yet (no compute pipeline wired up), but the app should not crash.

**Step 5: Commit**

```bash
git add Vulkan/Renderer.h App/App.h App/App.cpp
git commit -m "feat(camera): integrate orbit camera with per-frame UBO buffers and GLFW input"
```

---

## Summary

| Task | What | Files |
|------|------|-------|
| 1 | Fix shader references | `CMakeLists.txt` |
| 2 | Camera header + CameraUBOData | `App/Camera.h` |
| 3 | Camera implementation | `App/Camera.cpp`, `CMakeLists.txt` |
| 4 | Mapped buffer support | `Vulkan/Buffer.h`, `Vulkan/Buffer.cpp` |
| 5 | App integration (per-frame UBO, input) | `Vulkan/Renderer.h`, `App/App.h`, `App/App.cpp` |

## What this does NOT include

- Descriptor set layout / descriptor pool / descriptor set binding (needed to actually wire the UBO to the compute shader — separate task)
- Compute pipeline setup (replacing the current graphics pipeline — separate task)
- Sort and rasterization passes — separate tasks

The camera is self-contained and ready for use once the compute pipeline is wired up.
