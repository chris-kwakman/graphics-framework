
#include "common.h"

namespace glm
{
	using json = nlohmann::json;

	void from_json(json const& j, glm::vec2& v)
	{
		if (j.is_array())
		{
			for (unsigned int i = 0; i < 2; ++i)
				v[i] = j.at(i);
		}
		else
		{
			v[0] = j.at("x");
			v[1] = j.at("y");
		}
	}

	void to_json(json& j, glm::vec2 const& v)
	{
		j["x"] = v[0];
		j["y"] = v[1];
	}

	void from_json(json const& j, glm::vec3& v)
	{
		if (j.is_array())
		{
			for (unsigned int i = 0; i < 3; ++i)
				v[i] = j.at(i);
		}
		else
		{
			v[0] = j.at("x");
			v[1] = j.at("y");
			v[2] = j.at("z");
		}
	}

	void to_json(json& j, glm::vec3 const& v)
	{
		j["x"] = v[0];
		j["y"] = v[1];
		j["z"] = v[2];
	}

	void from_json(json const& j, glm::uvec3& v)
	{
		if (j.is_array())
		{
			for (unsigned int i = 0; i < 3; ++i)
				v[i] = j.at(i);
		}
		else
		{
			v[0] = j.at("x");
			v[1] = j.at("y");
			v[2] = j.at("z");
		}
	}

	void to_json(json& j, glm::uvec3 const& v)
	{
		j["x"] = v[0];
		j["y"] = v[1];
		j["z"] = v[2];
	}

	void from_json(json const& j, glm::vec4& v)
	{
		if (j.is_array())
		{
			for (unsigned int i = 0; i < 4; ++i)
				v[i] = j.at(i);
		}
		else
		{
			v[0] = j.at("x");
			v[1] = j.at("y");
			v[2] = j.at("z");
			v[3] = j.at("w");
		}
	}

	void to_json(json& j, glm::vec4 const& v)
	{
		j["x"] = v[0];
		j["y"] = v[1];
		j["z"] = v[2];
		j["w"] = v[3];
	}

	void from_json(json const& j, glm::quat& q)
	{
		if (j.is_array())
		{
			q.x = j.at(0);
			q.y = j.at(1);
			q.z = j.at(2);
			q.w = j.at(3);
		}
		// WXYZ format
		else
		{
			q.w = j.at("w");
			q.x = j.at("x");
			q.y = j.at("y");
			q.z = j.at("z");
		}
	}

	void to_json(json& j, glm::quat const& q)
	{
		// WXYZ format
		j["w"] = q.w;
		j["x"] = q.x;
		j["y"] = q.y;
		j["z"] = q.z;
	}

	void from_json(nlohmann::json const& j, glm::mat3& m)
	{
		for (unsigned int i = 0; i < 9; ++i)
			m[i / 3][i % 3] = (float)j.at(i);
	}

	void to_json(nlohmann::json& j, glm::mat3 const& m)
	{
		for (unsigned int i = 0; i < 9; ++i)
			j.push_back(m[i / 3][i % 3]);
	}

	void from_json(nlohmann::json const& j, glm::mat4& m)
	{
		for (unsigned int i = 0; i < 16; ++i)
			m[i / 4][i % 4] = (float)j.at(i);
	}

	void to_json(nlohmann::json& j, glm::mat4 const& m)
	{
		for (unsigned int i = 0; i < 16; ++i)
			j.push_back(m[i / 4][i % 4]);
	}
}