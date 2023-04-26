#pragma once

#include "RawAnimation.h"
#include "span.h"

struct RawAnimationJob
{
    RawAnimationJob();

	bool Run() const;


    float ratio;

    const RawAnimation* animation;

    class Context;
    Context* context;

    span<Math::Transform> output;
};

class RawAnimationJob::Context 
{
public:
    Context();
    ~Context();

    void Resize(int max_tracks);

    void Invalidate();

private:
    friend struct RawAnimationJob;

    // Steps the context in order to use it for a po
    void Step(const RawAnimation& _animation, float _ratio);


    const RawAnimation* m_animation;
    float m_ratio;   // The current time ratio in the animation.
    int m_max_tracks;
};