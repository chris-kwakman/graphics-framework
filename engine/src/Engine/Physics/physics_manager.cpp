#include "physics_manager.hpp"

#include <Engine/Components/Rigidbody.h>
#include <Engine/Physics/Collider.h>

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

		Singleton<Component::ColliderManager>().ComputeCollisionResolution(_dt);
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
			step = ImGui::Button("Physics Step");
			if (step)
			{
				if (paused)
				{
					m_session_data.end = m_session_data.begin + peek_frame + 1;
					peek_frame++;
				}
			}

			ImGui::BeginDisabled(!paused);
			if (ImGui::SliderInt("Frame Record", &peek_frame, 0, frame_count - 1, "%d"))
			{
				size_t const frame_index = (m_session_data.begin + peek_frame) % m_session_data.rigidbody_frame_data.max_size();
				Singleton<Component::RigidBodyManager>().Deserialize(m_session_data.rigidbody_frame_data[frame_index]);
				Singleton<Component::RigidBodyManager>().UpdateTransforms();
			}
			ImGui::EndDisabled();

			ImGui::End();
		}
	}

}
}