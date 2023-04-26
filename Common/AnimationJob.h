#pragma once

#include "../Math/3dMath.h"
#include "span.h"

class Animation;

// 在单位区间[0,1]（其中0为动画开始，1为结束）内按给定的时间比例采样一个动画，输出local-space中对应的姿势。
// AnimationJob 在采样时使用上下文（又名 AnimationJob::Context）来存储中间值（解压缩的动画关键帧...）。此上下文还存储预先计算的值，允许在向前播放/采样动画时进行大幅优化。
// 向后采样有效，但未通过上下文进行优化。该作业不拥有缓冲区（输入/输出），因此在作业销毁期间不会删除它们。
struct AnimationJob 
{
    AnimationJob();

    bool Validate() const;

    // Runs job's sampling task.
    // The job is validated before any operation is performed, see Validate() for
    // more details.
    // Returns false if *this job is not valid.
    bool Run() const;

    // Time ratio in the unit interval [0,1] used to sample animation (where 0 is
    // the beginning of the animation, 1 is the end). It should be computed as the
    // current time in the animation , divided by animation duration.
    // This ratio is clamped before job execution in order to resolves any
    // approximation issue on range bounds.
    float ratio;

    // The animation to sample.
    const Animation* animation;

    // Forward declares the context object used by the AnimationJob.
    class Context;

    // A context object that must be big enough to sample *this animation.
    Context* context;

    // Job output.
    // The output range to be filled with sampled joints during job execution.
    // If there are less joints in the animation compared to the output range,
    // then remaining SoaTransform are left unchanged.
    // If there are more joints in the animation, then the last joints are not
    // sampled.
    span<Math::Transform> output;
};


namespace internal 
{
struct InterpSoaFloat3;
struct InterpSoaQuaternion;
}

// Declares the context object used by the workload to take advantage of the
// frame coherency of animation sampling.
class AnimationJob::Context 
{
public:
    // Constructs an empty context. The context needs to be resized with the
    // appropriate number of tracks before it can be used with a AnimationJob.
    Context();

    // Constructs a context that can be used to sample any animation with at most
    // _max_tracks tracks. _num_tracks is internally aligned to a multiple of
    // soa size, which means max_tracks() can return a different (but bigger)
    // value than _max_tracks.
    explicit Context(int _max_tracks);

    // Disables copy and assignation.
    Context(Context const&) = delete;
    Context& operator=(Context const&) = delete;

    // Deallocates context.
    ~Context();

    // Resize the number of joints that the context can support.
    // This also implicitly invalidate the context.
    void Resize(int _max_tracks);

    void Invalidate();

    // The maximum number of tracks that the context can handle.
    int max_tracks() const { return max_tracks_; }

private:
    friend struct AnimationJob;

    // Steps the context in order to use it for a potentially new animation and
    // ratio. If the _animation is different from the animation currently cached,
    // or if the _ratio shows that the animation is played backward, then the
    // context is invalidated and reset for the new _animation and _ratio.
    void Step(const Animation& _animation, float _ratio);

    // The animation this context refers to. nullptr means that the context is
    // invalid.
    const Animation* animation_;

    // The current time ratio in the animation.
    float ratio_;

    // The number of soa tracks that can store this context.
    int max_tracks_;

    // Soa hot data to interpolate.
    std::vector<internal::InterpSoaFloat3> soa_translations_;
    std::vector<internal::InterpSoaQuaternion> soa_rotations_;
    std::vector<internal::InterpSoaFloat3> soa_scales_;

    // Points to the keys in the animation that are valid for the current time ratio.
    std::vector<int> translation_keys_;   
    std::vector<int> rotation_keys_;
    std::vector<int> scale_keys_;

    // Current cursors in the animation. 0 means that the context is invalid.
    int translation_cursor_;
    int rotation_cursor_;
    int scale_cursor_;

    // Outdated soa entries. One bit per soa entry (32 joints per byte).
    std::vector<uint8_t> outdated_translations_;
    std::vector<uint8_t> outdated_rotations_;
    std::vector<uint8_t> outdated_scales_;
};