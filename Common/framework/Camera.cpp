
#include "camera.h"
#include "IWindow.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../imgui/imgui.h"


const Math::Vec2 kDefaultAngle = Math::Vec2(-Math::PI * 1.f / 12.f, Math::PI * 1.f / 5.f);
const float kFovY = Math::PI / 3.f;
const float kNear = .01f;
const float kFar = 1000.f;
const float kFrameAllZoomOut = 1.3f;  // 30% bigger than the scene.
const float kKeyboardFactor = 100.f;
const float kDistanceFactor = .1f;
const float kAngleFactor = .01f;

// Setups initial values.
Camera::Camera()
	: projection_(Math::Mat4::IDENTITY),
	view_(Math::Mat4::IDENTITY),
	view_proj_(Math::Mat4::IDENTITY),
	angles_(kDefaultAngle),
	center_(),
	distance_(),
	mouse_last_x_(0),
	mouse_last_y_(0),
	mouse_last_wheel_(0),
	auto_framing_(true) {}

Camera::~Camera() {}

void Camera::Update(const Math::AABB& _box, float _delta_time, bool _first_frame) 
{
	// Frame the scene according to the provided box.
	if (!_box.IsEmpty())
	{
		if (auto_framing_ || _first_frame) 
		{
			center_ = (_box._max + _box._min) * .5f;
			if (_first_frame) {
				const float radius = (_box._max - _box._min).Length() * 0.5f;
				distance_ = radius * kFrameAllZoomOut / tanf(kFovY * .5f);
			}
		}
	}

	// Update manual controls.
	const Controls controls = UpdateControls(_delta_time);

	// Disable autoframing according to inputs.
	auto_framing_ &=
		!controls.panning && !controls.zooming && !controls.zooming_wheel;
}

Camera::Controls Camera::UpdateControls(float _delta_time)
{
	Controls controls;
	controls.zooming = false;
	controls.zooming_wheel = false;
	controls.rotating = false;
	controls.panning = false;

	// Fetches current mouse position and compute its movement since last frame.
	double x, y;
	glfwGetCursorPos(G_Windos, (double*)&x, (double*)&y);
	const int mdx = x - mouse_last_x_;
	const int mdy = y - mouse_last_y_;
	mouse_last_x_ = x;
	mouse_last_y_ = y;

	// Finds keyboard relative dx and dy commmands.
	const int timed_factor = Math::Max(1, static_cast<int>(kKeyboardFactor * _delta_time));
	const int kdx = timed_factor * (glfwGetKey(G_Windos, GLFW_KEY_LEFT) - glfwGetKey(G_Windos, GLFW_KEY_RIGHT));
	const int kdy = timed_factor * (glfwGetKey(G_Windos, GLFW_KEY_DOWN) - glfwGetKey(G_Windos, GLFW_KEY_UP));
	const bool keyboard_interact = kdx || kdy;

	// Computes composed keyboard and mouse dx and dy.
	const int dx = mdx + kdx;
	const int dy = mdy + kdy;

	// Mouse right button activates Zoom, Pan and Orbit modes.
	if (keyboard_interact || glfwGetMouseButton(G_Windos, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
	{
		if (glfwGetKey(G_Windos, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) 
		{  // Zoom mode.
			controls.zooming = true;

			distance_ += dy * kDistanceFactor;
		}
		else if (glfwGetKey(G_Windos, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) 
		{  // Pan mode.
#if 0
			controls.panning = true;

			const float dx_pan = -dx * kPanFactor;
			const float dy_pan = -dy * kPanFactor;

			// Moves along camera axes.
			Math::Mat4 transpose = Transpose(view_);
			Math::Float3 right_transpose, up_transpose;
			math::Store3PtrU(transpose.cols[0], &right_transpose.x);
			math::Store3PtrU(transpose.cols[1], &up_transpose.x);
			center_ = center_ + right_transpose * dx_pan + up_transpose * dy_pan;
#endif
		}
		else {  // Orbit mode.
			controls.rotating = true;

			angles_.x = fmodf(angles_.x - dy * kAngleFactor, Math::PI);
			angles_.y = fmodf(angles_.y - dx * kAngleFactor, Math::PI);
		}
	}

	// Build the model view matrix components.
	Math::Mat4 center;
	center.Translate(center_);
	Math::Mat4 y_rotation;
	y_rotation.Rotate(Math::Vec3::UNIT_Y, -angles_.y);
	Math::Mat4 x_rotation;
	x_rotation.Rotate(Math::Vec3::UNIT_X, -angles_.x);
	Math::Mat4 distance;
	distance.Translate(0, 0, distance_);

	// Concatenate view matrix components.
	view_ = (distance * x_rotation * y_rotation * center).GetInversed();

	return controls;
}

void Camera::Reset(const Math::Vec3& _center, const Math::Vec2& _angles, float _distance)
{
	center_ = _center;
	angles_ = _angles;
	distance_ = _distance;
}

void Camera::Bind3D() 
{
	// Updates internal vp matrix.
	view_proj_ = view_ * projection_;
}

void Camera::Bind2D() 
{

}

void Camera::Resize(int _width, int _height) 
{
	// Handle empty windows.
	if (_width <= 0 || _height <= 0) 
	{
		projection_ = Math::Mat4::IDENTITY;
		return;
	}

	const float ratio = 1.f * _width / _height;
	projection_ = Math::Mat4::CreatePerspective(kFovY, ratio, kNear, kFar);
}

bool Camera::OnGui()
{
	ImGui::Checkbox("Automatic", &auto_framing_);

	return true;
}
