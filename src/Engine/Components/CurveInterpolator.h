#ifndef COMPONENT_CURVEINTPERPOLATOR_H
#define COMPONENT_CURVEINTPERPOLATOR_H

#include <Engine/ECS/component_manager.h>

namespace Component
{
	using namespace Engine::ECS;

	glm::vec4 const COLOR_CURVE_LINE(1.0f, 1.0f, 0.0f, 1.0f);
	glm::vec4 const COLOR_NODE_POSITION(0.0f, 1.0f, 0.0f, 1.0f);
	glm::vec4 const COLOR_NODE_TANGENT(1.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 const COLOR_CURVE_LUT_POINT(0.5f, 0.5f, 0.5f, 1.0f);

	struct lookup_table
	{
		lookup_table() { m_resolution = 100; m_adaptive = false; }

		// Cumulative arclengths across whole curve.
		std::vector<float>		m_arclengths;
		// Normalized parameter corresponding to index is
		// (i / (N-1)) where N is the resolution of the curve
		// Sampled point of owning curve, corresponds to parameter.
		// If LUT is adaptive, store our own index-normalized parameter 'map'
		std::vector<float>		m_normalized_parameters;
		std::vector<glm::vec3>	m_points;

		union
		{
			unsigned int m_lut_metadata;
			struct
			{
				unsigned int m_resolution;
			};
			struct
			{
				union
				{
					float m_tolerance;
					struct
					{
						uint32_t m_tolerance_bits : 26;
						uint8_t m_max_subdivisions : 6;
					};
				};

			};
		};

		bool m_adaptive = false;

		float get_normalized_parameter(unsigned int _index) const;
		float compute_normalized_parameter(float _arclength) const;
	};

	struct piecewise_curve
	{
		enum EType { Linear, Hermite, Catmull, Bezier, COUNT };
		std::vector<glm::vec3>	m_nodes;
		lookup_table			m_lut;
		EType mutable			m_type;

		void set_curve_type(EType _type);

		glm::vec3 numerical_sample(float _param) const;
		glm::vec3 numerical_sample_linear(float _param) const;
		glm::vec3 numerical_sample_catmull(float _param) const;
		glm::vec3 numerical_sample_hermite(float _param) const;
		glm::vec3 numerical_sample_bezier(float _param) const;
	};

	class CurveInterpolatorManager;
	struct CurveInterpolator final : public IComp<CurveInterpolatorManager>
	{
		DECLARE_COMPONENT(CurveInterpolator);

		piecewise_curve const & GetPiecewiseCurve() const;
		// TODO: Separate into two different methods.
		void SetPiecewiseCurve(piecewise_curve _curve, unsigned int _resolution);
		void SetPiecewiseCurve(piecewise_curve _curve, float _tolerance, unsigned int _max_subdivisions);
	};
	class CurveInterpolatorManager final : public TCompManager<CurveInterpolator>
	{

		std::unordered_map<Entity, piecewise_curve, Entity::hash> m_map;

		std::unordered_set<Entity, Entity::hash> m_renderable_curves;
		std::unordered_set<Entity, Entity::hash> m_renderable_curve_nodes;
		std::unordered_set<Entity, Entity::hash> m_renderable_curve_lut;

		// Inherited via TCompManager
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) override;

		static void generate_curve_lut(piecewise_curve const* _curve, lookup_table* _lut, unsigned int _lut_resolution);
		static void generate_curve_lut_adaptive(piecewise_curve const* _curve, lookup_table* _lut, unsigned int _subdivisions, float _treshhold);

		static void rec_adaptive_forward_differencing(
			piecewise_curve const* _curve, 
			lookup_table* _lut,
			float _u_left, float _u_right,
			int _remaining_subdivisions, float _tolerance
		);

		friend struct CurveInterpolator;

	public:

		virtual const char* GetComponentTypeName() const override;

		// Used for debug rendering in graphics pipeline.
		decltype(m_renderable_curves) const& GetRenderableCurves() const;
		decltype(m_renderable_curve_nodes) const& GetRenderableCurveNodes() const;
		decltype(m_renderable_curve_lut) const& GetRenderableCurveLUTs() const;
	};
}

#endif // ENGINE_COMPONENT_CURVEINTPERPOLATOR_H
