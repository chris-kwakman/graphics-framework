#include "SkeletonAnimator.h"
#include "Renderable.h"
#include <ImGui/imgui_stdlib.h>

#include <array>
#include <Engine/Math/Transform3D.h>
#include <Engine/Utils/algorithm.h>

namespace Component
{

    void animation_instance::set_animation(animation_handle _anim)
    {
        m_animation_handle = _anim;
        m_global_time = 0.0f;
    }

    void animation_instance::set_anim_speed(float _speed)
    {
        m_anim_speed = _speed && ((~0) << 2);
    }


///////////////////////////////////////////////////////////////////////////////
//                     Animation Utility Functions
///////////////////////////////////////////////////////////////////////////////


namespace AnimationUtil
{
    void update_joint_transforms(
        animation_data const * _animation_data, float const* _in_anim_channel_data, 
        Engine::Math::transform3D* const _joint_transforms, unsigned int _joint_transform_count)
    {
        // Go over all animated properties and assign them to the corresponding transforms.
        unsigned int channel_idx = 0;
        unsigned int curr_joint_idx = 0;
        unsigned int instance_data_channel_offset = 0;
        unsigned int joint_channels_offset = 0;
        typedef animation_channel_data::E_target_path channel_target_path;
        Engine::Math::transform3D* modify_joint_transform = _joint_transforms;
        while (curr_joint_idx < _joint_transform_count)
        {
            unsigned int curr_joint_channel_count = 0;
            auto iter = _animation_data->m_skeleton_jointnode_channel_count.find(curr_joint_idx);
            if (iter != _animation_data->m_skeleton_jointnode_channel_count.end())
                curr_joint_channel_count = iter->second;
            for (unsigned int i = 0; i < curr_joint_channel_count; ++i)
            {
                switch (_animation_data->m_animation_channels[joint_channels_offset + i].m_target_path)
                {
                case channel_target_path::TRANSLATION:
                    modify_joint_transform->position = *(glm::vec3 const*)(_in_anim_channel_data + instance_data_channel_offset);
                    instance_data_channel_offset += 3;
                    break;
                case channel_target_path::SCALE:
                    modify_joint_transform->scale = *(glm::vec3 const*)(_in_anim_channel_data + instance_data_channel_offset);
                    instance_data_channel_offset += 3;
                    break;
                case channel_target_path::ROTATION:
                    modify_joint_transform->quaternion = *(glm::quat const*)(_in_anim_channel_data + instance_data_channel_offset);
                    instance_data_channel_offset += 4;
                    break;
                }
            }
            joint_channels_offset += curr_joint_channel_count;
            curr_joint_idx++;
            modify_joint_transform++;
        }
    }

    void compute_animation_channel_data(
        animation_data const * _animation_data,
        float _time, 
        float* _out_anim_channel_data,
        bool _use_slerp
    )
    {
        auto const & res_mgr = Singleton<ResourceManager>();

        unsigned int skeleton_joint_idx = -1;
        unsigned int joint_channels_offset = 0;
        unsigned int new_joint_channel_idx = -1;

        animation_interpolation_handle cached_input_handle = 0;
        animation_interpolation_data const* cached_input_interp_data = nullptr;

        float right_weight = 0.0f;
        unsigned int interp_left_idx, interp_right_idx;
        unsigned int instance_data_offset = 0;
        for (; joint_channels_offset < _animation_data->m_animation_channels.size(); ++joint_channels_offset)
        {
            // Fetch new input and output interpolation data.
            animation_channel_data const& channel_data = _animation_data->m_animation_channels[joint_channels_offset];
            auto& sampler_data = res_mgr.m_anim_sampler_data_map.at(channel_data.m_anim_sampler_handle);
            if (sampler_data.m_anim_interp_input_handle != cached_input_handle)
            {
                cached_input_handle = sampler_data.m_anim_interp_input_handle;
                cached_input_interp_data = &res_mgr.m_anim_interpolation_data_map.at(cached_input_handle);

            }
            animation_interpolation_data const& output_interp_data = res_mgr.m_anim_interpolation_data_map.at(sampler_data.m_anim_interp_output_handle);
            std::vector<float> const& keyframe_arr = cached_input_interp_data->m_data;

            // Search for upper and lower bound of key in keyframe array
            auto pair = Engine::Utils::float_binary_search(
                &keyframe_arr.front(),
                keyframe_arr.size(),
                _time
            );
            interp_left_idx = pair.first;
            interp_right_idx = pair.second;
            // If we are between two keyframes, interpolate between left and right index properties
            right_weight = 0.0f;
            if (interp_right_idx != interp_left_idx)
            {
                right_weight =
                    (_time - keyframe_arr[interp_left_idx])
                    / (keyframe_arr[interp_right_idx] - keyframe_arr[interp_left_idx]);
            }

            // Calculate interpolated property
            switch (channel_data.m_target_path)
            {
            case animation_channel_data::E_target_path::TRANSLATION:
            case animation_channel_data::E_target_path::SCALE:
                AnimationUtil::interpolate_vector(
                    reinterpret_cast<glm::vec3*>(_out_anim_channel_data + instance_data_offset),
                    reinterpret_cast<glm::vec3 const*>(&output_interp_data.m_data.front()) + interp_left_idx,
                    reinterpret_cast<glm::vec3 const*>(&output_interp_data.m_data.front()) + interp_right_idx,
                    right_weight,
                    sampler_data.m_interpolation_type
                );
                instance_data_offset += 3;
                break;
            case animation_channel_data::E_target_path::ROTATION:
                AnimationUtil::interpolate_quaternion(
                    reinterpret_cast<glm::quat*>(_out_anim_channel_data + instance_data_offset),
                    reinterpret_cast<glm::quat const*>(&output_interp_data.m_data.front()) + interp_left_idx,
                    reinterpret_cast<glm::quat const*>(&output_interp_data.m_data.front()) + interp_right_idx,
                    right_weight,
                    sampler_data.m_interpolation_type,
                    _use_slerp
                );
                instance_data_offset += 4;
                break;
            }


            // Set up next joint channel batch to process
            if (joint_channels_offset >= new_joint_channel_idx)
            {
                auto iter = _animation_data->m_skeleton_jointnode_channel_count.find(skeleton_joint_idx);
                if (iter != _animation_data->m_skeleton_jointnode_channel_count.end())
                    new_joint_channel_idx += iter->second;
                ++skeleton_joint_idx;
            }
        }
    }


