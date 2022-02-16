#include <unordered_map>
#include <vector>
#include <memory>
#include <glm/vec3.hpp>

#include <engine/Utils/filesystem.h>

namespace Engine {
namespace Physics {

	struct point_hull
	{
		std::vector<glm::vec3> m_points;
	};

	uint32_t LoadPointHull(fs::path const& _path);
	void UnloadPointHull(uint32_t _handle);

	class PointHullManager
	{
		std::unordered_map<uint32_t, std::unique_ptr<point_hull>> m_point_hulls;
		uint32_t m_handle_counter = 1;

	public:

		point_hull const* GetPointHull(uint32_t _handle) const;
		uint32_t RegisterPointHull(point_hull&& _hull);
		void DeletePointHull(uint32_t _handle);
	};

}
}