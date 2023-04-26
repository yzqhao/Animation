#include "AnimationJob.h"
#include "Animation.h"

namespace internal 
{
	struct InterpSoaFloat3 
	{
		float ratio[2];
		Math::Vec3 value[2];
	};
	struct InterpSoaQuaternion 
	{
		float ratio[2];
		Math::Quaternion value[2];
	};
}  // namespace internal


bool AnimationJob::Validate() const
{
	// Don't need any early out, as jobs are valid in most of the performance
	// critical cases.
	// Tests are written in multiple lines in order to avoid branches.
	bool valid = true;

	// Test for nullptr pointers.
	if (!animation || !context) {
		return false;
	}
	valid &= !output.empty();

	const int num_tracks = animation->num_tracks();

	// Tests context size.
	valid &= context->max_tracks() >= num_tracks;

	return valid;
}

namespace 
{
	// Loops through the sorted key frames and update context structure.
	template <typename _Key>
	void UpdateCacheCursor(float _ratio, int num_tracks, const std::vector<_Key>& _keys, int* _cursor, std::vector<int>& _cache, std::vector<uint8_t>& _outdated)
    {
		assert(_keys.begin() + num_tracks * 2 <= _keys.end());

        const int _num_soa_tracks = (num_tracks + 3) / 4;

		size_t cursor = 0;
		if (!*_cursor) 
		{
			// Initializes interpolated entries with the first 2 sets of key frames.
			// The sorting algorithm ensures that the first 2 key frames of a track
			// are consecutive.
			for (int i = 0; i < num_tracks; ++i)
			{
				const int in_index0 = i;                   // * soa size
				const int in_index1 = in_index0 + num_tracks;  // 2nd row.
				const int out_index = i * 2;
				_cache[out_index + 0] = in_index0 + 0;
				_cache[out_index + 1] = in_index1 + 0;
			}
			cursor = num_tracks * 2;  // New cursor position.

			// All entries are outdated. It cares to only flag valid soa entries as
			// this is the exit condition of other algorithms.
			const int num_outdated_flags = (_num_soa_tracks + 7) / 8;
			for (int i = 0; i < num_outdated_flags - 1; ++i) {
				_outdated[i] = 0xff;
			}
			_outdated[num_outdated_flags - 1] = 0xff >> (num_outdated_flags * 8 - _num_soa_tracks);
		}
		else 
		{
			cursor = *_cursor;  // Might be == end()
			assert(cursor >= num_tracks * 2 && cursor <= _keys.size());
		}

		// Search for the keys that matches _ratio.
		// Iterates while the context is not updated with left and right keys required
		// for interpolation at time ratio _ratio, for all tracks. Thanks to the
		// keyframe sorting, the loop can end as soon as it finds a key greater that
		// _ratio. It will mean that all the keys lower than _ratio have been
		// processed, meaning all context entries are up to date.
		while (cursor < _keys.size() && _keys[_cache[_keys[cursor].track * 2 + 1]].ratio <= _ratio)
		{
			// Flag this soa entry as outdated.
			_outdated[_keys[cursor].track / 32] |= (1 << ((_keys[cursor].track & 0x1f) / 4));
			// Updates context.
			const int base = _keys[cursor].track * 2;
			_cache[base] = _cache[base + 1];
			_cache[base + 1] = cursor;
			// Process next key.
			++cursor;
		}
		assert(cursor <= _keys.size());

		// Updates cursor output.
		*_cursor = cursor;
	}

