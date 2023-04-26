#include "Application.h"
#include "Camera.h"
#include "Renderer.h"
#include "IWindow.h"

#include "../imgui/backends/imgui_impl_glfw.h"
#include "../imgui/backends/imgui_impl_opengl2.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

GLFWwindow* G_Windos = nullptr;

Application* Application::application_ = nullptr;

Application::Application()
	: resolution_({ 1024, 768 })
	, exit_(false)
	, last_idle_time_(0.0)
	, time_factor_(1.0)
	, time_(0.f)
	, mouse_last_x_(0.0f)
	, mouse_last_y_(0.0f)
{

}

Application::~Application()
{

}


bool Application::OnInitialize()
{
    return true;
}

void Application::OnDestroy()
{

}

bool Application::OnDisplay(Renderer* _renderer)
{
	(void)_renderer;
	return true;
}

bool Application::OnUpdate(float dt, float time)
{
	return true;
}

bool Application::OnGui()
{
	return true;
}

bool Application::GetCameraInitialSetup(Math::Vec3* _center, Math::Vec2* _angles, float* _distance) const
{
	return false;
}

int Application::Run(int _argc, const char** _argv, const char* _version, const char* _title)
{
	// Only one application at a time can be ran.
	if (application_) 
	{
		return EXIT_FAILURE;
	}
	application_ = this;

	// Starting application
	std::cout << "Starting sample \"" << _title << "\" version \"" << _version << "\"" << std::endl;

	// Open an OpenGL window
	bool success = true;
	if (true) 
	{
		// Initialize GLFW
		if (!glfwInit()) {
			application_ = nullptr;
			return EXIT_FAILURE;
		}

		// Setup GL context.
		const int gl_version_major = 2, gl_version_minor = 0;
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);	// 必须这么写。opengl core必须用vao
		//glfwWindowHint(GLFW_FSAA_SAMPLES, 4);

		// Initializes rendering before looping.
		G_Windos = glfwCreateWindow(resolution_.width, resolution_.height, _title, NULL, NULL);
		if (!G_Windos) 
		{
			std::cout << "Failed to open OpenGL window. Required OpenGL version is "
				<< gl_version_major << "." << gl_version_minor << "."
				<< std::endl;
			success = false;
		}
		else 
		{
			glfwMakeContextCurrent(G_Windos);
			// glad: load all OpenGL function pointers
			// ---------------------------------------
			if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
			{
				std::cout << "Failed to initialize GLAD" << std::endl;
				return false;
			}
			std::cout << "Successfully opened OpenGL window version \""
				<< glGetString(GL_VERSION) << "\"." << std::endl;

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

				// Setup Dear ImGui style
				ImGui::StyleColorsDark();
				//ImGui::StyleColorsLight();

				// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
				ImGuiStyle& style = ImGui::GetStyle();
				if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					style.WindowRounding = 0.0f;
					style.Colors[ImGuiCol_WindowBg].w = 1.0f;
				}

				// Setup Platform/Renderer backends
				ImGui_ImplGlfw_InitForOpenGL(G_Windos, true);
				ImGui_ImplOpenGL2_Init();
			}

			camera_ = std::make_unique<Camera>();

			// Allocates and initializes renderer.
			renderer_ = std::make_unique<Renderer>(camera_.get());
			success = renderer_->Initialize();

			if (success) 
			{
				glfwSetWindowTitle(G_Windos, _title);

				// Setup the window and installs callbacks.
				glfwSwapInterval(1);  // Enables vertical sync by default.
				glfwSetFramebufferSizeCallback(G_Windos, ResizeCbk);
				glfwSetWindowCloseCallback(G_Windos, CloseCbk);
				glfwSetScrollCallback(G_Windos, ScrollCbk);

				ResizeCbk(G_Windos, resolution_.width, resolution_.height);

				// Loop the sample.
				success = Loop();
			}
			renderer_.reset();
			camera_.reset();

			{
				// Cleanup
				ImGui_ImplOpenGL2_Shutdown();
				ImGui_ImplGlfw_Shutdown();
				ImGui::DestroyContext();
			}
		}

		// Closes window and terminates GLFW.
		glfwTerminate();

	}
	else 
	{
		// Loops without any rendering initialization.
		success = Loop();
	}

	// Notifies that an error occurred.
	if (!success) 
	{
		std::cout << "An error occurred during sample execution." << std::endl;
	}

	application_ = nullptr;

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

bool Application::Loop()
{
	// Initialize sample.
	bool success = OnInitialize();

	for (int loops = 0; success; ++loops) 
	{
		const LoopStatus status = OneLoop(loops);
		success = status != kBreakFailure;
		if (status != kContinue) {
			break;
		}
	}

	// De-initialize sample, even in case of initialization failure.
	OnDestroy();

	return success;
}

