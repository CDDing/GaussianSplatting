# Orbit Camera Design

## Goal
Add an orbit camera to the Gaussian Splatting renderer that provides view/projection matrices for the projection compute shader (`proj.comp`).

## Architecture Decision
- **Location**: `App/Camera.h` + `App/Camera.cpp`
- **Separation of concerns**: Camera is a pure math class (GLM only, no Vulkan dependency). UBO buffer creation and upload handled by App/Renderer.
- **Camera type**: Orbit camera (target + distance + yaw/pitch)

## Data Structure

### CameraUBOData (matches proj.comp CameraUBO)
```cpp
struct CameraUBOData {
    glm::mat4 viewMatrix;   // offset 0
    glm::mat4 projMatrix;   // offset 64
    glm::vec4 camPos;       // offset 128, xyz = position
    glm::uvec2 screenSize;  // offset 144, width/height
    float fovX;             // offset 152
    float fovY;             // offset 156
    float zNear;            // offset 160
    float zFar;             // offset 164
};
```

### Camera Class
```
Camera
  - target_: vec3 (orbit center)
  - distance_: float (camera distance from target)
  - yaw_: float (horizontal angle)
  - pitch_: float (vertical angle, clamped)
  - fovY_: float (vertical FOV in radians)
  - aspect_: float (width/height)
  - zNear_, zFar_: float
  - screenWidth_, screenHeight_: uint32_t
```

## Input Mapping
| Input | Action | Camera Method |
|-------|--------|---------------|
| Left-click drag | Rotate | `Rotate(deltaYaw, deltaPitch)` |
| Scroll wheel | Zoom | `Zoom(delta)` |
| Right-click drag | Pan | `Pan(deltaX, deltaY)` |
| Window resize | Update aspect | `SetAspect()` + `SetScreenSize()` |

## UBO Upload Path
```
Per frame:
  1. camera_.GetUBOData() → CameraUBOData (pure math)
  2. memcpy to mapped UBO buffer
  3. Bind UBO buffer in compute dispatch
```

## Buffer Changes
Current `Buffer` class only supports device-local with staging upload. Need to add support for host-visible persistently mapped buffers for UBO (updated every frame).

## File Changes
| File | Change |
|------|--------|
| `App/Camera.h` | NEW - Camera class declaration |
| `App/Camera.cpp` | NEW - Camera implementation |
| `App/App.h` | Add Camera member, GLFW input callbacks |
| `App/App.cpp` | Input handling, per-frame UBO upload |
| `Vulkan/Buffer.h/cpp` | Add host-visible mapped buffer option |
| `CMakeLists.txt` | Add App/Camera.cpp to sources |
