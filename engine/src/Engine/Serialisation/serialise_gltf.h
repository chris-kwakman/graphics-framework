#ifndef ENGINE_SERIALISE_GLTF_H
#define ENGINE_SERIALISE_GLTF_H
#include <Engine/Graphics/manager.h>

#include <tiny_glft/tiny_gltf.h>
#include <nlohmann/json.hpp>

namespace Engine {
namespace Serialisation {

	using gltf_model_data = Engine::Graphics::ResourceManager::gltf_model_data;

	nlohmann::json SerializeGLTFScene(tinygltf::Model const& _model, gltf_model_data const & _model_data);

}
}

#endif // !ENGINE_SERIALISE_GLTF_H

