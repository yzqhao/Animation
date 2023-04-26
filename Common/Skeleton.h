#pragma once

#include "../System/Macros.h"
#include "../Math/3DMath.h"

class Skeleton
{
    friend class LoadFile;
public:
    Skeleton();
	~Skeleton();

	// Defines Skeleton constant values.
	enum Constants 
	{

		// Defines the maximum number of joints.
		// This is limited in order to control the number of bits required to store
		// a joint index. Limiting the number of joints also helps handling worst
		// size cases, like when it is required to allocate an array of joints on
		// the stack.
		kMaxJoints = 1024,

		// Defines the maximum number of SoA elements required to store the maximum
		// number of joints.
		kMaxSoAJoints = (kMaxJoints + 3) / 4,

		// Defines the index of the parent of the root joint (which has no parent in
		// fact).
		kNoParent = -1,
	};

	int num_joints() const { return joint_names_.size(); }

	// Returns joint's parent indices range.
	const std::vector<int16_t>& joint_parents() const { return joint_parents_; }
	
	// Returns joint's rest poses. Rest poses are stored in soa format.
	const std::vector<Math::Transform>& joint_rest_poses() const {
		return joint_rest_poses_;
	}

	// Returns joint's name collection.
	const std::vector<std::string>& joint_names() const {
		return joint_names_;
	}
private:

	// Rest pose of every joint in local space.
	std::vector<Math::Transform> joint_rest_poses_;

	// Array of joint parent indexes.
	std::vector<int16_t> joint_parents_;

	// Stores the name of every joint in an array of c-strings.
	std::vector<std::string> joint_names_;
};