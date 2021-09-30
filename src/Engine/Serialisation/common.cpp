
#include "common.h"

namespace glm
{
	using json = nlohmann::json;

	void from_json(json const& j, glm::vec2& v)
	{
		v[0] = j.at(0);
		v[1] = j.at(1);
	}

	void to_json(json& j, glm::vec2 const& v)
	{
		j.push_back(v[0]);
		j.push_back(v[1]);
	}

	void from_json(json const& j, glm::vec3& v)
	{
		v[0] = j.at(0);
		v[1] = j.at(1);
		v[2] = j.at(2);
	}

	void to_json(json& j, glm::vec3 const& v)
	{
		j.push_back(v[0]);
		j.push_back(v[1]);
		j.push_back(v[2]);
	}

	void from_json(json const& j, glm::vec4& v)
	{
		v[0] = j.at(0);
		v[1] = j.at(1);
		v[2] = j.at(2);
		v[3] = j.at(3);
	}

	void to_json(json& j, glm::vec4 const& v)
	{
		j.push_back(v[0]);
		j.push_back(v[1]);
		j.push_back(v[2]);
		j.push_back(v[3]);
	}

	void from_json(json const& j, glm::quat& q)
	{
		// WXYZ format
		q.w = j.at(0);
		q.x = j.at(1);
		q.y = j.at(2);
		q.z = j.at(3);
	}

	void to_json(json& j, glm::quat const& q)
	{
		// WXYZ format
		j.push_back(q.w);
		j.push_back(q.x);
		j.push_back(q.y);
		j.push_back(q.z);
	}
}