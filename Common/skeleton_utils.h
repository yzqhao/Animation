

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_SKELETON_UTILS_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_SKELETON_UTILS_H_

#include <cassert>

#include "skeleton.h"

#include "../Math/3DMath.h"

// Get rest-pose of a skeleton joint.
Math::Transform GetJointLocalRestPose(
    const Skeleton& _skeleton, int _joint);

// Test if a joint is a leaf. _joint number must be in range [0, num joints].
// "_joint" is a leaf if it's the last joint, or next joint's parent isn't
// "_joint".
inline bool IsLeaf(const Skeleton& _skeleton, int _joint) {
  const int num_joints = _skeleton.num_joints();
  assert(_joint >= 0 && _joint < num_joints && "_joint index out of range");
  const std::vector<int16_t>& parents = _skeleton.joint_parents();
  const int next = _joint + 1;
  return next == num_joints || parents[next] != _joint;
}

// Finds joint index by name. Uses a case sensitive comparison.
int FindJoint(const Skeleton& _skeleton, const char* _name);

// Applies a specified functor to each joint in a depth-first order.
// _Fct is of type void(int _current, int _parent) where the first argument
// is the child of the second argument. _parent is kNoParent if the _current
// joint is a root. _from indicates the joint from which the joint hierarchy
// traversal begins. Use Skeleton::kNoParent to traverse the whole
// hierarchy, in case there are multiple roots.
template <typename _Fct>
inline _Fct IterateJointsDF(const Skeleton& _skeleton, _Fct _fct,
                            int _from = Skeleton::kNoParent) {
  const std::vector<int16_t>& parents = _skeleton.joint_parents();
  const int num_joints = _skeleton.num_joints();
  //
  // parents[i] >= _from is true as long as "i" is a child of "_from".
  static_assert(Skeleton::kNoParent < 0,
                "Algorithm relies on kNoParent being negative");
  for (int i = _from < 0 ? 0 : _from, process = i < num_joints; process;
       ++i, process = i < num_joints && parents[i] >= _from) {
    _fct(i, parents[i]);
  }
  return _fct;
}

// Applies a specified functor to each joint in a reverse (from leaves to root)
// depth-first order. _Fct is of type void(int _current, int _parent) where the
// first argument is the child of the second argument. _parent is kNoParent if
// the _current joint is a root.
template <typename _Fct>
inline _Fct IterateJointsDFReverse(const Skeleton& _skeleton, _Fct _fct) {
  const span<const int16_t>& parents = _skeleton.joint_parents();
  for (int i = _skeleton.num_joints() - 1; i >= 0; --i) {
    _fct(i, parents[i]);
  }
  return _fct;
}

#endif  // OZZ_OZZ_ANIMATION_RUNTIME_SKELETON_UTILS_H_
