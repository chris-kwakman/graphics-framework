#include "serialise_gltf.h"

#include <Engine/Math/Transform3D.h>
#include <Engine/Components/Transform.h>
#include <Engine/Components/Renderable.h>

#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace Engine {
namespace Serialisation {

    using transform3D = Engine::Math::transform3D;
    using gltf_model_data = Engine::Graphics::ResourceManager::gltf_model_data;

    nlohmann::json SerializeGLTFScene(tinygltf::Model const& _model, gltf_model_data const& _model_data)
    {
        using namespace nlohmann;

        auto& resource_manager = Singleton<Engine::Graphics::ResourceManager>();

        json scene;
        assert(_model.scenes.size() == 1);
        scene["Name"] = _model.scenes[_model.defaultScene].name;
        json & entities = scene["Entities"];
        entities = json::array();


        std::vector<unsigned int> node_parents;
        node_parents.resize(_model.nodes.size(), Engine::ECS::Entity::InvalidEntity.ID());

        for (unsigned int i = 0; i < _model.nodes.size(); ++i)
            for (int child_idx : _model.nodes[i].children)
                node_parents[child_idx] = i;


        unsigned int node_index = 0;
        for (auto node : _model.nodes)
        {
            json entity_data;
            json & entity_components = entity_data["Components"];

            /*
            * Load node transform component
            */

            json& transform_component = entity_components[Singleton<Component::TransformManager>().GetComponentTypeName()];

            glm::vec3 scale(1.0f, 1.0f, 1.0f);
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 translation(0.0f, 0.0f, 0.0f);
            if (!node.matrix.empty())
            {
                glm::mat4x4 transform;
                for (unsigned int i = 0; i < 16; ++i)
                    transform[i / 4][i % 4] = (float)node.matrix[i];

                glm::vec3 skew;
                glm::vec4 perspective;

                glm::decompose(transform, scale, rotation, translation, skew, perspective);
            }
            else
            {
                if (!node.translation.empty())
                {
                    for (unsigned int i = 0; i < 3; ++i)
                        translation[i] = (float)node.translation[i];
                }
                if (!node.rotation.empty())
                {
                    for (unsigned int i = 0; i < 4; ++i)
                        rotation[i] = (float)node.rotation[i];
                }
                if (!node.scale.empty())
                {
                    for (unsigned int i = 0; i < 3; ++i)
                        scale[i] = (float)node.scale[i];
                }
            }
            transform3D transform;
            transform.position = translation;
            transform.scale = scale;
            transform.quaternion = rotation;

            transform_component["local_transform"] = transform;
            if(node_parents[node_index] != Engine::ECS::Entity::InvalidEntity.ID())
                transform_component["parent_index"] = node_parents[node_index];
            
            /*
            * Load renderable component
            */

            if (node.mesh >= 0)
            {
                json& renderable_component = entity_components[Singleton<Component::RenderableManager>().GetComponentTypeName()];
                renderable_component["mesh_name"] = resource_manager.GetMeshName(_model_data.m_meshes[node.mesh]);
            }

            // TODO: Load camera component

            entities.push_back(entity_data);
            node_index++;
        }

        return scene;
    }

}
}
