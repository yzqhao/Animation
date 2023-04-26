#pragma once

#include "../Math/3DMath.h"

#include "span.h"

// 定义动画关键帧类型（平移、旋转、缩放）。每种类型都是由关键时间比例及其轨道索引组成的相同基础。
// 这是必需的，因为关键帧不是按轨道排序，而是按比率排序以支持缓存一致性。
// 关键帧值根据其类型进行压缩。
// 解压缩是高效的，因为它是在 SoA 数据上完成的并且在缓存期间进行了缓存。

// 定义用于平移和缩放的 float3 关键帧类型。
// 转换值存储为每个组件 16 位的半精度浮点数。
struct Float3Key 
{
    float ratio;
    uint16_t track;
    uint16_t value[3];
};

// 定义旋转关键帧类型。
// 旋转值是一个四元数。四元数被归一化，这意味着每个分量都在 [0:1] 范围内。
// 此属性允许将 3 个分量量化为 3 个有符号整数 16 位值。
// 第 4 个组件在运行时恢复，使用 |w| 的知识= sqrt(1 - (a^2 + b^2 + c^2))。
// 第 4 个分量的符号使用从轨道成员中获取的 1 位存储。
//
// 更详细地说，压缩算法存储四元数的 3 个最小分量并恢复最大的分量。最小的 3 个可以预乘以 sqrt(2) 以获得一些精度
//
// 量化可以减少到 11-11-10 位，通常用于动画关键帧，但在这种情况下，RotationKey 结构会引入 16 位填充。
struct QuaternionKey 
{
    float ratio;
    uint16_t track : 13;   // The track this key frame belongs to.
    uint16_t largest : 2;  // The largest component of the quaternion.
    uint16_t sign : 1;     // The sign of the largest component. 1 for negative.
    int16_t value[3];      // The quantized value of the 3 smallest components.
};

//
// 定义运行时骨骼动画剪辑。
// 运行时动画数据结构为骨架的所有关节存储动画关键帧。该结构通常由 AnimationBuilder 填充并在运行时反序列化/加载。
// 对于每种变换类型（平移、旋转和缩放），动画结构存储一个关键帧数组，其中包含为骨架的所有关节设置动画所需的所有轨迹，匹配运行时骨架结构的广度优先关节顺序。
// 为了优化动画采样时的缓存一致性，此数组中的关键帧按时间排序，然后按轨道编号排序。

class Animation 
{
    friend class LoadFile;
public:
    // Builds a default animation.
    Animation();

    // Allow moves.
    Animation(Animation&&);
    Animation& operator=(Animation&&);

    // Delete copies.
    Animation(Animation const&) = delete;
    Animation& operator=(Animation const&) = delete;

    // Declares the public non-virtual destructor.
    ~Animation();

    // Gets the animation clip duration.
    float Duration() const { return duration_; }

    // Gets the number of animated tracks.
    int num_tracks() const { return num_tracks_; }

    // Gets animation name.
    const std::string& name() const { return name_; }

    // Gets the buffer of translations keys.
    const std::vector<Float3Key>& translations() const { return translations_; }

    // Gets the buffer of rotation keys.
    const std::vector<QuaternionKey>& rotations() const { return rotations_; }

    // Gets the buffer of scale keys.
    const std::vector<Float3Key>& scales() const { return scales_; }

    // Get the estimated animation's size in bytes.
    size_t size() const;

private:
    // AnimationBuilder class is allowed to instantiate an Animation.
    //friend class offline::AnimationBuilder;

    // Internal destruction function.
    void Allocate(size_t _translation_count, size_t _rotation_count, size_t _scale_count);
    void Deallocate();

    // Duration of the animation clip.
    float duration_;

    // The number of joint tracks. Can differ from the data stored in translation/
    // rotation/scale buffers because of SoA requirements.
    int num_tracks_;

    // Animation name.
    std::string name_;

    // Stores all translation/rotation/scale keys begin and end of buffers.
    std::vector<Float3Key> translations_;
    std::vector<QuaternionKey> rotations_;
    std::vector<Float3Key> scales_;
};
