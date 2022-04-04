#ifndef INERTIA_HPP
#define INERTIA_HPP

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <array>

namespace Engine {
namespace Physics {

    struct half_edge_data_structure;

    constexpr inline glm::mat3 inertiaTensorBox(float m, float w, float h, float d)
    {
        float c = (1.0f / 12.0f) * m;
        float w_sqr = w * w;
        float h_sqr = h * h;
        float d_sqr = d * d;
        return { { c * (h_sqr + d_sqr), 0, 0 },
                    { 0, c * (w_sqr + d_sqr), 0 },
                    { 0, 0, c * (w_sqr + h_sqr) } };
    }

    constexpr inline glm::mat3 inertiaTensorCylinder(float m, float r, float h)
    {
        float r2 = r * r;
        float h2 = h * h;
        float c = (1.0f / 12.0f) * m * (3 * r2 + h2);
        return { { c, 0, 0 },
                    { 0, 0.5f * m * r2, 0 },
                    { 0, 0, c } };
    }

    constexpr inline glm::mat3 inertiaTensorCone(float m, float r, float h)
    {
        float r2 = r * r;
        float h2 = h * h;
        float c = (3.0f / 5.0f) * m * h2 + 3.0f / 20.0f * m * r2;
        return { { c, 0, 0 },
                    { 0, 3.0f / 10.0f * m * r2, 0 },
                    { 0, 0, c } };
    }

    constexpr inline glm::mat3 inertiaTensorSphere(float m, float r)
    {
        float r2 = r * r;
        float c = (2.0f / 5.0f) * m * r2;
        return { { c, 0, 0 },
                    { 0, c, 0 },
                    { 0, 0, c } };
    }

    glm::mat3 inertialTensorConvexHull(half_edge_data_structure const* _hds, float* _out_mass, glm::vec3* _out_cm);
    glm::mat3 inertiaTensorTriangles(const std::vector<std::array<glm::vec3,3>>& triangles, float* out_mass, glm::vec3* out_cm);
}
}
#endif // INERTIA_HPP
