# Vulkan Triangle Renderer - Architecture Design

## Overview

Danite 프로젝트의 아키텍처 문제점을 개선한 Vulkan 삼각형 렌더러 초안.
Gaussian Splatting 렌더러의 기초 골격으로 확장 가능한 구조를 목표.

## Design Principles

| Danite 문제 | 해결 원칙 |
|---|---|
| 전역 `extern App* app` 싱글톤 | 의존성 주입 (생성자 매개변수) |
| pch.h가 App.h include | Core.h는 외부 라이브러리만 포함 |
| RenderManager 956줄 갓 클래스 | 단일 책임 원칙 (클래스당 하나의 역할) |
| Camera가 InputManager static 멤버 직접 접근 | 명시적 의존성 (인터페이스로 소통) |
| Image/Buffer 소멸자가 전역 app 사용 | 리소스 래퍼가 Context 참조 보유 |
| `#define FRAME_CNT 2`, 매직 넘버 | `constexpr` 상수 |
| Context.h에서 전역 const vector 정의 | .cpp에서 정의, 헤더에는 선언만 |
| Pipeline 추상화 미사용 | 사용하는 추상화만 작성 (YAGNI) |

## Directory Structure

```
GaussianSplatting/
├── CMakeLists.txt
├── main.cpp
├── Header/
│   └── Core.h                  # 외부 라이브러리 include만
├── Vulkan/
│   ├── Context.h/cpp           # Instance + PhysicalDevice + Device + VMA
│   ├── Swapchain.h/cpp         # Swapchain + ImageViews + Recreation
│   ├── Pipeline.h/cpp          # RenderPass + PipelineLayout + Pipeline
│   ├── Buffer.h/cpp            # VMA 기반 Buffer 래퍼
│   ├── CommandManager.h/cpp    # CommandPool + CommandBuffer + Sync 객체
│   └── Renderer.h/cpp          # DrawFrame 오케스트레이션
├── App/
│   └── App.h/cpp               # GLFW Window + 메인 루프
├── Shaders/
│   ├── triangle.vert
│   └── triangle.frag
└── docs/
    └── plans/
```

## Dependency Graph (No Cycles)

```
main
 └── App (owns all subsystems via unique_ptr)
      ├── Context          ← GLFWwindow*
      ├── Swapchain        ← Context&, GLFWwindow*
      ├── Pipeline         ← Context&, Swapchain&
      ├── Buffer           ← Context&
      ├── CommandManager   ← Context&
      └── Renderer         ← Context&, Swapchain&, Pipeline&, CommandManager&
```

## Destruction Order

RAII 소멸 순서는 멤버 선언의 역순. 안전한 순서를 보장하기 위해:

### App 소멸 순서 (선언 역순)
```
renderer_        → GPU 작업 완료 대기 (waitIdle) 후 소멸
commandManager_  → CommandPool 소멸
vertexBuffer_    → VMA 버퍼 해제
pipeline_        → Pipeline, RenderPass 소멸
swapchain_       → Swapchain, ImageViews 소멸
context_         → VmaAllocator → Device → Surface → Debug → Instance 소멸
window_          → glfwDestroyWindow + glfwTerminate
```

### Context 소멸자
```cpp
~Context() {
    device_->waitIdle();           // GPU 작업 완료 대기
    vmaDestroyAllocator(allocator_); // VMA 먼저 해제 (Device 소멸 전)
    // 나머지는 vk::raii가 선언 역순으로 자동 해제
}
```

## Vertex Definition

```cpp
// Vulkan/Vertex.h
struct Vertex {
    glm::vec2 position;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
};
```

## Class Interfaces

### App

```cpp
class App {
public:
    App(uint32_t width, uint32_t height, const char* title);
    ~App();   // waitIdle → glfwDestroyWindow → glfwTerminate
    void Run();
private:
    GLFWwindow* window_;

    // 선언 순서 = 생성 순서, 소멸은 역순
    std::unique_ptr<Context> context_;
    std::unique_ptr<Swapchain> swapchain_;
    std::unique_ptr<Pipeline> pipeline_;
    std::unique_ptr<Buffer> vertexBuffer_;
    std::unique_ptr<CommandManager> commandManager_;
    std::unique_ptr<Renderer> renderer_;

    bool framebufferResized_ = false;
    static void framebufferResizeCallback(GLFWwindow* window, int w, int h);
};
```

### Context

