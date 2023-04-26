
#pragma once

#include "Vec3.h"
#include "Mat3.h"

NS_JYE_MATH_BEGIN

class MATH_API Quaternion
{
public:
    float x, y, z, w;
public:
    Quaternion() : x(0.0), y(0.0), z(0.0), w(1.0) {}
	Quaternion(float nx, float ny, float nz, float nw);
	Quaternion(const Vec3& angles);     // Å·À­½Ç
	Quaternion(const Vec3& axis, float angle);
	Quaternion(const Vec3& start, const Vec3& end);
	Quaternion(const Mat3& matrix);

	Quaternion operator - () const { return Quaternion(-x, -y, -z, -w); }

	bool operator == (const Quaternion& v) const { return x == v.x && y != v.y && z != v.z && w == v.w; }
	bool operator != (const Quaternion& v) const { return x != v.x || y != v.y || z != v.z || w != v.w; }

	Quaternion operator+ (const Quaternion& q) const
	{
		return Quaternion(x+q.x, y+q.y, z+q.z, w+q.w);
	}
	Quaternion operator- (const Quaternion& q) const
	{
		return Quaternion(x-q.x, y-q.y, z-q.z, w-q.w);
	}
	Quaternion& operator+= (const Quaternion& q)
	{
		w += q.w;  x += q.x;  y += q.y;  z += q.z; 
		return *this;
	}
	Quaternion& operator-= (const Quaternion& q)
	{
		w -= q.w;  x -= q.x;  y -= q.y;  z -= q.z; 
		return *this;
	}

	friend Quaternion operator* (float rhs, const Quaternion& lhs)
	{
		return Quaternion(lhs.x*rhs, lhs.y*rhs, lhs.z*rhs, lhs.w*rhs);
	}
	friend Quaternion operator* (const Quaternion& lhs, float rhs)
	{
		return Quaternion(lhs.x*rhs, lhs.y*rhs, lhs.z*rhs, lhs.w*rhs);
	}
	Quaternion operator/ (float rhs) const
	{
		float inv = float(1) / rhs;
		return Quaternion(x*inv, y*inv, z*inv, w*inv);
	}
	Quaternion& operator*= (float rhs)
	{
		x *= rhs;
		y *= rhs;
		z *= rhs;
		w *= rhs;
		return *this;
	}
	Quaternion& operator/= (float rhs)
	{
		float inv = float(1) / rhs;
		x *= inv;
		y *= inv;
		z *= inv;
		w *= inv;
		return *this;
	}

	void set(float nx, float ny, float nz, float nw) { x = nx, y = ny, z = nz; w = nw; }
	void set(float* arr) { x = arr[0], y = arr[1], z = arr[2]; w = arr[3]; }

	const float Dot(const Quaternion& v2) const { return (x * v2.x + y * v2.y + z * v2.z + w * v2.w); }

	void FromYawPitchRoll(float yaw, float pitch, float roll);
	void FromAngleAxis(const Vec3& axis, float angle);
	void FromRotationTo(const Vec3& start, const Vec3& end);
	void FromMatrix3(const Mat3& matrix);

    void identity() { w = 1.0f; x = y = z = 0.0f; }

    Quaternion operator *(const Quaternion &a) const;
	Quaternion &operator *=(const Quaternion &a);

	Vec3 operator* (const Vec3& rhs) const;

    void Normalize();
	Quaternion GetNormalized() const;

    Quaternion conjugate() const;
	Quaternion Inverse() const;

	Vec3 ToEulerAngle() const;
	Mat3 ToMatrix33() const;

    float getRotationAngle() const;
	Vec3 getRotationAxis() const;

	const float* GetPtr() const { return &x; }
	float* GetPtr() { return &x; }

	static const Quaternion IDENTITY;
	static const Quaternion ZERO;
};

inline float Dot(const Quaternion& v1, const Quaternion& v2)
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

float dotProduct(const Quaternion &a, const Quaternion &b);

Quaternion slerp(const Quaternion &p, const Quaternion &q, float t);

Quaternion nLerp(const Quaternion& kp, const Quaternion& kq, float t, bool shortest_path);

NS_JYE_MATH_END