#include "Utils.h"
#include "Skeleton.h"
#include "LocalToModelJob.h"

void ComputeSkeletonBounds(const Skeleton& _skeleton, Math::AABB* _bound) 
{
	using Math::Mat4;

	assert(_bound);

	// Set a default box.
	*_bound = Math::AABB();

	const int num_joints = _skeleton.num_joints();
	if (!num_joints) {
		return;
	}

	// Allocate matrix array, out of memory is handled by the LocalToModelJob.
	std::vector<Math::Mat4> models(num_joints);

	// Compute model space rest pose.
	LocalToModelJob job;
	job.input = make_span(_skeleton.joint_rest_poses());
	job.output = make_span(models);
	job.skeleton = &_skeleton;
	if (job.Run()) 
	{
		// Forwards to posture function.
		ComputePostureBounds(job.output, _bound);
	}
}

// Loop through matrices and collect min and max bounds.
void ComputePostureBounds(span<const Math::Mat4> _matrices, Math::AABB* _bound)
{
	assert(_bound);

	// Set a default box.
	*_bound = Math::AABB();

	if (_matrices.empty()) {
		return;
	}

	// Loops through matrices and stores min/max.
	// Matrices array cannot be empty, it was checked at the beginning of the
	// function.
	const Math::Mat4* current = _matrices.begin();
	Math::Vec3 min(current->a41, current->a42, current->a43);
	Math::Vec3 max(current->a41, current->a42, current->a43);
	++current;
	while (current < _matrices.end()) 
	{
		min = Math::Vec3(Math::Min(min.x, current->a41), Math::Min(min.y, current->a42), Math::Min(min.z, current->a43));
		max = Math::Vec3(Math::Max(max.x, current->a41), Math::Max(max.y, current->a42), Math::Max(max.z, current->a43));
		++current;
	}

	// Stores in math::Box structure.
	_bound->_max = max;
	_bound->_min = min;

	return;
}