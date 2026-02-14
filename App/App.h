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

class App {
public:
    App(uint32_t width, uint32_t height, const char* title);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void Run();
    void LoadPLY(const char* filename);

private:
    GLFWwindow* window_ = nullptr;

    // Declaration order matters for destruction (reverse order)
    std::unique_ptr<Context> context_;
    std::unique_ptr<Swapchain> swapchain_;
    std::unique_ptr<Pipeline> pipeline_;
    std::unique_ptr<Buffer> vertexBuffer_;
    std::unique_ptr<CommandManager> commandManager_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<SplatSet> splatSet_;

    bool framebufferResized_ = false;

    void initWindow(uint32_t width, uint32_t height, const char* title);
    void initVulkan();
    void mainLoop();
    void recreateSwapchain();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};
