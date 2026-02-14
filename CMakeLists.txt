cmake_minimum_required(VERSION 4.1)

project(GaussianSplatting)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

# Shader compilation
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/Shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADER_SOURCES
    ${SHADER_DIR}/triangle.vert
    ${SHADER_DIR}/triangle.frag
)

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_SPV ${SHADER_OUT_DIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SHADER_SPV}
        COMMAND ${GLSLC} ${SHADER} -o ${SHADER_SPV}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER_NAME}"
    )
    list(APPEND SHADER_SPV_FILES ${SHADER_SPV})
endforeach()

add_custom_target(Shaders DEPENDS ${SHADER_SPV_FILES})

add_executable(GaussianSplatting
    main.cpp
    App/App.cpp
    Vulkan/Context.cpp
    Vulkan/Swapchain.cpp
    Vulkan/Pipeline.cpp
    Vulkan/Buffer.cpp
    Vulkan/CommandManager.cpp
    Vulkan/Renderer.cpp
    Loader/PlyLoader.cpp
    3rdparty/miniply/miniply.cpp
)

add_dependencies(GaussianSplatting Shaders)

target_include_directories(GaussianSplatting PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Header
    ${CMAKE_CURRENT_SOURCE_DIR}/Loader
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/miniply
)
target_link_libraries(GaussianSplatting PRIVATE
    glm::glm
    Vulkan::Vulkan
    glfw
    GPUOpen::VulkanMemoryAllocator
)
