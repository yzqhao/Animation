#pragma once

#include "span.h"
#include "../Math/3DMath.h"

struct BlendingJob 
{
	// Default constructor, initializes default values.
	BlendingJob();

	// Validates job parameters.
	// Returns true for a valid job, false otherwise:
	// -if layer range is not valid (can be empty though).
	// -if additive layer range is not valid (can be empty though).
	// -if any layer is not valid.
	// -if output range is not valid.
	// -if any buffer (including layers' content : transform, joint weights...) is
	// smaller than the rest pose buffer.
	// -if the threshold value is less than or equal to 0.f.
	bool Validate() const;

	// Runs job's blending task.
	// The job is validated before any operation is performed, see Validate() for
	// more details.
	// Returns false if *this job is not valid.
	bool Run() const;

	// Defines a layer of blending input data (local space transforms) and
	// parameters (weights).
	struct Layer 
	{
		// Default constructor, initializes default values.
		Layer();

		// Blending weight of this layer. Negative values are considered as 0.
		// Normalization is performed during the blending stage so weight can be in
		// any range, even though range [0:1] is optimal.
		float weight;

		// The range [begin,end[ of input layer posture. This buffer expect to store
		// local space transforms, that are usually outputted from a sampling job.
		// This range must be at least as big as the rest pose buffer, even though
		// only the number of transforms defined by the rest pose buffer will be
		// processed.
		span<const Math::Transform> transform;

		// Optional range [begin,end[ of blending weight for each joint in this
		// layer.
		// If both pointers are nullptr (default case) then per joint weight
		// blending is disabled. A valid range is defined as being at least as big
		// as the rest pose buffer, even though only the number of transforms
		// defined by the rest pose buffer will be processed. When a layer doesn't
		// specifies per joint weights, then it is implicitly considered as
		// being 1.f. This default value is a reference value for the normalization
		// process, which implies that the range of values for joint weights should
		// be [0,1]. Negative weight values are considered as 0, but positive ones
		// aren't clamped because they could exceed 1.f if all layers contains valid
		// joint weights.
		span<const float> joint_weights;
	};

	// The job blends the rest pose to the output when the accumulated weight of
	// all layers is less than this threshold value.
	// Must be greater than 0.f.
	float threshold;

	// Job input layers, can be empty or nullptr.
	// The range of layers that must be blended.
	span<const Layer> layers;

	// Job input additive layers, can be empty or nullptr.
	// The range of layers that must be added to the output.
	span<const Layer> additive_layers;

	// The skeleton rest pose. The size of this buffer defines the number of
	// transforms to blend. This is the reference because this buffer is defined
	// by the skeleton that all the animations belongs to.
	// It is used when the accumulated weight for a bone on all layers is
	// less than the threshold value, in order to fall back on valid transforms.
	span<const Math::Transform> rest_pose;

	// Job output.
	// The range of output transforms to be filled with blended layer
	// transforms during job execution.
	// Must be at least as big as the rest pose buffer, but only the number of
	// transforms defined by the rest pose buffer size will be processed.
	span<Math::Transform> output;
};