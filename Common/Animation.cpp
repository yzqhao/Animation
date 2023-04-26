#include "Animation.h"

Animation::Animation() : duration_(0.f), num_tracks_(0), name_() 
{
}

Animation::Animation(Animation&& _other) 
{
	*this = std::move(_other);
}

Animation& Animation::operator=(Animation&& _other) 
{
	std::swap(duration_, _other.duration_);
	std::swap(num_tracks_, _other.num_tracks_);
	std::swap(name_, _other.name_);
	std::swap(translations_, _other.translations_);
	std::swap(rotations_, _other.rotations_);
	std::swap(scales_, _other.scales_);

	return *this;
}

Animation::~Animation() 
{ 
    Deallocate(); 
}

void Animation::Allocate(size_t _translation_count, size_t _rotation_count, size_t _scale_count) 
{
	translations_.resize(_translation_count);
	rotations_.resize(_rotation_count);
	scales_.resize(_scale_count);
}

void Animation::Deallocate()
{
	translations_.clear();
    rotations_.clear();
    scales_.clear();
}

size_t Animation::size() const 
{
	JY_ASSERT(false);
	return 0;
}