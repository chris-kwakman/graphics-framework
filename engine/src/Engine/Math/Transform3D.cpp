#include "Transform3D.h"
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Engine {
namespace Math
{

	transform3D transform3D::GetInverse() const
	{
		transform3D inv;
		inv.rotation = glm::inverse(rotation);
		inv.scale = 1.0f / scale;
		inv.position = inv.rotation * (inv.scale * -position);
		return inv;
	}

	glm::mat4x4 transform3D::GetMatrix() const
	{
		glm::vec3 const r0 = scale.x *(rotation * glm::vec3(1.0f, 0.0f, 0.0f));
		glm::vec3 const r1 = scale.y *(rotation * glm::vec3(0.0f, 1.0f, 0.0f));
		glm::vec3 const r2 = scale.z *(rotation * glm::vec3(0.0f, 0.0f, 1.0f));

		return glm::mat4x4(
			r0.x, r0.y, r0.z, 0.0f,
			r1.x, r1.y, r1.z, 0.0f,
			r2.x, r2.y, r2.z, 0.0f,
			position.x, position.y, position.z, 1.0f
		);
	}

	transform3D transform3D::operator*(transform3D _other) const
	{
		return concatenate(*this, _other);
	}

	transform3D transform3D::operator*=(transform3D _other)
	{
		*this = concatenate(*this, _other);
		return *this;
	}

	glm::vec3 transform3D::TransformPoint(glm::vec3 _v) const
	{
		return position + glm::vec3(rotation * (scale * _v));
	}

	glm::vec3 transform3D::TransformVector(glm::vec3 _v) const
	{
		glm::vec3 const temp = rotation * (scale * _v);
		return temp;
	}

	transform3D transform3D::concatenate(transform3D _l, transform3D _r)
	{
		transform3D new_transform;
		new_transform.rotation = _l.rotation * _r.rotation;
		new_transform.scale = _l.scale * _r.scale;
		new_transform.position = _l.position + _l.rotation * (_l.scale * _r.position);
		return new_transform;
	}
}
}