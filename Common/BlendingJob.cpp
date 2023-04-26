#include "BlendingJob.h"
#include "Skeleton.h"

BlendingJob::Layer::Layer() : weight(0.f) {}

BlendingJob::BlendingJob() : threshold(.1f) {}

namespace 
{
	bool ValidateLayer(const BlendingJob::Layer& _layer, size_t _min_range) 
	{
		bool valid = true;

		// Tests transforms validity.
		valid &= _layer.transform.size() >= _min_range;

		// Joint weights are optional.
		if (!_layer.joint_weights.empty()) {
			valid &= _layer.joint_weights.size() >= _min_range;
		}
		else {
			valid &= _layer.joint_weights.empty();
		}
		return valid;
	}
}  // namespace

bool BlendingJob::Validate() const 
{
	// Don't need any early out, as jobs are valid in most of the performance
	// critical cases.
	// Tests are written in multiple lines in order to avoid branches.
	bool valid = true;

	// Test for valid threshold).
	valid &= threshold > 0.f;

	// Test for nullptr begin pointers.
	// Blending layers are mandatory, additive aren't.
	valid &= !rest_pose.empty();
	valid &= !output.empty();

	// The rest pose size defines the ranges of transforms to blend, so all
	// other buffers should be bigger.
	const size_t min_range = rest_pose.size();
	valid &= output.size() >= min_range;

	// Validates layers.
	for (const Layer& layer : layers) {
		valid &= ValidateLayer(layer, min_range);
	}

	// Validates additive layers.
	for (const Layer& layer : additive_layers) {
		valid &= ValidateLayer(layer, min_range);
	}

	return valid;
}

namespace
{
	static Math::Transform __BlendTransform(const Math::Transform& lhs, const Math::Transform& rhs, float t)
	{
		Math::Transform ret;
		ret.m_translation = lhs.m_translation * (1.0 - t) + rhs.m_translation * t;
		ret.m_scale = lhs.m_scale * (1.0 - t) + rhs.m_scale * t;
		ret.m_rotation = slerp(lhs.m_rotation, rhs.m_rotation, t);
		return ret;
	}
	static void __AddTransform(const Math::Transform& in, Math::Transform& out, float w)
	{
		out.m_translation += in.m_translation * w;
		out.m_scale = out.m_scale * (Math::Vec3(1-w) + (in.m_scale * w));
		out.m_rotation = (in.m_rotation * w).GetNormalized() * out.m_rotation;
	}

	// Defines parameters that are passed through blending stages.
	struct ProcessArgs 
	{
		ProcessArgs(const BlendingJob& _job)
			: job(_job),
			num_soa_joints(_job.rest_pose.size()),
			num_passes(0),
			num_partial_passes(0),
			accumulated_weight(0.f) 
		{
			// The range of all buffers has already been validated.
			assert(job.output.size() >= num_soa_joints);
			assert(JY_ARRAY_COUNT(accumulated_weights) >= num_soa_joints);
		}

		// Allocates enough space to store a accumulated weights per-joint.
		// It will be initialized by the first pass processed, if any.
		// This is quite big for a stack allocation (4 byte * maximum number of
		// joints). This is one of the reasons why the number of joints is limited
		// by the API.
		// Note that this array is used with SoA data.
		// This is the first argument in order to avoid wasting too much space with
		// alignment padding.
		float accumulated_weights[Skeleton::kMaxSoAJoints];

		// The job to process.
		const BlendingJob& job;

		// The number of transforms to process as defined by the size of the rest
		// pose.
		size_t num_soa_joints;

		// Number of processed blended passes (excluding passes with a weight <= 0.f),
		// including partial passes.
		int num_passes;

		// Number of processed partial blending passes (aka with a weight per-joint).
		int num_partial_passes;

		// The accumulated weight of all layers.
		float accumulated_weight;

	private:
		// Disables assignment operators.
		ProcessArgs(const ProcessArgs&);
		void operator=(const ProcessArgs&);
	};

