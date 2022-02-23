#include <gtest/gtest.h>

#include <Engine/Physics/intersection.h>
#include <Engine/Physics/convex_hull_loader.h>

#include <Engine/Math/Transform3D.h>

#include <glm/gtx/transform.hpp>

using namespace Engine::Physics;
using namespace Engine::Math;

void LoadCube(half_edge_data_structure & shape)
{
    shape = Engine::Physics::ConstructConvexHull_OBJ("meshes/cube.obj");
}

void LoadConvexHull(half_edge_data_structure & shape)
{
    shape = Engine::Physics::ConstructConvexHull_OBJ("meshes/suzanne.obj", true);
}

TEST(Sat, cubeIntersectionFace)
{
    // Shape
    half_edge_data_structure shape;
    LoadCube(shape);

    transform3D lhs_m2w, rhs_m2w;
    lhs_m2w.rotation = glm::quat(glm::radians(glm::vec3({ 0, 0, 0 })));
    lhs_m2w.position = glm::vec3({ 0, 0, 0 });
    rhs_m2w.rotation = glm::quat(glm::radians(glm::vec3({ 0, 60, 0 })));
    rhs_m2w.position = glm::vec3({ 1,0,0 });

    auto result = intersect_convex_hulls_sat(shape, lhs_m2w, shape, rhs_m2w);
    ASSERT_TRUE(result.collision_type & ECollideType::eAnyCollision);
    ASSERT_TRUE(result.collision_type & ECollideType::eFaceCollision);
}

TEST(Sat, cubeIntersectionEdges)
{
    // Shape
    half_edge_data_structure shape;
    LoadCube(shape);

    transform3D lhs_m2w, rhs_m2w;
    lhs_m2w.rotation = glm::quat(glm::radians(glm::vec3({ 0, 0, 0 })));
    lhs_m2w.position = glm::vec3({ 0, 0, -2 });
    rhs_m2w.rotation = glm::quat(glm::radians(glm::vec3({ 45, 45, 0 })));
    rhs_m2w.position = glm::vec3({ 1, 0.75, -2 });

    auto result = intersect_convex_hulls_sat(shape, lhs_m2w, shape, rhs_m2w);
    ASSERT_TRUE(result.collision_type & ECollideType::eAnyCollision);
    ASSERT_TRUE(result.collision_type & ECollideType::eEdgeCollision);
}


TEST(Sat, convexHullIntersectionFace)
{
    // Shape
    half_edge_data_structure shape;
    LoadConvexHull(shape);

    transform3D lhs_m2w, rhs_m2w;
    lhs_m2w.rotation = glm::quat(glm::radians(glm::vec3({ 0, 0, 0 })));
    lhs_m2w.position = glm::vec3({ 0, 0, 0 });
    rhs_m2w.rotation = glm::quat(glm::radians(glm::vec3({ 0, 60, 0 })));
    rhs_m2w.position = glm::vec3({ 2.07,0,0 });

    auto result = intersect_convex_hulls_sat(shape, lhs_m2w, shape, rhs_m2w);
    ASSERT_TRUE(result.collision_type & ECollideType::eAnyCollision);
    ASSERT_TRUE(result.collision_type & ECollideType::eFaceCollision);
}

TEST(Sat, convexHullIntersectionEdges)
{
    // Shape
    half_edge_data_structure shape;
    LoadConvexHull(shape);

    transform3D lhs_m2w, rhs_m2w;
    lhs_m2w.rotation = glm::quat(glm::radians(glm::vec3({ 0, 0, 0 })));
    lhs_m2w.position = glm::vec3({ 0, 0, -2 });
    rhs_m2w.rotation = glm::quat(glm::radians(glm::vec3({ 45,45, 0 })));
    rhs_m2w.position = glm::vec3({ 1,0.75,-2 });

    auto result = intersect_convex_hulls_sat(shape, lhs_m2w, shape, rhs_m2w);
    ASSERT_TRUE(result.collision_type & ECollideType::eAnyCollision);
    ASSERT_TRUE(result.collision_type & ECollideType::eEdgeCollision);
}
