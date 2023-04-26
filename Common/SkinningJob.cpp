#include "SkinningJob.h"

SkinningJob::SkinningJob()
	: vertex_count(0),
	influences_count(0),
	joint_indices_stride(0),
	joint_weights_stride(0),
	in_positions_stride(0),
	in_normals_stride(0),
	in_tangents_stride(0),
	out_positions_stride(0),
	out_normals_stride(0),
	out_tangents_stride(0) {}

bool SkinningJob::Validate() const 
{
	// Start validation of all parameters.
	bool valid = true;

	// Checks influences bounds.
	valid &= influences_count > 0;

	// Checks joints matrices, required.
	valid &= !joint_matrices.empty();

	// Prepares local variables used to compute buffer size.
	const int vertex_count_minus_1 = vertex_count > 0 ? vertex_count - 1 : 0;
	const int vertex_count_at_least_1 = vertex_count > 0;

	// Checks indices, required.
	valid &= joint_indices.size_bytes() >=
		joint_indices_stride * vertex_count_minus_1 +
		sizeof(uint16_t) * influences_count * vertex_count_at_least_1;

	// Checks weights, required if influences_count > 1.
	if (influences_count != 1) {
		valid &=
			joint_weights.size_bytes() >=
			joint_weights_stride * vertex_count_minus_1 +
			sizeof(float) * (influences_count - 1) * vertex_count_at_least_1;
	}

	// Checks positions, mandatory.
	valid &= in_positions.size_bytes() >=
		in_positions_stride * vertex_count_minus_1 +
		sizeof(float) * 3 * vertex_count_at_least_1;
	valid &= !out_positions.empty();
	valid &= out_positions.size_bytes() >=
		out_positions_stride * vertex_count_minus_1 +
		sizeof(float) * 3 * vertex_count_at_least_1;

	// Checks normals, optional.
	if (!in_normals.empty()) {
		valid &= in_normals.size_bytes() >=
			in_normals_stride * vertex_count_minus_1 +
			sizeof(float) * 3 * vertex_count_at_least_1;
		valid &= !out_normals.empty();
		valid &= out_normals.size_bytes() >=
			out_normals_stride * vertex_count_minus_1 +
			sizeof(float) * 3 * vertex_count_at_least_1;

		// Checks tangents, optional but requires normals.
		if (!in_tangents.empty()) {
			valid &= in_tangents.size_bytes() >=
				in_tangents_stride * vertex_count_minus_1 +
				sizeof(float) * 3 * vertex_count_at_least_1;
			valid &= !out_tangents.empty();
			valid &= out_tangents.size_bytes() >=
				out_tangents_stride * vertex_count_minus_1 +
				sizeof(float) * 3 * vertex_count_at_least_1;
		}
	}
	else {
		// Tangents are not supported if normals are not there.
		valid &= in_tangents.empty();
	}

	return valid;
}

void Skinning(const SkinningJob& _job) 
{
	bool bCalNormal = !_job.in_normals.empty();
	bool bCalTangent = !_job.in_tangents.empty();
	const size_t inf = _job.influences_count - 1;

	const uint16_t* joint_indices = _job.joint_indices.begin();
	const float* in_positions = _job.in_positions.begin();
	float* out_positions = _job.out_positions.begin();
	
	const float* in_normals = bCalNormal ?  _job.in_normals.begin() : nullptr;
	float* out_normals = bCalNormal ? _job.out_normals.begin() : nullptr;
	const float* in_tangents = bCalTangent ? _job.in_tangents.begin() : nullptr;
	float* out_tangents = bCalTangent ? _job.out_tangents.begin() : nullptr;
	const float* joint_weights = inf>0 ? _job.joint_weights.begin() : nullptr;

	const int loops = _job.vertex_count;
	for (int i = 0; i < loops; ++i) 
	{
		float total_weight = 0.0f;
		const Math::Vec4 in_p = Math::Vec4(in_positions[0], in_positions[1], in_positions[2], 1.0f);
		Math::Vec3 out_p, out_n, out_t;
		for (int j = 0; j <= inf; ++j)
		{
			float weight = 0.0f;
			if (j == inf)
				weight = 1.0 - total_weight;
			else
			{
				total_weight += joint_weights[j];
				weight = joint_weights[j];
			}
			const uint16_t ij = joint_indices[j];
			const Math::Mat4& m = _job.joint_matrices[ij];

			out_p += (in_p * m).XYZ() * weight;

			if (bCalNormal)
			{
				const Math::Vec4 in_n = Math::Vec4(in_normals[0], in_normals[1], in_normals[2], 0.0f);
				out_n += (in_n * m).XYZ() * weight;
			}
			if (bCalTangent)
			{
				const Math::Vec4 in_t = Math::Vec4(in_tangents[0], in_tangents[1], in_tangents[2], 0.0f);
				out_t += (in_t * m).XYZ() * weight;
			}
		}
		out_positions[0] = out_p.x; out_positions[1] = out_p.y; out_positions[2] = out_p.z;
		if (bCalNormal)
			out_normals[0] = out_n.x, out_normals[1] = out_n.y, out_normals[2] = out_n.z;
		if (bCalTangent)
			out_tangents[0] = out_t.x, out_tangents[1] = out_t.y, out_tangents[2] = out_t.z;

		if (i == loops - 1)
			break;

		joint_indices = reinterpret_cast<const uint16_t*>(reinterpret_cast<uintptr_t>(joint_indices) + _job.joint_indices_stride);
		in_positions = reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(in_positions) + _job.in_positions_stride);
		out_positions = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(out_positions) + _job.out_positions_stride);
		if (bCalNormal)
		{
			in_normals = reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(in_normals) + _job.in_normals_stride);
			out_normals = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(out_normals) + _job.out_normals_stride);
		}
		if (bCalTangent)
		{
			in_tangents = reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(in_tangents) + _job.in_tangents_stride);
			out_tangents = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(out_tangents) + _job.out_tangents_stride);
		}
	}
}

// Implements job Run function.
bool SkinningJob::Run() const 
{
	// Exit with an error if job is invalid.
	if (!Validate()) {
		return false;
	}

	// Early out if no vertex. This isn't an error.
	// Skinning function algorithm doesn't support the case.
	if (vertex_count == 0) {
		return true;
	}

	Skinning(*this);

	return true;
}

