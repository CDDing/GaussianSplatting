#include "App.h"
#include "../Vulkan/ProjectionPass.h"
#include "../Vulkan/SortPass.h"
#include "../Vulkan/RasterPass.h"

// Gaussian2D struct size in std430: 48 bytes per element
static constexpr vk::DeviceSize GAUSSIAN_2D_STRIDE = 48;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

App::App(uint32_t width, uint32_t height, const char* title) {
    initWindow(width, height, title);
    initVulkan();
}

App::~App() {
    if (context_) {
        context_->Device().waitIdle();
    }

    // Destroy in reverse dependency order
    renderer_.reset();

    // Passes (각 pass가 자기 pipeline + descriptor 소유)
    projPass_.reset();
    sortPass_.reset();
    rastPass_.reset();

    // Per-frame output buffers
    for (auto& buf : projected2DBuffers_) buf.reset();
    for (auto& buf : visibilityBuffers_) buf.reset();
    for (auto& buf : tileCountBuffers_) buf.reset();

    // UBO buffers
    for (auto& buf : uboStaging_) buf.reset();
    for (auto& buf : uboDevice_) buf.reset();

    commandManager_.reset();

    // Input buffers
    positionBuffer_.reset();
    shBuffer_.reset();
    opacityBuffer_.reset();
    scaleBuffer_.reset();
    rotationBuffer_.reset();

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
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
}

// ---------------------------------------------------------------------------
// initVulkan
// ---------------------------------------------------------------------------

