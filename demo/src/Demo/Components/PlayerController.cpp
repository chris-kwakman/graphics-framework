#include "PlayerController.h"

#include <Engine/Managers/input.h>
#include <Engine/Components/Transform.h>
#include <Engine/Editor/editor.h>

#include <glm/ext/quaternion_geometric.hpp>

namespace Component
{

    const char* PlayerControllerManager::GetComponentTypeName() const
    {
        return "PlayerController";
    }

    void PlayerControllerManager::impl_clear()
    {
        m_data.m_player_map.clear();
    }

    bool PlayerControllerManager::impl_create(Entity _e)
    {
        player_data new_player_data;
        new_player_data.m_velocity = glm::vec2(0.0f, 0.0f);
        new_player_data.m_velocity_cap = 5.789f;
        new_player_data.m_velocity_attenuation = 0.916f;
        new_player_data.m_acceleration = 45.0f;
        m_data.m_player_map.emplace(_e, std::move(new_player_data));
        return true;
    }

    void PlayerControllerManager::impl_destroy(Entity const* _entities, unsigned int _count)
    {
        for (unsigned int i = 0; i < _count; ++i)
        {
            m_data.m_player_map.erase(_entities[i]);
        }
    }

    bool PlayerControllerManager::impl_component_owned_by_entity(Entity _entity) const
    {
        return m_data.m_player_map.find(_entity) != m_data.m_player_map.end();
    }

    void PlayerControllerManager::impl_edit_component(Entity _entity)
    {
        player_data& data = m_data.m_player_map.at(_entity);
        ImGui::InputFloat2("Velocity", &data.m_velocity.x, "%.2f", ImGuiInputTextFlags_ReadOnly);
        ImGui::SliderFloat("Velocity Cap", &data.m_velocity_cap, 0.0f, 10.0f, "%.3f");
        ImGui::SliderFloat("Velocity Attenuation", &data.m_velocity_attenuation, 0.0f, 1.0f);
        ImGui::SliderFloat("Acceleration", &data.m_acceleration, 0.0f, 50.0f);
    }


