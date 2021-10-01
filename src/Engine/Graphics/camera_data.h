#ifndef ENGINE_GRAPHICS_CAMERA_DATA_H
#define ENGINE_GRAPHICS_CAMERA_DATA_H

#include <glm/mat4x4.hpp>

namespace Engine {
	namespace Graphics {

		struct camera_data
		{
			float m_fov_y;
			float m_aspect_ratio;
			float m_near;
			float m_far;

			glm::mat4 get_perspective_matrix() const;

		};
	}
}
#endif // !ENGINE_GRAPHICS_CAMERA_DATA_H
