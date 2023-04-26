#include "Renderer.h"
#include "../Skeleton.h"
#include "../Mesh.h"
#include "../skeleton_utils.h"
#include "../SkinningJob.h"
#include "Camera.h"
#include "Immediate.h"

namespace 
{
	// A vertex made of positions and normals.
	struct VertexPNC 
	{
		Math::Vec3 pos;
		Math::Vec3 normal;
		Color color;
	};
}  // namespace

Renderer::Model::Model() : vbo(0), mode(GL_POINTS), count(0) {}

Renderer::Model::~Model() 
{
	if (vbo) 
	{
		glDeleteBuffers(1, &vbo);
		vbo = 0;
	}
}

Renderer::ScratchBuffer::ScratchBuffer() : buffer_(nullptr), size_(0) {}

Renderer::ScratchBuffer::~ScratchBuffer() 
{
	free(buffer_);
}

void* Renderer::ScratchBuffer::Resize(size_t _size) 
{
	if (_size > size_) 
	{
		size_ = _size;
		free(buffer_);
		std::size_t allocsize = Align(size_, 16);
		buffer_ = malloc(allocsize);
	}
	return buffer_;
}

namespace
{
	int DrawPosture_FillUniforms(const Skeleton& _skeleton,
		const std::vector<Math::Mat4>& _matrices,
		float* _uniforms, int _max_instances) {
		// Prepares computation constants.
		const int num_joints = _skeleton.num_joints();
		const auto& parents = _skeleton.joint_parents();

		int instances = 0;
		for (int i = 0; i < num_joints && instances < _max_instances; ++i) {
			// Root isn't rendered.
			const int16_t parent_id = parents[i];
			if (parent_id == Skeleton::kNoParent) {
				continue;
			}

			// Selects joint matrices.
			const Math::Mat4& parent = _matrices[parent_id];
			const Math::Mat4& current = _matrices[i];

			// Copy parent joint's raw matrix, to render a bone between the parent
			// and current matrix.
			float* uniform = _uniforms + instances * 16;
			std::memcpy(uniform, &parent.a11, 16 * sizeof(float));

			// Set bone direction (bone_dir). The shader expects to find it at index
			// [3,7,11] of the matrix.
			// Index 15 is used to store whether a bone should be rendered,
			// otherwise it's a leaf.
			float bone_dir[4];
			uniform[3] = current.a41 - parent.a41;
			uniform[7] = current.a42 - parent.a42;
			uniform[11] = current.a43 - parent.a43;
			uniform[15] = 1.f;  // Enables bone rendering.

			// Next instance.
			++instances;
			uniform += 16;

			// Only the joint is rendered for leaves, the bone model isn't.
			if (IsLeaf(_skeleton, i)) {
				// Copy current joint's raw matrix.
				std::memcpy(uniform, &current.a11, 16 * sizeof(float));

				// Re-use bone_dir to fix the size of the leaf (same as previous bone).
				// The shader expects to find it at index [3,7,11] of the matrix.
				uniform[3] = bone_dir[0];
				uniform[7] = bone_dir[1];
				uniform[11] = bone_dir[2];
				uniform[15] = 0.f;  // Disables bone rendering.
				++instances;
			}
		}

		return instances;
	}
}  // namespace

Renderer::Renderer(Camera* cam)
	: m_cam(cam)
	, dynamic_array_bo_(0)
	, dynamic_index_bo_(0)
{

}

Renderer::~Renderer()
{
	if (dynamic_array_bo_) 
	{
		glDeleteBuffers(1, &dynamic_array_bo_);
		dynamic_array_bo_ = 0;
	}

	if (dynamic_index_bo_) 
	{
		glDeleteBuffers(1, &dynamic_index_bo_);
		dynamic_index_bo_ = 0;
	}
}

bool Renderer::Initialize()
{
	if (!InitPostureRendering()) 
	{
		return false;
	}
	if (!InitCheckeredTexture())
	{
		return false;
	}

	immediate_ = std::make_unique<GlImmediateRenderer>(this);
	if (!immediate_->Initialize()) 
	{
		return false;
	}

	// Builds the dynamic vbo
	glGenBuffers(1, &dynamic_array_bo_);
	glGenBuffers(1, &dynamic_index_bo_);

	// Instantiate ambient rendering shader.
	ambient_shader = AmbientShader::Build();
	if (!ambient_shader) {
		return false;
	}

	// Instantiate ambient textured rendering shader.
	ambient_textured_shader = AmbientTexturedShader::Build();
	if (!ambient_textured_shader) {
		return false;
	}

	// Instantiate instanced ambient rendering shader.
	points_shader = PointsShader::Build();
	if (!points_shader) {
		return false;
	}

	return true;
}

