#pragma once

#include <filesystem>
#include <string>
#include "SplatSet.h"

// Synchronous PLY loader for 3D Gaussian Splatting files.
// Uses miniply library (MIT license) for parsing.
//
// Usage:
//   SplatSet splats;
//   if (loadPly("scene.ply", splats)) {
//       // splats.size() returns number of Gaussians
//   }
bool loadPly(const std::filesystem::path& filename, SplatSet& output, bool convertToRub = true);
