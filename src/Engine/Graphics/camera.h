#ifndef ENGINE_GRAPHICS_CAMERA_H
#define ENGINE_GRAPHICS_CAMERA_H

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine {
namespace Graphics {

	struct camera
	{
		glm::vec3 m_right, m_up;
		float m_fov_y;
		float m_aspect_ratio;
		float m_near;
		float m_far;

		glm::quat	get_lookat_quat() const;

		glm::mat4x4 create_view_to_perspective_matrix() const;
	};

}
}

#endif // !ENGINE_GRAPHICS_CAMERA_H
