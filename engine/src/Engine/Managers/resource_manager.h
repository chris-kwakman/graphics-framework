#pragma once

#include "resource_manager_data.h"

namespace Engine {
namespace Managers {

	struct Resource : protected resource_typeid
	{
		Resource() : resource_typeid(0,0) {};
		Resource(resource_typeid _type);
		Resource(Resource const& _l);

		bool operator==(Resource const& _l) const;
		bool operator!=(Resource const& _l) const;
		bool operator<(Resource const& _l) const;

		uint32_t		Handle() const;
		resource_id		ID() const {return m_id;}
		resource_type	Type() const { return m_type; }
		std::string		Name() const;

		friend void from_json(nlohmann::json const& _j, Resource& _v);
		friend void to_json(nlohmann::json& _j, Resource const& _v);
	};

	void from_json(nlohmann::json const& _j, Resource & _v);
	void to_json(nlohmann::json& _j, Resource const& _v);

	class ResourceManager : public resource_manager_data
	{
		friend struct Resource;

	public:

		// Should be ran AFTER registering resource types.
		void ImportSceneResources(nlohmann::json const & _scene_resources);
		void ExportSceneResources(nlohmann::json& _scene_resources);

		void DisplayEditorWidget();
		void TryDragDropFile(fs::path _path);

	private:
		
		bool m_new_resources = false;
		fs::path m_drag_dropped_file_path;

	};

	void resource_dragdrop_source(Resource const _reference, const char* _type_name);
	bool resource_dragdrop_target(Resource & _reference, const char* _type_name);

}
}