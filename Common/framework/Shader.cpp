#include "Shader.h"


Shader::Shader() : program_(0), vertex_(0), fragment_(0) {}

Shader::~Shader() 
{
	if (vertex_) 
	{
		glDetachShader(program_, vertex_);
		glDeleteShader(vertex_);
	}
	if (fragment_) {
		glDetachShader(program_, fragment_);
		glDeleteShader(fragment_);
	}
	if (program_) {
		glDeleteProgram(program_);
	}
}

namespace 
{
	GLuint CompileShader(GLenum _type, int _count, const char** _src) 
	{
		GLuint shader = glCreateShader(_type);
		glShaderSource(shader, _count, _src, nullptr);
		glCompileShader(shader);

		int infolog_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infolog_length);
		if (infolog_length > 1) 
		{
			char* info_log = reinterpret_cast<char*>(malloc(infolog_length));
			int chars_written = 0;
			glGetShaderInfoLog(shader, infolog_length, &chars_written, info_log);
			std::cout << info_log << std::endl;
			free(info_log);
		}

		int status;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (status) {
			return shader;
		}

		glDeleteShader(shader);
		return 0;
	}
}  // namespace

bool Shader::BuildFromSource(int _vertex_count, const char** _vertex, int _fragment_count, const char** _fragment) 
{
	// Tries to compile shaders.
	GLuint vertex_shader = 0;
	if (_vertex) 
	{
		vertex_shader = CompileShader(GL_VERTEX_SHADER, _vertex_count, _vertex);
		if (!vertex_shader)
		{
			return false;
		}
	}
	GLuint fragment_shader = 0;
	if (_fragment) 
	{
		fragment_shader = CompileShader(GL_FRAGMENT_SHADER, _fragment_count, _fragment);
		if (!fragment_shader) 
		{
			if (vertex_shader) 
			{
				glDeleteShader(vertex_shader);
			}
			return false;
		}
	}

	// Shaders are compiled, builds program.
	program_ = glCreateProgram();
	vertex_ = vertex_shader;
	fragment_ = fragment_shader;
	glAttachShader(program_, vertex_shader);
	glAttachShader(program_, fragment_shader);
	glLinkProgram(program_);

	int infolog_length = 0;
	glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &infolog_length);
	if (infolog_length > 1) 
	{
		char* info_log = reinterpret_cast<char*>(malloc(infolog_length));
		int chars_written = 0;
		glGetProgramInfoLog(program_, infolog_length, &chars_written, info_log);
		std::cout << info_log << std::endl;
		free(info_log);
	}

	return true;
}

bool Shader::BindUniform(const char* _semantic) 
{
	if (!program_) 
	{
		return false;
	}
	GLint location = glGetUniformLocation(program_, _semantic);
	if (glGetError() != GL_NO_ERROR || location == -1) 
	{  // _semantic not found.
		return false;
	}
	uniforms_.push_back(location);
	return true;
}

bool Shader::FindAttrib(const char* _semantic) 
{
	if (!program_) 
	{
		return false;
	}
	GLint location = glGetAttribLocation(program_, _semantic);
	if (glGetError() != GL_NO_ERROR || location == -1) 
	{  // _semantic not found.
		return false;
	}
	attribs_.push_back(location);
	return true;
}

void Shader::UnbindAttribs() 
{
	for (size_t i = 0; i < attribs_.size(); ++i) 
	{
		glDisableVertexAttribArray(attribs_[i]);
	}
}

void Shader::Unbind() 
{
	UnbindAttribs();
	glUseProgram(0);
}