bool Renderer::InitPostureRendering()
{
	const float kInter = .2f;
	{  // Prepares bone mesh.
		const Math::Vec3 pos[6] = {
			Math::Vec3(1.f, 0.f, 0.f),     Math::Vec3(kInter, .1f, .1f),
			Math::Vec3(kInter, .1f, -.1f), Math::Vec3(kInter, -.1f, -.1f),
			Math::Vec3(kInter, -.1f, .1f), Math::Vec3(0.f, 0.f, 0.f) };
		const Math::Vec3 normals[8] = {
			Normalize(CrossProduct(pos[2] - pos[1], pos[2] - pos[0])),
			Normalize(CrossProduct(pos[1] - pos[2], pos[1] - pos[5])),
			Normalize(CrossProduct(pos[3] - pos[2], pos[3] - pos[0])),
			Normalize(CrossProduct(pos[2] - pos[3], pos[2] - pos[5])),
			Normalize(CrossProduct(pos[4] - pos[3], pos[4] - pos[0])),
			Normalize(CrossProduct(pos[3] - pos[4], pos[3] - pos[5])),
			Normalize(CrossProduct(pos[1] - pos[4], pos[1] - pos[0])),
			Normalize(CrossProduct(pos[4] - pos[1], pos[4] - pos[5])) };
		const Color white = { 0xff, 0xff, 0xff, 0xff };
		const VertexPNC bones[24] = {
			{pos[0], normals[0], white}, {pos[2], normals[0], white},
			{pos[1], normals[0], white}, {pos[5], normals[1], white},
			{pos[1], normals[1], white}, {pos[2], normals[1], white},
			{pos[0], normals[2], white}, {pos[3], normals[2], white},
			{pos[2], normals[2], white}, {pos[5], normals[3], white},
			{pos[2], normals[3], white}, {pos[3], normals[3], white},
			{pos[0], normals[4], white}, {pos[4], normals[4], white},
			{pos[3], normals[4], white}, {pos[5], normals[5], white},
			{pos[3], normals[5], white}, {pos[4], normals[5], white},
			{pos[0], normals[6], white}, {pos[1], normals[6], white},
			{pos[4], normals[6], white}, {pos[5], normals[7], white},
			{pos[4], normals[7], white}, {pos[1], normals[7], white} };

		// Builds and fills the vbo.
		Model& bone = models_[0];
		bone.mode = GL_TRIANGLES;
		bone.count = JY_ARRAY_COUNT(bones);
		glGenBuffers(1, &bone.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, bone.vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(bones), bones, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);  // Unbinds.

		// Init bone shader.
		bone.shader = BoneShader::Build();
		if (!bone.shader) 
        {
			return false;
		}
	}

	{  // Prepares joint mesh.
		const int kNumSlices = 20;
		const int kNumPointsPerCircle = kNumSlices + 1;
		const int kNumPointsYZ = kNumPointsPerCircle;
		const int kNumPointsXY = kNumPointsPerCircle + kNumPointsPerCircle / 4;
		const int kNumPointsXZ = kNumPointsPerCircle;
		const int kNumPoints = kNumPointsXY + kNumPointsXZ + kNumPointsYZ;
		const float kRadius = kInter;  // Radius multiplier.
		const Color red = { 0xff, 0xc0, 0xc0, 0xff };
		const Color green = { 0xc0, 0xff, 0xc0, 0xff };
		const Color blue = { 0xc0, 0xc0, 0xff, 0xff };
		VertexPNC joints[kNumPoints];

		// Fills vertices.
		int index = 0;
		for (int j = 0; j < kNumPointsYZ; ++j)
        {  // YZ plan.
			float angle = j * Math::TWO_PI / kNumSlices;
			float s = sinf(angle), c = cosf(angle);
			VertexPNC& vertex = joints[index++];
			vertex.pos = Math::Vec3(0.f, c * kRadius, s * kRadius);
			vertex.normal = Math::Vec3(0.f, c, s);
			vertex.color = red;
		}
		for (int j = 0; j < kNumPointsXY; ++j) 
        {  // XY plan.
			float angle = j * Math::TWO_PI / kNumSlices;
			float s = sinf(angle), c = cosf(angle);
			VertexPNC& vertex = joints[index++];
			vertex.pos = Math::Vec3(s * kRadius, c * kRadius, 0.f);
			vertex.normal = Math::Vec3(s, c, 0.f);
			vertex.color = blue;
		}
		for (int j = 0; j < kNumPointsXZ; ++j) 
        {  // XZ plan.
			float angle = j * Math::TWO_PI / kNumSlices;
			float s = sinf(angle), c = cosf(angle);
			VertexPNC& vertex = joints[index++];
			vertex.pos = Math::Vec3(c * kRadius, 0.f, -s * kRadius);
			vertex.normal = Math::Vec3(c, 0.f, -s);
			vertex.color = green;
		}
		assert(index == kNumPoints);

		// Builds and fills the vbo.
		Model& joint = models_[1];
		joint.mode = GL_LINE_STRIP;
		joint.count = JY_ARRAY_COUNT(joints);
		glGenBuffers(1, &joint.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, joint.vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(joints), joints, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);  // Unbinds.

		// Init joint shader.
		joint.shader = JointShader::Build();
		if (!joint.shader) 
        {
			return false;
		}
	}

	return true;
}

