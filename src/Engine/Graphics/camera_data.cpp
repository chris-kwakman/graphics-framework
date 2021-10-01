#include "camera_data.h"
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Engine::Graphics::camera_data::get_perspective_matrix() const
{
    return glm::perspective(m_fov_y, m_aspect_ratio, m_near, m_far);
}
