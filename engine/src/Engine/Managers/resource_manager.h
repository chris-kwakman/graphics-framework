#pragma once

#include "resource_manager_data.h"

namespace Engine {
namespace Managers {

	class ResourceManager : public resource_manager_data
	{
	public:

		void DisplayEditorWidget();
		void TryDragDropFile(fs::path _path);

	private:
		
		bool m_new_resources = false;
		fs::path m_drag_dropped_file_path;
	};

	void resource_dragdrop_source(resource_reference _reference, const char* _type_name);
	void resource_dragdrop_target(resource_reference & _reference, const char* _type_name);

}
}