	template <typename _Key, typename _InterpKey, typename _Decompress>
	void UpdateInterpKeyframes(int num_tracks,
		const std::vector<_Key>& _keys,
		const std::vector<int>& _interp, std::vector<uint8_t>& _outdated,
		std::vector<_InterpKey>& _interp_keys,
		const _Decompress& _decompress) 
	{
		const int _num_soa_tracks = (num_tracks + 3) / 4;
		const int num_outdated_flags = (_num_soa_tracks + 7) / 8;
		for (int j = 0; j < num_outdated_flags; ++j) 
        {
			uint8_t outdated = _outdated[j];
			_outdated[j] = 0;  // Reset outdated entries as all will be processed.
			for (int i = j * 8; outdated; ++i, outdated >>= 1) 
            {
				if (!(outdated & 1)) {
					continue;
				}
				const int base = i * 4 * 2;  // * soa size * 2 keys

				// Decompress left side keyframes and store them in soa structures.
				const _Key& k00 = _keys[_interp[base + 0]];
				const _Key& k10 = _keys[_interp[base + 2]];
				const _Key& k20 = _keys[_interp[base + 4]];
				const _Key& k30 = _keys[_interp[base + 6]];
				_interp_keys[i * 4 + 0].ratio[0] = k00.ratio;
				_interp_keys[i * 4 + 1].ratio[0] = k10.ratio;
				_interp_keys[i * 4 + 2].ratio[0] = k20.ratio;
				_interp_keys[i * 4 + 3].ratio[0] = k30.ratio;
				_decompress(k00, &_interp_keys[i * 4 + 0].value[0]);
				_decompress(k10, &_interp_keys[i * 4 + 1].value[0]);
				_decompress(k20, &_interp_keys[i * 4 + 2].value[0]);
				_decompress(k30, &_interp_keys[i * 4 + 3].value[0]);

				// Decompress right side keyframes and store them in soa structures.
				const _Key& k01 = _keys[_interp[base + 1]];
				const _Key& k11 = _keys[_interp[base + 3]];
				const _Key& k21 = _keys[_interp[base + 5]];
				const _Key& k31 = _keys[_interp[base + 7]];
				_interp_keys[i * 4 + 0].ratio[1] = k01.ratio;
				_interp_keys[i * 4 + 1].ratio[1] = k11.ratio;
				_interp_keys[i * 4 + 2].ratio[1] = k21.ratio;
				_interp_keys[i * 4 + 3].ratio[1] = k31.ratio;
				_decompress(k01, &_interp_keys[i * 4 + 0].value[1]);
				_decompress(k11, &_interp_keys[i * 4 + 1].value[1]);
				_decompress(k21, &_interp_keys[i * 4 + 2].value[1]);
				_decompress(k31, &_interp_keys[i * 4 + 3].value[1]);
			}
		}
	}

	inline void DecompressFloat3(const Float3Key& k, Math::Vec3* float3) 
	{
		float3->x = Math::HalfToFloat(k.value[0] & 0x0000ffff);
		float3->y = Math::HalfToFloat(k.value[1] & 0x0000ffff);
		float3->z = Math::HalfToFloat(k.value[2] & 0x0000ffff);
	}

	// Defines a mapping table that defines components assignation in the output
	// quaternion.
	constexpr int kCpntMapping[4][4] = { {0, 0, 1, 2}, {0, 0, 1, 2}, {0, 1, 0, 2}, {0, 1, 2, 0} };

	void DecompressQuaternion(const QuaternionKey& _k0, Math::Quaternion* _quaternion) 
	{
		// Selects proper mapping for each key.
		const int* m0 = kCpntMapping[_k0.largest];

		// Prepares an array of input values, according to the mapping required to
		// restore quaternion largest component.
		alignas(16) int cmp_keys[4] = 
		{
			_k0.value[m0[0]],
			_k0.value[m0[1]],
			_k0.value[m0[2]],
			_k0.value[m0[3]],
		};

		// Resets largest component to 0. Overwritting here avoids 16 branchings
		// above.
		cmp_keys[_k0.largest] = 0;

		// Rebuilds quaternion from quantized values.
		static const float kSqrt2 = 1.4142135623730950488016887242097f;
		const float kInt2Float = 1.f / (32767.f * kSqrt2);
		float cpnt[4] = 
		{
			kInt2Float* cmp_keys[0],
			kInt2Float* cmp_keys[1],
			kInt2Float* cmp_keys[2],
			kInt2Float* cmp_keys[3],
		};

		const float dot = cpnt[0] * cpnt[0] + cpnt[1] * cpnt[1] +
			cpnt[2] * cpnt[2] + cpnt[3] * cpnt[3];
		const float ww0 = 1.0f - dot;
		const float w0 = Math::Sqrt(ww0);
		// Re-applies 4th component' s sign.
		const int sign = _k0.sign;
		cpnt[_k0.largest] = w0 * (sign ? -1 : 1);

		// Stores result.
		_quaternion->x = cpnt[0];
		_quaternion->y = cpnt[1];
		_quaternion->z = cpnt[2];
		_quaternion->w = cpnt[3];
	}

	void Interpolates(float _anim_ratio, int _num_soa_tracks,
		const std::vector<internal::InterpSoaFloat3>& _translations,
		const std::vector<internal::InterpSoaQuaternion>& _rotations,
		const std::vector<internal::InterpSoaFloat3>& _scales,
		Math::Transform* _output)
	{
		const float anim_ratio = (_anim_ratio);
		for (int i = 0; i < _num_soa_tracks; ++i) 
		{
			// Prepares interpolation coefficients.
			const float interp_t_ratio =
				(anim_ratio - _translations[i].ratio[0]) /
				(_translations[i].ratio[1] - _translations[i].ratio[0]);
			const float interp_r_ratio =
				(anim_ratio - _rotations[i].ratio[0]) /
				(_rotations[i].ratio[1] - _rotations[i].ratio[0]);
			const float interp_s_ratio =
				(anim_ratio - _scales[i].ratio[0]) /
				(_scales[i].ratio[1] - _scales[i].ratio[0]);

			// Processes interpolations.
			// The lerp of the rotation uses the shortest path, because opposed
			// quaternions were negated during animation build stage (AnimationBuilder).
			_output[i].m_translation = Lerp(_translations[i].value[0], _translations[i].value[1], interp_t_ratio);
			_output[i].m_rotation = slerp(_rotations[i].value[0], _rotations[i].value[1], interp_r_ratio);
			_output[i].m_scale = Lerp(_scales[i].value[0], _scales[i].value[1], interp_s_ratio);
		}
	}
}  // namespace

