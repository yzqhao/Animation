#pragma once

#include "../../Math/3DMath.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GL_PTR_OFFSET(i) reinterpret_cast<void*>(static_cast<intptr_t>(i))

class Shader 
{
public:

    Shader();
    virtual ~Shader();

    // Returns the shader program that can be bound to the OpenGL context.
    GLuint program() const { return program_; }

    // Request an uniform location and pushes it to the uniform stack.
    // The uniform location is then accessible thought uniform().
    bool BindUniform(const char* _semantic);

    // Get an uniform location from the stack at index _index.
    GLint uniform(int _index) const { return uniforms_[_index]; }

    // Request an attribute location and pushes it to the uniform stack.
    // The varying location is then accessible thought attrib().
    bool FindAttrib(const char* _semantic);

    // Get an varying location from the stack at index _index.
    GLint attrib(int _index) const { return attribs_[_index]; }

    // Unblind shader.
    virtual void Unbind();

    protected:
    // Constructs a shader from _vertex and _fragment glsl sources.
    // Mutliple source files can be specified using the *count argument.
    bool BuildFromSource(int _vertex_count, const char** _vertex,
                        int _fragment_count, const char** _fragment);

    private:
    // Unbind all attribs from GL.
    void UnbindAttribs();

    // Shader program
    GLuint program_;

    // Vertex and fragment shaders
    GLuint vertex_;
    GLuint fragment_;

    // Uniform locations, in the order they were requested.
    std::vector<GLint> uniforms_;

    // Varying locations, in the order they were requested.
    std::vector<GLint> attribs_;
};

class ImmediatePCShader : public Shader 
{
public:
	ImmediatePCShader() {}
	virtual ~ImmediatePCShader() {}

	static std::unique_ptr<ImmediatePCShader> Build();

	// Binds the shader.
	void Bind(const Math::Mat4& _model, const Math::Mat4& _view_proj,
		GLsizei _pos_stride, GLsizei _pos_offset, GLsizei _color_stride,
		GLsizei _color_offset);
};

class ImmediatePTCShader : public Shader 
{
public:
	ImmediatePTCShader() {}
	virtual ~ImmediatePTCShader() {}

	static std::unique_ptr<ImmediatePTCShader> Build();

	// Binds the shader.
	void Bind(const Math::Mat4& _model, const Math::Mat4& _view_proj,
		GLsizei _pos_stride, GLsizei _pos_offset, GLsizei _tex_stride,
		GLsizei _tex_offset, GLsizei _color_stride, GLsizei _color_offset);
};

class PointsShader : public Shader 
{
public:
	PointsShader() {}
	virtual ~PointsShader() {}

	// Constructs the shader.
	// Returns nullptr if shader compilation failed or a valid Shader pointer on
	// success. The shader must then be deleted using default allocator Delete
	// function.
	static std::unique_ptr<PointsShader> Build();

	// Binds the shader.
	struct GenericAttrib {
		GLint color;
		GLint size;
	};
	GenericAttrib Bind(const Math::Mat4& _model,
		const Math::Mat4& _view_proj, GLsizei _pos_stride,
		GLsizei _pos_offset, GLsizei _color_stride,
		GLsizei _color_offset, GLsizei _size_stride,
		GLsizei _size_offset, bool _screen_space);
};

class SkeletonShader : public Shader 
{
public:
	SkeletonShader() {}
	virtual ~SkeletonShader() {}

	// Binds the shader.
	void Bind(const Math::Mat4& _model, const Math::Mat4& _view_proj,
		GLsizei _pos_stride, GLsizei _pos_offset, GLsizei _normal_stride,
		GLsizei _normal_offset, GLsizei _color_stride,
		GLsizei _color_offset);

	// Get an attribute location for the join, in cased of instanced rendering.
	GLint joint_instanced_attrib() const { return attrib(3); }

	// Get an uniform location for the join, in cased of non-instanced rendering.
	GLint joint_uniform() const { return uniform(1); }
};


class JointShader : public SkeletonShader 
{
public:
    JointShader() {}
    virtual ~JointShader() {}

    // Constructs the shader.
    // Returns nullptr if shader compilation failed or a valid Shader pointer on
    // success. The shader must then be deleted using default allocator Delete
    // function.
    static std::unique_ptr<JointShader> Build();
};

class BoneShader : public SkeletonShader 
{
public:
    BoneShader() {}
    virtual ~BoneShader() {}

    // Constructs the shader.
    // Returns nullptr if shader compilation failed or a valid Shader pointer on
    // success. The shader must then be deleted using default allocator Delete
    // function.
    static std::unique_ptr<BoneShader> Build();
};

class AmbientShader : public Shader 
{
public:
	AmbientShader() {}
	virtual ~AmbientShader() {}

	// Constructs the shader.
	// Returns nullptr if shader compilation failed or a valid Shader pointer on
	// success. The shader must then be deleted using default allocator Delete
	// function.
	static std::unique_ptr<AmbientShader> Build();

	// Binds the shader.
	void Bind(const Math::Mat4& _model, const Math::Mat4& _view_proj,
		GLsizei _pos_stride, GLsizei _pos_offset, GLsizei _normal_stride,
		GLsizei _normal_offset, GLsizei _color_stride,
		GLsizei _color_offset);

protected:
	bool InternalBuild(int _vertex_count, const char** _vertex,
		int _fragment_count, const char** _fragment);
};

class AmbientTexturedShader : public AmbientShader 
{
public:
	// Constructs the shader.
	// Returns nullptr if shader compilation failed or a valid Shader pointer on
	// success. The shader must then be deleted using default allocator Delete
	// function.
	static std::unique_ptr<AmbientTexturedShader> Build();

	// Binds the shader.
	void Bind(const Math::Mat4& _model, const Math::Mat4& _view_proj,
		GLsizei _pos_stride, GLsizei _pos_offset, GLsizei _normal_stride,
		GLsizei _normal_offset, GLsizei _color_stride,
		GLsizei _color_offset, GLsizei _uv_stride, GLsizei _uv_offset);
};