```cpp
class Context {
public:
    Context(GLFWwindow* window);
    ~Context();  // vmaDestroyAllocator 후 RAII 자동 소멸

    vk::raii::Device& Device();
    vk::raii::PhysicalDevice& PhysicalDevice();
    vk::raii::Instance& Instance();
    vk::Queue GetGraphicsQueue() const;
    vk::Queue GetPresentQueue() const;
    uint32_t GetGraphicsQueueFamily() const;
    VmaAllocator GetAllocator() const;

private:
    // 선언 순서 = 소멸 역순 (아래가 먼저 소멸)
    vk::raii::Context context_;
    vk::raii::Instance instance_       = nullptr;
    vk::raii::DebugUtilsMessengerEXT debug_ = nullptr;
    vk::raii::SurfaceKHR surface_      = nullptr;
    vk::raii::PhysicalDevice physical_ = nullptr;
    vk::raii::Device device_           = nullptr;
    VmaAllocator allocator_            = nullptr; // 소멸자에서 수동 해제
    uint32_t graphicsQueueFamily_      = 0;
    uint32_t presentQueueFamily_       = 0;

    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createAllocator();
};
```

Validation Layers: `#ifdef NDEBUG`로 Release 빌드에서 비활성화.

### Swapchain

```cpp
class Swapchain {
public:
    Swapchain(Context& context, GLFWwindow* window);

    void Recreate(Context& context, GLFWwindow* window);  // 윈도우 리사이즈 대응

    vk::Format GetFormat() const;
    vk::Extent2D GetExtent() const;
    uint32_t GetImageCount() const;
    const std::vector<vk::raii::ImageView>& GetImageViews() const;
    vk::SwapchainKHR GetHandle() const;

private:
    vk::raii::SwapchainKHR swapchain_ = nullptr;
    std::vector<vk::Image> images_;
    std::vector<vk::raii::ImageView> imageViews_;
    vk::Format format_;
    vk::Extent2D extent_;

    void create(Context& context, GLFWwindow* window);
};
```

### Pipeline

```cpp
class Pipeline {
public:
    Pipeline(Context& context, Swapchain& swapchain);

    vk::RenderPass GetRenderPass() const;
    vk::Pipeline GetHandle() const;
    vk::PipelineLayout GetLayout() const;

private:
    vk::raii::RenderPass renderPass_     = nullptr;
    vk::raii::PipelineLayout layout_     = nullptr;
    vk::raii::Pipeline pipeline_         = nullptr;

    void createRenderPass(Context& context, Swapchain& swapchain);
    void createPipeline(Context& context, Swapchain& swapchain);

    static std::vector<uint32_t> loadShader(const std::string& path);
};
```

### Buffer

```cpp
class Buffer {
public:
    // Host visible buffer (삼각형 초안에서는 이것만 사용)
    Buffer(Context& context, vk::BufferUsageFlags usage,
           vk::DeviceSize size, const void* data = nullptr);
    ~Buffer();

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    vk::Buffer GetHandle() const;

private:
    VmaAllocator allocator_ = nullptr;
    VkBuffer buffer_        = VK_NULL_HANDLE;
    VmaAllocation allocation_ = nullptr;
};
```

### CommandManager

```cpp
class CommandManager {
public:
    CommandManager(Context& context);

    // FRAMES_IN_FLIGHT개의 CommandBuffer를 미리 할당
    const std::vector<vk::raii::CommandBuffer>& GetCommandBuffers() const;

    void ImmediateSubmit(Context& context,
                         std::function<void(vk::CommandBuffer)>&& fn);

private:
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 사용
    // fence 대기 후 개별 CommandBuffer 리셋
    vk::raii::CommandPool pool_ = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers_;

    // ImmediateSubmit 전용
    vk::raii::CommandPool immediatePool_ = nullptr;
    vk::raii::CommandBuffer immediateBuffer_ = nullptr;
    vk::raii::Fence immediateFence_ = nullptr;
};
```

### Renderer

```cpp
class Renderer {
public:
    Renderer(Context& context, Swapchain& swapchain,
             Pipeline& pipeline, CommandManager& commands);

    // OUT_OF_DATE 시 true 반환 → App에서 swapchain 재생성
    bool DrawFrame(vk::Buffer vertexBuffer, uint32_t vertexCount);

    void RecreateFramebuffers(Context& context, Swapchain& swapchain,
                              Pipeline& pipeline);

private:
    static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    Context& context_;
    Swapchain& swapchain_;
    Pipeline& pipeline_;

    std::vector<vk::raii::Framebuffer> framebuffers_;

    std::array<vk::raii::Semaphore, FRAMES_IN_FLIGHT> imageAvailable_;
    std::array<vk::raii::Semaphore, FRAMES_IN_FLIGHT> renderFinished_;
    std::array<vk::raii::Fence, FRAMES_IN_FLIGHT> inFlight_;
    uint32_t currentFrame_ = 0;

    void createFramebuffers();
    void createSyncObjects();
    void recordCommandBuffer(vk::CommandBuffer cmd, uint32_t imageIndex,
                             vk::Buffer vertexBuffer, uint32_t vertexCount);
};
```