	// Blends all layers of the job to its output.
	void BlendLayers(ProcessArgs* _args) 
	{
		assert(_args);

		// Iterates through all layers and blend them to the output.
		for (const BlendingJob::Layer& layer : _args->job.layers)
		{
			// Asserts buffer sizes, which must never fail as it has been validated.
			assert(layer.transform.size() >= _args->num_soa_joints);
			assert(layer.joint_weights.empty() || (layer.joint_weights.size() >= _args->num_soa_joints));

			// Skip irrelevant layers.
			if (layer.weight <= 0.f) 
			{
				continue;
			}

			// Accumulates global weights.
			_args->accumulated_weight += layer.weight;
			const float layer_weight = (layer.weight);

			if (!layer.joint_weights.empty()) 
			{
				// This layer has per-joint weights.
				++_args->num_partial_passes;

				if (_args->num_passes == 0) 
				{
					for (size_t i = 0; i < _args->num_soa_joints; ++i) 
					{
						const Math::Transform& src = layer.transform[i];
						Math::Transform* dest = _args->job.output.begin() + i;
						const float bp_weight = layer_weight * layer.joint_weights[i];
						_args->accumulated_weights[i] = bp_weight;
						*dest = src;
					}
				}
				else
				{
					for (size_t i = 0; i < _args->num_soa_joints; ++i)
					{
						const Math::Transform& src = layer.transform[i];
						Math::Transform* dest = _args->job.output.begin() + i;
						const float bp_weight = layer_weight * layer.joint_weights[i];
						_args->accumulated_weights[i] += bp_weight;
						float temp = bp_weight / _args->accumulated_weights[i];
						*dest = __BlendTransform(*dest, src, temp);
					}
				}
			}
			else 
			{
				// This is a full layer.
				if (_args->num_passes == 0) 
				{
					for (size_t i = 0; i < _args->num_soa_joints; ++i) 
					{
						const Math::Transform& src = layer.transform[i];
						Math::Transform* dest = _args->job.output.begin() + i;
						*dest = src;
					}
				}
				else 
				{
					for (size_t i = 0; i < _args->num_soa_joints; ++i) 
					{
						const Math::Transform& src = layer.transform[i];
						Math::Transform* dest = _args->job.output.begin() + i;
						*dest = __BlendTransform(*dest, src, layer_weight / _args->accumulated_weight);
					}
				}
			}
			// One more pass blended.
			++_args->num_passes;
		}
	}

	// Process additive blending pass.
	void AddLayers(ProcessArgs* _args) 
	{
		// Iterates through all layers and blend them to the output.
		for (const BlendingJob::Layer& layer : _args->job.additive_layers) 
		{
			// Asserts buffer sizes, which must never fail as it has been validated.
			assert(layer.transform.size() >= _args->num_soa_joints);
			assert(layer.joint_weights.empty() ||
				(layer.joint_weights.size() >= _args->num_soa_joints));

			// Prepares constants.
			const float one = 1.0f;

			if (layer.weight > 0.f) 
			{
				// Weight is positive, need to perform additive blending.
				const float layer_weight = layer.weight;

				if (!layer.joint_weights.empty()) 
				{
					// This layer has per-joint weights.
					for (size_t i = 0; i < _args->num_soa_joints; ++i) 
					{
						const Math::Transform& src = layer.transform[i];
						Math::Transform& dest = _args->job.output[i];
						const float weight = layer_weight * layer.joint_weights[i];
						//const float one_minus_weight = one - weight;
						//const Math::Vec3 one_minus_weight_f3 = { one_minus_weight, one_minus_weight, one_minus_weight };
						__AddTransform(src, dest, weight);
					}
				}
				else 
				{
					// This is a full layer.
					for (size_t i = 0; i < _args->num_soa_joints; ++i) 
					{
						const Math::Transform& src = layer.transform[i];
						Math::Transform& dest = _args->job.output[i];
						__AddTransform(src, dest, layer_weight);
					}
				}
			}
			else if (layer.weight < 0.f) 
			{
#if 0
				// Weight is negative, need to perform subtractive blending.
				const math::SimdFloat4 layer_weight =
					math::simd_float4::Load1(-layer.weight);

				if (!layer.joint_weights.empty()) {
					// This layer has per-joint weights.
					for (size_t i = 0; i < _args->num_soa_joints; ++i) {
						const Math::Transform& src = layer.transform[i];
						Math::Transform& dest = _args->job.output[i];
						const math::SimdFloat4 weight =
							layer_weight * math::Max0(layer.joint_weights[i]);
						const math::SimdFloat4 one_minus_weight = one - weight;
						OZZ_SUB_PASS(src, weight, dest);
					}
				}
				else {
					// This is a full layer.
					const math::SimdFloat4 one_minus_weight = one - layer_weight;
					for (size_t i = 0; i < _args->num_soa_joints; ++i) {
						const Math::Transform& src = layer.transform[i];
						Math::Transform& dest = _args->job.output[i];
						OZZ_SUB_PASS(src, layer_weight, dest);
					}
				}
#endif
			}
			else 
			{
				// Skip layer as its weight is 0.
			}
		}
	}
}  // namespace

bool BlendingJob::Run() const 
{
	if (!Validate()) 
	{
		return false;
	}

	// Initializes blended parameters that are exchanged across blend stages.
	ProcessArgs process_args(*this);

	// Blends all layers to the job output buffers.
	BlendLayers(&process_args);

	// Process additive blending.
	//AddLayers(&process_args);

	return true;
}