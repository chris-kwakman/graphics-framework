#include "camera.h"

#include <glm/gtc/quaternion.hpp>

namespace Engine {
namespace Graphics {

	glm::quat camera::get_lookat_quat() const
	{
		return glm::quatLookAt(glm::normalize(glm::cross(m_up, m_right)), m_up);
	}
	glm::mat4x4 camera::create_view_to_perspective_matrix(float _fov, float _aspect, float _near, float _far) const
	{
		return glm::perspective(_fov, _aspect, _near, _far);
	}

}
}