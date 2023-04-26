#include "LoadFile.h"
#include "Skeleton.h"
#include "BundleReader.h"
#include "Animation.h"
#include "RawAnimation.h"
#include "Mesh.h"

#define READ_IF_RETURN(cmp) if (cmp) { return false ;}

struct Joint
{
	Joint(Joint* parent) : m_parent(parent) {}
	~Joint()
	{
		for (int i = 0; i < m_children.size(); ++i)
		{
			SAFE_DELETE(m_children[i]);
		}
	}

	void GetChildren(std::vector<Joint*>& allJoints)
	{
		allJoints.push_back(this);
		for (int i = 0; i < m_children.size(); ++i)
		{
			m_children[i]->GetChildren(allJoints);
		}
	}

	std::vector<Joint*> m_children;
	Joint* m_parent;
	std::string m_name;
	Math::Transform m_transform;
};

class RawSkeleton
{
	friend class LoadFile;
public:
	RawSkeleton() {}
	~RawSkeleton()
	{
		for (int i = 0; i < m_roots.size(); ++i)
		{
			SAFE_DELETE(m_roots[i]);
		}
	}

	void GetAllJoints(std::vector<Joint*>& allJoints)
	{
		for (int i = 0; i < m_roots.size(); ++i)
		{
			m_roots[i]->GetChildren(allJoints);
		}
	}

private:
	std::vector<Joint*> m_roots;
};

static bool __LoadJoint(BundleReader& binaryReader, Joint* joint)
{
	unsigned int joint_name_size;
	READ_IF_RETURN(binaryReader.read(&joint_name_size, 1, sizeof(joint_name_size)) != sizeof(joint_name_size));
	char joint_name[32] = {};
	READ_IF_RETURN(binaryReader.read(joint_name, 1, joint_name_size) != joint_name_size);
	joint->m_name = joint_name;

	READ_IF_RETURN(binaryReader.read(&joint->m_transform, 1, sizeof(joint->m_transform)) != sizeof(joint->m_transform));

	if (binaryReader.eof())
	{
		return true;
	}

	unsigned int num_child;
	READ_IF_RETURN(binaryReader.read(&num_child, 1, sizeof(num_child)) != sizeof(num_child));

	if (num_child > 0)
	{
		joint->m_children.reserve(num_child);

		unsigned int joint_version;
		READ_IF_RETURN(binaryReader.read(&joint_version, 1, sizeof(joint_version)) != sizeof(joint_version));
		JY_ASSERT(joint_version == 1);

		for (int i = 0; i < num_child; ++i)
		{
			joint->m_children.push_back(new Joint(joint));

			__LoadJoint(binaryReader, joint->m_children.back());
		}
	}

	return true;
}

bool LoadFile::LoadSkeleton(const std::string& filename, Skeleton& outSke)
{
    FILE* file = NULL;
	fopen_s(&file, filename.c_str(), "rb");
	if (file == NULL)
		return false;

	fseek(file, 0, SEEK_END);
	int rawLength = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (rawLength < 0)
	{
		fclose(file);
		return false;
	}

	std::string readTexts;
	readTexts.resize(rawLength);
	int readLength = fread(&*readTexts.begin(), 1, rawLength, file);
	fclose(file);

	BundleReader binaryReader;
	binaryReader.init((byte*)readTexts.data(), readTexts.size());

	unsigned char endianness;
	READ_IF_RETURN(binaryReader.read(&endianness, 1, 1) != 1);

	// Read identifier info
	std::string tag = "ozz-raw_skeleton";
	char sig[32] = {};
	int tagsize = tag.size() + 1;
	READ_IF_RETURN(binaryReader.read(sig, 1, tagsize) != tagsize || memcmp(sig, tag.data(), tag.size()) != 0);

	unsigned int ske_version;
	READ_IF_RETURN(binaryReader.read(&ske_version, 1, sizeof(ske_version)) != sizeof(ske_version));

	unsigned int num_root;
	READ_IF_RETURN(binaryReader.read(&num_root, 1, sizeof(num_root)) != sizeof(num_root));

	// children
	RawSkeleton rawSke;
	rawSke.m_roots.reserve(num_root);

	unsigned int joint_version;
	READ_IF_RETURN(binaryReader.read(&joint_version, 1, sizeof(joint_version)) != sizeof(joint_version));
	JY_ASSERT(joint_version == 1);

	for (int i = 0; i < num_root; ++i)
	{
		rawSke.m_roots.push_back(new Joint(nullptr));
		__LoadJoint(binaryReader, rawSke.m_roots.back());
	}

	// RawSkeleton TO Skeleton
	std::vector<Joint*> joints;
	std::unordered_map<Joint*, int> jointhash;
	rawSke.GetAllJoints(joints);
	auto numJoints = joints.size();
	outSke.joint_rest_poses_.resize(numJoints);
	outSke.joint_parents_.resize(numJoints, -1);
	outSke.joint_names_.resize(numJoints);
	for (int i = 0; i < numJoints; ++i)
	{
		outSke.joint_names_[i] = joints[i]->m_name;
		outSke.joint_rest_poses_[i] = joints[i]->m_transform;
		outSke.joint_parents_[i] = i == 0 ? -1 : jointhash[joints[i]->m_parent];
		jointhash[joints[i]] = i;
	}

	return true;
}

