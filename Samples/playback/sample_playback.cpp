
#include "../Common/framework/application.h"
#include "../Common/framework/renderer.h"
#include "../Common/RawAnimation.h"
#include "../Common/LocalToModelJob.h"
#include "../Common/AnimationJob.h"
#include "../Common/Animation.h"
#include "../Common/RawAnimationJob.h"
#include "../Common/Skeleton.h"
#include "../Common/LoadFile.h"
#include "../Common/framework/PlaybackController.h"
#include "../Common/Utils.h"
#include "../Common/imgui/imgui.h"

class PlaybackSampleApplication : public Application {
public:
	PlaybackSampleApplication() {}

protected:
	// Updates current animation time and skeleton pose.
	virtual bool OnUpdate(float _dt, float) 
	{
		// Updates current animation time.
		controller_.Update(animation_, _dt);

		// Samples optimized animation at t = animation_time_.
#if USE_RAW_ANIMATION
		RawAnimationJob sampling_job;
#else 
		AnimationJob sampling_job;
#endif
		sampling_job.animation = &animation_;
		sampling_job.context = &context_;
		sampling_job.ratio = controller_.time_ratio();
		sampling_job.output = make_span(locals_);
		if (!sampling_job.Run()) 
		{
			return false;
		}

		// Converts from local space to model space matrices.
		LocalToModelJob ltm_job;
		ltm_job.skeleton = &skeleton_;
		ltm_job.input = make_span(locals_);
		ltm_job.output = make_span(models_);
		if (!ltm_job.Run()) {
			return false;
		}

		return true;
	}

	// Samples animation, transforms to model space and renders.
	virtual bool OnDisplay(Renderer* _renderer) 
	{
		//return true;
		return _renderer->DrawPosture(skeleton_, models_, Math::Mat4::IDENTITY);
	}

	virtual bool OnInitialize() 
	{
		// Reading skeleton.
		if (!LoadFile::LoadSkeleton("../../media/Attack/skeleton.ozz", skeleton_))
		{
			return false;
		}

		// Reading animation.
#if USE_RAW_ANIMATION
		if (!LoadFile::LoadRawAnimation("../../media/Attack/Take 001_raw_animation.ozz", animation_))
#else
		if (!LoadFile::LoadAnimation("../../media/Attack/Take 001_animation.ozz", animation_))
#endif
		{
			return false;
		}

		// Skeleton and animation needs to match.
		if (skeleton_.num_joints() != animation_.num_tracks()) 
		{
			return false;
		}

		// Allocates runtime buffers.
		const int num_joints = skeleton_.num_joints();
		locals_.resize(num_joints);
		models_.resize(num_joints);

		// Allocates a context that matches animation requirements.
		context_.Resize(num_joints);

		return true;
	}

	virtual void OnDestroy() {}

	virtual bool OnGui()
	{
		ImGui::Begin("Sample");
		controller_.OnGui(animation_);
		ImGui::End();

		return true;
	}

	virtual void GetSceneBounds(Math::AABB* _bound) const 
	{
		ComputePostureBounds(make_span(models_), _bound);
	}

private:
	// Playback animation controller. This is a utility class that helps with
    // controlling animation playback time
	PlaybackController controller_;

	// Runtime skeleton.
	Skeleton skeleton_;

	// Runtime animation.
#if USE_RAW_ANIMATION
	RawAnimation animation_;
	RawAnimationJob::Context context_;
#else 
	Animation animation_;
	AnimationJob::Context context_;
#endif

	// Buffer of local transforms as sampled from animation_.
	std::vector<Math::Transform> locals_;

	// Buffer of model space matrices.
	std::vector<Math::Mat4> models_;
};

int main(int _argc, const char** _argv) 
{
	const char* title = "Playback";
	return PlaybackSampleApplication().Run(_argc, _argv, "1.0", title);
}