namespace 
{
	const char* kPassUv =
		"attribute vec2 a_uv;\n"
		"varying vec2 v_vertex_uv;\n"
		"void PassUv() {\n"
		"  v_vertex_uv = a_uv;\n"
		"}\n";
	const char* kPassNoUv =
		"void PassUv() {\n"
		"}\n";
	const char* kShaderUberVS =
		"uniform mat4 u_mvp;\n"
		"attribute vec3 a_position;\n"
		"attribute vec3 a_normal;\n"
		"attribute vec4 a_color;\n"
		"varying vec3 v_world_normal;\n"
		"varying vec4 v_vertex_color;\n"
		"void main() {\n"
		"  mat4 world_matrix = GetWorldMatrix();\n"
		"  vec4 vertex = vec4(a_position.xyz, 1.);\n"
		"  gl_Position = u_mvp * world_matrix * vertex;\n"
		"  mat3 cross_matrix = mat3(\n"
		"    cross(world_matrix[1].xyz, world_matrix[2].xyz),\n"
		"    cross(world_matrix[2].xyz, world_matrix[0].xyz),\n"
		"    cross(world_matrix[0].xyz, world_matrix[1].xyz));\n"
		"  float invdet = 1.0 / dot(cross_matrix[2], world_matrix[2].xyz);\n"
		"  mat3 normal_matrix = cross_matrix * invdet;\n"
		"  v_world_normal = normal_matrix * a_normal;\n"
		"  v_vertex_color = a_color;\n"
		"  PassUv();\n"
		"}\n";
	const char* kShaderAmbientFct =
		"vec4 GetAmbient(vec3 _world_normal) {\n"
		"  vec3 normal = normalize(_world_normal);\n"
		"  vec3 alpha = (normal + 1.) * .5;\n"
		"  vec2 bt = mix(vec2(.3, .7), vec2(.4, .8), alpha.xz);\n"
		"  vec3 ambient = mix(vec3(bt.x, .3, bt.x), vec3(bt.y, .8, bt.y), "
		"alpha.y);\n"
		"  return vec4(ambient, 1.);\n"
		"}\n";
	const char* kShaderAmbientFS =
		"varying vec3 v_world_normal;\n"
		"varying vec4 v_vertex_color;\n"
		"void main() {\n"
		"  vec4 ambient = GetAmbient(v_world_normal);\n"
		"  gl_FragColor = ambient *\n"
		"                 v_vertex_color;\n"
		"}\n";
	const char* kShaderAmbientTexturedFS =
		"uniform sampler2D u_texture;\n"
		"varying vec3 v_world_normal;\n"
		"varying vec4 v_vertex_color;\n"
		"varying vec2 v_vertex_uv;\n"
		"void main() {\n"
		"  vec4 ambient = GetAmbient(v_world_normal);\n"
		"  gl_FragColor = ambient *\n"
		"                 v_vertex_color *\n"
		"                 texture2D(u_texture, v_vertex_uv);\n"
		"}\n";
}  // namespace

std::unique_ptr<ImmediatePCShader> ImmediatePCShader::Build() 
{
	bool success = true;

	const char* kSimplePCVS =
		"uniform mat4 u_mvp;\n"
		"attribute vec3 a_position;\n"
		"attribute vec4 a_color;\n"
		"varying vec4 v_vertex_color;\n"
		"void main() {\n"
		"  vec4 vertex = vec4(a_position.xyz, 1.);\n"
		"  gl_Position = u_mvp * vertex;\n"
		"  v_vertex_color = a_color;\n"
		"}\n";
	const char* kSimplePCPS =
		"varying vec4 v_vertex_color;\n"
		"void main() {\n"
		"  gl_FragColor = v_vertex_color;\n"
		"}\n";

	const char* vs[] = { kSimplePCVS };
	const char* fs[] = { kSimplePCPS };

	std::unique_ptr<ImmediatePCShader> shader = std::make_unique<ImmediatePCShader>();
	success &=
		shader->BuildFromSource(JY_ARRAY_COUNT(vs), vs, JY_ARRAY_COUNT(fs), fs);

	// Binds default attributes
	success &= shader->FindAttrib("a_position");
	success &= shader->FindAttrib("a_color");

	// Binds default uniforms
	success &= shader->BindUniform("u_mvp");

	if (!success) 
	{
		shader.reset();
	}

	return shader;
}

void ImmediatePCShader::Bind(const Math::Mat4& _model,
	const Math::Mat4& _view_proj,
	GLsizei _pos_stride, GLsizei _pos_offset,
	GLsizei _color_stride, GLsizei _color_offset) 
{
	glUseProgram(program());

	const GLint position_attrib = attrib(0);
	glEnableVertexAttribArray(position_attrib);
	glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride,
		GL_PTR_OFFSET(_pos_offset));

	const GLint color_attrib = attrib(1);
	glEnableVertexAttribArray(color_attrib);
	glVertexAttribPointer(color_attrib, 4, GL_UNSIGNED_BYTE, GL_TRUE,
		_color_stride, GL_PTR_OFFSET(_color_offset));

	// Binds mvp uniform
	const GLint mvp_uniform = uniform(0);
	const Math::Mat4 mvp = _model * _view_proj;
	glUniformMatrix4fv(mvp_uniform, 1, false, mvp.GetPtr());
}