bool LoadFile::_LoadAnimation(BundleReader& binaryReader, Animation& outAni)
{
	// Read identifier info
	std::string tag = "ozz-animation";
	char sig[32] = {};
	int tagsize = tag.size() + 1;
	READ_IF_RETURN(binaryReader.read(sig, 1, tagsize) != tagsize || memcmp(sig, tag.data(), tag.size()) != 0);

	unsigned int ani_version;
	READ_IF_RETURN(binaryReader.read(&ani_version, 1, sizeof(ani_version)) != sizeof(ani_version));
	JY_ASSERT(ani_version == 6);

	READ_IF_RETURN(binaryReader.read(&outAni.duration_, 1, sizeof(outAni.duration_)) != sizeof(outAni.duration_));
	READ_IF_RETURN(binaryReader.read(&outAni.num_tracks_, 1, sizeof(outAni.num_tracks_)) != sizeof(outAni.num_tracks_));

	int32_t name_len;
	int32_t translation_count;
	int32_t rotation_count;
	int32_t scale_count;
	READ_IF_RETURN(binaryReader.read(&name_len, 1, sizeof(name_len)) != sizeof(name_len));
	READ_IF_RETURN(binaryReader.read(&translation_count, 1, sizeof(translation_count)) != sizeof(translation_count));
	READ_IF_RETURN(binaryReader.read(&rotation_count, 1, sizeof(rotation_count)) != sizeof(rotation_count));
	READ_IF_RETURN(binaryReader.read(&scale_count, 1, sizeof(scale_count)) != sizeof(scale_count));
	outAni.Allocate(translation_count, rotation_count, scale_count);

	outAni.name_.resize(name_len + 1);
	READ_IF_RETURN(binaryReader.read((char*)outAni.name_.data(), 1, name_len) != name_len);

	for (Float3Key& key : outAni.translations_)
	{
		READ_IF_RETURN(binaryReader.read(&key.ratio, 1, sizeof(key.ratio)) != sizeof(key.ratio));
		READ_IF_RETURN(binaryReader.read(&key.track, 1, sizeof(key.track)) != sizeof(key.track));
		READ_IF_RETURN(binaryReader.read(&key.value, 1, sizeof(uint16_t) * 3) != sizeof(uint16_t) * 3);
	}

	for (QuaternionKey& key : outAni.rotations_)
	{
		READ_IF_RETURN(binaryReader.read(&key.ratio, 1, sizeof(key.ratio)) != sizeof(key.ratio));
		uint16_t track;
		READ_IF_RETURN(binaryReader.read(&track, 1, sizeof(track)) != sizeof(track));
		key.track = track;
		uint8_t largest;
		READ_IF_RETURN(binaryReader.read(&largest, 1, sizeof(largest)) != sizeof(largest));
		key.largest = largest & 3;
		bool sign;
		READ_IF_RETURN(binaryReader.read(&sign, 1, sizeof(sign)) != sizeof(sign));
		key.sign = sign & 1;
		READ_IF_RETURN(binaryReader.read(&key.value, 1, sizeof(uint16_t) * 3) != sizeof(uint16_t) * 3);
	}

	for (Float3Key& key : outAni.scales_)
	{
		READ_IF_RETURN(binaryReader.read(&key.ratio, 1, sizeof(key.ratio)) != sizeof(key.ratio));
		READ_IF_RETURN(binaryReader.read(&key.track, 1, sizeof(key.track)) != sizeof(key.track));
		READ_IF_RETURN(binaryReader.read(&key.value, 1, sizeof(uint16_t) * 3) != sizeof(uint16_t) * 3);
	}

	return true;
}

bool LoadFile::LoadAnimation(const std::string& filename, Animation& outAni)
{
	FILE* file = NULL;
	fopen_s(&file, filename.c_str(), "rb");
	if (file == NULL)
		return false;

	fseek(file, 0, SEEK_END);
	int rawLength = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (rawLength < 0)
	{
		fclose(file);
		return false;
	}

	std::string readTexts;
	readTexts.resize(rawLength);
	int readLength = fread(&*readTexts.begin(), 1, rawLength, file);
	fclose(file);

	BundleReader binaryReader;
	binaryReader.init((byte*)readTexts.data(), readTexts.size());

	unsigned char endianness;
	READ_IF_RETURN(binaryReader.read(&endianness, 1, 1) != 1);

	return _LoadAnimation(binaryReader, outAni);
}

