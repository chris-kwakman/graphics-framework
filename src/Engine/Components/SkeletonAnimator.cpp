#include "SkeletonAnimator.h"
#include "Renderable.h"
#include <ImGui/imgui_stdlib.h>

#include <array>
#include <Engine/Math/Transform3D.h>

const char* Component::SkeletonAnimatorManager::GetComponentTypeName() const
{
    return "SkeletonAnimator";
}

void Component::SkeletonAnimatorManager::UpdateAnimatorInstances(float _dt)
{
    auto& res_mgr = Singleton<ResourceManager>();

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
        
        // TODO: Check if timer exceeds max keyframe time. If it has and this instance is looping, reset timer back to start.

        // Iterate over each property that will be animated.
        Component::Transform current_animated_joint = skeleton_joint_transforms.front();
        Engine::Math::transform3D current_transform = current_animated_joint.GetLocalTransform();

        unsigned int skeleton_joint_idx = -1;
        unsigned int joint_channels_offset = 0;
        unsigned int new_joint_channel_idx = -1;
        decltype(animation_data.m_skeleton_jointnode_channel_count)::const_iterator iter;
        do
        {
            skeleton_joint_idx++;
            iter = animation_data.m_skeleton_jointnode_channel_count.find(skeleton_joint_idx);
        } while (iter != animation_data.m_skeleton_jointnode_channel_count.end());

        animation_interpolation_handle cached_input_handle = 0;
        animation_interpolation_data const * cached_input_interp_data = nullptr;

        float right_weight = 0.0f; 
        unsigned int interp_left_idx, interp_right_idx;
        unsigned int instance_data_offset = 0;
        for (; joint_channels_offset < animation_data.m_animation_channels.size(); ++joint_channels_offset)
        {
            // Fetch new input and output interpolation data.
            animation_channel_data const& channel_data = animation_data.m_animation_channels[joint_channels_offset];
            auto & sampler_data = res_mgr.m_anim_sampler_data_map.at(channel_data.m_anim_sampler_handle);
            if (sampler_data.m_anim_interp_input_handle != cached_input_handle)
            {
                cached_input_handle = sampler_data.m_anim_interp_input_handle;
                cached_input_interp_data = &res_mgr.m_anim_interpolation_data_map.at(cached_input_handle);

            }
            animation_interpolation_data const& output_interp_data = res_mgr.m_anim_interpolation_data_map.at(sampler_data.m_anim_interp_output_handle);
            std::vector<float> const & keyframe_arr = cached_input_interp_data->m_data;

            // Search for upper and lower bound of key in keyframe array
            auto pair = interpolation_search(
                animation_inst.m_global_time, keyframe_arr, output_interp_data.m_data.size()
            );
            interp_left_idx = pair.first;
            interp_right_idx = pair.second;
            // If we are between two keyframes, interpolate between left and right index properties
            if (interp_right_idx != interp_left_idx)
            {
                right_weight = 
                    (animation_inst.m_global_time - keyframe_arr[interp_left_idx]) 
                    / (keyframe_arr[interp_right_idx] - keyframe_arr[interp_left_idx]);
            }

            // Calculate interpolated property
            switch (channel_data.m_target_path)
            {
            case animation_channel_data::E_target_path::TRANSLATION:
            case animation_channel_data::E_target_path::SCALE:
                interpolate_vector(
                    reinterpret_cast<glm::vec3*>(&animation_inst.m_instance_data.front() + instance_data_offset),
                    reinterpret_cast<glm::vec3 const*>(&output_interp_data.m_data.front()) + interp_left_idx,
                    reinterpret_cast<glm::vec3 const*>(&output_interp_data.m_data.front()) + interp_right_idx,
                    right_weight,
                    sampler_data.m_interpolation_type
                );
                instance_data_offset += 3;
                break;
            case animation_channel_data::E_target_path::ROTATION:
                interpolate_quaternion(
                    reinterpret_cast<glm::quat*>(&animation_inst.m_instance_data.front() + instance_data_offset),
                    reinterpret_cast<glm::quat const*>(&output_interp_data.m_data.front()) + interp_left_idx,
                    reinterpret_cast<glm::quat const*>(&output_interp_data.m_data.front()) + interp_right_idx,
                    right_weight,
                    sampler_data.m_interpolation_type
                );
                instance_data_offset += 4;
                break;
            }
            

            // Set up next joint channel batch to process
            if (joint_channels_offset >= new_joint_channel_idx)
            {
                auto iter = animation_data.m_skeleton_jointnode_channel_count.find(skeleton_joint_idx);
                if (iter != animation_data.m_skeleton_jointnode_channel_count.end())
                    new_joint_channel_idx += iter->second;
                ++skeleton_joint_idx; 
            }
        }

        // Go over all animated properties and assign them to the corresponding transforms.
        unsigned int channel_idx = 0;
        unsigned int curr_joint_idx = 0;
        unsigned int instance_data_channel_offset = 0;
        float const* instance_data_ptr = (&animation_inst.m_instance_data.front());
        joint_channels_offset = 0;
        typedef animation_channel_data::E_target_path channel_target_path;
        while (curr_joint_idx < skeleton_joint_transforms.size())
        {
            Component::Transform curr_transform = skeleton_joint_transforms[curr_joint_idx];
            Engine::Math::transform3D modify_joint_transform = curr_transform.GetLocalTransform();
            unsigned int curr_joint_channel_count = 0;
            auto iter = animation_data.m_skeleton_jointnode_channel_count.find(curr_joint_idx);
            if (iter != animation_data.m_skeleton_jointnode_channel_count.end())
                curr_joint_channel_count = iter->second;
            for (unsigned int i = 0; i < curr_joint_channel_count; ++i)
            {
                switch (animation_data.m_animation_channels[joint_channels_offset + i].m_target_path)
                {
                case channel_target_path::TRANSLATION: 
                    modify_joint_transform.position = *(glm::vec3 const*)(instance_data_ptr + instance_data_channel_offset);
                    instance_data_channel_offset += 3;
                    break;
                case channel_target_path::SCALE:
                    modify_joint_transform.scale = *(glm::vec3 const*)(instance_data_ptr + instance_data_channel_offset);
                    instance_data_channel_offset += 3;
                    break;
                case channel_target_path::ROTATION:
                    modify_joint_transform.quaternion = *(glm::quat const*)(instance_data_ptr + instance_data_channel_offset);
                    instance_data_channel_offset += 4;
                    break;
                }
            }
            if(curr_joint_channel_count > 0)
                curr_transform.SetLocalTransform(modify_joint_transform);
            joint_channels_offset += curr_joint_channel_count;
            curr_joint_idx += 1;
        }

        //for (unsigned int i = 0; i < animation_inst.m_instance_data.size(); ++i)
        //{
        //    if (skeleton_joint_transforms[animation_data.m_skeleton_jointnode_channel_count].Owner() != current_animated_joint.Owner())
        //    {
        //        
        //        current_animated_joint.SetLocalTransform(current_transform);
        //        current_animated_joint = skeleton_joint_transforms[i];
        //        current_transform = current_animated_joint.GetLocalTransform();
        //    }
        //}
        

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

void Component::SkeletonAnimatorManager::impl_clear()
{
    m_entity_anim_inst_map.clear();
}

bool Component::SkeletonAnimatorManager::impl_create(Entity _e)
{
    animation_instance instance;
    m_entity_anim_inst_map.emplace(_e, std::move(instance));
    set_instance_animation(_e, 0);
    return true;
}

void Component::SkeletonAnimatorManager::impl_destroy(Entity const* _entities, unsigned int _count)
{
    for (unsigned int i = 0; i < _count; ++i)
        m_entity_anim_inst_map.erase(_entities[i]);

}

bool Component::SkeletonAnimatorManager::impl_component_owned_by_entity(Entity _entity) const
{
    return m_entity_anim_inst_map.find(_entity) != m_entity_anim_inst_map.end();
}

static bool show_live_channel_properties = false;
void Component::SkeletonAnimatorManager::impl_edit_component(Entity _entity)
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
        ImGui::Text("Channels: %d", instance.m_instance_data.size());
        ImGui::Checkbox("Show live channel properties", &show_live_channel_properties);
        if (show_live_channel_properties)
        {
            if (ImGui::BeginListBox("Live Channel Properties"))
            {
                for (unsigned int i = 0; i < instance.m_instance_data.size(); ++i)
                    ImGui::Text("data[%d]: %.2f", i, instance.m_instance_data[i]);
                ImGui::EndListBox();
            }
        }
    }
}

