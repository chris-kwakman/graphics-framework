#include <gtest/gtest.h>
#include <engine/Physics/integration.h>
#include <glm/gtx/string_cast.hpp>

using namespace Engine::Physics;

#define EXPECT_VEC3_EQ(v1,v2,eps) EXPECT_TRUE(AttributeEquals(v1,v2,eps))

::testing::AssertionResult AttributeEquals(glm::vec3 _v1, glm::vec3 _v2, float _epsilon) {
	bool result = glm::all(glm::epsilonEqual(_v1, _v2, _epsilon));

	if (result) {
		return ::testing::AssertionSuccess();
	}
	else {
		return ::testing::AssertionFailure() << glm::to_string(_v1) << " not equal to " << glm::to_string(_v2);
	}
}

TEST(RigidBody, LinearIntegration_ForceFree)
{
	glm::vec3 positions[] = { glm::vec3(0.0f), glm::vec3(0.0f) };
	glm::vec3 velocities[] = { glm::vec3(0.0f), glm::vec3(1.0f) };
	glm::vec3 const forces[] = { glm::vec3(0.0f), glm::vec3(0.0f) };
	float const inv_masses[] = { 1.0f, 1.0f };

	integrate_linear(1.0f, positions, velocities, forces, inv_masses, 2);

	EXPECT_VEC3_EQ(positions[0], glm::vec3(0.0f), 0.00001f);
	EXPECT_VEC3_EQ(positions[1], glm::vec3(1.0f), 0.00001f);
}

TEST(RigidBody, LinearIntegration_Force)
{
	glm::vec3 positions[] = { glm::vec3(0.0f), glm::vec3(0.0f) };
	glm::vec3 velocities[] = { glm::vec3(0.0f), glm::vec3(1.0f) };
	glm::vec3 const forces[] = { glm::vec3(1.0f), glm::vec3(1.0f) };
	float const inv_masses[] = { 1.0f, 1.0f };

	integrate_linear(1.0f, positions, velocities, forces, inv_masses, 2);

	EXPECT_VEC3_EQ(positions[0], glm::vec3(1.0f), 0.00001f);
	EXPECT_VEC3_EQ(positions[1], glm::vec3(2.0f), 0.00001f);
}

TEST(RigidBody, AngularIntegration_ForceFree)
{
	glm::quat rotations[] = {
		glm::normalize(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
		glm::normalize(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
	};

	glm::vec3 const ROT_DEFAULT = { 0.0f, 0.0f, -1.0f };

	glm::vec3 angular_velocities[] = { glm::vec3(0.0f), glm::vec3(3.1415f, 0.0f, 0.0f) };

	//integrate_angular(1.0f, rotations, angular_velocities, 2);
	//for (size_t i = 0; i < sizeof(rotations) / sizeof(glm::quat); i++)
	//	rotations[i] = glm::normalize(rotations[i]);
	//
	//EXPECT_VEC3_EQ(ROT_DEFAULT * rotations[0], glm::vec3(0.0f, 0.0f, -1.0f), 0.0001f);
	//EXPECT_VEC3_EQ(ROT_DEFAULT * rotations[1], glm::vec3(0.0f, 0.0f, 1.0f), 0.0001f);
	
}