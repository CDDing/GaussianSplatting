#pragma once
#include "Core.h"
#include "../Vulkan/Context.h"
#include "../Vulkan/Swapchain.h"
#include "../Vulkan/Pipeline.h"
#include "../Vulkan/Buffer.h"
#include "../Vulkan/CommandManager.h"
#include "../Vulkan/Renderer.h"
#include "../Vulkan/Vertex.h"
#include "PlyLoader.h"
#include "Camera.h"

class ProjectionPass;
class SortPass;
class RasterPass;

class App {
public:
    App(uint32_t width, uint32_t height, const char* title);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void Run();
    void InitializePLY(const char* filename);

private:
    GLFWwindow* window_ = nullptr;

    // Declaration order matters for destruction (reverse order)
    std::unique_ptr<Context> context_;
    std::unique_ptr<Swapchain> swapchain_;
    std::unique_ptr<Pipeline> pipeline_;
    std::unique_ptr<CommandManager> commandManager_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<SplatSet> splatSet_;
    Camera camera_{glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 1000.0f};

    // UBO (per-frame)
    std::array<std::unique_ptr<Buffer>, CommandManager::FRAMES_IN_FLIGHT> uboStaging_;
    std::array<std::unique_ptr<Buffer>, CommandManager::FRAMES_IN_FLIGHT> uboDevice_;

    // Compute passes (각 pass가 자기 ComputePipeline을 소유)
    std::unique_ptr<ProjectionPass> projPass_;
    std::unique_ptr<SortPass> sortPass_;
    std::unique_ptr<RasterPass> rastPass_;

    // GPU buffers — Gaussian 입력 (SOA)
    std::unique_ptr<Buffer> positionBuffer_;
    std::unique_ptr<Buffer> shBuffer_;
    std::unique_ptr<Buffer> opacityBuffer_;
    std::unique_ptr<Buffer> scaleBuffer_;
    std::unique_ptr<Buffer> rotationBuffer_;

    // GPU buffers — Projection 출력 (per-frame)
    std::array<std::unique_ptr<Buffer>, CommandManager::FRAMES_IN_FLIGHT> projected2DBuffers_;
    std::array<std::unique_ptr<Buffer>, CommandManager::FRAMES_IN_FLIGHT> visibilityBuffers_;
    std::array<std::unique_ptr<Buffer>, CommandManager::FRAMES_IN_FLIGHT> tileCountBuffers_;

    uint32_t gaussianCount_ = 0;

    // Input state
    bool leftMouseDown_  = false;
    bool rightMouseDown_ = false;
    double lastMouseX_   = 0.0;
    double lastMouseY_   = 0.0;

    bool framebufferResized_ = false;

    void initWindow(uint32_t width, uint32_t height, const char* title);
    void initVulkan();
    void mainLoop();
    void recreateSwapchain();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};