void Component::SkeletonAnimatorManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
{
}

Component::SkeletonAnimatorManager::animation_instance& Component::SkeletonAnimatorManager::get_entity_instance(Entity _e)
{
    return m_entity_anim_inst_map.at(_e);
}

void Component::SkeletonAnimatorManager::set_instance_animation(Entity _e, animation_handle _animation)
{
    auto& res_mgr = Singleton<ResourceManager>();

    animation_instance & instance = m_entity_anim_inst_map.at(_e);
    instance.m_animation_handle = _animation;
    instance.m_anim_speed = 1.0f;
    instance.m_global_time = 0.0f;
    instance.m_paused = true;
    instance.m_loop = true;
    instance.m_instance_data.clear();

    auto res_mgr_anim_data_iter = res_mgr.m_anim_data_map.find(instance.m_animation_handle);
    if (instance.m_animation_handle && (res_mgr_anim_data_iter != res_mgr.m_anim_data_map.end()))
    {
        animation_data const& anim_data = res_mgr_anim_data_iter->second;
        //TODO: Set proper size according to each channel's target path
        unsigned int float_count = 0;
        for (unsigned int i = 0; i < anim_data.m_animation_channels.size(); ++i)
        {
            animation_channel_data const& acd = anim_data.m_animation_channels[i];
            switch (acd.m_target_path)
            {
                case animation_channel_data::E_target_path::ROTATION: float_count += 4; break;
                default: float_count += 3; break;
            }
        }
        instance.m_instance_data.resize(float_count);
        update_instance_properties(anim_data, instance, instance.m_global_time);
    }
}

