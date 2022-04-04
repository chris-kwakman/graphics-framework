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
		unsigned int iterations = 8;
		float		 beta = 0.5f;

		void Reset();
		void PhysicsStep(float _dt);
		void DisplayEditorWindow();
	};

}
}