AnimationJob::AnimationJob() : ratio(0.f), animation(nullptr), context(nullptr) {}

bool AnimationJob::Run() const 
{
	if (!Validate()) {
		return false;
	}

	const int num_tracks = animation->num_tracks();
	if (num_tracks == 0) {  // Early out if animation contains no joint.
		return true;
	}

	// Clamps ratio in range [0,duration].
	const float anim_ratio = Math::Clamp(0.f, ratio, 1.f);

	// Step the context to this potentially new animation and ratio.
	assert(context->max_tracks() >= num_tracks);
	context->Step(*animation, anim_ratio);

	// Fetch key frames from the animation to the context at r = anim_ratio.
	// Then updates outdated soa hot values.
	UpdateCacheCursor(anim_ratio, num_tracks, animation->translations(),
		&context->translation_cursor_, context->translation_keys_,
		context->outdated_translations_);
	UpdateInterpKeyframes(num_tracks, animation->translations(),
		context->translation_keys_,
		context->outdated_translations_,
		context->soa_translations_, &DecompressFloat3);

	UpdateCacheCursor(anim_ratio, num_tracks, animation->rotations(),
		&context->rotation_cursor_, context->rotation_keys_,
		context->outdated_rotations_);
	UpdateInterpKeyframes(num_tracks, animation->rotations(),
		context->rotation_keys_, context->outdated_rotations_,
		context->soa_rotations_, &DecompressQuaternion);

	UpdateCacheCursor(anim_ratio, num_tracks, animation->scales(),
		&context->scale_cursor_, context->scale_keys_,
		context->outdated_scales_);
	UpdateInterpKeyframes(num_tracks, animation->scales(),
		context->scale_keys_, context->outdated_scales_,
		context->soa_scales_, &DecompressFloat3);

	// only interp as much as we have output for.
	const int num_soa_interp_tracks = Math::Min(static_cast<int>(output.size()), num_tracks);

	// Interpolates soa hot data.
	Interpolates(anim_ratio, num_soa_interp_tracks, context->soa_translations_, context->soa_rotations_, context->soa_scales_, output.begin());

	return true;
}

//
// AnimationJob::Context
//
AnimationJob::Context::Context()
	: max_tracks_(0)
{  
	Invalidate();
}

AnimationJob::Context::Context(int _max_tracks)
	: max_tracks_(_max_tracks)
{  
	Resize(_max_tracks);
}

AnimationJob::Context::~Context() 
{
	
}

void AnimationJob::Context::Resize(int _max_tracks) 
{
	using internal::InterpSoaFloat3;
	using internal::InterpSoaQuaternion;

	// Reset existing data.
	Invalidate();

	max_tracks_ = _max_tracks;

    const size_t max_soa_tracks = (_max_tracks + 3) / 4;
    const size_t num_outdated = (max_soa_tracks + 7) / 8;

	soa_translations_.resize(_max_tracks);
	soa_rotations_.resize(_max_tracks);
	soa_scales_.resize(_max_tracks);

	translation_keys_.resize(_max_tracks * 2);
	rotation_keys_.resize(_max_tracks * 2);
	scale_keys_.resize(_max_tracks * 2);

	outdated_translations_.resize(num_outdated);
	outdated_rotations_.resize(num_outdated);
	outdated_scales_.resize(num_outdated);
}

void AnimationJob::Context::Step(const Animation& _animation, float _ratio) 
{
	// The context is invalidated if animation has changed or if it is being
	// rewind.
	if (animation_ != &_animation || _ratio < ratio_) 
	{
		animation_ = &_animation;
		translation_cursor_ = 0;
		rotation_cursor_ = 0;
		scale_cursor_ = 0;
	}
	ratio_ = _ratio;
}

void AnimationJob::Context::Invalidate() 
{
	animation_ = nullptr;
	ratio_ = 0.f;
	translation_cursor_ = 0;
	rotation_cursor_ = 0;
	scale_cursor_ = 0;
}