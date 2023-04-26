#pragma once

#include "../../Math/3dMath.h"

class Camera;
class Renderer;

struct GLFWwindow;

// Screen resolution settings.
struct Resolution 
{
    int width;
    int height;
};

class Application
{
public:
    Application();
    virtual ~Application();

	int Run(int _argc, const char** _argv, const char* _version, const char* _title);

private:

	virtual bool OnInitialize();
	virtual void OnDestroy();
	virtual bool OnDisplay(Renderer* _renderer);
	virtual bool OnUpdate(float dt, float time);
	virtual bool OnGui();

	virtual void GetSceneBounds(Math::AABB* _bound) const {}
	virtual bool GetCameraInitialSetup(Math::Vec3* _center, Math::Vec2* _angles, float* _distance) const;

	bool Gui();
	bool FrameworkGui();

	bool Loop();

	bool Idle(bool first_frame);

	// Implements framework internal one iteration loop function.
	enum LoopStatus {
		kContinue,      // Can continue with next loop.
		kBreak,         // Should stop looping (ex: exit).
		kBreakFailure,  // // Should stop looping beacause something went wrong.
	};
	LoopStatus OneLoop(int _loops);

	bool Display();

    static void ResizeCbk(GLFWwindow* windowint, int _width, int _height);
	static void CloseCbk(GLFWwindow* window);
	static void ScrollCbk(GLFWwindow* window, double xoffset, double yoffset);

    static Application* application_;
    
    bool exit_;
	double last_idle_time_;	// �ϴθ��µ�ʱ��
	float time_factor_;		// ʱ��ϵ����Ĭ��ֵ��1.0������ͨ��gui����
	float time_;			// ��ʱ��
	double mouse_last_x_;
	double mouse_last_y_;
    Resolution resolution_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Renderer> renderer_;
};