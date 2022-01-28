#include <Engine/ECS/component_manager.h>

namespace Component
{
	using namespace Engine::ECS;

	class NameableManager;
	struct Nameable : public IComp<NameableManager>
	{
		DECLARE_COMPONENT(Nameable);

		const char* GetName() const;
		const char* SetName(const char* _name);
		static constexpr uint8_t MaxNameLength(); // Excludes null-terminator
	};
	class NameableManager : public TCompManager<Nameable>
	{
	public:
		static unsigned int const MAX_STRING_SIZE = 64;
	private:

		std::unordered_map<Entity, unsigned int, Entity::hash> m_entity_index_map;
		std::vector<Entity> m_index_entities;
		std::vector<std::array<char,MAX_STRING_SIZE>> m_index_names;

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;

		uint8_t get_index_name_length(unsigned int _index) const;
		uint8_t set_index_name_length(unsigned int _index, uint8_t _length);
		void set_index_name(unsigned int _index, const char* _name);

		friend struct Nameable;

	public:

		virtual const char* GetComponentTypeName() const override;


		// Inherited via TCompManager
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;

	};
}