bool Application::Idle(bool first_frame)
{
	float delta;
	double time = glfwGetTime();
	if (first_frame ||  // Don't take into account time spent initializing.
		time == 0.) {    // Means glfw isn't initialized (rendering's disabled).
		delta = 1.f / 60.f;
	}
	else {
		delta = static_cast<float>(time - last_idle_time_);
	}
	last_idle_time_ = time;

	// Update dt, can be scaled, fixed, freezed...
	float update_delta = 0.0f;
	update_delta = delta * time_factor_;

	// Increment current application time
	time_ += update_delta;

	// Forwards update event to the inheriting application.
	bool update_result;
	{  // Profiles update scope.
		update_result = OnUpdate(update_delta, time_);
	}

	// Update camera model-view matrix.
	if (camera_) 
	{
		Math::AABB scene_bounds;
		GetSceneBounds(&scene_bounds);

		camera_->Update(scene_bounds, delta, first_frame);
	}

	return update_result;
}

Application::LoopStatus Application::OneLoop(int _loops) 
{
	// Tests for a manual exit request.
	if (exit_ || glfwGetKey(G_Windos, GLFW_KEY_ESCAPE) == GLFW_PRESS) 
	{
		return kBreak;
	}

	// Don't overload the cpu if the window is not active.
	if (glfwWindowShouldClose(G_Windos))
	{
		glfwWaitEvents();  // Wait...

		return kContinue;  // ...but don't do anything.
	}

	// Do the main loop.
	if (!Idle(_loops == 0)) 
	{
		return kBreakFailure;
	}

	{
		// Start the Dear ImGui frame
		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	// Skips display if "no_render" option is enabled.
	if (!Display()) 
	{
		return kBreakFailure;
	}

	return kContinue;
}

bool Application::Display()
{
	bool success = true;

	{  // Profiles rendering excluding GUI.
		glClearDepth(1.f);
		glClearColor(.4f, .42f, .38f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Setup default states
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LEQUAL);

		// Bind 3D camera matrices.
		camera_->Bind3D();

		// Forwards display event to the inheriting application.
		if (success)
		{
			success = OnDisplay(renderer_.get());
		}
	}

	{
		if (success)
		{
			success = Gui();
		}

		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(G_Windos, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		//ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		//glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		//glClear(GL_COLOR_BUFFER_BIT);

		// If you are using this code with non-legacy OpenGL header/contexts (which you should not, prefer using imgui_impl_opengl3.cpp!!),
		// you may need to backup/reset/restore other state, e.g. for current shader using the commented lines below.
		//GLint last_program;
		//glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
		//glUseProgram(0);
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		//glUseProgram(last_program);

		// Update and Render additional Platform Windows
		// (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
		//  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	// Swaps current window.
	glfwSwapBuffers(G_Windos);
	glfwPollEvents();

	return success;
}

bool Application::Gui()
{
	bool success = FrameworkGui();
	success &= OnGui();
	return success;
}

bool Application::FrameworkGui()
{
	ImGui::Begin("Framework");

	if (ImGui::CollapsingHeader("Statistics"))
	{
		if (ImGui::TreeNode("FPS"))
		{
			ImGui::TreePop();
			ImGui::Separator();
		}
		if (ImGui::TreeNode("Update"))
		{
			ImGui::TreePop();
			ImGui::Separator();
		}
		if (ImGui::TreeNode("Render"))
		{
			ImGui::TreePop();
			ImGui::Separator();
		}
	}
	if (ImGui::CollapsingHeader("Time control"))
	{
		ImGui::SliderFloat("Time factor", &time_factor_, -5.0f, 5.0f, "%.2f");
		if (ImGui::Button("Reset time factor"))
			time_factor_ = 1.0f;
	}
	if (ImGui::CollapsingHeader("Camera"))
	{
		camera_->OnGui();
	}

	ImGui::End();

	return true;
}

void Application::ResizeCbk(GLFWwindow* windowint, int _width, int _height)
{
	// Stores new resolution settings.
	application_->resolution_.width = _width;
	application_->resolution_.height = _height;

	// Uses the full viewport.
	glViewport(0, 0, _width, _height);

	application_->camera_->Resize(_width, _height);
}

void Application::CloseCbk(GLFWwindow* window) 
{
	application_->exit_ = true;
}

void Application::ScrollCbk(GLFWwindow* window, double xoffset, double yoffset)
{
	//application_->camera_->SetFov(application_->camera_->GetFovY() + yoffset);
	//application_->camera_->SetLens(application_->camera_->GetFovY(), application_->camera_->GetAspect(), 0.1f, 100.0f);
}

