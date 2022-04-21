#include <imgui.h>
#include <nlohmann/json.hpp>

namespace Engine {
namespace Physics {

	class ScenePhysicsManager
	{
	public:

		struct session_data
		{
			std::array<nlohmann::json,200> rigidbody_frame_data;
			size_t begin = 0;
			size_t end = 0;
		};

		session_data m_session_data;
		bool paused = false;
		bool step = false;
		unsigned int resolution_iterations_penetration = 8;
		unsigned int resolution_iterations_friction = 8;
		float		 beta = 0.2f;
		float		 slop = 0.02f;
		bool		 contact_caching = true;

		bool render_contacts = false;
		bool render_penetration = false;
		bool render_penetration_resolution = false;
		bool render_friction_resolution = false;
		float scale_lambda_resolution_vectors = 10.0f;

		void Reset();
		void PhysicsStep(float _dt);
		void DisplayEditorWindow();
	};

}
}