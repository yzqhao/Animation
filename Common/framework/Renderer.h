#pragma once

#include "../../Math/3DMath.h"
#include "Shader.h"
#include "../span.h"

class Skeleton;
class Mesh;
class Camera;
class GlImmediateRenderer;

struct Color 
{
	unsigned char r, g, b, a;
};

// Color constants.
static const Color kRed = { 0xff, 0, 0, 0xff };
static const Color kGreen = { 0, 0xff, 0, 0xff };
static const Color kBlue = { 0, 0, 0xff, 0xff };
static const Color kWhite = { 0xff, 0xff, 0xff, 0xff };
static const Color kYellow = { 0xff, 0xff, 0, 0xff };
static const Color kMagenta = { 0xff, 0, 0xff, 0xff };
static const Color kCyan = { 0, 0xff, 0xff, 0xff };
static const Color kGrey = { 0x80, 0x80, 0x80, 0xff };
static const Color kBlack = { 0x80, 0x80, 0x80, 0xff };

class Renderer
{
public:
	struct Options
	{
		bool triangles;  // Show triangles mesh.
		bool texture;    // Show texture (default checkered texture).
		bool vertices;   // Show vertices as points.
		bool normals;    // Show normals.
		bool tangents;   // Show tangents.
		bool binormals;  // Show binormals, computed from the normal and tangent.
		bool colors;     // Show vertex colors.
		bool wireframe;  // Show vertex colors.
		bool skip_skinning;  // Show texture (default checkered texture).

		Options()
			: triangles(true),
			texture(false),
			vertices(false),
			normals(false),
			tangents(false),
			binormals(false),
			colors(false),
			wireframe(false),
			skip_skinning(false) {}

		Options(bool _triangles, bool _texture, bool _vertices, bool _normals,
			bool _tangents, bool _binormals, bool _colors, bool _wireframe,
			bool _skip_skinning)
			: triangles(_triangles),
			texture(_texture),
			vertices(_vertices),
			normals(_normals),
			tangents(_tangents),
			binormals(_binormals),
			colors(_colors),
			wireframe(_wireframe),
			skip_skinning(_skip_skinning) {}
	};
public:
    Renderer(Camera* cam);
    ~Renderer();

    bool Initialize();

    bool DrawPosture(const Skeleton& skeleton, const std::vector<Math::Mat4>& matrices, const Math::Mat4& transform, bool draw_joints = true);

	bool DrawBoxIm(const Math::AABB& _box, const Math::Mat4& _transform, const Color _colors[2]);

	bool DrawSkinnedMesh(const Mesh& _mesh,
		const span<Math::Mat4> _skinning_matrices,
		const Math::Mat4& _transform,
		const Options& _options = Options());

	bool DrawMesh(const Mesh& _mesh,
		const Math::Mat4& _transform,
		const Options& _options = Options());

	bool DrawPoints(
		const span<const float>& _positions,
		const span<const float>& _sizes,
		const span<const Color>& _colors,
		const Math::Mat4& _transform, bool _round, bool _screen_space);

	bool DrawVectors(span<const float> _positions,
		size_t _positions_stride,
		span<const float> _directions,
		size_t _directions_stride, int _num_vectors,
		float _vector_length, Color _color,
		const Math::Mat4& _transform);

	bool DrawBinormals(
		span<const float> _positions, size_t _positions_stride,
		span<const float> _normals, size_t _normals_stride,
		span<const float> _tangents, size_t _tangents_stride,
		span<const float> _handenesses, size_t _handenesses_stride,
		int _num_vectors, float _vector_length, Color _color,
		const Math::Mat4& _transform);

	Camera* GetCamera() const { return m_cam; }

	GlImmediateRenderer* GetImmediateRenderer() const { return immediate_.get(); }

private:
	bool InitPostureRendering();

	bool InitCheckeredTexture();

	// Draw posture internal non-instanced rendering fall back implementation.
	void DrawPosture_Impl(const Math::Mat4& _transform, const float* _uniforms, int _instance_count, bool _draw_joints);

private:
	Camera* m_cam;

    struct Model 
    {
        Model();
        ~Model();

        GLuint vbo;
        GLenum mode;
        GLsizei count;
        std::unique_ptr<SkeletonShader> shader;
    };
    Model models_[2];

	// Volatile memory buffer that can be used within function scope.
	// Minimum alignment is 16 bytes.
	class ScratchBuffer 
	{
	public:
		ScratchBuffer();
		~ScratchBuffer();

		// Resizes the buffer to the new size and return the memory address.
		void* Resize(size_t _size);

	private:
		void* buffer_;
		size_t size_;
	};
	ScratchBuffer scratch_buffer_;

	GLuint dynamic_array_bo_;
	GLuint dynamic_index_bo_;

	std::unique_ptr<GlImmediateRenderer> immediate_;

	// Ambient rendering shader.
	std::unique_ptr<AmbientShader> ambient_shader;
	std::unique_ptr<AmbientTexturedShader> ambient_textured_shader;
	std::unique_ptr<PointsShader> points_shader;

	// Checkered texture
	unsigned int checkered_texture_;
};