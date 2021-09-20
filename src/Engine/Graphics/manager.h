#ifndef ENGINE_GRAPHICS_MANAGER_H
#define ENGINE_GRAPHICS_MANAGER_H

#include <unordered_map>
#include <GL/glew.h>
#include <Engine/Math/Transform.h>
#include <filesystem>

namespace Engine {
namespace Graphics {

	void GLClearError();
	bool GLLogCall(const char* _fn, const char* _file, int _line);

	class ResourceManager
	{

		//////////////////////////////////////////////////////
		//			Defined Handle Types
		//////////////////////////////////////////////////////

	public:

		typedef unsigned int	buffer_handle;	// Equivalent to buffer_view handle in glTF.
		typedef unsigned int	mesh_handle;
		typedef unsigned int	material_handle;
		typedef unsigned int	texture_handle;
		typedef unsigned int	framebuffer_handle;

		//////////////////////////////////////////////////////
		//			OpenGL Graphics Assets Data
		//////////////////////////////////////////////////////

	public:


		// Mesh data
		struct mesh_primitive_data
		{
			GLuint			m_vao_gl_id;			// VAO referring to IBO and VBOs
			buffer_handle	m_index_buffer_handle;	// Handle pointing to IBO (component type of IBO stored in index_buffer_component_map)
			material_handle m_material_handle;
			unsigned char	m_render_mode = GL_TRIANGLES; // Default according to specification
		};

		// Defines information for both vertex buffers and index buffers
		struct buffer_info
		{
			GLuint			m_gl_id;
			GLint			m_target;
		};

		struct index_buffer_info
		{
			GLuint			m_type;
			size_t			m_index_count;
		};

		typedef std::vector<mesh_primitive_data> mesh_primitive_list;

		// Handle counters
		mesh_handle			m_mesh_handle_counter = 1;
		buffer_handle		m_buffer_handle_counter = 1;

		std::unordered_map<std::string, mesh_handle>			m_named_mesh_map;
		std::unordered_map<mesh_handle, mesh_primitive_list>	m_mesh_primitives_map;
		std::unordered_map<buffer_handle, buffer_info>			m_buffer_info_map;
		std::unordered_map<buffer_handle, index_buffer_info>	m_index_buffer_info_map;


		//////////////////////////////////////////////////////
		//					Material Data
		//////////////////////////////////////////////////////

	public:

		struct pbr_metallic_roughness_data
		{
			// Defaults are according to specification of glTF
			double					m_base_color_factor[4]{ 1.0,1.0,1.0,1.0 };
			double					m_metallic_factor = 1.0;	// [0,1]
			double					m_roughness_factor = 1.0;	// [0,1]
			texture_handle			m_texture_base_color;
			texture_handle			m_texture_metallic_roughness;
		};

		struct material_data
		{
			enum class alpha_mode : unsigned char { eOPAQUE, eMASK, eBLEND };

			pbr_metallic_roughness_data m_pbr_metallic_roughness;

			double						m_emissive_factor[3]{ 0.0f, 0.0f, 0.0f };

			// Handles = 0 if they do not exist.
			texture_handle	m_texture_normal;
			texture_handle	m_texture_occlusion;
			texture_handle	m_texture_emissive;

			/* if alpha_mode	== eOPAQUE	--> Alpha value is ignored
								== MASK		--> Alpha value < cutoff value: Transparent
															> cutoff value: Opaque
												Used to simulate tree leaves or wire fences.
								== BLEND	--> Combined with background using normal painting operation (Porter and Duff over operator)
			*/
			double			m_alpha_cutoff;
			alpha_mode		m_alpha_mode : 7;
			bool			m_double_sided;

			std::string		m_name;
		};

		material_handle		m_material_handle_counter = 1;
		std::unordered_map<material_handle, material_data>	m_material_data_map;

		//////////////////////////////////////////////////////
		//					Texture Data
		//////////////////////////////////////////////////////

	public:

		struct texture_info
		{
			GLuint	m_gl_source_id;
			GLuint	m_target = GL_INVALID_ENUM;
			glm::uvec3 m_size = { 0,0,0 };
			//TODO: Texture ref count?
		};