    void interpolate_vector(
        glm::vec3* _dest, 
        glm::vec3 const* _left, 
        glm::vec3 const* _right, 
        float _right_weight, 
        animation_sampler_data::E_interpolation_type _interpolation_type
    )
    {
        switch (_interpolation_type)
        {
        case animation_sampler_data::E_interpolation_type::CUBICSPLINE:
            //        break;
        case animation_sampler_data::E_interpolation_type::STEP:
            //        break;
        default:
            *_dest = (1.0f - _right_weight) * (*_left) + _right_weight * (*_right);
            break;
        }
    }

    void interpolate_quaternion(
        glm::quat* _dest, 
        glm::quat const* _left, 
        glm::quat const* _right, 
        float _right_weight, 
        animation_sampler_data::E_interpolation_type _interpolation_type,
        bool _use_slerp
    )
    {
        float const theta = acosf(std::clamp(glm::dot(*_left, *_right), -1.0f, 1.0f));
        switch (_interpolation_type)
        {
        case animation_sampler_data::E_interpolation_type::CUBICSPLINE:
            //        break;
        case animation_sampler_data::E_interpolation_type::STEP:
            //        break;
        default:
            if (_use_slerp && theta != 0)
                *_dest = (sinf((1 - _right_weight) * theta) * (*_left) + sin(_right_weight * theta) * (*_right)) / sin(theta);
            else
                *_dest = glm::normalize((1 - _right_weight) * (*_left) + (_right_weight) * (*_right));
            break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//                     Skeleton Animator (Single Animation)
///////////////////////////////////////////////////////////////////////////////

const char* SkeletonAnimatorManager::GetComponentTypeName() const
{
    return "SkeletonAnimator";
}

void SkeletonAnimatorManager::UpdateAnimatorInstances(float _dt)
{
    auto& res_mgr = Singleton<ResourceManager>();

    float stack_instance_anim_channel_data[1024];

    for (auto & pair : m_entity_anim_inst_map)
    {
        Entity animator_entity = pair.first;
        animation_instance & animation_inst = pair.second;

        // Skip if handle is invalid.
        if (animation_inst.m_animation_handle == 0 || animation_inst.m_paused)
            continue;

        Component::Skin skin_comp = animator_entity.GetComponent<Component::Skin>();
        assert(skin_comp.IsValid() && "Animator entity does not have valid Skin component");
        std::vector<Component::Transform> const & skeleton_joint_transforms = skin_comp.GetSkeletonInstanceNodes();
        if (skeleton_joint_transforms.empty())
            continue;

        animation_data const& animation_data = res_mgr.m_anim_data_map.at(animation_inst.m_animation_handle);

        AnimationUtil::compute_animation_channel_data(
            &animation_data,
            animation_inst.m_global_time,
            stack_instance_anim_channel_data,
            m_use_slerp
        );

        // Copy transform data of joints into local vector.
        std::vector<Engine::Math::transform3D> local_joint_transforms(skeleton_joint_transforms.size());
        for (int i = 0; i < skeleton_joint_transforms.size(); ++i)
            local_joint_transforms[i] = skeleton_joint_transforms[i].GetLocalTransform();

        AnimationUtil::update_joint_transforms(
            &animation_data,
            stack_instance_anim_channel_data,
            &local_joint_transforms.front(),
            skeleton_joint_transforms.size()
        );

        // Reset transform component data for each joint
        for (unsigned int i = 0; i < skeleton_joint_transforms.size(); ++i)
        {
            Component::Transform edit_transform = skeleton_joint_transforms[i];
            edit_transform.SetLocalTransform(local_joint_transforms[i]);
        }

        animation_inst.m_global_time += _dt * animation_inst.m_anim_speed;
        if (animation_inst.m_global_time > animation_data.m_duration)
        {
            if (animation_inst.m_loop)
                animation_inst.m_global_time -= animation_data.m_duration;
            else
                animation_inst.m_paused = true;
        }
    }
}

void SkeletonAnimatorManager::impl_clear()
{
    m_entity_anim_inst_map.clear();
}

bool SkeletonAnimatorManager::impl_create(Entity _e)
{
    animation_instance instance;
    instance.m_anim_speed = 1.0f;
    instance.m_global_time = 0.0f;
    instance.m_paused = true;
    instance.m_loop = true;
    m_entity_anim_inst_map.emplace(_e, std::move(instance));
    set_instance_animation(_e, 0);
    return true;
}

void SkeletonAnimatorManager::impl_destroy(Entity const* _entities, unsigned int _count)
{
    for (unsigned int i = 0; i < _count; ++i)
        m_entity_anim_inst_map.erase(_entities[i]);

}

bool SkeletonAnimatorManager::impl_component_owned_by_entity(Entity _entity) const
{
    return m_entity_anim_inst_map.find(_entity) != m_entity_anim_inst_map.end();
}

void SkeletonAnimatorManager::impl_edit_component(Entity _entity)
{
    animation_instance& instance = get_entity_instance(_entity);
    auto & res_mgr = Singleton<ResourceManager>();

    instance.m_animation_handle;

    auto iter = res_mgr.m_anim_data_map.find(instance.m_animation_handle);
    bool valid_anim = false;
    std::string anim_handle_label;
    if (instance.m_animation_handle == 0)
        anim_handle_label = "No Animation";
    else if (iter == res_mgr.m_anim_data_map.end())
        anim_handle_label = "Invalid Animation";
    else
    {
        anim_handle_label = iter->second.m_name;
        valid_anim = true;
    }

    ImGui::InputText("Animation", &anim_handle_label, ImGuiInputTextFlags_ReadOnly);
    if (ImGui::BeginDragDropTarget())
    {
        if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload("RESOURCE_ANIMATION"))
            set_instance_animation(_entity, *reinterpret_cast<animation_handle*>(payload->Data));
       ImGui::EndDragDropTarget();
    }

    if (valid_anim)
    {
        bool    is_paused = instance.m_paused,
                is_looping = instance.m_loop;
        float   timer = instance.m_global_time;

        if (ImGui::Checkbox("Paused", &is_paused))
            instance.m_paused = is_paused;
        if (ImGui::Checkbox("Loop", &is_looping))
            instance.m_loop = is_looping;
        if (ImGui::InputFloat("Time", &timer, 0.01f, 0.1f, "%.2f"))
            instance.m_global_time = timer;
        float anim_speed = instance.m_anim_speed;
        if (ImGui::SliderFloat("Animation Speed", &anim_speed, 0.001f, 10.0f, "%.2f"))
            instance.m_anim_speed = anim_speed;

        ImGui::Checkbox("Use SLERP", &m_use_slerp);

        ImGui::Separator();

        auto& anim_data = res_mgr.m_anim_data_map.at(instance.m_animation_handle);

        ImGui::Text("Animation Duration: %.2f", anim_data.m_duration);
    }
}

void SkeletonAnimatorManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
{
}

animation_instance& SkeletonAnimatorManager::get_entity_instance(Entity _e)
{
    return m_entity_anim_inst_map.at(_e);
}

void SkeletonAnimatorManager::set_instance_animation(Entity _e, animation_handle _animation)
{
    auto& res_mgr = Singleton<ResourceManager>();

    animation_instance & instance = m_entity_anim_inst_map.at(_e);
    instance.m_animation_handle = _animation;
    instance.m_global_time = 0.0f;    
}

Engine::Graphics::animation_handle SkeletonAnimator::GetAnimationHandle() const
{
    return GetManager().m_entity_anim_inst_map.at(m_owner).m_animation_handle;
}

void SkeletonAnimator::SetAnimationHandle(animation_handle _animation)
{
    GetManager().set_instance_animation(m_owner, _animation);
}

void SkeletonAnimator::SetAnimation(std::string _animationName)
{
    Engine::Graphics::animation_handle _handle = Singleton<ResourceManager>().FindNamedAnimation(_animationName);
    GetManager().set_instance_animation(m_owner, _handle);
}

bool SkeletonAnimator::IsPaused() const
{
    return GetManager().m_entity_anim_inst_map.at(m_owner).m_paused;
}

void SkeletonAnimator::SetPaused(bool _paused)
{
    GetManager().m_entity_anim_inst_map.at(m_owner).m_paused = _paused;
}

///////////////////////////////////////////////////////////////////////////////
//                      Skeleton Animator 1D
///////////////////////////////////////////////////////////////////////////////

}