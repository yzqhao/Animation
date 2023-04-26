
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

class PlaybackSampleApplication : public Application {
public:
	PlaybackSampleApplication()
		: upper_body_root_(0)
		, threshold_(BlendingJob().threshold) {}

protected:
	// Updates current animation time and skeleton pose.
	virtual bool OnUpdate(float _dt, float) 
	{
		// Updates and samples all animations to their respective local space
		// transform buffers.
		for (int i = 0; i < kNumLayers; ++i)
		{
			Sampler& sampler = samplers_[i];

			// Updates animations time.
			sampler.controller.Update(sampler.animation, _dt);

			// Setup sampling job.
#if USE_RAW_ANIMATION
			RawAnimationJob sampling_job;
#else
			AnimationJob sampling_job;
#endif
			sampling_job.animation = &sampler.animation;
			sampling_job.context = &sampler.context;
			sampling_job.ratio = sampler.controller.time_ratio();
			sampling_job.output = make_span(sampler.locals);

			// Samples animation.
			if (!sampling_job.Run()) 
			{
				return false;
			}
		}

		// Blends animations.
		// Blends the local spaces transforms computed by sampling all animations
		// (1st stage just above), and outputs the result to the local space
		// transform buffer blended_locals_

		// Prepares blending layers.
		BlendingJob::Layer layers[kNumLayers];
		for (int i = 0; i < kNumLayers; ++i) 
		{
			layers[i].transform = make_span(samplers_[i].locals);
			layers[i].weight = samplers_[i].weight_setting;

			// Set per-joint weights for the partially blended layer.
			layers[i].joint_weights = make_span(samplers_[i].joint_weights);
		}

		// Setups blending job.
		BlendingJob blend_job;
		blend_job.threshold = threshold_;
		blend_job.layers = layers;
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

	// Samples animation, transforms to model space and renders.
	virtual bool OnDisplay(Renderer* _renderer) 
	{
		//return true;
		return _renderer->DrawPosture(skeleton_, models_, Math::Mat4::IDENTITY);
	}

	virtual bool OnInitialize() 
	{
		// Reading skeleton.
		if (!LoadFile::LoadSkeleton("../../media/Walk/skeleton.ozz", skeleton_))	//"../../media/partial_blend_test/skeleton.ozz"
		{
			return false;
		}

		const int num_joints = skeleton_.num_joints();

		// Reading animations.
		const char* filenames[] = 
		{
			//"../../media/partial_blend_test/walk.ozz", 
			//"../../media/partial_blend_test/crossarms.ozz",
#if USE_RAW_ANIMATION
			"../../media/Walk/Take 001_raw_animation.ozz",
			"../../media/Attack/Take 001_raw_animation.ozz",
#else
			"../../media/Walk/Take 001_animation.ozz",
			"../../media/Attack/Take 001_animation.ozz",
#endif
		};
		static_assert(JY_ARRAY_COUNT(filenames) == kNumLayers, "Arrays mistmatch.");
		for (int i = 0; i < kNumLayers; ++i) 
		{
			Sampler& sampler = samplers_[i];

#if USE_RAW_ANIMATION
			if (!LoadFile::LoadRawAnimation(filenames[i], sampler.animation)) 
#else 
			if (!LoadFile::LoadAnimation(filenames[i], sampler.animation))
#endif
			{
				return false;
			}

			// Allocates sampler runtime buffers.
			sampler.locals.resize(num_joints);

			sampler.joint_weights.resize(num_joints);

			// Allocates a context that matches animation requirements.
			sampler.context.Resize(num_joints);
		}

		// Default weight settings.
		Sampler& lower_body_sampler = samplers_[kLowerBody];
		lower_body_sampler.weight_setting = 1.f;
		lower_body_sampler.joint_weight_setting = 0.f;

		Sampler& upper_body_sampler = samplers_[kUpperBody];
		upper_body_sampler.weight_setting = 1.f;
		upper_body_sampler.joint_weight_setting = 1.f;

		// Allocates local space runtime buffers of blended data.
		blended_locals_.resize(num_joints);

		// Allocates model space runtime buffers of blended data.
		models_.resize(num_joints);

		// Finds the "Spine1" joint in the joint hierarchy.
		upper_body_root_ = FindJoint(skeleton_, "Bip01 R Clavicle");
		if (upper_body_root_ < 0) {
			return false;
		}
		SetupPerJointWeights();

		return true;
	}

	// Helper functor used to set weights while traversing joints hierarchy.
	struct WeightSetupIterator
	{
		WeightSetupIterator(std::vector<float>* _weights,
			float _weight_setting)
			: weights(_weights), weight_setting(_weight_setting) {}
		void operator()(int _joint, int) 
		{
			float& soa_weight = weights->at(_joint);
			soa_weight = weight_setting;
		}
		std::vector<float>* weights;
		float weight_setting;
	};

	void SetupPerJointWeights()
	{
		// Setup partial animation mask. This mask is defined by a weight_setting
		// assigned to each joint of the hierarchy. Joint to disable are set to a
		// weight_setting of 0.f, and enabled joints are set to 1.f.
		// Per-joint weights of lower and upper body layers have opposed values
		// (weight_setting and 1 - weight_setting) in order for a layer to select
		// joints that are rejected by the other layer.
		Sampler& lower_body_sampler = samplers_[kLowerBody];
		Sampler& upper_body_sampler = samplers_[kUpperBody];

		// Disables all joints: set all weights to 0.
		for (int i = 0; i < skeleton_.num_joints(); ++i) 
		{
			lower_body_sampler.joint_weights[i] = 1;
			upper_body_sampler.joint_weights[i] = 0;
		}

		// Sets the weight_setting of all the joints children of the lower and upper
		// body weights. Note that they are stored in SoA format.
		WeightSetupIterator lower_it(&lower_body_sampler.joint_weights,
			lower_body_sampler.joint_weight_setting);
		IterateJointsDF(skeleton_, lower_it, upper_body_root_);

		WeightSetupIterator upper_it(&upper_body_sampler.joint_weights,
			upper_body_sampler.joint_weight_setting);
		IterateJointsDF(skeleton_, upper_it, upper_body_root_);
	}

	virtual void OnDestroy() {}

	virtual bool OnGui()
	{
		ImGui::Begin("Sample");

		if (ImGui::TreeNode("Root"))
		{
			ImGui::Text("Root of the upper body hierarchy:");

			static float coeff = 1.f;  // All power to the partial animation.
			if (ImGui::SliderInt(skeleton_.joint_names()[upper_body_root_].c_str(), &upper_body_root_, 0, skeleton_.num_joints() - 1))
			{
				SetupPerJointWeights();
			}

			ImGui::TreePop();
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNode("Animation control"))
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5);

			for (int i = 0; i < kNumLayers; ++i)
			{
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
				Sampler& sampler = samplers_[i];
				sampler.controller.OnGui(sampler.animation, i + 1);
			}

			ImGui::PopItemFlag();
			ImGui::PopStyleVar();

			ImGui::TreePop();
		}

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

