#pragma once

#include "../../Math/3DMath.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Renderer;
class ImmediatePCShader;
class ImmediatePNShader;
class ImmediatePTCShader;

// Declares supported vertex formats.
// Position + Color.
struct VertexPC 
{
	float pos[3];
	GLubyte rgba[4];
};

// Declares supported vertex formats.
// Position + Normal.
struct VertexPN 
{
	float pos[3];
	float normal[3];
};

// Position + Texture coordinate + Color.
struct VertexPTC 
{
	float pos[3];
	float uv[2];
	GLubyte rgba[4];
};

// Declares Immediate mode types.
template <typename _Ty>
class GlImmediate;
typedef GlImmediate<VertexPC> GlImmediatePC;
typedef GlImmediate<VertexPN> GlImmediatePN;
typedef GlImmediate<VertexPTC> GlImmediatePTC;

// GL immedialte mode renderer.
// Should be used with a GlImmediate object to push vertices to the renderer.
class GlImmediateRenderer 
{
public:
	GlImmediateRenderer(Renderer* _renderer);
	~GlImmediateRenderer();

	// Initializes immediate  mode renderer. Can fail.
	bool Initialize();

private:
	// GlImmediate is used to work with the renderer.
	template <typename _Ty>
	friend class GlImmediate;

	// Begin stacking vertices.
	void Begin();

	// End stacking vertices. Call GL rendering.
	template <typename _Ty>
	void End(GLenum _mode, const Math::Mat4& _transform);

	// Push a new vertex to the buffer.
	template <typename _Ty>
	FORCEINLINE void PushVertex(const _Ty& _vertex) 
	{
		// Resize buffer if needed.
		const size_t new_size = size_ + sizeof(_Ty);
		if (new_size > buffer_.size()) 
		{
			buffer_.resize(new_size);
		}

		// Copy this last vertex.
		std::memcpy(buffer_.data() + size_, &_vertex, sizeof(_Ty));
		size_ = new_size;
	}

	// The vertex object used by the renderer.
	GLuint vbo_;

	// Buffer of vertices.
	std::vector<char> buffer_;

	// Number of vertices.
	size_t size_;

	// Immediate mode shaders;
	std::unique_ptr<ImmediatePCShader> immediate_pc_shader;
	std::unique_ptr<ImmediatePTCShader> immediate_ptc_shader;

	// The renderer object.
	Renderer* renderer_;
};

// RAII object that allows to push vertices to the imrender stack.
template <typename _Ty>
class GlImmediate 
{
public:
	// Immediate object vertex format.
	typedef _Ty Vertex;

	// Start a new immediate stack.
	GlImmediate(GlImmediateRenderer* _renderer, GLenum _mode, const Math::Mat4& _transform)
		: transform_(_transform), renderer_(_renderer), mode_(_mode) 
	{
		renderer_->Begin();
	}

	// End immediate vertex stacking, and renders all vertices.
	~GlImmediate() { renderer_->End<_Ty>(mode_, transform_); }

	// Pushes a new vertex to the stack.
	FORCEINLINE void PushVertex(const _Ty& _vertex) 
	{
		renderer_->PushVertex(_vertex);
	}

private:
	// Non copyable.
	GlImmediate(const GlImmediate&);
	void operator=(const GlImmediate&);

	// Transformation matrix.
	const Math::Mat4 transform_;

	// Shared renderer.
	GlImmediateRenderer* renderer_;

	// Draw array mode GL_POINTS, GL_LINE_STRIP, ...
	GLenum mode_;
};