std::unique_ptr<ImmediatePTCShader> ImmediatePTCShader::Build() 
{
	bool success = true;

	const char* kSimplePCVS =
		"uniform mat4 u_mvp;\n"
		"attribute vec3 a_position;\n"
		"attribute vec2 a_tex_coord;\n"
		"attribute vec4 a_color;\n"
		"varying vec4 v_vertex_color;\n"
		"varying vec2 v_texture_coord;\n"
		"void main() {\n"
		"  vec4 vertex = vec4(a_position.xyz, 1.);\n"
		"  gl_Position = u_mvp * vertex;\n"
		"  v_vertex_color = a_color;\n"
		"  v_texture_coord = a_tex_coord;\n"
		"}\n";
	const char* kSimplePCPS =
		"uniform sampler2D u_texture;\n"
		"varying vec4 v_vertex_color;\n"
		"varying vec2 v_texture_coord;\n"
		"void main() {\n"
		"  vec4 tex_color = texture2D(u_texture, v_texture_coord);\n"
		"  gl_FragColor = v_vertex_color * tex_color;\n"
		"  if(gl_FragColor.a < .01) discard;\n"  // Implements alpha testing.
		"}\n";

	const char* vs[] = { kSimplePCVS };
	const char* fs[] = { kSimplePCPS };

	std::unique_ptr<ImmediatePTCShader> shader = std::make_unique<ImmediatePTCShader>();
	success &=
		shader->BuildFromSource(JY_ARRAY_COUNT(vs), vs, JY_ARRAY_COUNT(fs), fs);

	// Binds default attributes
	success &= shader->FindAttrib("a_position");
	success &= shader->FindAttrib("a_tex_coord");
	success &= shader->FindAttrib("a_color");

	// Binds default uniforms
	success &= shader->BindUniform("u_mvp");
	success &= shader->BindUniform("u_texture");

	if (!success) 
	{
		shader.reset();
	}

	return shader;
}

void ImmediatePTCShader::Bind(const Math::Mat4& _model,
	const Math::Mat4& _view_proj,
	GLsizei _pos_stride, GLsizei _pos_offset,
	GLsizei _tex_stride, GLsizei _tex_offset,
	GLsizei _color_stride, GLsizei _color_offset) 
{
	glUseProgram(program());

	const GLint position_attrib = attrib(0);
	glEnableVertexAttribArray(position_attrib);
	glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride,
		GL_PTR_OFFSET(_pos_offset));

	const GLint tex_attrib = attrib(1);
	glEnableVertexAttribArray(tex_attrib);
	glVertexAttribPointer(tex_attrib, 2, GL_FLOAT, GL_FALSE, _tex_stride, GL_PTR_OFFSET(_tex_offset));

	const GLint color_attrib = attrib(2);
	glEnableVertexAttribArray(color_attrib);
	glVertexAttribPointer(color_attrib, 4, GL_UNSIGNED_BYTE, GL_TRUE,
		_color_stride, GL_PTR_OFFSET(_color_offset));

	// Binds mvp uniform
	const GLint mvp_uniform = uniform(0);
	const Math::Mat4 mvp = _model * _view_proj;
	glUniformMatrix4fv(mvp_uniform, 1, false, mvp.GetPtr());

	// Binds texture
	const GLint texture = uniform(1);
	glUniform1i(texture, 0);
}

