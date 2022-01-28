#ifndef ENGINE_SERIALISATION_COMMON_H
#define ENGINE_SERIALISATION_COMMON_H

#include <nlohmann/json.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/quaternion.hpp>

namespace glm
{
	void from_json(nlohmann::json const& j, glm::vec2& v);
	void to_json(nlohmann::json& j, glm::vec2 const& v);

	void from_json(nlohmann::json const& j, glm::vec3& v);
	void to_json(nlohmann::json& j, glm::vec3 const& v);

	void from_json(nlohmann::json const& j, glm::uvec3& v);
	void to_json(nlohmann::json& j, glm::uvec3 const& v);

	void from_json(nlohmann::json const& j, glm::vec4& v);
	void to_json(nlohmann::json& j, glm::vec4 const& v);

	void from_json(nlohmann::json const& j, glm::quat& q);
	void to_json(nlohmann::json& j, glm::quat const& q);

	void from_json(nlohmann::json const& j, glm::mat3& m);
	void to_json(nlohmann::json& j, glm::mat3 const& m);

	void from_json(nlohmann::json const& j, glm::mat4& m);
	void to_json(nlohmann::json & j, glm::mat4 const & m);
}

#endif // !ENGINE_SERIALISATION_COMMON_H