bool Renderer::InitCheckeredTexture()
{
	const int kWidth = 1024;
	const int kCases = 64;

	glGenTextures(1, &checkered_texture_);
	glBindTexture(GL_TEXTURE_2D, checkered_texture_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Allocates for biggest mip level.
	const size_t buffer_size = 3 * kWidth * kWidth;
	uint8_t* pixels = reinterpret_cast<uint8_t*>(malloc(buffer_size));

	// Create the checkered pattern on all mip levels.
	int level_width = kWidth;
	for (int level = 0; level_width > 0; ++level, level_width /= 2) {
		if (level_width >= kCases) {
			const int case_width = level_width / kCases;
			for (int j = 0; j < level_width; ++j) {
				const int cpntj = (j / case_width) & 1;
				for (int i = 0; i < kCases; ++i) {
					const int cpnti = i & 1;
					const bool white_case = (cpnti ^ cpntj) != 0;
					const uint8_t cpntr =
						white_case ? 0xff : j * 255 / level_width & 0xff;
					const uint8_t cpntg = white_case ? 0xff : i * 255 / kCases & 0xff;
					const uint8_t cpntb = white_case ? 0xff : 0;

					const int case_start = j * level_width + i * case_width;
					for (int k = case_start; k < case_start + case_width; ++k) {
						pixels[k * 3 + 0] = cpntr;
						pixels[k * 3 + 1] = cpntg;
						pixels[k * 3 + 2] = cpntb;
					}
				}
			}
		}
		else {
			// Mimaps where width is smaller than the number of cases.
			for (int j = 0; j < level_width; ++j) {
				for (int i = 0; i < level_width; ++i) {
					pixels[(j * level_width + i) * 3 + 0] = 0x7f;
					pixels[(j * level_width + i) * 3 + 1] = 0x7f;
					pixels[(j * level_width + i) * 3 + 2] = 0x7f;
				}
			}
		}

		glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, level_width, level_width, 0,
			GL_RGB, GL_UNSIGNED_BYTE, pixels);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	free(pixels);

	return true;
}

void Renderer::DrawPosture_Impl(const Math::Mat4& _transform, const float* _uniforms, int _instance_count, bool _draw_joints)
{
	// Loops through models and instances.
	for (int i = 0; i < (_draw_joints ? 2 : 1); ++i) 
	{
		const Model& model = models_[i];

		// Setup model vertex data.
		glBindBuffer(GL_ARRAY_BUFFER, model.vbo);

		// Bind shader
		model.shader->Bind(_transform, m_cam->view_proj(), sizeof(VertexPNC), 0,
			sizeof(VertexPNC), 12, sizeof(VertexPNC), 24);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// Draw loop.
		const GLint joint_uniform = model.shader->joint_uniform();
		for (int j = 0; j < _instance_count; ++j) {
			glUniformMatrix4fv(joint_uniform, 1, false, _uniforms + 16 * j);
			glDrawArrays(model.mode, 0, model.count);
		}

		model.shader->Unbind();
	}
}

bool Renderer::DrawPosture(const Skeleton& _skeleton, const std::vector<Math::Mat4>& _matrices, const Math::Mat4& _transform, bool _draw_joints) 
{
	if (_matrices.size() < static_cast<size_t>(_skeleton.num_joints())) 
	{
		return false;
	}

	// Convert matrices to uniforms.
	const int max_skeleton_pieces = Skeleton::kMaxJoints * 2;
	const size_t max_uniforms_size = max_skeleton_pieces * 2 * 16 * sizeof(float);
	float* uniforms = static_cast<float*>(scratch_buffer_.Resize(max_uniforms_size));

	const int instance_count = DrawPosture_FillUniforms(
		_skeleton, _matrices, uniforms, max_skeleton_pieces);
	assert(instance_count <= max_skeleton_pieces);

	DrawPosture_Impl(_transform, uniforms, instance_count, _draw_joints);

	return true;
}

bool Renderer::DrawBoxIm(const Math::AABB& _box, const Math::Mat4& _transform, const Color _colors[2])
{
	{  // Filled boxed
		GlImmediatePC im(GetImmediateRenderer(), GL_TRIANGLE_STRIP, _transform);
		GlImmediatePC::Vertex v = {
			{0, 0, 0}, {_colors[0].r, _colors[0].g, _colors[0].b, _colors[0].a} };
		// First 3 cube faces
		v.pos[0] = _box._max.x;
		v.pos[1] = _box._min.y;
		v.pos[2] = _box._min.z;
		im.PushVertex(v);
		v.pos[0] = _box._min.x;
		im.PushVertex(v);
		v.pos[0] = _box._max.x;
		v.pos[1] = _box._max.y;
		im.PushVertex(v);
		v.pos[0] = _box._min.x;
		im.PushVertex(v);
		v.pos[0] = _box._max.x;
		v.pos[2] = _box._max.z;
		im.PushVertex(v);
		v.pos[0] = _box._min.x;
		im.PushVertex(v);
		v.pos[0] = _box._max.x;
		v.pos[1] = _box._min.y;
		im.PushVertex(v);
		v.pos[0] = _box._min.x;
		im.PushVertex(v);
		// Link next 3 cube faces with degenerated triangles.
		im.PushVertex(v);
		v.pos[0] = _box._min.x;
		v.pos[1] = _box._max.y;
		im.PushVertex(v);
		im.PushVertex(v);
		// Last 3 cube faces.
		v.pos[2] = _box._min.z;
		im.PushVertex(v);
		v.pos[1] = _box._min.y;
		v.pos[2] = _box._max.z;
		im.PushVertex(v);
		v.pos[2] = _box._min.z;
		im.PushVertex(v);
		v.pos[0] = _box._max.x;
		v.pos[2] = _box._max.z;
		im.PushVertex(v);
		v.pos[2] = _box._min.z;
		im.PushVertex(v);
		v.pos[1] = _box._max.y;
		v.pos[2] = _box._max.z;
		im.PushVertex(v);
		v.pos[2] = _box._min.z;
		im.PushVertex(v);
	}

	{  // Wireframe boxed
		GlImmediatePC im(GetImmediateRenderer(), GL_LINES, _transform);
		GlImmediatePC::Vertex v = {
			{0, 0, 0}, {_colors[1].r, _colors[1].g, _colors[1].b, _colors[1].a} };
		// First face.
		v.pos[0] = _box._min.x;
		v.pos[1] = _box._min.y;
		v.pos[2] = _box._min.z;
		im.PushVertex(v);
		v.pos[1] = _box._max.y;
		im.PushVertex(v);
		im.PushVertex(v);
		v.pos[0] = _box._max.x;
		im.PushVertex(v);
		im.PushVertex(v);
		v.pos[1] = _box._min.y;
		im.PushVertex(v);
		im.PushVertex(v);
		v.pos[0] = _box._min.x;
		im.PushVertex(v);
		// Second face.
		v.pos[2] = _box._max.z;
		im.PushVertex(v);
		v.pos[1] = _box._max.y;
		im.PushVertex(v);
		im.PushVertex(v);
		v.pos[0] = _box._max.x;
		im.PushVertex(v);
		im.PushVertex(v);
		v.pos[1] = _box._min.y;
		im.PushVertex(v);
		im.PushVertex(v);
		v.pos[0] = _box._min.x;
		im.PushVertex(v);
		// Link faces.
		im.PushVertex(v);
		v.pos[2] = _box._min.z;
		im.PushVertex(v);
		v.pos[1] = _box._max.y;
		im.PushVertex(v);
		v.pos[2] = _box._max.z;
		im.PushVertex(v);
		v.pos[0] = _box._max.x;
		im.PushVertex(v);
		v.pos[2] = _box._min.z;
		im.PushVertex(v);
		v.pos[1] = _box._min.y;
		im.PushVertex(v);
		v.pos[2] = _box._max.z;
		im.PushVertex(v);
	}

	return true;
}

bool Renderer::DrawPoints(const span<const float>& _positions,
	const span<const float>& _sizes,
	const span<const Color>& _colors,
	const Math::Mat4& _transform,
	bool _round, bool _screen_space) 
{
	// Early out if no instance to render.
	if (_positions.size() == 0) {
		return true;
	}

	// Sizes and colors must be of size 1 or equal to _positions' size.
	if (_sizes.size() != 1 && _sizes.size() != _positions.size() / 3) {
		return false;
	}
	if (_colors.size() != 1 && _colors.size() != _positions.size() / 3) {
		return false;
	}

	const GLsizei positions_size = static_cast<GLsizei>(_positions.size_bytes());
	const GLsizei colors_size =
		static_cast<GLsizei>(_colors.size() == 1 ? 0 : _colors.size_bytes());
	const GLsizei sizes_size =
		static_cast<GLsizei>(_sizes.size() == 1 ? 0 : _sizes.size_bytes());
	const GLsizei buffer_size = positions_size + colors_size + sizes_size;
	const GLsizei positions_offset = 0;
	const GLsizei colors_offset = positions_offset + positions_size;
	const GLsizei sizes_offset = colors_offset + colors_size;

	// Reallocate vertex buffer.
	glBindBuffer(GL_ARRAY_BUFFER, dynamic_array_bo_);
	glBufferData(GL_ARRAY_BUFFER, buffer_size, nullptr, GL_STREAM_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, positions_offset, positions_size,
		_positions.data());
	glBufferSubData(GL_ARRAY_BUFFER, colors_offset, colors_size,
		_colors.data());
	glBufferSubData(GL_ARRAY_BUFFER, sizes_offset, sizes_size, _sizes.data());

	// Square or round sprites. Beware msaa makes sprites round if GL_POINT_SPRITE
	// isn't enabled
	if (_round) {
		glEnable(GL_POINT_SMOOTH);
		glDisable(GL_POINT_SPRITE);
	}
	else {
		glDisable(GL_POINT_SMOOTH);
		glEnable(GL_POINT_SPRITE);
	}

	// Size is managed in vertex shader side.
	glEnable(GL_PROGRAM_POINT_SIZE);

	const PointsShader::GenericAttrib attrib =
		points_shader->Bind(_transform, GetCamera()->view_proj(), 12,
			positions_offset, colors_size ? 4 : 0, colors_offset,
			sizes_size ? 4 : 0, sizes_offset, _screen_space);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Apply remaining general attributes
	if (_colors.size() <= 1) {
		const Color color = _colors.empty() ? kWhite : _colors[0];
		glVertexAttrib4f(attrib.color, color.r / 255.f, color.g / 255.f,
			color.b / 255.f, color.a / 255.f);
	}
	if (_sizes.size() <= 1) {
		const float size = _sizes.empty() ? 1.f : _sizes[0];
		glVertexAttrib1f(attrib.size, size);
	}

	// Draws the mesh.
	glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(_positions.size() / 3));

	// Unbinds.
	points_shader->Unbind();

	return true;
}