void App::initVulkan() {
    context_        = std::make_unique<Context>(window_);
    swapchain_      = std::make_unique<Swapchain>(*context_, window_);
    pipeline_       = std::make_unique<Pipeline>(*context_, *swapchain_);

    for (uint32_t i = 0; i < CommandManager::FRAMES_IN_FLIGHT; i++) {
        uboStaging_[i] = std::make_unique<Buffer>(
            Buffer::CreateHostVisible(*context_,
                vk::BufferUsageFlagBits::eTransferSrc,
                sizeof(CameraUBOData)));
        uboDevice_[i] = std::make_unique<Buffer>(
            Buffer::CreateDeviceLocal(*context_,
                vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
                sizeof(CameraUBOData)));
    }
    camera_.SetScreenSize(swapchain_->GetExtent().width, swapchain_->GetExtent().height);

    // ─── 3 compute passes (각 pass가 자기 pipeline 소유) ───
    projPass_ = std::make_unique<ProjectionPass>(
        *context_, "Shaders/proj.comp.spv", CommandManager::FRAMES_IN_FLIGHT);
    sortPass_ = std::make_unique<SortPass>(*context_, "Shaders/sort.comp.spv");
    rastPass_ = std::make_unique<RasterPass>(*context_, "Shaders/rast.comp.spv");

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

void App::InitializePLY(const char* filename)
{
    auto splatSet = std::make_unique<SplatSet>();
    if (!loadPly(filename, *splatSet)) {
        std::cerr << "Failed to load PLY: " << filename << std::endl;
        return;
    }

    gaussianCount_ = static_cast<uint32_t>(splatSet->size());

    // ─── SOA 입력 버퍼 업로드 ───
    positionBuffer_ = std::make_unique<Buffer>(
        Buffer::CreateDeviceLocal(*context_,
            vk::BufferUsageFlagBits::eStorageBuffer,
            sizeof(float) * splatSet->positions.size(),
            splatSet->positions.data()));

    shBuffer_ = std::make_unique<Buffer>(
        Buffer::CreateDeviceLocal(*context_,
            vk::BufferUsageFlagBits::eStorageBuffer,
            sizeof(float) * splatSet->f_dc.size(),
            splatSet->f_dc.data()));

    opacityBuffer_ = std::make_unique<Buffer>(
        Buffer::CreateDeviceLocal(*context_,
            vk::BufferUsageFlagBits::eStorageBuffer,
            sizeof(float) * splatSet->opacity.size(),
            splatSet->opacity.data()));

    scaleBuffer_ = std::make_unique<Buffer>(
        Buffer::CreateDeviceLocal(*context_,
            vk::BufferUsageFlagBits::eStorageBuffer,
            sizeof(float) * splatSet->scale.size(),
            splatSet->scale.data()));

    rotationBuffer_ = std::make_unique<Buffer>(
        Buffer::CreateDeviceLocal(*context_,
            vk::BufferUsageFlagBits::eStorageBuffer,
            sizeof(float) * splatSet->rotation.size(),
            splatSet->rotation.data()));

    // ─── Per-frame 출력 버퍼 (빈 device-local) ───
    for (uint32_t i = 0; i < CommandManager::FRAMES_IN_FLIGHT; i++) {
        projected2DBuffers_[i] = std::make_unique<Buffer>(
            Buffer::CreateDeviceLocal(*context_,
                vk::BufferUsageFlagBits::eStorageBuffer,
                GAUSSIAN_2D_STRIDE * gaussianCount_));

        visibilityBuffers_[i] = std::make_unique<Buffer>(
            Buffer::CreateDeviceLocal(*context_,
                vk::BufferUsageFlagBits::eStorageBuffer,
                sizeof(uint32_t) * gaussianCount_));

        tileCountBuffers_[i] = std::make_unique<Buffer>(
            Buffer::CreateDeviceLocal(*context_,
                vk::BufferUsageFlagBits::eStorageBuffer,
                sizeof(uint32_t) * gaussianCount_));
    }

    // ─── Per-frame descriptor update ───
    for (uint32_t i = 0; i < CommandManager::FRAMES_IN_FLIGHT; i++) {
        ProjectionPass::Buffers buffers{
            positionBuffer_->GetHandle(),
            shBuffer_->GetHandle(),
            opacityBuffer_->GetHandle(),
            scaleBuffer_->GetHandle(),
            rotationBuffer_->GetHandle(),
            projected2DBuffers_[i]->GetHandle(),
            visibilityBuffers_[i]->GetHandle(),
            tileCountBuffers_[i]->GetHandle(),
        };
        ProjectionPass::BufferSizes sizes{
            positionBuffer_->GetSize(),
            shBuffer_->GetSize(),
            opacityBuffer_->GetSize(),
            scaleBuffer_->GetSize(),
            rotationBuffer_->GetSize(),
            projected2DBuffers_[i]->GetSize(),
            visibilityBuffers_[i]->GetSize(),
            tileCountBuffers_[i]->GetSize(),
        };
        projPass_->UpdateDescriptors(*context_, i,
                                     uboDevice_[i]->GetHandle(),
                                     uboDevice_[i]->GetSize(),
                                     buffers, sizes);
    }

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

        // Wait for current frame's fence before writing UBO
        renderer_->WaitForCurrentFrame(*context_);

        // Update camera UBO for current frame (safe: fence guarantees GPU is done)
        uint32_t frameIdx = renderer_->GetCurrentFrame();
        CameraUBOData uboData = camera_.GetUBOData();
        uboStaging_[frameIdx]->Upload(&uboData, sizeof(uboData));

        // Set up projection pass for current frame
        if (gaussianCount_ > 0) {
            projPass_->SetFrameIndex(frameIdx);

            uint32_t tileWidth  = (swapchain_->GetExtent().width  + 15) / 16;
            uint32_t tileHeight = (swapchain_->GetExtent().height + 15) / 16;
            projPass_->SetPushConstants({gaussianCount_, tileWidth, tileHeight});
        }

        bool needsRecreation = renderer_->DrawFrame(
            *context_, *swapchain_, *pipeline_, *commandManager_,
            uboStaging_[frameIdx].get(), uboDevice_[frameIdx].get(),
            gaussianCount_ > 0 ? projPass_.get() : nullptr,
            sortPass_.get(), rastPass_.get()
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
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    context_->Device().waitIdle();
    swapchain_->Recreate(*context_, window_);
    renderer_->RecreateFramebuffers(*context_, *swapchain_, *pipeline_);
    camera_.SetScreenSize(swapchain_->GetExtent().width, swapchain_->GetExtent().height);
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

// ---------------------------------------------------------------------------
// mouseButtonCallback
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// cursorPosCallback
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// scrollCallback
// ---------------------------------------------------------------------------

void App::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    app->camera_.Zoom(static_cast<float>(yoffset) * 0.5f);
}
