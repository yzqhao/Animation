#include "Immediate.h"
#include "renderer.h"
#include "Shader.h"
#include "Camera.h"


GlImmediateRenderer::GlImmediateRenderer(Renderer* _renderer)
	: vbo_(0), size_(0), renderer_(_renderer) {}

GlImmediateRenderer::~GlImmediateRenderer() 
{
	assert(size_ == 0 && "Immediate rendering still in use.");

	if (vbo_) 
	{
		glDeleteBuffers(1, &vbo_);
		vbo_ = 0;
	}
}

bool GlImmediateRenderer::Initialize()
{
	glGenBuffers(1, &vbo_);

	immediate_pc_shader = ImmediatePCShader::Build();
	if (!immediate_pc_shader) 
	{
		return false;
	}
	immediate_ptc_shader = ImmediatePTCShader::Build();
	if (!immediate_ptc_shader) 
	{
		return false;
	}

	return true;
}

void GlImmediateRenderer::Begin() 
{
	assert(size_ == 0 && "Immediate rendering already in use.");
}

template <>
void GlImmediateRenderer::End<VertexPC>(GLenum _mode, const Math::Mat4& _transform) 
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, size_, buffer_.data(), GL_STREAM_DRAW);

	immediate_pc_shader->Bind(_transform, renderer_->GetCamera()->view_proj(),
		sizeof(VertexPC), 0, sizeof(VertexPC), 12);

	const int count = static_cast<int>(size_ / sizeof(VertexPC));
	glDrawArrays(_mode, 0, count);

	immediate_pc_shader->Unbind();

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Reset vertex count for the next call
	size_ = 0;
}

template <>
void GlImmediateRenderer::End<VertexPTC>(GLenum _mode, const Math::Mat4& _transform) 
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, size_, buffer_.data(), GL_STREAM_DRAW);

	immediate_ptc_shader->Bind(_transform, renderer_->GetCamera()->view_proj(),
		sizeof(VertexPTC), 0, sizeof(VertexPTC), 12,
		sizeof(VertexPTC), 20);

	const int count = static_cast<int>(size_ / sizeof(VertexPTC));
	glDrawArrays(_mode, 0, count);

	immediate_ptc_shader->Unbind();

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Reset vertex count for the next call
	size_ = 0;
}