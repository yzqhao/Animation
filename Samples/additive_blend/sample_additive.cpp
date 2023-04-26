
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
#include "../Common/BlendingJob.h"
#include "../Common/imgui/imgui_internal.h"
#include "../Common/skeleton_utils.h"

class AdditiveBlendSampleApplication : public Application {
public:
	AdditiveBlendSampleApplication()
		: base_weight_(0.f),
		additive_weigths_{ .3f, .9f },
		auto_animate_weights_(true) {}

protected:
	// Updates current animation time and skeleton pose.
	virtual bool OnUpdate(float _dt, float) 
	{
		// For the sample purpose, animates additive weights automatically so the
		// hand moves.
		if (auto_animate_weights_) 
		{
			AnimateWeights(_dt);
		}

		// Updates base animation time for main animation.
		controller_.Update(base_animation_, _dt);

		// Setup sampling job.
		RawAnimationJob sampling_job;
		sampling_job.animation = &base_animation_;
		sampling_job.context = &context_;
		sampling_job.ratio = controller_.time_ratio();
		sampling_job.output = make_span(locals_);

		// Samples animation.
		if (!sampling_job.Run()) {
			return false;
		}

		// Setups blending job layers.

		// Main animation is used as-is.
		BlendingJob::Layer layers[1];
		layers[0].transform = make_span(locals_);
		layers[0].weight = base_weight_;
		layers[0].joint_weights = make_span(base_joint_weights_);

		// The two additive layers (curl and splay) are blended on top of the main
		// layer.
		BlendingJob::Layer additive_layers[kNumLayers];
		for (size_t i = 0; i < kNumLayers; ++i) {
			additive_layers[i].transform = make_span(additive_locals_[i]);
			additive_layers[i].weight = additive_weigths_[i];
		}

		// Setups blending job.
		BlendingJob blend_job;
		blend_job.layers = layers;
		blend_job.additive_layers = additive_layers;
		blend_job.rest_pose = make_span(skeleton_.joint_rest_poses());
		blend_job.output = make_span(blended_locals_);

		// Blends.
		if (!blend_job.Run()) 
		{
			return false;
		}

		// Converts from local space to model space matrices.
		// Gets the output of the blending stage, and converts it to model space.

		// Setup local-to-model conversion job.
		LocalToModelJob ltm_job;
		ltm_job.skeleton = &skeleton_;
		ltm_job.input = make_span(blended_locals_);
		ltm_job.output = make_span(models_);

		// Runs ltm job.
		if (!ltm_job.Run()) 
		{
			return false;
		}

		return true;
	}

	void AnimateWeights(float _dt) 
	{
		static float t = 0.f;
		t += _dt;
		additive_weigths_[0] = .5f + std::cos(t * 1.7f) * .5f;
		additive_weigths_[1] = .5f + std::cos(t * 2.5f) * .5f;
	}

	// Samples animation, transforms to model space and renders.
	virtual bool OnDisplay(Renderer* _renderer) 
	{
		//return true;
		return _renderer->DrawPosture(skeleton_, models_, Math::Mat4::IDENTITY);
	}

	bool SetJointWeights(const char* _name, float _weight) 
	{
		const auto set_joint = [this, _weight](int _joint, int) 
		{
			float& soa_weight = base_joint_weights_[_joint];
			soa_weight = _weight;
		};

		const int joint = FindJoint(skeleton_, _name);
		if (joint >= 0)
		{
			IterateJointsDF(skeleton_, set_joint, joint);
			return true;
		}
		return false;
	}

	virtual bool OnInitialize() 
	{
		// Reading skeleton.
		if (!LoadFile::LoadSkeleton("../../media/additive/skeleton.ozz", skeleton_))	//"../../media/blend_test/skeleton.ozz"
		{
			return false;
		}

		if (!LoadFile::LoadRawAnimation("../../media/additive/walk.ozz", base_animation_))
		{
			return false;
		}

		if (skeleton_.num_joints() != base_animation_.num_tracks())
		{
			return false;
		}

		const int num_joints = skeleton_.num_joints();

		context_.Resize(num_joints);
		locals_.resize(num_joints);
		models_.resize(num_joints);
		blended_locals_.resize(num_joints);

		// Allocates and sets base animation mask weights to one.
		base_joint_weights_.resize(num_joints, 1.0f);
		SetJointWeights("Lefthand", 0.f);
		SetJointWeights("RightHand", 0.f);

		// Reads and extract additive animations pose.
		const char* filenames[] = 
		{ 
			"../../media/additive/splay.ozz", 
			"../../media/additive/curl.ozz" 
		};
		for (int i = 0; i < kNumLayers; ++i)
		{
			// Reads animation on the stack as it won't need to be maintained in
			// memory. Only the pose is needed.
			RawAnimation animation;
			if (!LoadFile::LoadRawAnimation(filenames[i], animation)) 
			{
				return false;
			}

			if (num_joints != animation.num_tracks()) {
				return false;
			}

			// Allocates additive poses, aka buffers of Soa tranforms.
			additive_locals_[i].resize(num_joints);

			// Samples the first frame pose.
			RawAnimationJob sampling_job;
			sampling_job.animation = &animation;
			sampling_job.context = &context_;
			sampling_job.ratio = 0.f;  // Only needs the first frame pose
			sampling_job.output = make_span(additive_locals_[i]);

			// Samples animation.
			if (!sampling_job.Run()) {
				return false;
			}

			// Invalidates context which will be re-used for another animation.
			// This is usually not needed, animation address on the stack is the same
			// each loop, hence creating an issue as animation content is changing.
			context_.Invalidate();
		}

		return true;
	}

	virtual void OnDestroy() {}

	virtual bool OnGui()
	{
		ImGui::Begin("Sample");



		ImGui::End();

		return true;
	}

	virtual void GetSceneBounds(Math::AABB* _bound) const 
	{
		ComputePostureBounds(make_span(models_), _bound);
	}

private:

	// Runtime skeleton.
	Skeleton skeleton_;

	// The number of additive layers to blend.
	enum { kSplay, kCurl, kNumLayers };

	// Runtime animation.
	RawAnimation base_animation_;

	// Per-joint weights used to define the base animation mask. Allows to remove
	// hands from base animations.
	std::vector<float> base_joint_weights_;

	// Main animation controller. This is a utility class that helps with
	// controlling animation playback time.
	PlaybackController controller_;

	// Sampling context.
	RawAnimationJob::Context context_;

	// Buffer of local transforms as sampled from main animation_.
	std::vector<Math::Transform> locals_;

	// Blending weight of the base animation layer.
	float base_weight_;

	// Poses of local transforms as sampled from curl and splay animations.
	// They are sampled during initialization, as a single pose is used.
	std::vector<Math::Transform> additive_locals_[kNumLayers];

	// Blending weight of the additive animation layer.
	float additive_weigths_[kNumLayers];

	// Buffer of local transforms which stores the blending result.
	std::vector<Math::Transform> blended_locals_;

	// Buffer of model space matrices. These are computed by the local-to-model
	// job after the blending stage.
	std::vector<Math::Mat4> models_;

	// Automatically animates additive weights.
	bool auto_animate_weights_;
};

int main(int _argc, const char** _argv) 
{
	const char* title = "Ozz-animation sample: Additive animations blending";
	return AdditiveBlendSampleApplication().Run(_argc, _argv, "1.0", title);
}
