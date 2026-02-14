#pragma once

// C++, STL
#include <vector>
#include <iostream>
#include <memory>
#include <array>
#include <functional>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>

#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// VMA - forward declare, implementation in Context.cpp
#include <vk_mem_alloc.h>