std::unique_ptr<PointsShader> PointsShader::Build() 
{
	bool success = true;

	const char* kSimplePointsVS =
		"uniform mat4 u_mvp;\n"
		"attribute vec3 a_position;\n"
		"attribute vec4 a_color;\n"
		"attribute float a_size;\n"
		"attribute float a_screen_space;\n"
		"varying vec4 v_vertex_color;\n"
		"void main() {\n"
		"  vec4 vertex = vec4(a_position.xyz, 1.);\n"
		"  gl_Position = u_mvp * vertex;\n"
		"  gl_PointSize = a_screen_space == 0. ? a_size / gl_Position.w : a_size;\n"
		"  v_vertex_color = a_color;\n"
		"}\n";
	const char* kSimplePointsPS =
		"varying vec4 v_vertex_color;\n"
		"void main() {\n"
		"  gl_FragColor = v_vertex_color;\n"
		"}\n";

	const char* vs[] = { kSimplePointsVS };
	const char* fs[] = { kSimplePointsPS };

	std::unique_ptr<PointsShader> shader = std::make_unique<PointsShader>();
	success &= shader->BuildFromSource(JY_ARRAY_COUNT(vs), vs, JY_ARRAY_COUNT(fs), fs);

	// Binds default attributes
	success &= shader->FindAttrib("a_position");
	success &= shader->FindAttrib("a_color");
	success &= shader->FindAttrib("a_size");
	success &= shader->FindAttrib("a_screen_space");

	// Binds default uniforms
	success &= shader->BindUniform("u_mvp");

	if (!success) {
		shader.reset();
	}

	return shader;
}

PointsShader::GenericAttrib PointsShader::Bind(
	const Math::Mat4& _model, const Math::Mat4& _view_proj,
	GLsizei _pos_stride, GLsizei _pos_offset, GLsizei _color_stride,
	GLsizei _color_offset, GLsizei _size_stride, GLsizei _size_offset,
	bool _screen_space) {
	glUseProgram(program());

	const GLint position_attrib = attrib(0);
	glEnableVertexAttribArray(position_attrib);
	glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride,
		GL_PTR_OFFSET(_pos_offset));

	const GLint color_attrib = attrib(1);
	if (_color_stride) {
		glEnableVertexAttribArray(color_attrib);
		glVertexAttribPointer(color_attrib, 4, GL_UNSIGNED_BYTE, GL_TRUE,
			_color_stride, GL_PTR_OFFSET(_color_offset));
	}
	const GLint size_attrib = attrib(2);
	if (_size_stride) {
		glEnableVertexAttribArray(size_attrib);
		glVertexAttribPointer(size_attrib, 1, GL_FLOAT, GL_TRUE, _size_stride,
			GL_PTR_OFFSET(_size_offset));
	}
	const GLint screen_space_attrib = attrib(3);
	glVertexAttrib1f(screen_space_attrib, 1.f * _screen_space);

	// Binds mvp uniform
	const GLint mvp_uniform = uniform(0);
	const Math::Mat4 mvp = _view_proj * _model;
	glUniformMatrix4fv(mvp_uniform, 1, false, mvp.GetPtr());

	return { _color_stride ? -1 : color_attrib, _size_stride ? -1 : size_attrib };
}

void SkeletonShader::Bind(const Math::Mat4& _model,
	const Math::Mat4& _view_proj, GLsizei _pos_stride,
	GLsizei _pos_offset, GLsizei _normal_stride,
	GLsizei _normal_offset, GLsizei _color_stride,
	GLsizei _color_offset) 
{
	glUseProgram(program());

	const GLint position_attrib = attrib(0);
	glEnableVertexAttribArray(position_attrib);
	glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride, (void*)0);

	const GLint normal_attrib = attrib(1);
	glEnableVertexAttribArray(normal_attrib);
	glVertexAttribPointer(normal_attrib, 3, GL_FLOAT, GL_FALSE, _normal_stride, GL_PTR_OFFSET(_normal_offset));

	const GLint color_attrib = attrib(2);
	glEnableVertexAttribArray(color_attrib);
	glVertexAttribPointer(color_attrib, 4, GL_UNSIGNED_BYTE, GL_TRUE, _color_stride, GL_PTR_OFFSET(_color_offset));

	// Binds mvp uniform
	const GLint mvp_uniform = uniform(0);
	const Math::Mat4 mvp = _model * _view_proj;
	glUniformMatrix4fv(mvp_uniform, 1, false, mvp.GetPtr());
}

