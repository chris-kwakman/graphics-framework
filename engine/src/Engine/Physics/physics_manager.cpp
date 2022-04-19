#include "physics_manager.hpp"

#include <Engine/Components/Rigidbody.h>
#include <Engine/Physics/Collider.h>
#include "resolution.hpp"

namespace Engine {
namespace Physics {

	void ScenePhysicsManager::Reset()
	{
		m_session_data = session_data();
	}

	void ScenePhysicsManager::PhysicsStep(float _dt)
	{
		auto & rb_mgr = Singleton<Component::RigidBodyManager>();

		nlohmann::json frame_json;
		rb_mgr.Serialize(frame_json);

		m_session_data.rigidbody_frame_data[m_session_data.end % m_session_data.rigidbody_frame_data.max_size()] = std::move(frame_json);
		m_session_data.end++;
		if (m_session_data.end - m_session_data.begin > m_session_data.rigidbody_frame_data.max_size())
			m_session_data.begin++;

		compute_resolution_gauss_seidel(
			Singleton<Component::ColliderManager>().m_data.m_global_contact_data,
			resolution_iterations_penetration,
			resolution_iterations_friction,
			_dt,
			beta
		);
		rb_mgr.Integrate(_dt);
		rb_mgr.UpdateTransforms();
	}

	void ScenePhysicsManager::DisplayEditorWindow()
	{
		static int peek_frame;
		if (ImGui::Begin("Physics Debug"))
		{
			int frame_count = m_session_data.end - m_session_data.begin;
			peek_frame = std::clamp(peek_frame, 0, std::max(frame_count - 1,0));
			if (!paused)
				peek_frame = frame_count - 1;
			if (ImGui::Checkbox("Paused", &paused))
			{
				if(!paused)
					m_session_data.end = m_session_data.begin + peek_frame;
			}
			ImGui::BeginDisabled(!paused);
			step = ImGui::Button("Physics Step");
			if (step)
			{
				if (paused)
				{
					m_session_data.end = m_session_data.begin + peek_frame + 1;
					peek_frame++;
				}
			}
			if (ImGui::SliderInt("Frame Record", &peek_frame, 0, frame_count - 1, "%d"))
			{
				size_t const frame_index = (m_session_data.begin + peek_frame) % m_session_data.rigidbody_frame_data.max_size();
				Singleton<Component::RigidBodyManager>().Deserialize(m_session_data.rigidbody_frame_data[frame_index]);
				Singleton<Component::RigidBodyManager>().UpdateTransforms();
			}
			ImGui::EndDisabled();

			int iters = 0;
			iters = resolution_iterations_penetration;
			if (ImGui::DragInt("Penetration Constraint Iterations", &iters, 0.1f, 1, 128, "%d", ImGuiSliderFlags_AlwaysClamp))
				resolution_iterations_penetration = iters;
			iters = resolution_iterations_friction;
			if (ImGui::DragInt("Friction Constraint Iterations", &iters, 0.1f, 1, 128, "%d", ImGuiSliderFlags_AlwaysClamp))
				resolution_iterations_friction = iters;
			ImGui::SliderFloat("Constraint Solver Beta", &beta, 0.0f, 1.0f, "%.3f");

			ImGui::Checkbox("Render Penetration Vectors", &render_penetration);
			ImGui::Checkbox("Render Penetration Resolution", &render_penetration_resolution);
			ImGui::Checkbox("Render Friction Resolution", &render_friction_resolution);
			ImGui::SliderFloat("Scale Resolution Lambda Vectors", &scale_lambda_resolution_vectors, 1.0f, 25.0f, "%.2f");


			// Display contacts this frame


			if (paused)
			{
				auto const & collider_mgr = Singleton<Component::ColliderManager>();
				auto const& global_contact_data = collider_mgr.m_data.m_global_contact_data;

				auto const& all_contacts = global_contact_data.all_contacts;;
				for (contact_manifold const cm : global_contact_data.all_contact_manifolds)
				{
					ImGui::Separator();
					ImGui::Text("RigidBody A: %s (ID: %u)", cm.rigidbody_A.Owner().GetName(), cm.rigidbody_A.Owner().ID());
					ImGui::Text("RigidBody B: %s (ID: %u)", cm.rigidbody_B.Owner().GetName(), cm.rigidbody_B.Owner().ID());
					ImGui::Text("Contact Count: %u", cm.contact_count);
					ImGui::Text("Is edge-edge intersection: %s", cm.is_edge_edge ? "true" : "false");
					ImGui::Indent(); 
					for (size_t i = cm.first_contact_index; i < cm.first_contact_index + cm.contact_count; i++)
					{
						contact c = all_contacts[i];
						ImGui::Text("Point: (%.2f, %.2f, %.2f)", c.point.x, c.point.y, c.point.z);
						ImGui::Text("Normal: (%.2f, %.2f, %.2f)", c.normal.x, c.normal.y, c.normal.z);
					}
					ImGui::Unindent();
				}
			}


			ImGui::End();
		}
	}

}
}