bool LoadFile::_LoadRawAnimation(BundleReader& binaryReader, RawAnimation& outAni)
{
	// Read identifier info
	std::string tag = "ozz-raw_animation";
	char sig[32] = {};
	int tagsize = tag.size() + 1;
	READ_IF_RETURN(binaryReader.read(sig, 1, tagsize) != tagsize || memcmp(sig, tag.data(), tag.size()) != 0);

	unsigned int ani_version;
	READ_IF_RETURN(binaryReader.read(&ani_version, 1, sizeof(ani_version)) != sizeof(ani_version));
	JY_ASSERT(ani_version == 3);

	READ_IF_RETURN(binaryReader.read(&outAni.duration, 1, sizeof(outAni.duration)) != sizeof(outAni.duration));

	unsigned int num_tracks;
	READ_IF_RETURN(binaryReader.read(&num_tracks, 1, sizeof(num_tracks)) != sizeof(num_tracks));
	outAni.tracks.reserve(num_tracks);

	unsigned int JointTrackVersion;
	READ_IF_RETURN(binaryReader.read(&JointTrackVersion, 1, sizeof(JointTrackVersion)) != sizeof(JointTrackVersion));
	JY_ASSERT(JointTrackVersion == 1);

	for (int i = 0; i < num_tracks; ++i)
	{
		outAni.tracks.push_back(RawAnimation::JointTrack());

		unsigned int num_trans;
		READ_IF_RETURN(binaryReader.read(&num_trans, 1, sizeof(num_trans)) != sizeof(num_trans));
		outAni.tracks.reserve(num_trans);
		unsigned int TranslationKeyVersion;
		READ_IF_RETURN(binaryReader.read(&TranslationKeyVersion, 1, sizeof(TranslationKeyVersion)) != sizeof(TranslationKeyVersion));
		for (int j = 0; j < num_trans; ++j)
		{
			outAni.tracks[i].translations.push_back(RawAnimation::TranslationKey());
			READ_IF_RETURN(binaryReader.read(&outAni.tracks[i].translations[j].time, 1, sizeof(float)) != sizeof(float));
			READ_IF_RETURN(binaryReader.read(&outAni.tracks[i].translations[j].value, 1, sizeof(Math::Vec3)) != sizeof(Math::Vec3));
		}

		unsigned int num_rotate;
		READ_IF_RETURN(binaryReader.read(&num_rotate, 1, sizeof(num_rotate)) != sizeof(num_rotate));
		outAni.tracks.reserve(num_rotate);
		unsigned int RotateKeyVersion;
		READ_IF_RETURN(binaryReader.read(&RotateKeyVersion, 1, sizeof(RotateKeyVersion)) != sizeof(RotateKeyVersion));
		for (int j = 0; j < num_rotate; ++j)
		{
			outAni.tracks[i].rotations.push_back(RawAnimation::RotationKey());
			READ_IF_RETURN(binaryReader.read(&outAni.tracks[i].rotations[j].time, 1, sizeof(float)) != sizeof(float));
			READ_IF_RETURN(binaryReader.read(&outAni.tracks[i].rotations[j].value, 1, sizeof(Math::Quaternion)) != sizeof(Math::Quaternion));
		}

		unsigned int num_scale;
		READ_IF_RETURN(binaryReader.read(&num_scale, 1, sizeof(num_scale)) != sizeof(num_scale));
		outAni.tracks.reserve(num_scale);
		unsigned int ScaleKeyVersion;
		READ_IF_RETURN(binaryReader.read(&ScaleKeyVersion, 1, sizeof(ScaleKeyVersion)) != sizeof(ScaleKeyVersion));
		for (int j = 0; j < num_scale; ++j)
		{
			outAni.tracks[i].scales.push_back(RawAnimation::ScaleKey());
			READ_IF_RETURN(binaryReader.read(&outAni.tracks[i].scales[j].time, 1, sizeof(float)) != sizeof(float));
			READ_IF_RETURN(binaryReader.read(&outAni.tracks[i].scales[j].value, 1, sizeof(Math::Vec3)) != sizeof(Math::Vec3));
		}
	}

	//READ_IF_RETURN(binaryReader.read(&outAni.duration, 1, sizeof(outAni.duration)) != sizeof(outAni.duration));
	unsigned int ani_name_size;
	READ_IF_RETURN(binaryReader.read(&ani_name_size, 1, sizeof(ani_name_size)) != sizeof(ani_name_size));
	char ani_name[32] = {};
	READ_IF_RETURN(binaryReader.read(ani_name, 1, ani_name_size) != ani_name_size);
	outAni.name = ani_name;

	return true;
}

