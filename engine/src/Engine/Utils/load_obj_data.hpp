#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "filesystem.h"

namespace Engine {
namespace Utils {

	void load_obj_data(
		fs::path const& _path,
		std::vector<glm::uvec3>* _p_face_vertex_indices,
		std::vector<glm::vec3>* _p_vertices,
		std::vector<glm::vec2>* _p_uvs,
		std::vector<glm::vec3>* _p_normals
	);

}
}