#ifndef ENGINE_GRAPHICS_CAMERA_DATA_H
#define ENGINE_GRAPHICS_CAMERA_DATA_H

#include <glm/mat4x4.hpp>
#include <Engine/Serialisation/common.h>

namespace Engine {
	namespace Graphics {

		struct camera_data
		{
			float m_aspect_ratio;
			float m_near;
			float m_far;

			union
			{
				float m_camera_type_value;
				struct
				{
					unsigned int m_value_bits : 31;
					bool m_is_orthogonal_camera : 1;
				};
			};

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(camera_data, m_aspect_ratio, m_near, m_far, m_camera_type_value);

			float get_vertical_fov() const { return m_camera_type_value; }
			float get_orthogonal_width() const { return m_camera_type_value; }
			float get_clipping_depth(float _world_depth) const;
			float get_world_depth(float _ndc_depth) const;
			void set_vertical_fov(float _value) { m_camera_type_value = _value; m_is_orthogonal_camera = false; }
			void set_orthogonal_width(float _value) { m_camera_type_value = _value; m_is_orthogonal_camera = true; }
			bool is_orthogonal_camera() const { return m_is_orthogonal_camera; }
			glm::mat4 get_perspective_matrix() const;
			glm::mat4 get_orthogonal_matrix() const;

		};

	}
}
#endif // !ENGINE_GRAPHICS_CAMERA_DATA_H