bool LoadFile::LoadRawAnimation(const std::string& filename, RawAnimation& outAni)
{
	FILE* file = NULL;
	fopen_s(&file, filename.c_str(), "rb");
	if (file == NULL)
		return false;

	fseek(file, 0, SEEK_END);
	int rawLength = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (rawLength < 0)
	{
		fclose(file);
		return false;
	}

	std::string readTexts;
	readTexts.resize(rawLength);
	int readLength = fread((char*)readTexts.data(), 1, rawLength, file);
	bool bf = feof(file);
	fclose(file);

	BundleReader binaryReader;
	binaryReader.init((byte*)readTexts.data(), readTexts.size());

	unsigned char endianness;
	READ_IF_RETURN(binaryReader.read(&endianness, 1, 1) != 1);

	return _LoadRawAnimation(binaryReader, outAni);
}

template<typename T>
static bool __LoadMeshData(BundleReader& binaryReader, std::vector<T>& data, int size)
{
	unsigned int num;
	READ_IF_RETURN(binaryReader.read(&num, 1, sizeof(num)) != sizeof(num));
	data.resize(num);
	READ_IF_RETURN(binaryReader.read(data.data(), 1, sizeof(T) * num) != sizeof(T) * num);
	return true;
}

static bool __LoadMesh(BundleReader& binaryReader, Mesh& outMesh)
{
	// Read identifier info
	std::string tag = "ozz-sample-Mesh";
	char sig[32] = {};
	int tagsize = tag.size() + 1;
	READ_IF_RETURN(binaryReader.read(sig, 1, tagsize) != tagsize || memcmp(sig, tag.data(), tag.size()) != 0);

	unsigned int mesh_version;
	READ_IF_RETURN(binaryReader.read(&mesh_version, 1, sizeof(mesh_version)) != sizeof(mesh_version));
	JY_ASSERT(mesh_version == 1);

	unsigned int num_part;
	READ_IF_RETURN(binaryReader.read(&num_part, 1, sizeof(num_part)) != sizeof(num_part));

	if (num_part > 0)
	{
		outMesh.parts.resize(num_part);

		unsigned int part_version;
		READ_IF_RETURN(binaryReader.read(&part_version, 1, sizeof(part_version)) != sizeof(part_version));
		JY_ASSERT(part_version == 1);

		for (int i = 0; i < num_part; ++i)
		{
			__LoadMeshData(binaryReader, outMesh.parts[i].positions, Mesh::Part::kPositionsCpnts);
			__LoadMeshData(binaryReader, outMesh.parts[i].normals, Mesh::Part::kNormalsCpnts);
			__LoadMeshData(binaryReader, outMesh.parts[i].tangents, Mesh::Part::kTangentsCpnts);
			__LoadMeshData(binaryReader, outMesh.parts[i].uvs, Mesh::Part::kUVsCpnts);
			__LoadMeshData(binaryReader, outMesh.parts[i].colors, Mesh::Part::kColorsCpnts);
			__LoadMeshData(binaryReader, outMesh.parts[i].joint_indices, 4);
			__LoadMeshData(binaryReader, outMesh.parts[i].joint_weights, 4);
		}
	}

	__LoadMeshData(binaryReader, outMesh.triangle_indices, 0);
	__LoadMeshData(binaryReader, outMesh.joint_remaps, 0);
	__LoadMeshData(binaryReader, outMesh.inverse_bind_poses, 0);

	return true;
}

bool LoadFile::LoadMesh(const std::string& filename, std::vector<Mesh>& outMeshes)
{
	FILE* file = NULL;
	fopen_s(&file, filename.c_str(), "rb");
	if (file == NULL)
		return false;

	fseek(file, 0, SEEK_END);
	int rawLength = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (rawLength < 0)
	{
		fclose(file);
		return false;
	}

	std::string readTexts;
	readTexts.resize(rawLength);
	int readLength = fread((char*)readTexts.data(), 1, rawLength, file);
	bool bf = feof(file);
	fclose(file);

	BundleReader binaryReader;
	binaryReader.init((byte*)readTexts.data(), readTexts.size());

	unsigned char endianness;
	READ_IF_RETURN(binaryReader.read(&endianness, 1, 1) != 1);

	while (!binaryReader.eof())
	{
		outMeshes.resize(outMeshes.size() + 1);
		__LoadMesh(binaryReader, outMeshes[outMeshes.size()-1]);
	}

	return true;
}