## VMA Configuration

VMA는 C Vulkan API 기반이므로 vk::raii 핸들에서 변환 필요:
```cpp
// Context::createAllocator()
VmaAllocatorCreateInfo info{};
info.physicalDevice = static_cast<VkPhysicalDevice>(*physical_);
info.device         = static_cast<VkDevice>(*device_);
info.instance       = static_cast<VkInstance>(*instance_);
info.flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
vmaCreateAllocator(&info, &allocator_);
```

`#define VMA_IMPLEMENTATION`은 **Context.cpp에서만** 1회 정의.

## Implementation Plan

### Step 1: 프로젝트 설정
- [ ] vcpkg.json 생성 (glfw3, glm, vulkan-memory-allocator)
- [ ] CMakeLists.txt 업데이트 (VMA find_package, 새 소스 파일들)
- [ ] Core.h 업데이트 (VMA include 추가)
- [ ] 디렉토리 생성 (App/, Shaders/)
- [ ] Vertex.h 작성 (position + color 구조체)

### Step 2: Shaders 작성 + 컴파일
- [ ] triangle.vert (Vertex Buffer에서 position/color 읽기)
- [ ] triangle.frag (pass-through color)
- [ ] SPIR-V 컴파일 (glslc 또는 glslangValidator)
- [ ] CMake custom command로 셰이더 자동 컴파일 설정

### Step 3: Context 구현
- [ ] Instance 생성 (Debug/Release별 validation layers)
- [ ] Debug messenger (NDEBUG 조건부)
- [ ] Surface 생성
- [ ] Physical device 선택
- [ ] Logical device + queues
- [ ] VMA allocator (VMA_IMPLEMENTATION 정의)
- [ ] 소멸자에서 vmaDestroyAllocator 호출

### Step 4: Swapchain 구현
- [ ] Swapchain 생성 (format: B8G8R8A8_SRGB, present: Mailbox 우선)
- [ ] ImageView 생성
- [ ] Recreate 메서드 (윈도우 리사이즈/최소화 대응)

### Step 5: Pipeline 구현
- [ ] loadShader 유틸리티 (throw 포함!)
- [ ] RenderPass 생성
- [ ] Pipeline 생성 (Vertex Input 바인딩 포함)

### Step 6: Buffer 구현
- [ ] VMA 기반 Buffer 래퍼 (Host Visible, Mapped)
- [ ] 삼각형 Vertex buffer 생성

### Step 7: CommandManager 구현
- [ ] CommandPool (RESET_COMMAND_BUFFER_BIT 플래그)
- [ ] FRAMES_IN_FLIGHT개 CommandBuffer 사전 할당
- [ ] ImmediateSubmit (전용 pool + fence)

### Step 8: Renderer 구현
- [ ] Framebuffer 생성
- [ ] Sync 객체 (Semaphore, Fence)
- [ ] DrawFrame (acquire → record → submit → present)
- [ ] OUT_OF_DATE / SUBOPTIMAL 에러 핸들링
- [ ] RecreateFramebuffers 메서드

### Step 9: App + main 통합
- [ ] glfwInit + 윈도우 생성
- [ ] framebufferResizeCallback 등록
- [ ] 모든 서브시스템 순서대로 초기화
- [ ] 메인 루프 (DrawFrame → swapchain 재생성 분기)
- [ ] 소멸자에서 waitIdle → glfwDestroyWindow → glfwTerminate

## Swapchain Recreation Flow

```
DrawFrame() returns true (OUT_OF_DATE)
  └── App::Run()
       ├── context_->Device()->waitIdle()
       ├── swapchain_->Recreate(*context_, window_)
       └── renderer_->RecreateFramebuffers(*context_, *swapchain_, *pipeline_)
```

윈도우 최소화(extent 0x0) 시:
```cpp
while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window_, &width, &height);
    glfwWaitEvents();
}
```

## Future Extensions (Not In Scope)

삼각형 렌더러 이후 확장 시 고려:
- Camera 시스템 (InputState 구조체로 Camera에 전달)
- ImGui 통합 (별도 ImGuiRenderer 클래스)
- Depth buffer (별도 Image 래퍼)
- Compute pipeline (Hi-Z, Gaussian Splatting)
- Staging Buffer를 통한 Device Local 전송 (ImmediateSubmit 활용)
- Buffer 풀링 (다수 Buffer 시 VMA allocation 최적화)