std::unique_ptr<JointShader> JointShader::Build()
{
	bool success = true;

	const char* vs_joint_to_world_matrix =
		"mat4 GetWorldMatrix() {\n"
		"  // Rebuilds joint matrix.\n"
		"  mat4 joint_matrix;\n"
		"  joint_matrix[0] = vec4(normalize(joint[0].xyz), 0.);\n"
		"  joint_matrix[1] = vec4(normalize(joint[1].xyz), 0.);\n"
		"  joint_matrix[2] = vec4(normalize(joint[2].xyz), 0.);\n"
		"  joint_matrix[3] = vec4(joint[3].xyz, 1.);\n"

		"  // Rebuilds bone properties.\n"
		"  vec3 bone_dir = vec3(joint[0].w, joint[1].w, joint[2].w);\n"
		"  float bone_len = length(bone_dir);\n"

		"  // Setup rendering world matrix.\n"
		"  mat4 world_matrix;\n"
		"  world_matrix[0] = joint_matrix[0] * bone_len;\n"
		"  world_matrix[1] = joint_matrix[1] * bone_len;\n"
		"  world_matrix[2] = joint_matrix[2] * bone_len;\n"
		"  world_matrix[3] = joint_matrix[3];\n"
		"  return world_matrix;\n"
		"}\n";
	const char* vs[] = { kPassNoUv, "uniform mat4 joint;\n",
						vs_joint_to_world_matrix, kShaderUberVS };
	const char* fs[] = { kShaderAmbientFct,
						kShaderAmbientFS };

	std::unique_ptr<JointShader> shader = std::make_unique<JointShader>();
	success &=
		shader->BuildFromSource(JY_ARRAY_COUNT(vs), vs, JY_ARRAY_COUNT(fs), fs);

	// Binds default attributes
	success &= shader->FindAttrib("a_position");
	success &= shader->FindAttrib("a_normal");
	success &= shader->FindAttrib("a_color");

	// Binds default uniforms
	success &= shader->BindUniform("u_mvp");

	success &= shader->BindUniform("joint");

	if (!success) {
		shader.reset();
	}

	return shader;
}

std::unique_ptr<BoneShader> BoneShader::Build() 
{  // Builds a world matrix from joint uniforms, 
   // sticking bone model between
	bool success = true;

	// parent and child joints.
	const char* vs_joint_to_world_matrix =
		"mat4 GetWorldMatrix() {\n"
		"  // Rebuilds bone properties.\n"
		"  // Bone length is set to zero to disable leaf rendering.\n"
		"  float is_bone = joint[3].w;\n"
		"  vec3 bone_dir = vec3(joint[0].w, joint[1].w, joint[2].w) * is_bone;\n"
		"  float bone_len = length(bone_dir);\n"

		"  // Setup rendering world matrix.\n"
		"  float dot1 = dot(joint[2].xyz, bone_dir);\n"
		"  float dot2 = dot(joint[0].xyz, bone_dir);\n"
		"  vec3 binormal = abs(dot1) < abs(dot2) ? joint[2].xyz : joint[0].xyz;\n"

		"  mat4 world_matrix;\n"
		"  world_matrix[0] = vec4(bone_dir, 0.);\n"
		"  world_matrix[1] = \n"
		"    vec4(bone_len * normalize(cross(binormal, bone_dir)), 0.);\n"
		"  world_matrix[2] =\n"
		"    vec4(bone_len * normalize(cross(bone_dir, world_matrix[1].xyz)), "
		"0.);\n"
		"  world_matrix[3] = vec4(joint[3].xyz, 1.);\n"
		"  return world_matrix;\n"
		"}\n";
	const char* vs[] = { kPassNoUv, "uniform mat4 joint;\n",
						vs_joint_to_world_matrix, kShaderUberVS };
	const char* fs[] = { kShaderAmbientFct,
						kShaderAmbientFS };

	std::unique_ptr<BoneShader> shader = std::make_unique<BoneShader>();
	success &=
		shader->BuildFromSource(JY_ARRAY_COUNT(vs), vs, JY_ARRAY_COUNT(fs), fs);

	// Binds default attributes
	success &= shader->FindAttrib("a_position");
	success &= shader->FindAttrib("a_normal");
	success &= shader->FindAttrib("a_color");

	// Binds default uniforms
	success &= shader->BindUniform("u_mvp");

	success &= shader->BindUniform("joint");

	if (!success) {
		shader.reset();
	}

	return shader;
}


