cmake_minimum_required(VERSION 4.1)


project(GaussianSplatting)

set(CMAKE_CXX_STANDARD 20)

find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(GaussianSplatting main.cpp)

target_link_libraries(GaussianSplatting PRIVATE glm::glm)
target_link_libraries(GaussianSplatting PRIVATE Vulkan::Vulkan)