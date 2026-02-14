#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

// Storage for a 3D Gaussian Splatting model loaded from PLY file.
// Based on the INRIA 3DGS format.
struct SplatSet
{
    std::vector<float> positions; // x,y,z per splat (3 floats)
    std::vector<float> f_dc;     // base color per splat (f_dc_0, f_dc_1, f_dc_2 — 3 floats)
    std::vector<float> f_rest;   // SH coefficients per splat (f_rest_0..f_rest_44 — up to 45 floats)
    std::vector<float> opacity;  // 1 float per splat
    std::vector<float> scale;    // 3 floats per splat (scale_0, scale_1, scale_2)
    std::vector<float> rotation; // quaternion per splat (rot_0..rot_3 — 4 floats)

    size_t size() const { return positions.size() / 3; }

    // Returns max SH degree (0-3), or -1 if empty
    int32_t maxShDegree() const
    {
        const size_t splatCount = size();
        if (splatCount == 0)
            return -1;
        const size_t totalSHComponents    = f_rest.size() / splatCount;
        const size_t shCoeffsPerChannel   = totalSHComponents / 3;
        if (shCoeffsPerChannel >= 15) return 3;
        if (shCoeffsPerChannel >= 8)  return 2;
        if (shCoeffsPerChannel >= 3)  return 1;
        return 0;
    }

    // Convert from RDF (Right-Down-Forward) to RUB (Right-Up-Back) coordinate system.
    // PLY files from INRIA 3DGS training use RDF; Vulkan typically uses RUB.
    // Flips Y and Z axes for positions, quaternion components, and SH coefficients.
    void convertRdfToRub()
    {
        // Flip Y and Z for positions
        for (size_t i = 0; i < positions.size(); i += 3)
        {
            positions[i + 1] = -positions[i + 1]; // flip Y
            positions[i + 2] = -positions[i + 2]; // flip Z
        }

        // Flip quaternion Y and Z components (index 0 is scalar w, leave it)
        for (size_t i = 0; i < rotation.size(); i += 4)
        {
            rotation[i + 2] = -rotation[i + 2]; // flip qY
            rotation[i + 3] = -rotation[i + 3]; // flip qZ
        }

        // Flip SH coefficients referencing Y and Z axes.
        // Derived from spz::coordinateConverter(RDF, RUB) where x=1, y=-1, z=-1:
        //   [0]=y, [1]=z, [2]=x, [3]=xy, [4]=yz, [5]=1, [6]=xz, [7]=1,
        //   [8]=y, [9]=xyz, [10]=y, [11]=z, [12]=x, [13]=z, [14]=x
        static constexpr float shFlip[15] = {
            -1.0f, -1.0f,  1.0f,                          // degree 1: y, z, x
            -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,            // degree 2: xy, yz, 1, xz, 1
            -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f  // degree 3: y, xyz, y, z, x, z, x
        };

        const size_t numPoints = size();
        if (numPoints == 0 || f_rest.empty()) return;

        const size_t numCoeffs         = f_rest.size() / 3;
        const size_t numCoeffsPerPoint = numCoeffs / numPoints;

        size_t idx = 0;
        for (size_t i = 0; i < numPoints; ++i)
        {
            for (size_t j = 0; j < numCoeffsPerPoint && j < 15; ++j)
            {
                const float flip = shFlip[j];
                f_rest[idx + j]                          *= flip; // R
                f_rest[idx + numCoeffsPerPoint + j]      *= flip; // G
                f_rest[idx + numCoeffsPerPoint * 2 + j]  *= flip; // B
            }
            idx += 3 * numCoeffsPerPoint;
        }
    }
};