void Component::SkeletonAnimatorManager::update_instance_properties(animation_data const & _data, animation_instance& _instance, float _time)
{
    for (unsigned int i = 0; i < _instance.m_instance_data.size(); ++i)
    {

    }
}

std::pair<unsigned int, unsigned int> Component::SkeletonAnimatorManager::interpolation_search(float _time, std::vector<float> const& _keyframe_arr, size_t _keyframe_count)
{
    //TODO: Probably going out of bounds here. Try make our own binary search
    _time = std::clamp(_time, 0.0f, _keyframe_arr.back());
    auto lower_iter = std::lower_bound(_keyframe_arr.begin(), _keyframe_arr.end(), _time);
    if (*lower_iter != _time && lower_iter != _keyframe_arr.begin())
        lower_iter -= 1;
    auto upper_iter = std::upper_bound(lower_iter, lower_iter + 1, _time);
    return std::pair<unsigned int, unsigned int>(
        std::distance(_keyframe_arr.begin(), lower_iter),
        std::distance(_keyframe_arr.begin(), upper_iter)
    );
}

void Component::SkeletonAnimatorManager::interpolate_vector(glm::vec3* _dest, glm::vec3 const* _left, glm::vec3 const* _right, float _right_weight, animation_sampler_data::E_interpolation_type _interpolation_type)
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

void Component::SkeletonAnimatorManager::interpolate_quaternion(glm::quat* _dest, glm::quat const* _left, glm::quat const* _right, float _right_weight, animation_sampler_data::E_interpolation_type _interpolation_type)
{
    float const theta = acosf(std::clamp(glm::dot(*_left, *_right), -1.0f, 1.0f));
    switch (_interpolation_type)
    {
    case animation_sampler_data::E_interpolation_type::CUBICSPLINE:
        //        break;
    case animation_sampler_data::E_interpolation_type::STEP:
        //        break;
    default:
        if (m_use_slerp && theta != 0)
            *_dest = (sinf((1 - _right_weight) * theta) * (*_left) + sin(_right_weight * theta) * (*_right)) / sin(theta);
        else
            *_dest = glm::normalize((1 - _right_weight) * (*_left) + (_right_weight) * (*_right));
        break;
    }
}

Engine::Graphics::animation_handle Component::SkeletonAnimator::GetAnimationHandle() const
{
    return GetManager().m_entity_anim_inst_map.at(m_owner).m_animation_handle;
}

void Component::SkeletonAnimator::SetAnimationHandle(animation_handle _animation)
{
    GetManager().set_instance_animation(m_owner, _animation);
}

void Component::SkeletonAnimator::SetAnimation(std::string _animationName)
{
    Engine::Graphics::animation_handle _handle = Singleton<ResourceManager>().FindNamedAnimation(_animationName);
    GetManager().set_instance_animation(m_owner, _handle);
}

bool Component::SkeletonAnimator::IsPaused() const
{
    return GetManager().m_entity_anim_inst_map.at(m_owner).m_paused;
}

void Component::SkeletonAnimator::SetPaused(bool _paused)
{
    GetManager().m_entity_anim_inst_map.at(m_owner).m_paused = _paused;
}
