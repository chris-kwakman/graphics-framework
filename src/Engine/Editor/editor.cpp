#include "editor.h"

#include <GL/glew.h>

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <ImGui/imgui_impl_sdl.h>
#include <ImGui/imgui_impl_opengl3.h>

#include <Engine/Utils/singleton.h>
#include <Engine/Graphics/sdl_window.h>

namespace Engine {
namespace Editor {


	///////////////////////////////////////////////////
	//				ImGui Management
	//////////////////////////////////////////////////

	/*
	* Most of the code here is taken directly from the examples
	* in Ocorunut's ImGui repository
	*/

	void imgui_setup_context()
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		//io.ConfigViewportsNoAutoMerge = true;
		//io.ConfigViewportsNoTaskBarIcon = true;
	}

	void imgui_setup_style()
	{
		ImGuiIO& io = ImGui::GetIO();

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 0.75f;
		}

		//io.Fonts->AddFontDefault();
		io.Fonts->AddFontFromFileTTF("data/fonts/Roboto-Medium.ttf", 16.0f);
	}

	void imgui_setup_renderer()
	{
		auto& sdl_manager = Singleton<Engine::sdl_manager>();

		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForOpenGL(sdl_manager.m_window, sdl_manager.m_gl_context);

		// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100
		const char* glsl_version = "#version 100";
#elif defined(__APPLE__)
	// GL 3.2 Core + GLSL 150
		const char* glsl_version = "#version 150";
#else
	// GL 3.0 + GLSL 130
		const char* glsl_version = "#version 130";
#endif

		ImGui_ImplOpenGL3_Init(glsl_version);
	}

	void imgui_shutdown()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
	}

	void imgui_new_frame()
	{
		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
	}

	void imgui_render()
	{
		// Rendering
		ImGui::Render();

		auto& io = ImGui::GetIO();

		ImVec4 const clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		//glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		//glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Update and Render additional Platform Windows
		// (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
		//  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
			SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
		}
	}

	///////////////////////////////////////////////////
	//				Public methods
	//////////////////////////////////////////////////

	void Editor::Initialise()
	{
		imgui_setup_context();
		imgui_setup_renderer();
		imgui_setup_style();
	}

	void Editor::ProcessSDLEvent(SDL_Event* _event)
	{
		ImGui_ImplSDL2_ProcessEvent(_event);
	}

	void Editor::NewFrame()
	{
		imgui_new_frame();
	}

	void Editor::Render()
	{
		imgui_render();
	}

	void Editor::Shutdown()
	{
		imgui_shutdown();
	}

}
}