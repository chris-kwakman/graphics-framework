#include "inertia.hpp"
#include "convex_hull.h"

namespace Engine {
namespace Physics {

    glm::mat3 inertialTensorConvexHull(convex_hull const* _hull, float * _out_mass, glm::vec3 * _out_cm)
    {
        if (!_hull)
            return glm::mat3(1.0f);

        std::vector<std::array<glm::vec3, 3>> hull_triangles;

        decltype(hull_triangles)::value_type arr;
        for (convex_hull::face const& face : _hull->m_faces)
        {
            for (size_t i = 1; i < face.m_vertices.size() - 2; i++)
            {
                arr = {
                    _hull->m_vertices[face.m_vertices[0]],
                    _hull->m_vertices[face.m_vertices[i]],
                    _hull->m_vertices[face.m_vertices[i + 1]]
                };
                hull_triangles.emplace_back(std::move(arr));
            }
            arr = { 
                _hull->m_vertices[face.m_vertices[0]],
                _hull->m_vertices[face.m_vertices[face.m_vertices.size() - 2]],
                _hull->m_vertices[face.m_vertices[face.m_vertices.size() - 1]]
            };
            hull_triangles.emplace_back(std::move(arr));
        }
        return inertiaTensorTriangles(hull_triangles, _out_mass, _out_cm);
    }

    glm::mat3 inertiaTensorTriangles(
        const std::vector < std::array<glm::vec3,3> > & triangles,
        float* out_mass, glm::vec3* out_cm
    )
    {
        struct macro_result
        {
            macro_result(glm::vec3 wi)
            {
                float temp0 = wi[0] + wi[1];
                float temp1 = wi[0] * wi[0];
                float temp2 = temp1 + wi[1] * temp0;
                f1 = temp0 + wi[2];
                f2 = temp2 + wi[2]*f1;
                f3 = wi[0] * temp1 + wi[1] * temp2 + wi[2] * f2;
                g0 = f2 + wi[0] * (f1 + wi[0]);
                g1 = f2 + wi[1] * (f1 + wi[1]);
                g2 = f2 + wi[2] * (f1 + wi[2]);
            }
            float f1, f2, f3, g0, g1, g2;
        };

        union accumulator
        {
            float integrals[10] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            struct
            {
                float accum_int_x;       // Mass Accumulator
                float accum_int_x2;      // Momenta X
                float accum_int_y2;      // Momenta Y
                float accum_int_z2;      // Momenta Z
                float accum_int_x3;
                float accum_int_y3;
                float accum_int_z3;
                float accum_int_x2y;
                float accum_int_y2z;
                float accum_int_z2x;
            };
        };

        accumulator a;

        float const mult[10] = { 1.0f / 6.0f, 1.0f / 24.0f, 1.0f / 24.0f, 1.0f / 24.0f, 1.0f / 60.0f,1.0f / 60.0f ,1.0f / 60.0f, 1.0f / 120.0f,1.0f / 120.0f ,1.0f / 120.0f };

        for (std::array<glm::vec3,3> const & tri : triangles)
        {
            glm::vec3 const E1 = tri[1] - tri[0];
            glm::vec3 const E2 = tri[2] - tri[0];
            glm::vec3 const deltas = glm::cross(E1, E2);

            macro_result const macro_x(glm::vec3(tri[0].x, tri[1].x, tri[2].x));
            macro_result const macro_y(glm::vec3(tri[0].y, tri[1].y, tri[2].y));
            macro_result const macro_z(glm::vec3(tri[0].z, tri[1].z, tri[2].z));
            
            a.accum_int_x += deltas[0] * macro_x.f1;
            a.accum_int_x2 += deltas[0] * macro_x.f2;
            a.accum_int_y2 += deltas[1] * macro_y.f2;
            a.accum_int_z2 += deltas[2] * macro_z.f2;
            a.accum_int_x3 += deltas[0] * macro_x.f3;
            a.accum_int_y3 += deltas[1] * macro_y.f3;
            a.accum_int_z3 += deltas[2] * macro_z.f3;
            a.accum_int_x2y += deltas[0] * (tri[0].y * macro_x.g0 + tri[1].y * macro_x.g1 + tri[2].y * macro_x.g2);
            a.accum_int_y2z += deltas[1] * (tri[0].z * macro_y.g0 + tri[1].z * macro_y.g1 + tri[2].z * macro_y.g2);
            a.accum_int_z2x += deltas[2] * (tri[0].x * macro_z.g0 + tri[1].x * macro_z.g1 + tri[2].x * macro_z.g2);
        }

        for (size_t i = 0; i < 10; i++)
            a.integrals[i] *= mult[i];

        float const mass = a.accum_int_x;
        glm::vec3 const cm = glm::vec3(a.accum_int_x2, a.accum_int_y2, a.accum_int_z2) / mass;

        float const I_xx = (a.accum_int_y3 + a.accum_int_z3) - mass * (cm.y * cm.y + cm.z * cm.z);
        float const I_yy = (a.accum_int_x3 + a.accum_int_z3) - mass * (cm.x * cm.x + cm.z * cm.z);
        float const I_zz = (a.accum_int_x3 + a.accum_int_y3) - mass * (cm.x * cm.x + cm.y * cm.y);
        float const I_xy = (a.accum_int_x2y) - mass * (cm.x * cm.y);
        float const I_yz = (a.accum_int_y2z) - mass * (cm.y * cm.z);
        float const I_xz = (a.accum_int_z2x) - mass * (cm.x * cm.z);

        *out_mass = mass;
        *out_cm = cm;

        return glm::mat3(
            I_xx, -I_xy, -I_xz,
            -I_xy, I_yy, -I_yz,
            -I_xz, -I_yz, I_zz
        );
    }
}
}