std::unique_ptr<AmbientShader> AmbientShader::Build() 
{
	const char* vs[] = {
		kPassNoUv,
		"uniform mat4 u_mw;\n mat4 GetWorldMatrix() {return u_mw;}\n",
		kShaderUberVS };
	const char* fs[] = { kShaderAmbientFct,
						kShaderAmbientFS };

	std::unique_ptr<AmbientShader> shader = std::make_unique<AmbientShader>();
	bool success =
		shader->InternalBuild(JY_ARRAY_COUNT(vs), vs, JY_ARRAY_COUNT(fs), fs);

	if (!success) {
		shader.reset();
	}

	return shader;
}

bool AmbientShader::InternalBuild(int _vertex_count, const char** _vertex,
	int _fragment_count, const char** _fragment) 
{
	bool success = true;

	success &=
		BuildFromSource(_vertex_count, _vertex, _fragment_count, _fragment);

	// Binds default attributes
	success &= FindAttrib("a_position");
	success &= FindAttrib("a_normal");
	success &= FindAttrib("a_color");

	// Binds default uniforms
	success &= BindUniform("u_mw");
	success &= BindUniform("u_mvp");

	return success;
}

void AmbientShader::Bind(const Math::Mat4& _model,
	const Math::Mat4& _view_proj, GLsizei _pos_stride,
	GLsizei _pos_offset, GLsizei _normal_stride,
	GLsizei _normal_offset, GLsizei _color_stride,
	GLsizei _color_offset)
{
	glUseProgram(program());

	const GLint position_attrib = attrib(0);
	glEnableVertexAttribArray(position_attrib);
	glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride,
		GL_PTR_OFFSET(_pos_offset));

	const GLint normal_attrib = attrib(1);
	glEnableVertexAttribArray(normal_attrib);
	glVertexAttribPointer(normal_attrib, 3, GL_FLOAT, GL_TRUE, _normal_stride,
		GL_PTR_OFFSET(_normal_offset));

	const GLint color_attrib = attrib(2);
	glEnableVertexAttribArray(color_attrib);
	glVertexAttribPointer(color_attrib, 4, GL_UNSIGNED_BYTE, GL_TRUE,
		_color_stride, GL_PTR_OFFSET(_color_offset));

	// Binds mw uniform
	const GLint mw_uniform = uniform(0);
	glUniformMatrix4fv(mw_uniform, 1, false, _model.GetPtr());

	// Binds mvp uniform
	const GLint mvp_uniform = uniform(1);
	glUniformMatrix4fv(mvp_uniform, 1, false, _view_proj.GetPtr());
}

std::unique_ptr<AmbientTexturedShader> AmbientTexturedShader::Build() 
{
	const char* vs[] = {
		kPassUv,
		"uniform mat4 u_mw;\n mat4 GetWorldMatrix() {return u_mw;}\n",
		kShaderUberVS };
	const char* fs[] = { kShaderAmbientFct,
						kShaderAmbientTexturedFS };

	std::unique_ptr<AmbientTexturedShader> shader = std::make_unique<AmbientTexturedShader>();
	bool success =
		shader->InternalBuild(JY_ARRAY_COUNT(vs), vs, JY_ARRAY_COUNT(fs), fs);

	success &= shader->FindAttrib("a_uv");

	if (!success) {
		shader.reset();
	}

	return shader;
}

void AmbientTexturedShader::Bind(const Math::Mat4& _model,
	const Math::Mat4& _view_proj,
	GLsizei _pos_stride, GLsizei _pos_offset,
	GLsizei _normal_stride, GLsizei _normal_offset,
	GLsizei _color_stride, GLsizei _color_offset,
	GLsizei _uv_stride, GLsizei _uv_offset) 
{
	AmbientShader::Bind(_model, _view_proj, _pos_stride, _pos_offset,
		_normal_stride, _normal_offset, _color_stride,
		_color_offset);

	const GLint uv_attrib = attrib(3);
	glEnableVertexAttribArray(uv_attrib);
	glVertexAttribPointer(uv_attrib, 2, GL_FLOAT, GL_FALSE, _uv_stride,
		GL_PTR_OFFSET(_uv_offset));
}