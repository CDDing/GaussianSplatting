#include "App.h"

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

App::App(uint32_t width, uint32_t height, const char* title) {
    initWindow(width, height, title);
    initVulkan();
}

App::~App() {
    // Ensure GPU is done before destroying resources
    if (context_) {
        context_->Device().waitIdle();
    }

    // Destroy in reverse dependency order (renderer first, context last)
    renderer_.reset();
    commandManager_.reset();
    vertexBuffer_.reset();
    pipeline_.reset();
    swapchain_.reset();
    splatSet_.reset();
    context_.reset();

    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

// ---------------------------------------------------------------------------
// initWindow
// ---------------------------------------------------------------------------

void App::initWindow(uint32_t width, uint32_t height, const char* title) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height),
                               title, nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
}

// ---------------------------------------------------------------------------
// initVulkan
// ---------------------------------------------------------------------------

void App::initVulkan() {
    context_        = std::make_unique<Context>(window_);
    swapchain_      = std::make_unique<Swapchain>(*context_, window_);
    pipeline_       = std::make_unique<Pipeline>(*context_, *swapchain_);

    // Triangle vertices
    const std::vector<Vertex> vertices = {
        {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},  // top, red
        {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},  // bottom right, green
        {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},  // bottom left, blue
    };

    vertexBuffer_   = std::make_unique<Buffer>(
        *context_,
        vk::BufferUsageFlagBits::eVertexBuffer,
        sizeof(Vertex) * vertices.size(),
        vertices.data()
    );

    commandManager_ = std::make_unique<CommandManager>(*context_);
    renderer_       = std::make_unique<Renderer>(*context_, *swapchain_,
                                                 *pipeline_, *commandManager_);
}

// ---------------------------------------------------------------------------
// Run / mainLoop
// ---------------------------------------------------------------------------

void App::Run() {
    mainLoop();
}

void App::LoadPLY(const char* filename)
{
    auto splatSet = std::make_unique<SplatSet>();
    if (!loadPly(filename, *splatSet)) {
        std::cerr << "Failed to load PLY: " << filename << std::endl;
        return;
    }

    std::cout << "Loaded " << splatSet->size() << " splats" << std::endl;
    splatSet_ = std::move(splatSet);
}

void App::mainLoop() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Handle minimization - wait until window is restored
        int width = 0, height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        if (width == 0 || height == 0) {
            glfwWaitEvents();
            continue;
        }

        bool needsRecreation = renderer_->DrawFrame(
            *context_, *swapchain_, *pipeline_, *commandManager_,
            vertexBuffer_->GetHandle(), 3  // 3 vertices for the triangle
        );

        if (needsRecreation || framebufferResized_) {
            framebufferResized_ = false;
            recreateSwapchain();
        }
    }

    context_->Device().waitIdle();
}

// ---------------------------------------------------------------------------
// recreateSwapchain
// ---------------------------------------------------------------------------

void App::recreateSwapchain() {
    // Wait for minimization to end
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    context_->Device().waitIdle();
    swapchain_->Recreate(*context_, window_);
    renderer_->RecreateFramebuffers(*context_, *swapchain_, *pipeline_);
}

// ---------------------------------------------------------------------------
// framebufferResizeCallback
// ---------------------------------------------------------------------------

void App::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->framebufferResized_ = true;
    }
}
