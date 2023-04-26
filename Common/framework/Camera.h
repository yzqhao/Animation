#pragma once

#include "../../Math/3DMath.h"

class Camera
{
public:
	// Initializes camera to its default framing.
	Camera();

	// Destructor.
	~Camera();

	// Updates camera framing: mouse manipulation, timed transitions...
	// Returns actions that the user applied to the camera during the frame.
	void Update(const Math::AABB& _box, float _delta_time, bool _first_frame);

	// Resets camera center, angles and distance.
	void Reset(const Math::Vec3& _center, const Math::Vec2& _angles,
		float _distance);

	// Binds 3d projection and view matrices to the current matrix.
	void Bind3D();

	// Binds 2d projection and view matrices to the current matrix.
	void Bind2D();

	// Resize notification, used to rebuild projection matrix.
	void Resize(int _width, int _height);

	bool OnGui();

	// Get the current projection matrix.
	const Math::Mat4& projection() { return projection_; }

	// Get the current model-view matrix.
	const Math::Mat4& view() { return view_; }

	// Get the current model-view-projection matrix.
	const Math::Mat4& view_proj() { return view_proj_; }

	// Set to true to automatically frame the camera on the whole scene.
	void set_auto_framing(bool _auto) { auto_framing_ = _auto; }
	// Get auto framing state.
	bool auto_framing() const { return auto_framing_; }

private:
	struct Controls {
		bool zooming;
		bool zooming_wheel;
		bool rotating;
		bool panning;
	};
	Controls UpdateControls(float _delta_time);

	// The current projection matrix.
	Math::Mat4 projection_;

	// The current model-view matrix.
	Math::Mat4 view_;

	// The current model-view-projection matrix.
	Math::Mat4 view_proj_;

	// The angles in degree of the camera rotation around x and y axes.
	Math::Vec2 angles_;

	// The center of the rotation.
	Math::Vec3 center_;

	// The view distance, from the center of rotation.
	float distance_;

	// The position of the mouse, the last time it has been seen.
	int mouse_last_x_;
	int mouse_last_y_;
	int mouse_last_wheel_;

	// Set to true to automatically frame the camera on the whole scene.
	bool auto_framing_;
};

