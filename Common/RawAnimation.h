#pragma once

#include "../Math/3DMath.h"

struct RawAnimation 
{
    // Constructs a valid RawAnimation with a 1s default duration.
    RawAnimation();

    // Tests for *this validity.
    // Returns true if animation data (duration, tracks) is valid:
    //  1. Animation duration is greater than 0.
    //  2. Keyframes' time are sorted in a strict ascending order.
    //  3. Keyframes' time are all within [0,animation duration] range.
    bool Validate() const;

    // Get the estimated animation's size in bytes.
    size_t size() const;

    // Defines a raw translation key frame.
    struct TranslationKey 
    {
        // Key frame time.
        float time;
        // Key frame value.
        typedef Math::Vec3 Value;
        Value value;
        // Provides identity transformation for a translation key.
        static Math::Vec3 identity() { return Math::Vec3::ZERO; }
    };

    // Defines a raw rotation key frame.
    struct RotationKey 
    {
        // Key frame time.
        float time;
        // Key frame value.
        typedef Math::Quaternion Value;
        Math::Quaternion value;
        // Provides identity transformation for a rotation key.
        static Math::Quaternion identity() { return Math::Quaternion::IDENTITY; }
    };

    // Defines a raw scaling key frame.
    struct ScaleKey 
    {
        // Key frame time.
        float time;
        // Key frame value.
        typedef Math::Vec3 Value;
        Math::Vec3 value;
        // Provides identity transformation for a scale key.
        static Math::Vec3 identity() { return Math::Vec3::ONE; }
    };

    // Defines a track of key frames for a bone, including translation, rotation
    // and scale.
    struct JointTrack 
    {
        typedef std::vector<TranslationKey> Translations;
        Translations translations;
        typedef std::vector<RotationKey> Rotations;
        Rotations rotations;
        typedef std::vector<ScaleKey> Scales;
        Scales scales;

        // Validates track. See RawAnimation::Validate for more details.
        // Use an infinite value for _duration if unknown. This will validate
        // keyframe orders, but not maximum duration.
        bool Validate(float _duration) const;
    };

    // Returns the number of tracks of this animation.
    int num_tracks() const { return static_cast<int>(tracks.size()); }

    float Duration() const { return duration; }

    // Stores per joint JointTrack, ie: per joint animation key-frames.
    // tracks_.size() gives the number of animated joints.
    std::vector<JointTrack> tracks;

    // The duration of the animation. All the keys of a valid RawAnimation are in
    // the range [0,duration].
    float duration;

    // Name of the animation.
    std::string name;
};