bool Renderer::DrawVectors(span<const float> _positions,
	size_t _positions_stride,
	span<const float> _directions,
	size_t _directions_stride, int _num_vectors,
	float _vector_length, Color _color,
	const Math::Mat4& _transform) 
{
	// Invalid range length.
	if (PointerStride(_positions.begin(), _positions_stride * _num_vectors) >
		_positions.end() ||
		PointerStride(_directions.begin(), _directions_stride * _num_vectors) >
		_directions.end()) 
	{
		return false;
	}

	GlImmediatePC im(immediate_.get(), GL_LINES, _transform);
	GlImmediatePC::Vertex v = { {0, 0, 0}, {_color.r, _color.g, _color.b, _color.a} };

	for (int i = 0; i < _num_vectors; ++i) 
	{
		const float* position = PointerStride(_positions.data(), _positions_stride * i);
		v.pos[0] = position[0];
		v.pos[1] = position[1];
		v.pos[2] = position[2];
		im.PushVertex(v);

		const float* direction = PointerStride(_directions.data(), _directions_stride * i);
		v.pos[0] = position[0] + direction[0] * _vector_length;
		v.pos[1] = position[1] + direction[1] * _vector_length;
		v.pos[2] = position[2] + direction[2] * _vector_length;
		im.PushVertex(v);
	}

	return true;
}

