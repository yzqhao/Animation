
#include "skeleton_utils.h"
#include <assert.h>
#include <cstring>

int FindJoint(const Skeleton& _skeleton, const char* _name)
{
	const auto& names = _skeleton.joint_names();
	for (size_t i = 0; i < names.size(); ++i)
	{
		if (std::strcmp(names[i].c_str(), _name) == 0)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

// Unpacks skeleton rest pose stored in soa format by the skeleton.
Math::Transform GetJointLocalRestPose(const Skeleton& _skeleton, int _joint)
{
	assert(_joint >= 0 && _joint < _skeleton.num_joints() &&
		"Joint index out of range.");

	const Math::Transform& soa_transform =
		_skeleton.joint_rest_poses()[_joint / 4];

	// Transpose SoA data to AoS.
	// TODO

	// Stores to the Transform object.
	Math::Transform rest_pose;

	return rest_pose;
}


