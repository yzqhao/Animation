
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

class PlaybackSampleApplication : public Application {
public:
	PlaybackSampleApplication()
		: blend_ratio_(.3f)
		, manual_(false)
		, threshold_(BlendingJob().threshold) {}

protected:
	// Updates current animation time and skeleton pose.
	virtual bool OnUpdate(float _dt, float) 
	{
		// Updates blending parameters and synchronizes animations if control mode
		// is not manual.
		if (!manual_) 
		{
			UpdateRuntimeParameters();
		}

		// Updates and samples all animations to their respective local space
		// transform buffers.
		for (int i = 0; i < kNumLayers; ++i)
		{
			Sampler& sampler = samplers_[i];

			// Updates animations time.
			sampler.controller.Update(sampler.animation, _dt);

			// Early out if this sampler weight makes it irrelevant during blending.
			if (samplers_[i].weight <= 0.f) 
			{
				continue;
			}

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
			layers[i].weight = samplers_[i].weight;
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

	// Computes blending weight and synchronizes playback speed when the "manual"
	// option is off.
	void UpdateRuntimeParameters() 
	{
		// Computes weight parameters for all samplers.
		const float kNumIntervals = kNumLayers - 1;
		const float kInterval = 1.f / kNumIntervals;
		for (int i = 0; i < kNumLayers; ++i) 
		{
			const float med = i * kInterval;
			const float x = blend_ratio_ - med;
			const float y = ((x < 0.f ? x : -x) + kInterval) * kNumIntervals;
			samplers_[i].weight = Math::Max(0.f, y);
		}

		// Synchronizes animations.
		// First computes loop cycle duration. Selects the 2 samplers that define
		// interval that contains blend_ratio_.
		// Uses a maximum value smaller that 1.f (-epsilon) to ensure that
		// (relevant_sampler + 1) is always valid.
		const int relevant_sampler = static_cast<int>((blend_ratio_ - 1e-3f) * (kNumLayers - 1));
		assert(relevant_sampler + 1 < kNumLayers);
		Sampler& sampler_l = samplers_[relevant_sampler];
		Sampler& sampler_r = samplers_[relevant_sampler + 1];

		// Interpolates animation durations using their respective weights, to
		// find the loop cycle duration that matches blend_ratio_.
		const float loop_duration = sampler_l.animation.Duration() * sampler_l.weight + sampler_r.animation.Duration() * sampler_r.weight;

		// Finally finds the speed coefficient for all samplers.
		const float inv_loop_duration = 1.f / loop_duration;
		for (int i = 0; i < kNumLayers; ++i) 
		{
			Sampler& sampler = samplers_[i];
			const float speed = sampler.animation.Duration() * inv_loop_duration;
			sampler.controller.set_playback_speed(speed);
		}
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
		if (!LoadFile::LoadSkeleton("../../media/Monster/skeleton.ozz", skeleton_))	//"../../media/blend_test/skeleton.ozz"
		{
			return false;
		}

		const int num_joints = skeleton_.num_joints();

		// Reading animations.
		const char* filenames[] = 
		{
#if USE_RAW_ANIMATION
			"../../media/Monster/Take 001_raw_animation.ozz", 
			"../../media/Walk/Take 001_raw_animation.ozz",
			"../../media/Attack/Take 001_raw_animation.ozz",
#else 
			"../../media/Monster/Take 001_animation.ozz",
			"../../media/Walk/Take 001_animation.ozz",
			"../../media/Attack/Take 001_animation.ozz",
#endif
			//"../../media/blend_test/raw_animation1.ozz", 
			//"../../media/blend_test/raw_animation2.ozz",
			//"../../media/blend_test/raw_animation3.ozz",
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

			// Allocates a context that matches animation requirements.
			sampler.context.Resize(num_joints);
		}

		// Allocates local space runtime buffers of blended data.
		blended_locals_.resize(num_joints);

		// Allocates model space runtime buffers of blended data.
		models_.resize(num_joints);

		return true;
	}

	virtual void OnDestroy() {}

	virtual bool OnGui()
	{
		ImGui::Begin("Sample");

		if (ImGui::TreeNode("Blending parameters"))
		{
			if (ImGui::Checkbox("Manual settings", &manual_) && !manual_)
			{
				for (int i = 0; i < kNumLayers; ++i)
				{
					Sampler& sampler = samplers_[i];
					sampler.controller.Reset();
				}
			}

			if (manual_)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5);
			}
			ImGui::SliderFloat("Blend ratio", &blend_ratio_, 0.0f, 1.0f, "%.2f");
			if (manual_)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}

			for (int i = 0; i < kNumLayers; ++i) 
			{
				Sampler& sampler = samplers_[i];
				if (!manual_)
				{
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5);
				}
				ImGui::SliderFloat(std::string("Weight" + std::to_string(i+1)).c_str(), &sampler.weight, 0.0f, 1.0f, "%.2f");
				if (!manual_)
				{
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			}

			ImGui::SliderFloat("Threshold", &threshold_, 0.01f, 1.0f, "%.2f");

			ImGui::TreePop();
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNode("Animation control"))
		{
			if (!manual_)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5);
			}
			
			for (int i = 0; i < kNumLayers; ++i)
			{
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
				Sampler& sampler = samplers_[i];
				sampler.controller.OnGui(sampler.animation, i+1);
			}

			if (!manual_)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}
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

	// Global blend ratio in range [0,1] that controls all blend parameters and
	// synchronizes playback speeds. A value of 0 gives full weight to the first
	// animation, and 1 to the last.
	float blend_ratio_;

	// Switch to manual control of animations and blending parameters.
	bool manual_;

	// The number of layers to blend.
	enum {
		kNumLayers = 3,
	};

	// Sampler structure contains all the data required to sample a single
	// animation.
	struct Sampler {
		// Constructor, default initialization.
		Sampler() : weight(1.f) {}

		// Playback animation controller. This is a utility class that helps with
		// controlling animation playback time.
		PlaybackController controller;

		// Blending weight for the layer.
		float weight;

		// Runtime animation.
#if USE_RAW_ANIMATION
		RawAnimation animation;
		RawAnimationJob::Context context;
#else 
		Animation animation;
		AnimationJob::Context context;
#endif

		// Buffer of local transforms as sampled from animation_.
		std::vector<Math::Transform> locals;
	} samplers_[kNumLayers];  // kNumLayers animations to blend.

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
	const char* title = "Blend";
	return PlaybackSampleApplication().Run(_argc, _argv, "1.0", title);
}