	// The number of layers to blend.
	enum {
		kLowerBody = 0,
		kUpperBody = 1,
		kNumLayers = 2,
	};

	struct Sampler 
	{
		// Constructor, default initialization.
		Sampler() : weight_setting(1.f), joint_weight_setting(1.f) {}

		// Playback animation controller. This is a utility class that helps with
		// controlling animation playback time.
		PlaybackController controller;

		// Blending weight_setting for the layer.
		float weight_setting;

		// Blending weight_setting setting of the joints of this layer that are
		// affected
		// by the masking.
		float joint_weight_setting;

#if USE_RAW_ANIMATION
		RawAnimation animation;
		RawAnimationJob::Context context;
#else 
		Animation animation;
		AnimationJob::Context context;
#endif

		// Buffer of local transforms as sampled from animation_.
		std::vector<Math::Transform> locals;

		// Per-joint weights used to define the partial animation mask. Allows to
		// select which joints are considered during blending, and their individual
		// weight_setting.
		std::vector<float> joint_weights;
	} samplers_[kNumLayers];  // kNumLayers animations to blend.

	// Index of the joint at the base of the upper body hierarchy.
	int upper_body_root_;

	// Blending job rest pose threshold.
	float threshold_;

	// Buffer of local transforms which stores the blending result.
	std::vector<Math::Transform> blended_locals_;

	// Buffer of model space matrices. These are computed by the local-to-model
	// job after the blending stage.
	std::vector<Math::Mat4> models_;
};

int main(int _argc, const char** _argv) 
{
	const char* title = "Partial_blend";
	return PlaybackSampleApplication().Run(_argc, _argv, "1.0", title);
}
