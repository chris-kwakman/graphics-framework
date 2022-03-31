#include <imgui.h>

namespace Engine {
namespace Physics {

	class ScenePhysicsManager
	{
	public:

		bool paused = false;
		bool step = false;

		void DisplayEditorWindow()
		{
			if(ImGui::Begin("Physics Debug"))
			{
				ImGui::Checkbox("Paused", &paused);
				step = ImGui::Button("Physics Step");
				ImGui::End();
			}
		}
	};

}
}