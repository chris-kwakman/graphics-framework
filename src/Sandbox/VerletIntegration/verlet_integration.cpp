#include "verlet_integration.h"
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <deque>

namespace Sandbox {
namespace VerletIntegration {

	/*
	* Performs verlet integration on vectors of current positions, previous positions, accelerations
	* @param	glm::vec3[]			Array of current positions that will be updated
	* @param	glm::vec3[]			Array of previous positions that will be updated
	* @param	glm::vec3[]	const 	Array of accelerations
	* @param	size_t				Number of elements in vectors
	* @param	float				Delta time parameter
	* @param	float				Drag coefficient, small hack
	*/
	void verlet_integration(
		glm::vec3*			_updateable_positions, 
		glm::vec3*			_previous_positions, 
		glm::vec3 const*	_accelerations, 
		size_t				_count, 
		float				_dt,
		float				_drag
	)
	{
		float const dt2 = _dt * _dt;
		for (size_t i = 0; i < _count; ++i)
		{
			glm::vec3 const temp = _updateable_positions[i];
			_updateable_positions[i] += (_updateable_positions[i] - _previous_positions[i])*_drag + _accelerations[i] * dt2;
			_previous_positions[i] = temp;
		}
	}

	/*
	* Resets input acceleration vector to given value
	* @param	glm::vec3[]		Vector of accelerations to reset
	* @param	glm::vec3		Default value (i.e. gravity)
	*/
	void verlet_set_value(glm::vec3* _accelerations, size_t _count, glm::vec3 _value)
	{
		for (size_t i = 0; i < _count; ++i)
			_accelerations[i] = _value;
	}


	particle_system::~particle_system()
	{
		delete[] m_arr_values;
		m_current_positions = nullptr;
		m_previous_positions = nullptr;
		m_accelerations = nullptr;
	}

	void particle_system::allocate_particles(glm::vec3* _positions, size_t _count)
	{
		if (m_arr_values)
		{
			delete[] m_arr_values;
			m_current_positions = nullptr;
			m_previous_positions = nullptr;
			m_accelerations = nullptr;
		}
		m_arr_values = new glm::vec3[sizeof(glm::vec3) * _count * 3];
		m_current_positions = (glm::vec3*)m_arr_values;
		m_previous_positions = m_current_positions + _count;
		m_accelerations = m_previous_positions + _count;

		memcpy(m_current_positions, _positions, _count * sizeof(glm::vec3));
		memcpy(m_previous_positions, m_current_positions, _count * sizeof(glm::vec3));
		m_particle_count = _count;
	}

	void particle_system::solve_constraints(float _dt)
	{
		for (unsigned int i = 0; i < m_constraints.size(); ++i)
		{
			m_constraints[i]->solve_constraint(
				m_current_positions,
				m_accelerations,
				m_particle_count,
				_dt
			);
		}
	}

	

	SpringConstraint::SpringConstraint(
		std::vector<Link> const& _links, 
		float _stiffness, 
		float _restLength
	) :
		m_links(_links)
	{
		m_stiffness = glm::clamp(_stiffness, 0.0f, 1.0f);
		m_expired_links = 0;
		m_rest_length = _restLength;
	}

	void SpringConstraint::solve_constraint(
		glm::vec3* const _positions,
		glm::vec3* const _accelerations, 
		size_t _count, 
		float _dt
	)
	{
		float const restlength_2 = m_rest_length * m_rest_length;
		for (size_t i = 0; i < m_links.size() - m_expired_links; ++i)
		{
			Link const & current_link = m_links[i];
			glm::vec3 & a = _positions[current_link.P1];
			glm::vec3 & b = _positions[current_link.P2];
			glm::vec3 delta = b - a;
			float const deltaLength = glm::length(delta);
			float const diff = (deltaLength - m_rest_length) / deltaLength;
			a += delta * 0.5f * diff;
			b -= delta * 0.5f * diff;
		}
	}

	StaticConstraint::StaticConstraint(std::vector<unsigned int> const& _indices, glm::vec3 const* _positions)
	{
		m_static_particle_indices = _indices;
		m_particle_positions.resize(_indices.size());
		for (unsigned int i = 0; i < m_static_particle_indices.size(); ++i)
			m_particle_positions[i] = _positions[m_static_particle_indices[i]];
	}

	void StaticConstraint::solve_constraint(glm::vec3* const _positions, glm::vec3* const _accelerations, size_t _count, float _dt)
	{
		for (unsigned int i = 0; i < m_static_particle_indices.size(); ++i)
			_positions[m_static_particle_indices[i]] = m_particle_positions[i];
	}

	

	SphereCollisionConstraint::SphereCollisionConstraint(Component::Transform _collider_transform)
		: m_collider_transform(_collider_transform)
	{}

	void SphereCollisionConstraint::solve_constraint(
		glm::vec3* const _positions, 
		glm::vec3* const _accelerations, 
		size_t _count, 
		float _dt
	)
	{
		// Do multiple iterations
		Engine::Math::transform3D collider_world_transform = m_collider_transform.ComputeWorldTransform();
		glm::vec3 const sphere_world_pos = collider_world_transform.position;
		glm::vec3 const sphere_world_scale = collider_world_transform.scale;
		float const SPHERE_RADIUS = sphere_world_scale.x * 0.5f;
		float const RADIUS_2 = SPHERE_RADIUS * SPHERE_RADIUS;
		for (unsigned int i = 0; i < _count; ++i)
		{
			glm::vec3 delta = _positions[i] - sphere_world_pos;
			if (glm::dot(delta,delta) < RADIUS_2)
				_positions[i] = SPHERE_RADIUS * glm::normalize(delta) + sphere_world_pos;
		}
	}

}
}