    void PlayerControllerManager::update_player(float _dt, Entity _e, player_data& _data)
    {
        using InputManager = Engine::Managers::InputManager;
        using button_state = InputManager::button_state;
        auto const& io = Singleton<InputManager>();

        Transform player_transform = _e.GetComponent<Transform>();
        Entity camera_entity = Singleton<Engine::Editor::Editor>().EditorCameraEntity;
        Transform camera_transform = camera_entity.GetComponent<Transform>();

        Engine::Math::transform3D cam_transform_3d, player_transform_3d;
        cam_transform_3d = camera_transform.ComputeWorldTransform();
        player_transform_3d = player_transform.ComputeWorldTransform();

        glm::vec2  acc_dir(0.0f);
        if (io.GetKeyboardButtonState(SDL_SCANCODE_LCTRL) == button_state::eDown)
        {
            if (io.GetKeyboardButtonState(SDL_SCANCODE_A) == button_state::eDown)
                acc_dir.x += 1.0f;
            if (io.GetKeyboardButtonState(SDL_SCANCODE_D) == button_state::eDown)
                acc_dir.x -= 1.0f;
            if (io.GetKeyboardButtonState(SDL_SCANCODE_S) == button_state::eDown)
                acc_dir.y -= 1.0f;
            if (io.GetKeyboardButtonState(SDL_SCANCODE_W) == button_state::eDown)
                acc_dir.y += 1.0f;
        }

        _data.m_velocity *= _data.m_velocity_attenuation;

        if (glm::any(glm::epsilonNotEqual(acc_dir, glm::vec2(0.0f), glm::epsilon<float>())))
        {
            // Apply acceleration relative to camera look direction.
            glm::vec3 const view_dir = cam_transform_3d.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec3 const projected_view_dir = glm::vec3(view_dir.x, 0.0f, view_dir.z);
            glm::quat const rot_applied_velocity = glm::quatLookAt(projected_view_dir, glm::vec3(0.0f, 1.0f, 0.0f));

            glm::vec2 add_velocity = glm::normalize(acc_dir) * _data.m_acceleration * _dt;
            glm::vec3 add_world_velocity = rot_applied_velocity * glm::vec3(add_velocity.x, 0.0f, add_velocity.y);
            _data.m_velocity += glm::vec2(add_world_velocity.x, add_world_velocity.z);
        }
        if (glm::any(glm::epsilonNotEqual(_data.m_velocity, glm::vec2(0.0f), glm::epsilon<float>())))
        {
            float abs_vel = glm::length(_data.m_velocity);
            if (abs_vel > _data.m_velocity_cap)
            {
                _data.m_velocity *= _data.m_velocity_cap / abs_vel;
                abs_vel = _data.m_velocity_cap;
            }

            // Generate player orientation depending on velocity
            if (abs_vel > 0.0f)
            {
                glm::vec3 const default_lookat(0.0f, 0.0f, -1.0f);

                glm::vec2 const vel_dir = -_data.m_velocity / abs_vel;
                glm::quat new_rotation = glm::quatLookAt(glm::vec3(vel_dir.x, 0.0f, vel_dir.y), glm::vec3(0.0f, 1.0f, 0.0f));
                _e.GetComponent<Component::Transform>().SetLocalRotation(new_rotation);

                player_transform.SetLocalPosition(player_transform.GetLocalPosition() + glm::vec3(_data.m_velocity.x, 0.0f, _data.m_velocity.y) * _dt);
            }
            // Set animation blend parameter
            if (_data.m_anim_ptr_move_blend)
            {
                *_data.m_anim_ptr_move_blend = 2.0f * (abs_vel / _data.m_velocity_cap);
            }
        }
        else
        {
            // Set animation blend parameter
            if (_data.m_anim_ptr_move_blend)
            {
                *_data.m_anim_ptr_move_blend = 0.0f;
            }
        }

        // Make player character look in direction of camera.
        if (camera_transform.IsValid() && _data.m_anim_ptr_look_blend)
        {

            glm::vec3 const target_dir = glm::normalize(
                cam_transform_3d.position - 
                (player_transform_3d.position + glm::vec3(0.0f, 1.0f, 0.0f) * player_transform_3d.scale)
            );
            glm::vec2 const target_dir_horizontal = glm::vec2(target_dir.x, target_dir.z);

            glm::vec3 const player_view_vec = player_transform_3d.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 const player_right_vec = glm::cross(player_view_vec, glm::vec3(0.0f, 1.0f, 0.0f));

            float const rightness = glm::dot(
                glm::vec2(player_right_vec.x, player_right_vec.z), 
                glm::vec2(target_dir.x, target_dir.z)
            );
            float const upness = target_dir.y;

            glm::vec2 blend_pos(-rightness, upness);
            // Clamp blending position to diamond with axis ranging from [-1,1] and [-1,1])
            float sum = std::clamp(abs(blend_pos.x) + abs(blend_pos.y), 1.0f, FLT_MAX);
            blend_pos /= sum; // Clamp such that |blend_pos.x| + |blend_pos.y| = 1;
            *_data.m_anim_ptr_look_blend = blend_pos;
        }
    }

    void PlayerControllerManager::Update(float _dt)
    {
        for (auto & pair : m_data.m_player_map)
        {
            update_player(_dt, pair.first, pair.second);
        }
    }

    void PlayerControllerManager::SetPlayerMoveBlendParam(Entity _e, float* _param)
    {
        auto find = m_data.m_player_map.find(_e);
        if (find != m_data.m_player_map.end())
        {
            find->second.m_anim_ptr_move_blend = _param;
        }
    }

    void PlayerControllerManager::SetPlayerLookBlendParam(Entity _e, glm::vec2* _param)
    {
        auto find = m_data.m_player_map.find(_e);
        if (find != m_data.m_player_map.end())
        {
            find->second.m_anim_ptr_look_blend = _param;
        }
    }

    void PlayerControllerManager::impl_deserialize_data(nlohmann::json const& _j)
    {
        int const serializer_version = _j["serializer_version"];
        if (serializer_version == 1)
        {
            m_data = _j["m_data"];
        }
    }

    void PlayerControllerManager::impl_serialize_data(nlohmann::json& _j) const
    {
        _j["serializer_version"] = 1;

        _j["m_data"] = m_data;
    }

    void PlayerController::BindMoveBlendParameter(float* _param)
    {
        if(_param)
            GetManager().SetPlayerMoveBlendParam(Owner(), _param);
    }

    void PlayerController::BindLookBlendParameter(glm::vec2* _param)
    {
        if (_param)
            GetManager().SetPlayerLookBlendParam(Owner(), _param);
    }

}


