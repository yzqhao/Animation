#pragma once

#include "../Math/3DMath.h"
#include "span.h"

struct SkinningJob
{
	// Default constructor, initializes default values.
	SkinningJob();

	// Validates job parameters.
	// Returns true for a valid job, false otherwise:
	// - if any range is invalid. See each range description.
	// - if normals are provided but positions aren't.
	// - if tangents are provided but normals aren't.
	// - if no output is provided while an input is. For example, if input normals
	// are provided, then output normals must also.
	bool Validate() const;

	// Runs job's skinning task.
	// The job is validated before any operation is performed, see Validate() for
	// more details.
	// Returns false if *this job is not valid.
	bool Run() const;

	// Number of vertices to transform. All input and output arrays must store at
	// least this number of vertices.
	int vertex_count;

	// Maximum number of joints influencing each vertex. Must be greater than 0.
	// The number of influences drives how joint_indices and joint_weights are
	// sampled:
	// - influences_count joint indices are red from joint_indices for each
	// vertex.
	// - influences_count - 1 joint weights are red from joint_weightrs for each
	// vertex. The weight of the last joint is restored (weights are normalized).
	int influences_count;

	// Array of matrices for each joint. Joint are indexed through indices array.
	span<const Math::Mat4> joint_matrices;

	// 每个关节的可选逆转置矩阵数组。如果提供，此数组用于转换向量（法线和切线），否则使用 joint_matrices 数组。
	// 如红皮书此处 (http://www.glprogramming.com/red/appendixf.html) 所述，当变换矩阵具有缩放或剪切时，变换法线需要特别注意。
	// 在这种情况下，正确的变换是通过变换点的变换的逆转置来完成的。任何旋转矩阵都很好。
	// 这些矩阵是可选的，因为它们的计算成本可能很高，并且还会落入蒙皮算法中成本更高的代码路径。
	span<const Math::Mat4> joint_inverse_transpose_matrices;

	// Array of joints indices. This array is used to indexes matrices in joints
	// array.
	// Each vertex has influences_max number of indices, meaning that the size of
	// this array must be at least influences_max * vertex_count.
	span<const uint16_t> joint_indices;
	size_t joint_indices_stride;

	// Array of joints weights. This array is used to associate a weight to every
	// joint that influences a vertex. The number of weights required per vertex
	// is "influences_max - 1". The weight for the last joint (for each vertex) is
	// restored at runtime thanks to the fact that the sum of the weights for each
	// vertex is 1.
	// Each vertex has (influences_max - 1) number of weights, meaning that the
	// size of this array must be at least (influences_max - 1)* vertex_count.
	span<const float> joint_weights;
	size_t joint_weights_stride;

	// Input vertex positions array (3 float values per vertex) and stride (number
	// of bytes between each position).
	// Array length must be at least vertex_count * in_positions_stride.
	span<const float> in_positions;
	size_t in_positions_stride;

	// Input vertex normals (3 float values per vertex) array and stride (number
	// of bytes between each normal).
	// Array length must be at least vertex_count * in_normals_stride.
	span<const float> in_normals;
	size_t in_normals_stride;

	// Input vertex tangents (3 float values per vertex) array and stride (number
	// of bytes between each tangent).
	// Array length must be at least vertex_count * in_tangents_stride.
	span<const float> in_tangents;
	size_t in_tangents_stride;

	// Output vertex positions (3 float values per vertex) array and stride
	// (number of bytes between each position).
	// Array length must be at least vertex_count * out_positions_stride.
	span<float> out_positions;
	size_t out_positions_stride;

	// Output vertex normals (3 float values per vertex) array and stride (number
	// of bytes between each normal).
	// Note that output normals are not normalized by the skinning job. This task
	// should be handled by the application, who knows if transform matrices have
	// uniform scale, and if normals are re-normalized later in the rendering
	// pipeline (shader vertex transformation stage).
	// Array length must be at least vertex_count * out_normals_stride.
	span<float> out_normals;
	size_t out_normals_stride;

	// Output vertex positions (3 float values per vertex) array and stride
	// (number of bytes between each tangent).
	// Like normals, Note that output tangents are not normalized by the skinning
	// job.
	// Array length must be at least vertex_count * out_tangents_stride.
	span<float> out_tangents;
	size_t out_tangents_stride;
};