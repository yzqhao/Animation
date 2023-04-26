#include "RawAnimationJob.h"

RawAnimationJob::RawAnimationJob() : ratio(0.f), animation(nullptr), context(nullptr)
{

}

template <typename _Key, typename T>
void UpdateKey(float time, const std::vector<_Key>& keys, T(*LERP_FUNC)(const T&, const T&, float), T& outkey)
{
	int t1 = 0;
	for (int j = 1; j < keys.size(); ++j)
	{
		if (keys[j].time > time) break;
		else t1 = j;
	}
	int t2 = Math::Min(t1 + 1, (int)keys.size()-1);
	float lerp = t2 == t1 ? 0.0 : (time - keys[t1].time) / (keys[t2].time - keys[t1].time);
	if (t1 == 0 && keys.size() > 1)
	{
		int ii = 0;
	}
	outkey = LERP_FUNC(keys[t1].value, keys[t2].value, lerp);
}

bool RawAnimationJob::Run() const
{
	for (int i = 0; i < animation->tracks.size(); ++i)
	{
		float time = ratio * animation->duration;
		UpdateKey<RawAnimation::TranslationKey, Math::Vec3>(time, animation->tracks[i].translations, Math::Lerp<Math::Vec3>, output[i].m_translation);
		UpdateKey<RawAnimation::RotationKey, Math::Quaternion>(time, animation->tracks[i].rotations, Math::slerp, output[i].m_rotation);
		UpdateKey<RawAnimation::ScaleKey, Math::Vec3>(time, animation->tracks[i].scales, Math::Lerp<Math::Vec3>, output[i].m_scale);
	}

	return true;
}


//
// RawAnimationJob::Context
//
RawAnimationJob::Context::Context()
	: m_max_tracks(0)
{ 
	Invalidate();
}

RawAnimationJob::Context::~Context()
{
	
}

void RawAnimationJob::Context::Resize(int _max_tracks)
{
	
}

void RawAnimationJob::Context::Step(const RawAnimation& _animation, float _ratio)
{
	// The context is invalidated if animation has changed or if it is being
	// rewind.
	if (m_animation != &_animation || _ratio < m_ratio)
	{
		m_animation = &_animation;
	}
	m_ratio = _ratio;
}

void RawAnimationJob::Context::Invalidate()
{
	m_animation = nullptr;
	m_ratio = 0.f;
}