		texture_handle			m_texture_handle_counter = 1;

		std::unordered_map<texture_handle, texture_info>	m_texture_info_map;

		//////////////////////////////////////////////////////
		//				Framebuffer Data
		//////////////////////////////////////////////////////

	public:

		struct framebuffer_info
		{
			GLuint	m_gl_object;
			GLenum	m_target = GL_INVALID_ENUM;
		};

		framebuffer_handle		m_framebuffer_handle_counter = 1;

		std::unordered_map<framebuffer_handle, framebuffer_info>			m_framebuffer_info_map;

		//////////////////////////////////////////////////////
		//				OpenGL Shader Data
		//////////////////////////////////////////////////////

	public:

		typedef char shader_data;
		typedef unsigned int shader_handle;
		typedef unsigned int shader_program_handle;
		typedef std::string	 filepath_string;

		struct shader_info
		{
			GLuint m_gl_shader_object;
			// Last time shader file was modified / written to. Used for refreshing shaders.
			std::filesystem::file_time_type m_last_time_written; 
			// In the event that a shader file is modified, the shader can be
			// re-compiled, and all programs using this shader can be re-linked.
			std::vector<shader_program_handle> m_programs_using_shader;
		};

		struct shader_program_info
		{
			GLuint						m_gl_program_object;
			std::vector<shader_handle>	m_linked_shader_handles;
		};

	public:

		// Handle counters
		shader_handle			m_shader_handle_counter = 1;
		shader_program_handle	m_shader_program_handle_counter = 1;

		std::unordered_map<filepath_string, shader_handle>				m_filepath_shader_map;
		std::unordered_map<shader_handle, shader_info>					m_shader_info_map;
		std::unordered_map<filepath_string, shader_program_handle>		m_named_shader_program_map;
		std::unordered_map<shader_program_handle, shader_program_info>	m_shader_program_info_map;

		shader_program_handle	m_bound_program = 0;
		GLuint					m_bound_gl_program_object = 0;

		//////////////////////////////////////////////////////
		//						Methods
		//////////////////////////////////////////////////////

		/*
		* OpenGL graphics assets loading methods
		*/

	public:

		bool LoadModel(const char* _filepath);

	private:

		static unsigned int get_gl_component_size(GLuint _componentType);
		static unsigned int get_glTF_type_component_count(int _attributeType);

		bool load_gltf_model(const char * _filepath);

		/*
		* Buffer Methods
		*/

	public:

		buffer_info					GetBufferInfo(buffer_handle _buffer) const;
		index_buffer_info			GetIndexBufferInfo(buffer_handle _ibo) const;

		/*
		* Mesh Methods
		*/

	public:

		mesh_handle					FindMesh(const char* _mesh_name) const;
		mesh_primitive_list const&	GetMeshPrimitives(mesh_handle _mesh) const;
		
		/*
		* Material methods
		*/

	public:

		material_data				GetMaterial(material_handle _material) const;

		/*
		* Texture methods
		*/

	public:

		struct texture_parameters
		{
			int m_wrap_s = GL_REPEAT;
			int m_wrap_t = GL_REPEAT;
			int m_wrap_r = GL_REPEAT;
			int m_mag_filter = GL_LINEAR;
			int m_min_filter = GL_LINEAR;
		};


		texture_handle	CreateTexture(const char * _debug_name = nullptr);
		void			DeleteTexture(texture_handle _texture_handle);
		void			BindTexture(texture_handle _texture_handle) const;
		texture_info	GetTextureInfo(texture_handle _texture_handle) const;
		void AllocateTextureStorage2D(
			texture_handle _texture_handle, GLenum _internal_format, 
			glm::uvec2 _size, 
			texture_parameters _params, 
			unsigned int _texture_levels = 1
		);
		void SpecifyAndUploadTexture2D(
			texture_handle _texture_handle, GLint _internal_format, glm::uvec2 _size, unsigned int _texture_levels,
			GLenum _input_format, GLenum _input_component_type, void * _data
		);
		void SetTextureParameters(texture_handle _texture_handle, texture_parameters _params);

	private:

