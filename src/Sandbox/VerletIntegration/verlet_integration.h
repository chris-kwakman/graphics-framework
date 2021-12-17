#ifndef SANDBOX_VERLETINTEGRATION_H
#define SANDBOX_VERLETINTEGRATION_H

#include <glm/vec3.hpp>
#include <vector>
#include <memory>
#include <Engine/Components/Transform.h>

namespace Sandbox {
namespace VerletIntegration {

	void verlet_integration(
		glm::vec3* _updateable_positions,
		glm::vec3* _previous_positions,
		glm::vec3 const* _accelerations,
		size_t _count,
		float _dt,
		float _drag = 0.99f
	);

	void verlet_set_value(
		glm::vec3* _accelerations,
		size_t _count,
		glm::vec3 _value
	);

	struct IConstraint
	{
		virtual void solve_constraint(
			glm::vec3 *	const	_positions,
			glm::vec3 *	const	_accelerations, 
			size_t				_count, 
			float				_dt
		) = 0;
	};
	struct Link
	{
		Link() = default;
		Link(unsigned int _p1, unsigned int _p2) : P1(_p1), P2(_p2) {}
		unsigned int P1;
		unsigned int P2;
	};

	/*
	* Constraint solver for creating spring effect between links
	*/
	struct SpringConstraint : public IConstraint
	{
		SpringConstraint(
			std::vector<Link> const& _links,
			float _stiffness,
			float _restLength
		);

		std::vector<Link> m_links;
		size_t	m_expired_links;
		float	m_rest_length;
		float	m_stiffness;


		// Inherited via IConstraint
		virtual void solve_constraint(
			glm::vec3* const _positions,
			glm::vec3* const _accelerations, 
			size_t _count, 
			float _dt
		) override;
	};
	struct StaticConstraint : public IConstraint
	{
		StaticConstraint(
			std::vector<unsigned int> const& _indices, 
			glm::vec3 const* _positions
		);

		// Inherited via IConstraint
		virtual void solve_constraint(glm::vec3* const _positions, glm::vec3* const _accelerations, size_t _count, float _dt) override;
		std::vector<unsigned int> m_static_particle_indices;
		std::vector<glm::vec3> m_particle_positions;
	};

	struct SphereCollisionConstraint : public IConstraint
	{
		SphereCollisionConstraint(Component::Transform _collider_transform);

		Component::Transform m_collider_transform;

		// Inherited via IConstraint
		virtual void solve_constraint(glm::vec3* const _positions, glm::vec3* const _accelerations, size_t _count, float _dt) override;
	};

	struct particle_system
	{
		~particle_system();
		void allocate_particles(glm::vec3* _positions, size_t _count);

		void solve_constraints(float _dt);

		glm::vec3* const current_positions() const { return m_current_positions; }
		glm::vec3* const previous_positions() const { return m_previous_positions; }
		glm::vec3* const accelerations() const { return m_accelerations; }
		size_t particle_count() const { return m_particle_count; }

		std::vector<std::unique_ptr<IConstraint>> m_constraints;

	private:

		size_t m_particle_count;

		// Stores ptr to particle memory block
		void * m_arr_values;

		glm::vec3* m_current_positions;
		glm::vec3* m_previous_positions;
		glm::vec3* m_accelerations;
	};

}
}

#endif // !SANDBOX_VERLETINTEGRATION_H
