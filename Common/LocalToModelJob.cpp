#include "LocalToModelJob.h"
#include "../Math/3DMath.h"
#include "Skeleton.h"

LocalToModelJob::LocalToModelJob()
	: skeleton(nullptr),
	root(nullptr),
	from(Skeleton::kNoParent),
	to(Skeleton::kMaxJoints),
	from_excluded(false) {}

bool LocalToModelJob::Validate() const
{
	// Don't need any early out, as jobs are valid in most of the performance
	// critical cases.
	// Tests are written in multiple lines in order to avoid branches.
	bool valid = true;

	if (!skeleton) 
	{
		return false;
	}

	const size_t num_joints = static_cast<size_t>(skeleton->num_joints());

	valid &= input.size() >= num_joints;
	valid &= output.size() >= num_joints;

	return valid;
}

bool LocalToModelJob::Run() const
{
	if (!Validate()) {
		return false;
	}

	const std::vector<int16_t>& parents = skeleton->joint_parents();

	// Initializes an identity matrix that will be used to compute roots model
	// matrices without requiring a branch.
	const Math::Mat4 identity = Math::Mat4::IDENTITY;
	const Math::Mat4* root_matrix = (root == nullptr) ? &identity : root;

	const int end = Math::Min(to + 1, skeleton->num_joints());
	for (int i = 0; i < parents.size(); ++i) 
	{
		// Builds soa matrices from soa transforms.
		const Math::Transform& transform = input[i];
		const Math::Mat4 local_aos_matrices = Math::MathUtil::Transformation(
			transform.m_scale, transform.m_rotation, transform.m_translation);

		// parents[i] >= from is true as long as "i" is a child of "from".
		const int parent = parents[i];
		const Math::Mat4* parent_matrix =
			parent == Skeleton::kNoParent ? root_matrix : &output[parent];
		output[i] = local_aos_matrices  * (*parent_matrix);
	}

	return true;
}