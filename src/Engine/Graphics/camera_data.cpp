#include "camera_data.h"
#include <glm/gtc/matrix_transform.hpp>

float Engine::Graphics::camera_data::get_clipping_depth(float _view_depth) const
{
    return ((m_far + m_near) / (m_far - m_near)) + (2.0f * m_far * m_near / (-_view_depth * (m_far - m_near)));
}

float Engine::Graphics::camera_data::get_world_depth(float _ndc_depth) const
{
    float const diff_inv = 1.0f / (m_far - m_near);
    float const A = -(m_far + m_near) * diff_inv;
    float const B = -2 * m_far * m_near * diff_inv;
    return B / (A + _ndc_depth);
}

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
