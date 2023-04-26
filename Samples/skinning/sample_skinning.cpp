
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
#include "../Common/Mesh.h"

class SkinningSampleApplication : public Application {
public:
	SkinningSampleApplication() {}

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
		bool success = true;
		const Math::Mat4 transform = Math::Mat4::IDENTITY;

		if (draw_skeleton_)
		{
			success &= _renderer->DrawPosture(skeleton_, (models_), transform);
		}

		if (draw_mesh_)
		{
			// Builds skinning matrices, based on the output of the animation stage.
			// The mesh might not use (aka be skinned by) all skeleton joints. We
			// use the joint remapping table (available from the mesh object) to
			// reorder model-space matrices and build skinning ones.
			for (const Mesh& mesh : meshes_)
			{
				for (size_t i = 0; i < mesh.joint_remaps.size(); ++i)
				{
					skinning_matrices_[i] = mesh.inverse_bind_poses[i] * models_[mesh.joint_remaps[i]];
				}

				// Renders skin.
				success &= _renderer->DrawSkinnedMesh(
					mesh, make_span(skinning_matrices_), transform, render_options_);
			}
		}
		return success;
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

		if (!LoadFile::LoadMesh("../../media/Attack/Attack.mesh", meshes_))
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

		// Computes the number of skinning matrices required to skin all meshes.
		// A mesh is skinned by only a subset of joints, so the number of skinning
		// matrices might be less that the number of skeleton joints.
		// Mesh::joint_remaps is used to know how to order skinning matrices. So
		// the number of matrices required is the size of joint_remaps.
		size_t num_skinning_matrices = 0;
		for (const Mesh& mesh : meshes_)
		{
			num_skinning_matrices = Math::Max(num_skinning_matrices, mesh.joint_remaps.size());
		}

		// Allocates skinning matrices.
		skinning_matrices_.resize(num_skinning_matrices);

		// Check the skeleton matches with the mesh, especially that the mesh
		// doesn't expect more joints than the skeleton has.
		for (const Mesh& mesh : meshes_)
		{
			if (num_joints < mesh.highest_joint_index())
			{
				std::cout << "The provided mesh doesn't match skeleton "
					"(joint count mismatch)."
					<< std::endl;
				return false;
			}
		}

		return true;
	}

	virtual void OnDestroy() {}

	virtual bool OnGui()
	{
		ImGui::Begin("Sample");

		controller_.OnGui(animation_);

		if (ImGui::TreeNode("Display options"))
		{
			ImGui::Checkbox("Draw skeleton", &draw_skeleton_);
			ImGui::Checkbox("Draw mesh", &draw_mesh_);

			if (ImGui::TreeNode("Rendering options"))
			{
				ImGui::Checkbox("Show triangles", &render_options_.triangles);
				ImGui::Checkbox("Show texture", &render_options_.texture);
				ImGui::Checkbox("Show vertices", &render_options_.vertices);
				ImGui::Checkbox("Show normals", &render_options_.normals);
				ImGui::Checkbox("Show tangents", &render_options_.tangents);
				ImGui::Checkbox("Show binormals", &render_options_.binormals);
				ImGui::Checkbox("Show colors", &render_options_.colors);
				ImGui::Checkbox("Wireframe", &render_options_.wireframe);
				ImGui::Checkbox("Skip skinning", &render_options_.skip_skinning);

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		ImGui::End();

		return true;
	}

	virtual void GetSceneBounds(Math::AABB* _bound) const
	{
		ComputeSkeletonBounds(skeleton_, _bound);
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

	// Buffer of skinning matrices, result of the joint multiplication of the
	// inverse bind pose with the model space matrix.
	std::vector<Math::Mat4> skinning_matrices_;

	// The mesh used by the sample.
	std::vector<Mesh> meshes_;

	// Redering options.
	bool draw_skeleton_ = false;
	bool draw_mesh_ = true;

	// Mesh rendering options.
	Renderer::Options render_options_;
};

int main(int _argc, const char** _argv)
{
	const char* title = "Skinning";
	return SkinningSampleApplication().Run(_argc, _argv, "1.0", title);
}
