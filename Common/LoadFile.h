#pragma once

#include <string>
#include <vector>

class Skeleton;
class Animation;
class RawAnimation;
class Mesh;

class BundleReader;

class LoadFile
{
public:
	static bool LoadSkeleton(const std::string& filename, Skeleton& outSke);
	static bool LoadAnimation(const std::string& filename, Animation& outAni);
	static bool LoadRawAnimation(const std::string& filename, RawAnimation& outAni);
	static bool LoadMesh(const std::string& filename, std::vector<Mesh>& outAni);
private:
	static bool _LoadAnimation(BundleReader& binaryReader, Animation& outAni);
	static bool _LoadRawAnimation(BundleReader& binaryReader, RawAnimation& outAni);
};