		texture_info & set_texture_target_and_bind(texture_handle _texture_handle, GLenum _target);

		/*
		* Framebuffer Methods
		*/

	public:

		framebuffer_handle	CreateFramebuffer();
		void				DeleteFramebuffer(framebuffer_handle _framebuffer);
		framebuffer_info	BindFramebuffer(framebuffer_handle _framebuffer) const;
		void				UnbindFramebuffer(GLenum _framebuffer_target = GL_FRAMEBUFFER) const;
		void				AttachTextureToFramebuffer(framebuffer_handle _framebuffer, GLenum _attachment_point, texture_handle _texture);
		void				DrawFramebuffers(framebuffer_handle _framebuffer, unsigned int _arr_size, GLenum const* _arr_attachment_points) const;

	private:

		/*
		* OpenGL shader management methods
		*/

	public:

		std::vector<shader_handle>	LoadShaders(std::vector<std::filesystem::path> _shader_paths);
		shader_program_handle		LoadShaderProgram(std::string _program_name, std::vector<shader_handle> _shader_handles);
		shader_program_handle		LoadShaderProgram(std::string _program_name, std::vector<std::filesystem::path> _shader_filepaths);
		shader_program_handle		FindShaderProgram(std::string _program_name) const;
		void						RefreshShaders();

		void UseProgram(shader_program_handle _program_handle);
		void SetBoundProgramUniform(unsigned int _uniform_location, unsigned int _uniform_value);
		void SetBoundProgramUniform(unsigned int _uniform_location, int _uniform_value);
		void SetBoundProgramUniform(unsigned int _uniform_location, float _uniform_value);
		void SetBoundProgramUniform(unsigned int _uniform_location, glm::uvec2 _uniform_value);
		void SetBoundProgramUniform(unsigned int _uniform_location, glm::vec2 _uniform_value);
		void SetBoundProgramUniform(unsigned int _uniform_location, glm::vec3 _uniform_value);
		void SetBoundProgramUniform(unsigned int _uniform_location, glm::vec4 _uniform_value);
		void SetBoundProgramUniform(unsigned int _uniform_location, glm::mat3 const & _uniform_value);
		void SetBoundProgramUniform(unsigned int _uniform_location, glm::mat4 const & _uniform_value);

	private:

		static GLenum			get_extension_shader_type(filepath_string const& _extension);

		// OpenGL shader creation, loading and compilation, program creation and linking
		static bool				load_shader_data(std::filesystem::path const& _path, ResourceManager::shader_data** _out_data_ptr);
		static void				unload_shader_data(shader_data* _shader_data);
		static bool				compile_gl_shader(GLuint _gl_shader_object, shader_data* _data, GLenum _type);
		static GLuint			create_program_and_link_gl_shaders(std::vector<GLuint> const& _gl_shader_objects, bool * _success);
		static void				attach_gl_program_shaders(GLuint _gl_program_object, std::vector<GLuint> const& _gl_shader_objects);
		static bool				link_gl_program_shaders(GLuint _gl_program_object);

		/*
		* Graphics asset management methods
		*/

	public:

		void Reset();
		void DeleteAllGraphicsResources();

	private:

		void reset_counters();

		void delete_meshes(std::vector<mesh_handle> const& _meshes);
		void delete_buffers(std::vector<buffer_handle> const& _buffers);
		void delete_materials(std::vector<material_handle> const& _materials);
		void delete_textures(std::vector<texture_handle> const& _textures);
		void delete_framebuffers(std::vector<framebuffer_handle> const& _framebuffers);
		void delete_shaders(std::vector<shader_handle> _shaders);
		void delete_programs(std::vector<shader_program_handle> _programs);
	};

}
}

#define GfxAssert(x) if(!(x)) __debugbreak();
#define GfxCall(x) \
	Engine::Graphics::GLClearError();\
	x;\
	Engine::Graphics::GLLogCall(#x, __FILE__, __LINE__);

#define GfxCallReturn(r, x) \
	Engine::Graphics::GLClearError();\
	r = x;\
	Engine::Graphics::GLLogCall(#x, __FILE__, __LINE__);

#endif // !ENGINE_GRAPHICS_MANAGER_H
