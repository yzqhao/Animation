
#include "../Common/framework/application.h"
#include "../Common/framework/renderer.h"
#include "../Common/RawAnimation.h"
#include "../Common/LocalToModelJob.h"
#include "../Common/AnimationJob.h"
#include "../Common/RawAnimationJob.h"
#include "../Common/Skeleton.h"
#include "../Common/LoadFile.h"
#include "../Common/framework/PlaybackController.h"
#include "../Common/Utils.h"
#include "../Common/imgui/imgui.h"
#include "../Common/skeleton_utils.h"

class AttachSampleApplication : public Application {
public:
	AttachSampleApplication() : attachment_(0), offset_(-0.02f, 0.03f, 0.05f) {}

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
		if (!_renderer->DrawPosture(skeleton_, models_, Math::Mat4::IDENTITY))
			return false;

		const Math::Mat4& joint = models_[attachment_];
		Math::Mat4 transform;
		transform.Translate(offset_);
		transform = transform * joint;

		// Prepare rendering.
		const float thickness = .01f;
		const float length = .5f;
		const Math::AABB box(Math::Vec3(-thickness, -thickness, -length), Math::Vec3(thickness, thickness, 0.f));
		const Color colors[2] = { kRed, kGreen };

		return _renderer->DrawBoxIm(box, transform, colors);
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

		// Finds the joint where the object should be attached.
		attachment_ = FindJoint(skeleton_, "Bip01 L Finger01");
		if (attachment_ < 0) 
		{
			return false;
		}

		return true;
	}

	virtual void OnDestroy() {}

	virtual bool OnGui()
	{
		ImGui::Begin("Sample");

		controller_.OnGui(animation_);

		if (ImGui::TreeNode("Attachment joint"))
		{
			ImGui::SliderInt("Select joint", &attachment_, 0, skeleton_.num_joints()-1, "%d");
			ImGui::Text("Attachment offset");
			ImGui::SliderFloat("x", &offset_.x, -5.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("y", &offset_.y, -5.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("z", &offset_.z, -5.0f, 5.0f, "%.2f");
		}

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

	int attachment_;	// 附加关节的索引
	Math::Vec3 offset_;	// 物体相对关节的位置
};

int main(int _argc, const char** _argv) 
{
	const char* title = "Attach";
	return AttachSampleApplication().Run(_argc, _argv, "1.0", title);
}