bool Renderer::DrawBinormals(
	span<const float> _positions, size_t _positions_stride,
	span<const float> _normals, size_t _normals_stride,
	span<const float> _tangents, size_t _tangents_stride,
	span<const float> _handenesses, size_t _handenesses_stride,
	int _num_vectors, float _vector_length, Color _color,
	const Math::Mat4& _transform) 
{
	// Invalid range length.
	if (PointerStride(_positions.begin(), _positions_stride * _num_vectors) >
		_positions.end() ||
		PointerStride(_normals.begin(), _normals_stride * _num_vectors) >
		_normals.end() ||
		PointerStride(_tangents.begin(), _tangents_stride * _num_vectors) >
		_tangents.end() ||
		PointerStride(_handenesses.begin(), _handenesses_stride * _num_vectors) >
		_handenesses.end()) 
	{
		return false;
	}

	GlImmediatePC im(immediate_.get(), GL_LINES, _transform);
	GlImmediatePC::Vertex v = { {0, 0, 0}, {_color.r, _color.g, _color.b, _color.a} };

	for (int i = 0; i < _num_vectors; ++i)
	{
		const float* position = PointerStride(_positions.data(), _positions_stride * i);
		v.pos[0] = position[0];
		v.pos[1] = position[1];
		v.pos[2] = position[2];
		im.PushVertex(v);

		// Compute binormal.
		const float* p_normal = PointerStride(_normals.data(), _normals_stride * i);
		const Math::Vec3 normal(p_normal[0], p_normal[1], p_normal[2]);
		const float* p_tangent =
			PointerStride(_tangents.data(), _tangents_stride * i);
		const Math::Vec3 tangent(p_tangent[0], p_tangent[1], p_tangent[2]);
		const float* p_handedness =
			PointerStride(_handenesses.data(), _handenesses_stride * i);
		// Handedness is used to flip binormal.
		const Math::Vec3 binormal = CrossProduct(normal, tangent) * p_handedness[0];

		v.pos[0] = position[0] + binormal.x * _vector_length;
		v.pos[1] = position[1] + binormal.y * _vector_length;
		v.pos[2] = position[2] + binormal.z * _vector_length;
		im.PushVertex(v);
	}
	return true;
}


namespace 
{
	const uint8_t kDefaultColorsArray[][4] = {
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
		{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255} };

	const float kDefaultNormalsArray[][3] = {
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f},
		{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f} };

	const float kDefaultUVsArray[][2] = {
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f} };
}  // namespace

