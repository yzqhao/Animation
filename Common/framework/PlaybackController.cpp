#include "PlaybackController.h"
#include "../RawAnimation.h"

#include "../imgui/imgui.h"

PlaybackController::PlaybackController()
	: time_ratio_(0.f),
	previous_time_ratio_(0.f),
	playback_speed_(1.f),
	play_(true),
	loop_(true) {}

void PlaybackController::Update(const RawAnimation& _animation, float _dt) 
{
	float new_time = time_ratio_;

	if (play_) {
		new_time = time_ratio_ + _dt * playback_speed_ / _animation.duration;
	}

	// Must be called even if time doesn't change, in order to update previous
	// frame time ratio. Uses set_time_ratio function in order to update
	// previous_time_ an wrap time value in the unit interval (depending on loop
	// mode).
	set_time_ratio(new_time);
}

void PlaybackController::Update(const Animation& _animation, float _dt)
{
	float new_time = time_ratio_;

	if (play_) {
		new_time = time_ratio_ + _dt * playback_speed_ / _animation.Duration();
	}

	// Must be called even if time doesn't change, in order to update previous
	// frame time ratio. Uses set_time_ratio function in order to update
	// previous_time_ an wrap time value in the unit interval (depending on loop
	// mode).
	set_time_ratio(new_time);
}

bool PlaybackController::OnGui(const RawAnimation& animation, uint hash)
{
	bool time_changed = false;

	if (ImGui::TreeNode(("Animation"+std::to_string(hash)).c_str()))
	{
		if (ImGui::Button(play_ ? "Pause" : "Play"))
			play_ = !play_;

		ImGui::Checkbox(("Loop" + std::to_string(hash)).c_str(), &loop_);

		// changes. Otherwise previous time would be incorrect.
		float ratio = time_ratio();
		float show_ratio = ratio * animation.Duration();
		if (ImGui::SliderFloat(("Animation time" + std::to_string(hash)).c_str(), &show_ratio, 0.0f, animation.Duration(), "%.2f"))
		{
			set_time_ratio(show_ratio / animation.Duration());
			play_ = false;
			time_changed = true;
		}

		ImGui::SliderFloat(("Playback speed" + std::to_string(hash)).c_str(), &playback_speed_, -5.0f, 5.0f, "%.2f");

		if (ImGui::Button(("Reset playback speed" + std::to_string(hash)).c_str()))
			playback_speed_ = 1.0f;

		ImGui::TreePop();
		ImGui::Separator();
	}

	return time_changed;
}

bool PlaybackController::OnGui(const Animation& animation, uint hash)
{
	bool time_changed = false;

	if (ImGui::TreeNode(("Animation" + std::to_string(hash)).c_str()))
	{
		if (ImGui::Button(play_ ? "Pause" : "Play"))
			play_ = !play_;

		ImGui::Checkbox(("Loop" + std::to_string(hash)).c_str(), &loop_);

		// changes. Otherwise previous time would be incorrect.
		float ratio = time_ratio();
		float show_ratio = ratio * animation.Duration();
		if (ImGui::SliderFloat(("Animation time" + std::to_string(hash)).c_str(), &show_ratio, 0.0f, animation.Duration(), "%.2f"))
		{
			set_time_ratio(show_ratio / animation.Duration());
			play_ = false;
			time_changed = true;
		}

		ImGui::SliderFloat(("Playback speed" + std::to_string(hash)).c_str(), &playback_speed_, -5.0f, 5.0f, "%.2f");

		if (ImGui::Button(("Reset playback speed" + std::to_string(hash)).c_str()))
			playback_speed_ = 1.0f;

		ImGui::TreePop();
		ImGui::Separator();
	}

	return time_changed;
}

void PlaybackController::set_time_ratio(float _ratio) 
{
	previous_time_ratio_ = time_ratio_;
	if (loop_) 
	{
		// Wraps in the unit interval [0:1], even for negative values (the reason
		// for using floorf).
		time_ratio_ = _ratio - floorf(_ratio);
	}
	else {
		// Clamps in the unit interval [0:1].
		time_ratio_ = Math::Clamp(0.f, _ratio, 1.f);
	}
}

// Gets animation current time.
float PlaybackController::time_ratio() const { return time_ratio_; }

// Gets animation time of last update.
float PlaybackController::previous_time_ratio() const 
{
	return previous_time_ratio_;
}

void PlaybackController::Reset() 
{
	previous_time_ratio_ = time_ratio_ = 0.f;
	playback_speed_ = 1.f;
	play_ = true;
}
