#pragma once

#include "../Math/3DMath.h"
#include "span.h"

class Skeleton;

// Computes the bounding box of _skeleton. This is the box that encloses all
// skeleton's joints in model space.
// _bound must be a valid Math::AABB instance.
void ComputeSkeletonBounds(const Skeleton& _skeleton, Math::AABB* _bound);

// Computes the bounding box of posture defines be _matrices range.
// _bound must be a valid Math::AABB instance.
void ComputePostureBounds(span<const Math::Mat4> _matrices, Math::AABB* _bound);