bool Renderer::DrawSkinnedMesh(
	const Mesh& _mesh, const span<Math::Mat4> _skinning_matrices,
	const Math::Mat4& _transform, const Options& _options) 
{
	// Forward to DrawMesh function is skinning is disabled.
	if (_options.skip_skinning || !_mesh.skinned()) 
	{
		return DrawMesh(_mesh, _transform, _options);
	}

	if (_options.wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	const int vertex_count = _mesh.vertex_count();

	// Positions and normals are interleaved to improve caching while executing
	// skinning job.

	const GLsizei positions_offset = 0;
	const GLsizei positions_stride = sizeof(float) * 3;
	const GLsizei normals_offset = vertex_count * positions_stride;
	const GLsizei normals_stride = sizeof(float) * 3;
	const GLsizei tangents_offset =
		normals_offset + vertex_count * normals_stride;
	const GLsizei tangents_stride = sizeof(float) * 3;
	const GLsizei skinned_data_size =
		tangents_offset + vertex_count * tangents_stride;

	// Colors and uvs are contiguous. They aren't transformed, so they can be
	// directly copied from source mesh which is non-interleaved as-well.
	// Colors will be filled with white if _options.colors is false.
	// UVs will be skipped if _options.textured is false.
	const GLsizei colors_offset = skinned_data_size;
	const GLsizei colors_stride = sizeof(uint8_t) * 4;
	const GLsizei colors_size = vertex_count * colors_stride;
	const GLsizei uvs_offset = colors_offset + colors_size;
	const GLsizei uvs_stride = _options.texture ? sizeof(float) * 2 : 0;
	const GLsizei uvs_size = vertex_count * uvs_stride;
	const GLsizei fixed_data_size = colors_size + uvs_size;

	// Reallocate vertex buffer.
	const GLsizei vbo_size = skinned_data_size + fixed_data_size;
	void* vbo_map = scratch_buffer_.Resize(vbo_size);

	// Iterate mesh parts and fills vbo.
	// Runs a skinning job per mesh part. Triangle indices are shared
	// across parts.
	size_t processed_vertex_count = 0;
	for (size_t i = 0; i < _mesh.parts.size(); ++i) 
	{
		const Mesh::Part& part = _mesh.parts[i];

		// Skip this iteration if no vertex.
		const size_t part_vertex_count = part.positions.size() / 3;
		if (part_vertex_count == 0) {
			continue;
		}

		// Fills the job.
		SkinningJob skinning_job;
		skinning_job.vertex_count = static_cast<int>(part_vertex_count);
		const int part_influences_count = part.influences_count();

		// Clamps joints influence count according to the option.
		skinning_job.influences_count = part_influences_count;

		// Setup skinning matrices, that came from the animation stage before being
		// multiplied by inverse model-space bind-pose.
		skinning_job.joint_matrices = _skinning_matrices;

		// Setup joint's indices.
		skinning_job.joint_indices = make_span(part.joint_indices);
		skinning_job.joint_indices_stride = sizeof(uint16_t) * part_influences_count;

		// Setup joint's weights.
		if (part_influences_count > 1) {
			skinning_job.joint_weights = make_span(part.joint_weights);
			skinning_job.joint_weights_stride =
				sizeof(float) * (part_influences_count - 1);
		}

		// Setup input positions, coming from the loaded mesh.
		skinning_job.in_positions = make_span(part.positions);
		skinning_job.in_positions_stride = sizeof(float) * Mesh::Part::kPositionsCpnts;

		// Setup output positions, coming from the rendering output mesh buffers.
		// We need to offset the buffer every loop.
		float* out_positions_begin = reinterpret_cast<float*>(PointerStride(vbo_map, positions_offset + processed_vertex_count * positions_stride));
		float* out_positions_end = PointerStride(out_positions_begin, part_vertex_count * positions_stride);
		skinning_job.out_positions = { out_positions_begin, out_positions_end };
		skinning_job.out_positions_stride = positions_stride;

		// Setup normals if input are provided.
		float* out_normal_begin = reinterpret_cast<float*>(PointerStride(vbo_map, normals_offset + processed_vertex_count * normals_stride));
		float* out_normal_end = PointerStride(out_normal_begin, part_vertex_count * normals_stride);

		if (part.normals.size() / Mesh::Part::kNormalsCpnts == part_vertex_count)
		{
			// Setup input normals, coming from the loaded mesh.
			skinning_job.in_normals = make_span(part.normals);
			skinning_job.in_normals_stride =
				sizeof(float) * Mesh::Part::kNormalsCpnts;

			// Setup output normals, coming from the rendering output mesh buffers.
			// We need to offset the buffer every loop.
			skinning_job.out_normals = { out_normal_begin, out_normal_end };
			skinning_job.out_normals_stride = normals_stride;
		}
		else 
		{
			// Fills output with default normals.
			for (float* normal = out_normal_begin; normal < out_normal_end; normal = PointerStride(normal, normals_stride)) 
			{
				normal[0] = 0.f;
				normal[1] = 1.f;
				normal[2] = 0.f;
			}
		}

		// Setup tangents if input are provided.
		float* out_tangent_begin = reinterpret_cast<float*>(PointerStride(vbo_map, tangents_offset + processed_vertex_count * tangents_stride));
		float* out_tangent_end = PointerStride(out_tangent_begin, part_vertex_count * tangents_stride);

		if (part.tangents.size() / Mesh::Part::kTangentsCpnts == part_vertex_count) 
		{
			// Setup input tangents, coming from the loaded mesh.
			skinning_job.in_tangents = make_span(part.tangents);
			skinning_job.in_tangents_stride = sizeof(float) * Mesh::Part::kTangentsCpnts;

			// Setup output tangents, coming from the rendering output mesh buffers.
			// We need to offset the buffer every loop.
			skinning_job.out_tangents = { out_tangent_begin, out_tangent_end };
			skinning_job.out_tangents_stride = tangents_stride;
		}
		else 
		{
			// Fills output with default tangents.
			for (float* tangent = out_tangent_begin; tangent < out_tangent_end; tangent = PointerStride(tangent, tangents_stride)) 
			{
				tangent[0] = 1.f;
				tangent[1] = 0.f;
				tangent[2] = 0.f;
			}
		}

		// Execute the job, which should succeed unless a parameter is invalid.
		if (!skinning_job.Run()) {
			return false;
		}

		// Renders debug normals.
		if (_options.normals && skinning_job.out_normals.size() > 0) 
		{
			DrawVectors(skinning_job.out_positions, skinning_job.out_positions_stride,
				skinning_job.out_normals, skinning_job.out_normals_stride,
				skinning_job.vertex_count, .03f, kGreen,
				_transform);
		}

		// Renders debug tangents.
		if (_options.tangents && skinning_job.out_tangents.size() > 0) 
		{
			DrawVectors(skinning_job.out_positions, skinning_job.out_positions_stride,
				skinning_job.out_tangents, skinning_job.out_tangents_stride,
				skinning_job.vertex_count, .03f, kRed,
				_transform);
		}

		// Renders debug binormals.
		if (_options.binormals && skinning_job.out_normals.size() > 0 && skinning_job.out_tangents.size() > 0)
		{
			DrawBinormals(skinning_job.out_positions,
				skinning_job.out_positions_stride, skinning_job.out_normals,
				skinning_job.out_normals_stride, skinning_job.out_tangents,
				skinning_job.out_tangents_stride,
				span<const float>(skinning_job.in_tangents.begin() + 3,
					skinning_job.in_tangents.end() + 3),
				skinning_job.in_tangents_stride, skinning_job.vertex_count,
				.03f, kBlue, _transform);
		}

		// Handles colors which aren't affected by skinning.
		if (_options.colors && part_vertex_count == part.colors.size() / Mesh::Part::kColorsCpnts)
		{
			// Optimal path used when the right number of colors is provided.
			memcpy(
				PointerStride(
					vbo_map, colors_offset + processed_vertex_count * colors_stride),
				(part.colors).data(), part_vertex_count * colors_stride);
		}
		else 
		{
			// Un-optimal path used when the right number of colors is not provided.
			static_assert(sizeof(kDefaultColorsArray[0]) == colors_stride, "Stride mismatch");

			for (size_t j = 0; j < part_vertex_count;
				j += JY_ARRAY_COUNT(kDefaultColorsArray)) {
				const size_t this_loop_count = Math::Min(
					JY_ARRAY_COUNT(kDefaultColorsArray), part_vertex_count - j);
				memcpy(PointerStride(
					vbo_map, colors_offset +
					(processed_vertex_count + j) * colors_stride),
					kDefaultColorsArray, colors_stride * this_loop_count);
			}
		}

		// Copies uvs which aren't affected by skinning.
		if (_options.texture) 
		{
			if (part_vertex_count == part.uvs.size() / Mesh::Part::kUVsCpnts) 
			{
				// Optimal path used when the right number of uvs is provided.
				memcpy(PointerStride(
					vbo_map, uvs_offset + processed_vertex_count * uvs_stride),
					part.uvs.data(), part_vertex_count * uvs_stride);
			}
			else 
			{
				// Un-optimal path used when the right number of uvs is not provided.
				assert(sizeof(kDefaultUVsArray[0]) == uvs_stride);
				for (size_t j = 0; j < part_vertex_count; j += JY_ARRAY_COUNT(kDefaultUVsArray)) 
				{
					const size_t this_loop_count = Math::Min(JY_ARRAY_COUNT(kDefaultUVsArray), part_vertex_count - j);
					memcpy(PointerStride(
						vbo_map,
						uvs_offset + (processed_vertex_count + j) * uvs_stride),
						kDefaultUVsArray, uvs_stride * this_loop_count);
				}
			}
		}

		// Some more vertices were processed.
		processed_vertex_count += part_vertex_count;
	}

	if (_options.triangles) 
	{
		// Updates dynamic vertex buffer with skinned data.
		glBindBuffer(GL_ARRAY_BUFFER, dynamic_array_bo_);
		glBufferData(GL_ARRAY_BUFFER, vbo_size, nullptr, GL_STREAM_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, vbo_size, vbo_map);

		// Binds shader with this array buffer, depending on rendering options.
		Shader* shader = nullptr;
		if (_options.texture) {
			ambient_textured_shader->Bind(
				_transform, GetCamera()->view_proj(), positions_stride, positions_offset,
				normals_stride, normals_offset, colors_stride, colors_offset,
				uvs_stride, uvs_offset);
			shader = ambient_textured_shader.get();

			// Binds default texture
			glBindTexture(GL_TEXTURE_2D, checkered_texture_);
		}
		else {
			ambient_shader->Bind(_transform, GetCamera()->view_proj(), positions_stride,
				positions_offset, normals_stride, normals_offset,
				colors_stride, colors_offset);
			shader = ambient_shader.get();
		}

		// Maps the index dynamic buffer and update it.
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamic_index_bo_);
		const Mesh::TriangleIndices& indices = _mesh.triangle_indices;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			indices.size() * sizeof(Mesh::TriangleIndices::value_type),
			indices.data(), GL_STREAM_DRAW);

		// Draws the mesh.
		static_assert(sizeof(Mesh::TriangleIndices::value_type) == 2,
			"Expects 2 bytes indices.");
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
			GL_UNSIGNED_SHORT, 0);

		// Unbinds.
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		shader->Unbind();
	}

	if (_options.wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	// Renders debug vertices.
	if (_options.vertices) 
	{
		Color color = kWhite;
		span<const Color> colors;
		if (_options.colors) 
		{
			colors = { reinterpret_cast<const Color*>(PointerStride(vbo_map, colors_offset)), static_cast<size_t>(vertex_count) };
		}
		else {
			colors = { &color, 1 };
		}

		const span<const float> vertices{reinterpret_cast<const float*>(PointerStride(vbo_map, positions_offset)), static_cast<size_t>(vertex_count * 3) };
		const float size = 2.f;
		DrawPoints(vertices, { &size, 1 }, colors, _transform, true, true);
	}

	return true;
}


