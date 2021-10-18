#include "camera_data.h"
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Engine::Graphics::camera_data::get_perspective_matrix() const
{
    assert(!is_orthogonal_camera());
    return glm::perspective(get_vertical_fov(), m_aspect_ratio, m_near, m_far);
}

glm::mat4 Engine::Graphics::camera_data::get_orthogonal_matrix() const
{
    assert(is_orthogonal_camera());
    float const ortho_width = m_camera_type_value;
    float const ortho_height = ortho_width / m_aspect_ratio;
    return glm::ortho(
        -ortho_width * 0.5f, ortho_width * 0.5f,
        -ortho_height * 0.5f, ortho_height * 0.5f,
        m_near, m_far
    );
}
