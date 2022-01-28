#ifndef ENGINE_TRANSFORM_H
#define ENGINE_TRANSFORM_H

#include <Engine/Serialisation/common.h>

namespace Engine
{
	namespace Math
	{

		struct transform3D
		{
			glm::vec3 position{ 0.0f,0.0f,0.0f };
			glm::vec3 scale{ 1.0f,1.0f,1.0f };
			glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(transform3D, position, scale, rotation)

			transform3D GetInverse() const;

			glm::mat4x4 GetMatrix() const;
			glm::mat4x4 GetInvMatrix() const { return GetInverse().GetMatrix(); }

			transform3D operator*(transform3D _other) const;
			transform3D operator*=(transform3D _other);

			glm::vec3 TransformPoint(glm::vec3 _v) const;
			glm::vec4 TransformPoint(glm::vec4 _v) const { return glm::vec4(TransformPoint(glm::vec3(_v)), 0.0f); }
			glm::vec3 TransformVector(glm::vec3 _v) const;
			glm::vec4 TransformVector(glm::vec4 _v) const { return glm::vec4(TransformVector(glm::vec3(_v)), 1.0f); }

		private:

			static transform3D concatenate(transform3D _l, transform3D _r);
		};
	}
}

#endif // !ENGINE_TRANFORM_H