bool Renderer::DrawMesh(const Mesh& _mesh,
	const Math::Mat4& _transform,
	const Options& _options) 
{
	if (_options.wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	const int vertex_count = _mesh.vertex_count();
	const GLsizei positions_offset = 0;
	const GLsizei positions_stride =
		sizeof(float) * Mesh::Part::kPositionsCpnts;
	const GLsizei positions_size = vertex_count * positions_stride;
	const GLsizei normals_offset = positions_offset + positions_size;
	const GLsizei normals_stride =
		sizeof(float) * Mesh::Part::kNormalsCpnts;
	const GLsizei normals_size = vertex_count * normals_stride;
	// Colors will be filled with white if _options.colors is false.
	const GLsizei colors_offset = normals_offset + normals_size;
	const GLsizei colors_stride =
		sizeof(uint8_t) * Mesh::Part::kColorsCpnts;
	const GLsizei colors_size = vertex_count * colors_stride;
	// Uvs are skipped if _options.texture is false.
	const GLsizei uvs_offset = colors_offset + colors_size;
	const GLsizei uvs_stride =
		_options.texture ? sizeof(float) * Mesh::Part::kUVsCpnts : 0;
	const GLsizei uvs_size = vertex_count * uvs_stride;

	// Reallocate vertex buffer.
	const GLsizei vbo_size =
		positions_size + normals_size + colors_size + uvs_size;
	glBindBuffer(GL_ARRAY_BUFFER, dynamic_array_bo_);
	glBufferData(GL_ARRAY_BUFFER, vbo_size, nullptr, GL_STREAM_DRAW);

	// Iterate mesh parts and fills vbo.
	size_t vertex_offset = 0;
	for (size_t i = 0; i < _mesh.parts.size(); ++i) {
		const Mesh::Part& part = _mesh.parts[i];
		const size_t part_vertex_count = part.vertex_count();

		// Handles positions.
		glBufferSubData(
			GL_ARRAY_BUFFER, positions_offset + vertex_offset * positions_stride,
			part_vertex_count * positions_stride, (part.positions).data());

		// Handles normals.
		const size_t part_normal_count =
			part.normals.size() / Mesh::Part::kNormalsCpnts;
		if (part_vertex_count == part_normal_count) {
			// Optimal path used when the right number of normals is provided.
			glBufferSubData(
				GL_ARRAY_BUFFER, normals_offset + vertex_offset * normals_stride,
				part_normal_count * normals_stride, (part.normals).data());
		}
		else {
			// Un-optimal path used when the right number of normals is not provided.
			static_assert(sizeof(kDefaultNormalsArray[0]) == normals_stride,
				"Stride mismatch");
			for (size_t j = 0; j < part_vertex_count;
				j += JY_ARRAY_COUNT(kDefaultNormalsArray)) {
				const size_t this_loop_count = Math::Min(
					JY_ARRAY_COUNT(kDefaultNormalsArray), part_vertex_count - j);
				glBufferSubData(GL_ARRAY_BUFFER,
					normals_offset + (vertex_offset + j) * normals_stride,
					normals_stride * this_loop_count,
					kDefaultNormalsArray);
			}
		}

		// Handles colors.
		const size_t part_color_count =
			part.colors.size() / Mesh::Part::kColorsCpnts;
		if (_options.colors && part_vertex_count == part_color_count) {
			// Optimal path used when the right number of colors is provided.
			glBufferSubData(
				GL_ARRAY_BUFFER, colors_offset + vertex_offset * colors_stride,
				part_color_count * colors_stride, (part.colors).data());
		}
		else {
			// Un-optimal path used when the right number of colors is not provided.
			static_assert(sizeof(kDefaultColorsArray[0]) == colors_stride,
				"Stride mismatch");
			for (size_t j = 0; j < part_vertex_count;
				j += JY_ARRAY_COUNT(kDefaultColorsArray)) {
				const size_t this_loop_count = Math::Min(
					JY_ARRAY_COUNT(kDefaultColorsArray), part_vertex_count - j);
				glBufferSubData(GL_ARRAY_BUFFER,
					colors_offset + (vertex_offset + j) * colors_stride,
					colors_stride * this_loop_count, kDefaultColorsArray);
			}
		}

		// Handles uvs.
		if (_options.texture) {
			const size_t part_uvs_count =
				part.uvs.size() / Mesh::Part::kUVsCpnts;
			if (part_vertex_count == part_uvs_count) {
				// Optimal path used when the right number of uvs is provided.
				glBufferSubData(GL_ARRAY_BUFFER,
					uvs_offset + vertex_offset * uvs_stride,
					part_uvs_count * uvs_stride, (part.uvs).data());
			}
			else {
				// Un-optimal path used when the right number of uvs is not provided.
				assert(sizeof(kDefaultUVsArray[0]) == uvs_stride);
				for (size_t j = 0; j < part_vertex_count;
					j += JY_ARRAY_COUNT(kDefaultUVsArray)) {
					const size_t this_loop_count = Math::Min(
						JY_ARRAY_COUNT(kDefaultUVsArray), part_vertex_count - j);
					glBufferSubData(GL_ARRAY_BUFFER,
						uvs_offset + (vertex_offset + j) * uvs_stride,
						uvs_stride * this_loop_count, kDefaultUVsArray);
				}
			}
		}

		// Computes next loop offset.
		vertex_offset += part_vertex_count;
	}

	if (_options.triangles) {
		// Binds shader with this array buffer, depending on rendering options.
		Shader* shader = nullptr;
		if (_options.texture) {
			ambient_textured_shader->Bind(
				_transform, GetCamera()->view_proj(), positions_stride, positions_offset,
				normals_stride, normals_offset, colors_stride, colors_offset,
				uvs_stride, uvs_offset);
			shader = ambient_textured_shader.get();

			// Binds default texture
			glBindTexture(GL_TEXTURE_2D, checkered_texture_);
		}
		else {
			ambient_shader->Bind(_transform, GetCamera()->view_proj(), positions_stride,
				positions_offset, normals_stride, normals_offset,
				colors_stride, colors_offset);
			shader = ambient_shader.get();
		}

		// Maps the index dynamic buffer and update it.
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamic_index_bo_);
		const Mesh::TriangleIndices& indices = _mesh.triangle_indices;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			indices.size() * sizeof(Mesh::TriangleIndices::value_type),
			(indices).data(), GL_STREAM_DRAW);

		// Draws the mesh.
		static_assert(sizeof(Mesh::TriangleIndices::value_type) == 2,
			"Expects 2 bytes indices.");
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
			GL_UNSIGNED_SHORT, 0);

		// Unbinds.
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		shader->Unbind();
	}

	if (_options.wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	// Renders debug vertices.
	if (_options.vertices) {
		for (size_t i = 0; i < _mesh.parts.size(); ++i) {
			const Mesh::Part& part = _mesh.parts[i];
			Color color = kWhite;
			span<const Color> colors;
			if (_options.colors && part.colors.size() == part.positions.size() / 3) {
				colors = {
					reinterpret_cast<const Color*>(part.colors.data()),
					part.positions.size() / 3 };
			}
			else {
				colors = { &color, 1 };
			}
			const float size = 2.f;
			DrawPoints({ part.positions.data(), part.positions.size() }, { &size, 1 },
				{ &color, 1 }, _transform, true, true);
		}
	}

	// Renders debug normals.
	if (_options.normals) {
		for (size_t i = 0; i < _mesh.parts.size(); ++i) {
			const Mesh::Part& part = _mesh.parts[i];
			DrawVectors(make_span(part.positions),
				Mesh::Part::kPositionsCpnts * sizeof(float),
				make_span(part.normals),
				Mesh::Part::kNormalsCpnts * sizeof(float),
				part.vertex_count(), .03f, kGreen, _transform);
		}
	}

	// Renders debug tangents.
	if (_options.tangents) {
		for (size_t i = 0; i < _mesh.parts.size(); ++i) {
			const Mesh::Part& part = _mesh.parts[i];
			if (part.normals.size() != 0) {
				DrawVectors(make_span(part.positions),
					Mesh::Part::kPositionsCpnts * sizeof(float),
					make_span(part.tangents),
					Mesh::Part::kTangentsCpnts * sizeof(float),
					part.vertex_count(), .03f, kRed, _transform);
			}
		}
	}

	// Renders debug binormals.
	if (_options.binormals) {
		for (size_t i = 0; i < _mesh.parts.size(); ++i) {
			const Mesh::Part& part = _mesh.parts[i];
			if (part.normals.size() != 0 && part.tangents.size() != 0) {
				DrawBinormals(
					make_span(part.positions),
					Mesh::Part::kPositionsCpnts * sizeof(float),
					make_span(part.normals),
					Mesh::Part::kNormalsCpnts * sizeof(float),
					make_span(part.tangents),
					Mesh::Part::kTangentsCpnts * sizeof(float),
					span<const float>(&part.tangents[3], part.tangents.size()),
					Mesh::Part::kTangentsCpnts * sizeof(float),
					part.vertex_count(), .03f, kBlue, _transform);
			}
		}
	}

	return true;
}
