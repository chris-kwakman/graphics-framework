#include "SkeletonAnimator.h"
#include "Renderable.h"
#include <ImGui/imgui_stdlib.h>
#include <ImGui/imgui_internal.h>

#include <array>
#include <Engine/Math/Transform3D.h>
#include <Engine/Utils/algorithm.h>
#include <Engine/Utils/logging.h>
#include <Engine/Managers/input.h>

#include <fstream>
#include <filesystem>

#include <Engine/Graphics/sdl_window.h>
#include <delaunator/delaunator.hpp>

namespace Component
{

    static std::filesystem::path const s_blendtree_dir = std::filesystem::path("data\\blend_trees");
    static char blend_tree_file_name_buffer[128];
    static bool s_open_blendmask_edit_popup = false;
    static animation_blend_mask* s_p_edit_blend_mask = nullptr;

///////////////////////////////////////////////////////////////////////////////
//                      Animation Data Methods
///////////////////////////////////////////////////////////////////////////////

    void animation_instance::set_anim_speed(float _speed)
    {
        m_anim_speed = _speed && ((~0) << 2);
    }

    /*
    * Applies given blend mask to this pose.
    * @param    animation_blend_mask        Mask to apply
    * @details  TODO: My understanding is probably wrong and should instead perhaps be
    * interpolating between the desired joint transform and the bind pose joint transform.
    */
    void animation_pose::apply_blend_mask(animation_blend_mask const & _mask, animation_pose const& _other)
    {
        using interp_type = Engine::Graphics::animation_sampler_data::E_interpolation_type;

        for (unsigned int i = 0; i < m_joint_transforms.size(); ++i)
        {
            bool use_left = _mask[i] >= 1.0f;
            m_joint_transforms[i] = use_left ? 
                m_joint_transforms[i] : 
                _other.m_joint_transforms[i];
        }
    }

    animation_pose& animation_pose::apply_blend_factor(float _factor, animation_pose const & _bind_pose)
    {
        using interp_type = Engine::Graphics::animation_sampler_data::E_interpolation_type;

        glm::vec3 const zero_vec(0.0f);
        glm::vec3 const one_vec(1.0f);
        glm::quat const identity_quat(zero_vec);
        float const inv_mask = 1.0f - _factor;
        for (unsigned int i = 0; i < m_joint_transforms.size(); ++i)
        {
            Engine::Math::transform3D& current_transform = m_joint_transforms[i];
            Engine::Math::transform3D const & bind_transform = _bind_pose.m_joint_transforms[i];
            AnimationUtil::interpolate_vector(
                &current_transform.position,
                &current_transform.position,
                &bind_transform.position,
                inv_mask,
                interp_type::LINEAR
            );
            AnimationUtil::interpolate_vector(
                &current_transform.scale,
                &current_transform.scale,
                &bind_transform.scale,
                inv_mask,
                interp_type::LINEAR
            );
            AnimationUtil::interpolate_quaternion(
                &current_transform.quaternion,
                &current_transform.quaternion,
                &bind_transform.quaternion,
                inv_mask,
                interp_type::LINEAR,
                false
            );
        }

        return *this;
    }

    animation_pose& animation_pose::mix(animation_pose const& _left, animation_pose const& _right, float _blend_param)
    {
        using interp_type = Engine::Graphics::animation_sampler_data::E_interpolation_type;
        if (_left.m_joint_transforms.size() == _right.m_joint_transforms.size())
            m_joint_transforms.resize(_left.m_joint_transforms.size());
        for (unsigned int i = 0; i < m_joint_transforms.size(); ++i)
        {
            Engine::Math::transform3D& current_transform = m_joint_transforms[i];
            AnimationUtil::interpolate_vector(
                &current_transform.position,
                &_left.m_joint_transforms[i].position,
                &_right.m_joint_transforms[i].position,
                _blend_param,
                interp_type::LINEAR
            );
            AnimationUtil::interpolate_vector(
                &current_transform.scale,
                &_left.m_joint_transforms[i].scale,
                &_right.m_joint_transforms[i].scale,
                _blend_param,
                interp_type::LINEAR
            );
            AnimationUtil::interpolate_quaternion(
                &current_transform.quaternion,
                &_left.m_joint_transforms[i].quaternion,
                &_right.m_joint_transforms[i].quaternion,
                _blend_param,
                interp_type::LINEAR,
                false
            );
        }
        return *this;
    }

    /*
    * Barycentric interpolation between three input poses using 3 input barycentric weights
    * @param    animation_pose const[3]     Pointer to array of 3 poses to interpolate between
    * @param    float const[3]              Pointer to array of 3 barycentric weights used for interpolation.
    * @returns  animation_pose &            
    */
    animation_pose& animation_pose::mix(animation_pose const _poses[3], float const _blend_params[3])
    {

        if (
            _poses[0].m_joint_transforms.size() == _poses[1].m_joint_transforms.size() &&
            _poses[1].m_joint_transforms.size() == _poses[2].m_joint_transforms.size()
            )
        {
            m_joint_transforms.resize(_poses[0].m_joint_transforms.size());
        }
        // Perform linear interpolation between all components of each joint transform of all input poses
        for (unsigned int i = 0; i < m_joint_transforms.size(); ++i)
        {
            Engine::Math::transform3D const& p0_transform = _poses[0].m_joint_transforms[i];
            Engine::Math::transform3D const& p1_transform = _poses[1].m_joint_transforms[i];
            Engine::Math::transform3D const& p2_transform = _poses[2].m_joint_transforms[i];
            // TODO: Apply blend masks
            m_joint_transforms[i].position =
                p0_transform.position * _blend_params[0] +
                p1_transform.position * _blend_params[1] +
                p2_transform.position * _blend_params[2];
            m_joint_transforms[i].scale =
                p0_transform.scale * _blend_params[0] +
                p1_transform.scale * _blend_params[1] +
                p2_transform.scale * _blend_params[2];
            m_joint_transforms[i].quaternion = glm::normalize(
                p0_transform.quaternion * _blend_params[0] +
                p1_transform.quaternion * _blend_params[1] +
                p2_transform.quaternion * _blend_params[2]
            );
        }
        return *this;
    }

    void animation_leaf_node::set_animation(animation_handle _animation)
    {
        m_animation = _animation;
        animation_data const* anim_data = Singleton<ResourceManager>().FindAnimationData(m_animation);
    }

    void animation_leaf_node::compute_pose(
        float _time, 
        animation_pose* _out_pose, 
        compute_pose_context const & _context
    ) const
    {
        animation_data const * anim_data = Singleton<Engine::Graphics::ResourceManager>()
                                            .FindAnimationData(m_animation);

        float anim_channel_data[1024];
        _out_pose->m_joint_transforms.resize(_context.m_bind_pose->m_joint_transforms.size());
        // Use animation to compute new pose.
        if (anim_data)
        {
            AnimationUtil::compute_animation_channel_data(
                anim_data,
                _time,
                anim_channel_data,
                true
            );
            AnimationUtil::convert_joint_channels_to_transforms(
                anim_data,
                anim_channel_data,
                &_out_pose->m_joint_transforms.front(),
                (unsigned int)_out_pose->m_joint_transforms.size()
            );
        }
        // Use bind pose as fallback pose.
        else
        {
            *_out_pose = *_context.m_bind_pose;
        }
    }

    /*
    * @returns  float   Original duration of animation set in this leaf node.
    */
    float animation_leaf_node::duration() const
    {
        if (m_animation == 0) return 0.0f;
        animation_data const* data = Singleton<ResourceManager>().FindAnimationData(m_animation);
        return data ? data->m_duration : 0.0f;
    }

    /*
    * @param    std::unique_ptr<animation_tree_node> &&     Blend tree node to add to this node
    * @param    float                                       Point within 1D blend space
    * @param    animation_blend_mask                        Blend mask to use. If left emtpy, all joints are used.
    */
    void animation_blend_1D::add_node(std::unique_ptr<animation_tree_node> && _node, float _range_start, animation_blend_mask _blend_mask)
    {
        m_child_blend_nodes.emplace_back(std::move(_node));
        m_blendspace_points.emplace_back(_range_start);
        m_time_warps.emplace_back(1.0f);

        edit_node_blendspace_point((unsigned int)m_child_blend_nodes.size() - 1, _range_start);
    }

    /*
    * Set point in blendspace of edited node. Will re-sort internal list(s) of nodes.
    * @param    unsigned int        Index to edit.
    * @param    float               Point in blendspace to set edited node index to.
    */
    void animation_blend_1D::edit_node_blendspace_point(unsigned int _index, float _point)
    {
        auto swap_indices = [&](unsigned int idx1, unsigned int idx2)
        {
            std::swap(m_child_blend_nodes[idx1], m_child_blend_nodes[idx2]);
            std::swap(m_blendspace_points[idx1], m_blendspace_points[idx2]);
            std::swap(m_time_warps[idx1], m_time_warps[idx2]);
        };

        m_blendspace_points[_index] = _point;
        while (true)
        {
            if ((_index != m_blendspace_points.size() - 1) && _point > m_blendspace_points[_index + 1])
            {
                swap_indices(_index, _index + 1);
                ++_index;
            }
            else if (_index != 0 && _point < m_blendspace_points[_index - 1])
            {
                swap_indices(_index, _index - 1);
                --_index;
            }
            else
                break;
        }
    }

    /*
    * Remove node at index from internal list
    * @param    unsigned int    Node to remove
    */
    void animation_blend_1D::remove_node(unsigned int _index)
    {
        m_child_blend_nodes.erase(m_child_blend_nodes.begin() + _index);
        m_blendspace_points.erase(m_blendspace_points.begin() + _index);
        m_time_warps.erase(m_time_warps.begin() + _index);
    }

    float animation_blend_1D::node_warped_duration(unsigned int _index) const
    {
        return m_child_blend_nodes[_index]->duration() * m_time_warps[_index];
    }

    /*
    * @param    float               Time at which to blend contained poses.
    * @param    animation_pose *    Pointer to output pose.
    */
    void animation_blend_1D::compute_pose(float _time, animation_pose* _out_pose, compute_pose_context const& _context) const
    {
        // Return early if we have no valid nodes to interpolate between
        if (m_child_blend_nodes.empty())
        {
            *_out_pose = *_context.m_bind_pose;
            return;
        }

        auto & res_mgr = Singleton<ResourceManager>();
        auto [bound_left, bound_right] = Engine::Utils::float_binary_search(
            &m_blendspace_points.front(), m_blendspace_points.size(), m_blend_parameter
        );
        int bounds[2] = { bound_left, bound_right };

        if (bound_left == bound_right)
        {
            m_child_blend_nodes[bound_left]->compute_pose(_time, _out_pose, _context);
            return;
        }

        bool apply_mask = !m_blend_mask.m_joint_blend_masks.empty();

        animation_pose bound_poses[2]{
            animation_pose(),
            animation_pose()
        };
        m_child_blend_nodes[bound_left]->compute_pose(
            AnimationUtil::rollover_modulus(_time * m_time_warps[bound_left], node_warped_duration(bound_left)),
            &bound_poses[0],
            _context
        );
        m_child_blend_nodes[bound_right]->compute_pose(
            AnimationUtil::rollover_modulus(_time * m_time_warps[bound_right], node_warped_duration(bound_right)),
            &bound_poses[1],
            _context
        );
        // Blend smoothly between bounding nodes.
        if (!apply_mask)
        {

            // Return early if we are not able to find two valid poses to interpolate between.
            if (bound_poses[0].m_joint_transforms.empty() || bound_poses[1].m_joint_transforms.empty())
            {
                *_out_pose = *_context.m_bind_pose;
                return;
            }

            float clamped_blend_param = glm::clamp(m_blend_parameter, m_blendspace_points.front(), m_blendspace_points.back());
            float segment_blend_parameter = (clamped_blend_param - m_blendspace_points[bound_left]) / (m_blendspace_points[bound_right] - m_blendspace_points[bound_left]);
            _out_pose->mix(bound_poses[0], bound_poses[1], segment_blend_parameter);
        }
        // No blending, pick and choose nodes to use for animation.
        else
        {
            *_out_pose = bound_poses[0];
            _out_pose->apply_blend_mask(
                m_blend_mask,
                bound_poses[1]
            );
        }

    }

    float animation_blend_1D::duration() const
    {
        float max_duration = 0.0f;
        for (unsigned int i = 0; i < m_child_blend_nodes.size(); ++i)
            max_duration = std::max(max_duration, node_warped_duration(i));
        return max_duration;
    }

    void animation_blend_2D::add_node(std::unique_ptr<animation_tree_node>&& _node, glm::vec2 _new_point, animation_blend_mask _blend_mask)
    {
        m_child_blend_nodes.emplace_back(std::move(_node));
        m_blendspace_points.emplace_back(_new_point);
        m_time_warps.emplace_back(1.0f);
        triangulate_blendspace_points();
    }

    void animation_blend_2D::edit_node_blendspace_point(unsigned int _index, glm::vec2 _new_point)
    {
        m_blendspace_points[_index] = _new_point;
        triangulate_blendspace_points();
    }

    void animation_blend_2D::remove_node(unsigned int _index)
    {
        m_child_blend_nodes.erase(m_child_blend_nodes.begin() + _index);
        m_blendspace_points.erase(m_blendspace_points.begin() + _index);
        triangulate_blendspace_points();
    }

    float animation_blend_2D::node_warped_duration(unsigned int _index) const
    {
        return m_child_blend_nodes[_index]->duration() * m_time_warps[_index];
    }

    /*
    * Runs delaunator library on node data
    */
    void animation_blend_2D::triangulate_blendspace_points()
    {
        using namespace delaunator;
        m_triangles.clear();
        if (m_blendspace_points.size() < 3)
            return;
        // Could rewrite parts of Delaunator lib to only use floating points, but 
        // I'm too lazy to do that.
        std::vector<double> double_coordinates(m_blendspace_points.size() * 2);
        for (unsigned int i = 0; i < m_blendspace_points.size(); ++i)
        {
            double_coordinates[2u * i] = m_blendspace_points[i].x;
            double_coordinates[2u * i + 1u] = m_blendspace_points[i].y;
        }
        try
        {
            Delaunator my_delaunator(double_coordinates);
            // Load triangle indices into internal node data.
            m_triangles.resize(my_delaunator.triangles.size()/3);
            for (unsigned int i = 0; i < m_triangles.size(); ++i)
            {
                m_triangles[i] = triangle(
                    (uint8_t)my_delaunator.triangles[3u * i],
                    (uint8_t)my_delaunator.triangles[3u * i + 1u],
                    (uint8_t)my_delaunator.triangles[3u * i + 2u]
                );
            }
        }
        catch (std::runtime_error& e)
        {
            Engine::Utils::print_error("[animator_blend_2D] Error occurred when applying delaunator: %s.", e.what());
        }
    }

    /*
    * Finds triangle that contains input blend parameter
    * @param    glm::vec2               2D blend parameter
    * @returns  find_tri_result         Pair of triangle of node indices and barycentric coordinates.
    *                                   If no triangle was found, return max index on triangle.
    */
    animation_blend_2D::find_tri_result animation_blend_2D::find_triangle(glm::vec2 _blend_param) const
    {
        auto determinant = [](glm::vec2 const& l, glm::vec2 const& r)->float
        {
            return l.x * r.y - l.y * r.x;
        };

        for (unsigned int i = 0; i < m_triangles.size(); ++i)
        {
            triangle const& test_tri = m_triangles[i];
            glm::vec2 const verts[3] = {
                m_blendspace_points[std::get<0>(test_tri)],
                m_blendspace_points[std::get<1>(test_tri)],
                m_blendspace_points[std::get<2>(test_tri)]
            };

            float const tri_determinant = determinant(verts[1] - verts[0], verts[2] - verts[0]);
            float u = (determinant(_blend_param, verts[2] - verts[0]) - determinant(verts[0], verts[2] - verts[0])) / tri_determinant;
            float v = -(determinant(_blend_param, verts[1] - verts[0]) - determinant(verts[0], verts[1] - verts[0])) / tri_determinant;

            if (u >= 0.0f && v >= 0.0f && u + v >= 0.0f && u + v <= 1.0f)
            {
                return { test_tri, glm::vec2(u,v) };
            }
        }
        return { triangle(255,255,255), glm::vec2(-1.0f, -1.0f) };
    }

    void animation_blend_2D::compute_pose(float _time, animation_pose* _out_pose, compute_pose_context const& _context) const
    {
        // Return early if we have no valid nodes to interpolate between
        if (m_child_blend_nodes.empty())
        {
            *_out_pose = *_context.m_bind_pose;
            return;
        }

        auto& res_mgr = Singleton<ResourceManager>();
        
        auto find_tri_result = find_triangle(m_blend_parameter);

        animation_pose bound_poses[3];

        uint8_t const tri_node_indices[3]{ 
            std::get<0>(find_tri_result.first),
            std::get<1>(find_tri_result.first),
            std::get<2>(find_tri_result.first)
        };

        if (
            tri_node_indices[0] == tri_node_indices[1] &&
            tri_node_indices[1] == tri_node_indices[2] &&
            tri_node_indices[0] == std::numeric_limits<uint8_t>::max()
            )
        {
            *_out_pose = *_context.m_bind_pose;
            return;
        }

        for (unsigned int i = 0; i < 3; ++i)
        {
            m_child_blend_nodes[tri_node_indices[i]]->compute_pose(
                AnimationUtil::rollover_modulus(_time * m_time_warps[i], node_warped_duration(tri_node_indices[i])),
                &bound_poses[i],
                _context
            );
            // Early return if we failed to animate pose.
            // This happens when the node does not have a valid animation.
            if (bound_poses[i].m_joint_transforms.empty())
            {
                *_out_pose = *_context.m_bind_pose;
            }
        }

        
        float const blend_v = find_tri_result.second.x;
        float const blend_w = find_tri_result.second.y;
        float const blend_u = 1.0f - blend_v - blend_w;
        float const blend_values[3] = { blend_u, blend_v, blend_w };
        _out_pose->mix(bound_poses, blend_values);
    }

    float animation_blend_2D::duration() const
    {
        float max_duration = 0.0f;
        for (unsigned int i = 0; i < m_child_blend_nodes.size(); ++i)
            max_duration = std::max(max_duration, node_warped_duration(i));
        return max_duration;
    }



    static bool gui_dragdrop_animation_handle(animation_handle* _anim)
    {
        auto& res_mgr = Singleton<ResourceManager>();

        std::string anim_handle_label = "Bind Pose";
        if (_anim && *_anim)
        {
            animation_data const * data = res_mgr.FindAnimationData(*_anim);
            if (!data)
                anim_handle_label = "Invalid Animation";
            else
            {
                anim_handle_label = data->m_name;
            }
        }

        ImGui::PushStyleColor(ImGuiCol_Button, glm::vec4(0.8f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        if (ImGui::SmallButton("X"))
            *_anim = 0;
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            const char* display_button_str = "Reset to bind pose";
            ImGui::Text(display_button_str);
            anim_handle_label = "Bind Pose";
            ImGui::EndTooltip();
            return true;
        }
        ImGui::SameLine();

        ImGui::InputText("Animation", &anim_handle_label, ImGuiInputTextFlags_ReadOnly);
        if (ImGui::BeginDragDropTarget())
        {
            if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload("RESOURCE_ANIMATION"))
                *_anim = *reinterpret_cast<animation_handle*>(payload->Data);
            ImGui::EndDragDropTarget();
            return *_anim != 0 && Singleton<ResourceManager>().FindAnimationData(*_anim) != nullptr;
        }
        return false;
    }

    enum ECreatableNodeFlags { eNone = 0, eLeafNode = 1, eBlendNode1D = 2, eBlendNode2D = 4 };
    ECreatableNodeFlags operator|(ECreatableNodeFlags a, ECreatableNodeFlags b)
    {
        return static_cast<ECreatableNodeFlags>((int)a | (int)b);
    }

    static std::unique_ptr<animation_tree_node> show_node_creation_menu(ECreatableNodeFlags _flags)
    {
        std::unique_ptr<animation_tree_node> output{ nullptr };
        if (ImGui::BeginMenu("Add Blend node"))
        {
            if (_flags & ECreatableNodeFlags::eLeafNode && ImGui::MenuItem("Leaf Blend node"))
            {
                output = std::make_unique<animation_leaf_node>();
            }
            if (_flags & ECreatableNodeFlags::eBlendNode1D && ImGui::MenuItem("1D Blend Node"))
            {
                output = std::make_unique<animation_blend_1D>();
            }
            if (_flags & ECreatableNodeFlags::eBlendNode2D && ImGui::MenuItem("2D Blend Node"))
            {
                output = std::make_unique<animation_blend_2D>();
            }
            ImGui::EndMenu();
        }
        return output;
    }

    int animation_tree_node::gui_node(gui_node_context& _context)
    {
        _context.node_index++;
        int result = impl_gui_node(_context);
        return result;
    }

    static ImGuiTreeNodeFlags s_default_treenode_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    int animation_leaf_node::impl_gui_node(gui_node_context& _context)
    {
        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Bullet | s_default_treenode_flags;
        if (*_context.selected_node_ptr == this)
            node_flags |= ImGuiTreeNodeFlags_Selected;
        ImGui::PushID(_context.node_index);
        bool is_open = ImGui::TreeNodeEx(m_name.c_str(), node_flags);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            *_context.selected_node_ptr = this;
        int return_value = 0;
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Destroy Node"))
                return_value = -1;
            ImGui::EndPopup();
        }
        if (is_open)
        {
            ImGui::TreePop();
        }
        ImGui::PopID();
        return return_value;
    }

    void animation_leaf_node::gui_edit()
    {
        if (gui_dragdrop_animation_handle(&m_animation))
        {
            if (m_animation != 0)
                m_name = Singleton<ResourceManager>().FindAnimationData(m_animation)->m_name;
            else
                m_name = "Bind Pose";
        }
    }


    int animation_blend_1D::impl_gui_node(gui_node_context & _context)
    {
        ImGuiTreeNodeFlags node_flags = s_default_treenode_flags;
        if (*_context.selected_node_ptr == this)
            node_flags |= ImGuiTreeNodeFlags_Selected;
        ImGui::PushID(_context.node_index);
        bool is_open = ImGui::TreeNodeEx(m_name.c_str(), node_flags);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            *_context.selected_node_ptr = this;
        int return_value = 0;
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Destroy Node"))
                return_value = -1;

            auto result = show_node_creation_menu(
                ECreatableNodeFlags::eBlendNode1D |
                ECreatableNodeFlags::eBlendNode2D |
                ECreatableNodeFlags::eLeafNode
            );
            if (result)
            {
                float range_start = 0.0f;
                if (!m_blendspace_points.empty())
                    range_start = m_blendspace_points.back() + 1;
                add_node(std::move(result), range_start, animation_blend_mask());
                m_blend_parameter = glm::clamp(m_blend_parameter, m_blendspace_points.front(), m_blendspace_points.back());
            }
            ImGui::EndPopup();
        }
        if (is_open)
        {
            for (int i = 0; i < m_child_blend_nodes.size(); ++i)
            {
                int result = m_child_blend_nodes[i]->gui_node(_context);
                if (result == -1)
                {
                    if (*_context.selected_node_ptr == m_child_blend_nodes[i].get())
                        *_context.selected_node_ptr = nullptr;
                    remove_node(i);
                    i -= 1;
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();

        return return_value;
    }

    void animation_blend_1D::gui_edit()
    {
        float slider_left = -1.0f;
        float slider_right = 1.0f;
        if (!m_blendspace_points.empty())
        {
            slider_left = m_blendspace_points.front();
            slider_right = m_blendspace_points.back();
        }
        ImGui::SliderFloat("Blend Parameter", &m_blend_parameter, slider_left, slider_right, "%.2f", ImGuiSliderFlags_AlwaysClamp);

        float avail_width = ImGui::GetContentRegionAvailWidth();
        ImGui::SetNextItemWidth(avail_width * 0.9f);
        ImGui::Indent(avail_width * 0.05f);
        if (ImGui::Button("Edit Blend Mask"))
        {
            s_p_edit_blend_mask = &m_blend_mask;
            s_open_blendmask_edit_popup = true;
        }

        //if (ImGui::Button("Scale Time Warp to Least Common Multiple of durations"))
        //{
        //    std::vector<float> child_durations(m_child_blend_nodes.size());
        //    for (unsigned int i = 0; i < child_durations.size(); ++i)
        //        child_durations[i] = m_child_blend_nodes[i]->duration();
        //    float const lcm_duration = AnimationUtil::find_array_lcm(&child_durations.front(), child_durations.size());
        //    for (unsigned int i = 0; i < m_child_blend_nodes.size(); ++i)
        //    {
        //        m_time_warps[i] = lcm_duration / child_durations[i];
        //    }
        //}

        if (ImGui::BeginTable("Nodes", 4, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide);
            ImGui::TableSetupColumn("Blend Point", ImGuiTableColumnFlags_WidthFixed );
            ImGui::TableSetupColumn("Time Warp", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (unsigned int i = 0; i < m_child_blend_nodes.size(); ++i)
            {
                ImGui::PushID(i);
                float new_blendspace_point = m_blendspace_points[i];

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%u", i);
                ImGui::TableNextColumn();
                bool edit_blend_point = ImGui::InputFloat("###Blend Point", &new_blendspace_point, 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::TableNextColumn();
                ImGui::DragFloat("###Time Warp", &m_time_warps[i], 0.1f, 0.0f, FLT_MAX, "%.2f");
                ImGui::TableNextColumn();
                ImGui::Text("(W) %.2f / (A) %.2f", node_warped_duration(i), m_child_blend_nodes[i]->duration());

                if (edit_blend_point)
                    edit_node_blendspace_point(i, new_blendspace_point);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }




    int animation_blend_2D::impl_gui_node(gui_node_context& _context)
    {
        ImGuiTreeNodeFlags node_flags = s_default_treenode_flags;
        if (*_context.selected_node_ptr == this)
            node_flags |= ImGuiTreeNodeFlags_Selected;
        ImGui::PushID(_context.node_index);
        bool is_open = ImGui::TreeNodeEx(m_name.c_str(), node_flags);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            *_context.selected_node_ptr = this;
        int return_value = 0;
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Destroy Node"))
                return_value = -1;

            auto result = show_node_creation_menu(
                ECreatableNodeFlags::eBlendNode1D |
                ECreatableNodeFlags::eBlendNode2D |
                ECreatableNodeFlags::eLeafNode
            );
            if (result)
            {
                add_node(std::move(result), glm::vec2(0.0f, 0.0f), animation_blend_mask());
            }
            ImGui::EndPopup();
        }
        if (is_open)
        {
            for (int i = 0; i < m_child_blend_nodes.size(); ++i)
            {
                int result = m_child_blend_nodes[i]->gui_node(_context);
                if (result == -1)
                {
                    if (*_context.selected_node_ptr == m_child_blend_nodes[i].get())
                        *_context.selected_node_ptr = nullptr;
                    remove_node(i);
                    i -= 1;
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();

        return return_value;
    }

    void animation_blend_2D::gui_edit()
    {
        glm::vec2 range_min = glm::vec2(FLT_MAX), range_max = glm::vec2(-FLT_MAX);
        if (!m_blendspace_points.empty())
        {
            for (glm::vec2 point : m_blendspace_points)
            {
                range_min = glm::min(range_min, point);
                range_max = glm::max(range_max, point);
            }
        }
        else
        {
            range_min = glm::vec2(-1.0f);
            range_max = glm::vec2(1.0f);
        }

        bool blend_parameter_edited = false;

        ImVec2 child_size = ImGui::GetWindowSize();
        child_size.y *= 0.3f;
        if (ImGui::IsWindowAppearing())
            ImGui::SetNextWindowSize(child_size, ImGuiCond_FirstUseEver);
        if (ImGui::BeginChild("Edit Blend Parameter", ImVec2(0.0f, child_size.y), false))
        {
            blend_parameter_edited |= ImGui::VSliderFloat(
                "###BlendParam_Y",
                ImVec2(child_size.x * 0.05f, child_size.y - 2.0f * ImGui::GetTextLineHeightWithSpacing()),
                &m_blend_parameter.y, range_min.y, range_max.y, "%.2f",
                ImGuiSliderFlags_AlwaysClamp
            );
            ImGui::SameLine();
            float cursor_x_frame = 0.0f;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, glm::vec2(20.0f));
            if (ImGui::BeginChild("###Child Node Position Editor", ImVec2(0, child_size.y - 2.0f * ImGui::GetTextLineHeightWithSpacing()), true))
            {

                ImVec2 const topleft_corner_screenpos = ImGui::GetCursorScreenPos();
                ImVec2 const frame_size = ImGui::GetContentRegionAvail();
                ImDrawList* drawlist = ImGui::GetWindowDrawList();
                cursor_x_frame = ImGui::GetCursorScreenPos().x;

                float const point_radius = 4.0f;

                auto convert_point = [&](glm::vec2 _blendspace_point)->glm::vec2
                {
                    glm::vec2 normalized_pos = (_blendspace_point - range_min) / (range_max - range_min);
                    normalized_pos.y = 1.0f - normalized_pos.y;
                    return glm::vec2(topleft_corner_screenpos) + glm::vec2(frame_size) * normalized_pos;
                };

                auto draw_point = [&](glm::vec2 _blendspace_point, unsigned int _color)
                {
                    glm::vec2 const center = convert_point(_blendspace_point);
                    drawlist->AddCircle(center, point_radius, _color, 20, 1.0f);
                };

                // Draw Triangles
                auto const found_tri = find_triangle(m_blend_parameter);
                for (unsigned int i = 0; i < m_triangles.size(); ++i)
                {
                    triangle const tri = m_triangles[i];
                    glm::vec2 points[3] = {
                        m_blendspace_points[std::get<0>(tri)],
                        m_blendspace_points[std::get<1>(tri)],
                        m_blendspace_points[std::get<2>(tri)],
                    };

                    unsigned int color = 0x40FFFFFF;
                    if (found_tri.first == tri)
                        color = 0xA000FFFF;

                    for (unsigned int j = 0; j < 3; ++j)
                        points[j] = convert_point(points[j]);
                    drawlist->AddTriangleFilled(
                        points[0], points[1], points[2], color
                    );
                    drawlist->AddTriangle(
                        points[0], points[1], points[2], 0xA0FFFFFF, 2.0f
                    );
                }

                draw_point(m_blend_parameter, 0xFF00FF00);
                // Display child nodes as positions within frame
                for (unsigned int i = 0; i < m_child_blend_nodes.size(); ++i)
                {
                    glm::vec2 const blendspace_point = m_blendspace_points[i];
                    glm::vec2 const draw_at_point = convert_point(blendspace_point);
                    draw_point(blendspace_point, 0xFF0000FF);
                    std::string const& draw_text = m_child_blend_nodes[i]->m_name;
                    glm::vec2 const text_size = ImGui::CalcTextSize(draw_text.c_str());
                    drawlist->AddText(
                        draw_at_point + glm::vec2(0.0f, 0.5f) * point_radius + glm::vec2(-text_size.x, text_size.y) * 0.5f,
                        0xAAFFFFFF,
                        draw_text.c_str()
                    );
                }
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::CalcItemWidth();
            ImGui::SetCursorPosX( cursor_x_frame- ImGui::GetCurrentWindow()->Pos.x);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            blend_parameter_edited |= ImGui::SliderFloat(
                "###BlendParam_X",
                &m_blend_parameter.x, range_min.x, range_max.x, "%.2f",
                ImGuiSliderFlags_AlwaysClamp
            );
        }
        ImGui::EndChild();

        //if (ImGui::Button("Scale Time Warp to Least Common Multiple of durations"))
        //{
        //    std::vector<float> child_durations(m_child_blend_nodes.size());
        //    for (unsigned int i = 0; i < child_durations.size(); ++i)
        //        child_durations[i] = m_child_blend_nodes[i]->duration();
        //    float const lcm_duration = AnimationUtil::find_array_lcm(&child_durations.front(), child_durations.size());
        //    for (unsigned int i = 0; i < m_child_blend_nodes.size(); ++i)
        //    {
        //        m_time_warps[i] = lcm_duration / child_durations[i];
        //    }
        //}

        if (ImGui::BeginTable("Nodes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide);
            ImGui::TableSetupColumn("Blend Point", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Time Warp", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (unsigned int i = 0; i < m_child_blend_nodes.size(); ++i)
            {
                ImGui::PushID(i);
                glm::vec2 new_blendspace_point = m_blendspace_points[i];

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%u", i);
                ImGui::TableNextColumn();
                bool edit_blend_point = ImGui::DragFloat2("###Blend Point", &new_blendspace_point.x, 0.0f, -10.0f, 10.0f, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::TableNextColumn();
                ImGui::DragFloat("###Time Warp", &m_time_warps[i], 0.1f, 0.0f, FLT_MAX, "%.2f");
                ImGui::TableNextColumn();
                ImGui::Text("(W) %.2f / (A) %.2f", node_warped_duration(i), m_child_blend_nodes[i]->duration());
                
                if (edit_blend_point)
                    edit_node_blendspace_point(i, new_blendspace_point);

                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    /*
    *           Serialization / Deserialization
    */


    std::unique_ptr<animation_tree_node> animation_tree_node::create(nlohmann::json const& _j)
    {
        std::unique_ptr<animation_tree_node> new_node;
        std::string const node_type = _j["type"];
        if (node_type == "leaf")
            new_node = std::make_unique<animation_leaf_node>();
        else if (node_type == "1D")
            new_node = std::make_unique<animation_blend_1D>();
        else if (node_type == "2D")
            new_node = std::make_unique<animation_blend_2D>();
        else
            Engine::Utils::assert_print_error(false, "Invalid type \"%s\" when deserializing JSON", node_type);
        new_node->deserialize(_j);
        return std::move(new_node);
    }



    nlohmann::json animation_tree_node::serialize() const
    {
        nlohmann::json j;
        j["name"] = m_name;
        j.update(impl_serialize());
        return j;
    }

    void animation_tree_node::deserialize(nlohmann::json const& _json)
    {
        m_name = _json.value("name", default_name());

        impl_deserialize(_json);
    }


    nlohmann::json animation_leaf_node::impl_serialize() const
    {
        nlohmann::json j;
        j["type"] = "leaf";
        Engine::Graphics::to_json(j["animation"], m_animation);
        return j;
    }

    void animation_leaf_node::impl_deserialize(nlohmann::json const& _json)
    {
        Engine::Graphics::from_json(_json["animation"], m_animation);
    }


    nlohmann::json animation_blend_1D::impl_serialize() const
    {
        nlohmann::json j;
        j["type"] = "1D";
        j["blend_parameter"] = m_blend_parameter;
        j["child_blendspace_points"] = m_blendspace_points;
        j["child_time_warps"] = m_time_warps;
        j["blend_mask"] = m_blend_mask;
        if (!m_child_blend_nodes.empty())
        {
            nlohmann::json & child_nodes = j["child_nodes"];
            for (unsigned int i = 0; i < m_child_blend_nodes.size(); ++i)
                child_nodes.push_back(m_child_blend_nodes[i]->serialize());
        }
        return j;
    }

    void animation_blend_1D::impl_deserialize(nlohmann::json const& _json)
    {
        m_blend_parameter = _json["blend_parameter"];
        m_blendspace_points = _json["child_blendspace_points"].get<decltype(m_blendspace_points)>();
        m_time_warps = _json.value("child_time_warps", decltype(m_time_warps)(m_blendspace_points.size(), 1.0f));
        m_blend_mask = _json.value("blend_mask", animation_blend_mask());
        auto child_node_arr_iter = _json.find("child_nodes");
        if (child_node_arr_iter != _json.end())
        {
            for (nlohmann::json const& child_j : *child_node_arr_iter)
            {
                m_child_blend_nodes.emplace_back(
                    std::move(animation_tree_node::create(child_j))
                );
            }
        }
    }

    nlohmann::json animation_blend_2D::impl_serialize() const
    {
        nlohmann::json j;
        j["type"] = "2D";
        j["blend_parameter"] = m_blend_parameter;
        j["child_blendspace_points"] = m_blendspace_points;
        j["child_time_warps"] = m_time_warps;
        if (!m_child_blend_nodes.empty())
        {
            nlohmann::json& child_nodes = j["child_nodes"];
            for (unsigned int i = 0; i < m_child_blend_nodes.size(); ++i)
                child_nodes.push_back(m_child_blend_nodes[i]->serialize());
        }
        return j;
    }

    void animation_blend_2D::impl_deserialize(nlohmann::json const& _json)
    {
        m_blend_parameter = _json["blend_parameter"];
        m_blendspace_points = _json["child_blendspace_points"].get<decltype(m_blendspace_points)>();
        m_time_warps = _json.value("child_time_warps", decltype(m_time_warps)(m_blendspace_points.size(), 1.0f));
        auto child_node_arr_iter = _json.find("child_nodes");
        if (child_node_arr_iter != _json.end())
        {
            for (nlohmann::json const& child_j : *child_node_arr_iter)
            {
                m_child_blend_nodes.emplace_back(
                    std::move(animation_tree_node::create(child_j))
                );
            }
        }
        triangulate_blendspace_points();
    }


///////////////////////////////////////////////////////////////////////////////
//                     Animation Utility Functions
///////////////////////////////////////////////////////////////////////////////


namespace AnimationUtil
{
    /*
    * Apply modulus to value within [0,_maximum]. Works with negative numbers
    * @param    float   Value to apply operation to
    * @param    float   Maximum value of range
    * @returns  float   
    */
    float rollover_modulus(float _value, float _maximum)
    {
        float mod_time = fmodf(_value, _maximum);
        return mod_time + (mod_time < 0 ? _maximum : 0);
    }

    float fgcd(float l, float r)
    {
        return r > 0.01f ? fgcd(r, fmod(l, r)) : l;
    }

    float find_array_fgcd(float const* _values, unsigned int _count)
    {
        if (!(_values && _count > 0))
            return 1.0f;
        if (_count == 1)
            return _values[0];
        float gcd = _values[0];
        for (unsigned int i = 1; i < _count; ++i)
        {
            gcd = fgcd(gcd, _values[i]);
        }
        return gcd;
    }

    float find_array_lcm(float const* _values, unsigned int _count)
    {
        if (_count > 2)
        {
            float sub_lcm = find_array_lcm(_values + 1, _count - 1);
            return _values[0] * sub_lcm / fgcd(_values[0], find_array_fgcd(_values+1, _count-1));
        }
        else if (_count == 2)
        {
            return abs(_values[0] * _values[1]) / fgcd(_values[0], _values[1]);
        }
        else if (_count == 1)
            return _values[0];
        else
            return 0.0f;
    }

    void convert_joint_channels_to_transforms(
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
            uint8_t const curr_joint_channel_count = _animation_data->get_skeleton_joint_index_channel_count(curr_joint_idx);
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
                new_joint_channel_idx += _animation_data->get_skeleton_joint_index_channel_count(skeleton_joint_idx);
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

    for (auto & pair : m_entity_animator_data)
    {
        Entity animator_entity = pair.first;

        animator_data & animator = pair.second;

        // Skip if handle is invalid.
        if (animator.m_blendtree_root_node == nullptr || animator.m_instance.m_paused)
            continue;

        Component::Skin skin_comp = animator_entity.GetComponent<Component::Skin>();
        assert(skin_comp.IsValid() && "Animator entity does not have valid Skin component");
        std::vector<Component::Transform> const & skeleton_joint_transforms = skin_comp.GetSkeletonInstanceNodes();
        if (skeleton_joint_transforms.empty())
            continue;

        compute_pose_context context;
        context.m_bind_pose = &animator.m_bind_pose;

        animation_pose new_pose;
        animator.m_blendtree_root_node->compute_pose(
            animator.m_instance.m_global_time, 
            &new_pose,
            context
        );

        // Check if we were able to compute new pose.
        if (!new_pose.m_joint_transforms.empty())
        {
            // Update transform component data for each joint
            update_joint_transform_components(
                &skeleton_joint_transforms.front(), 
                &new_pose.m_joint_transforms.front(),
                (unsigned int)new_pose.m_joint_transforms.size()
            );
        }

        // Rollover time between [0, duration]
        float const animator_duration = animator.m_blendtree_root_node->duration();
        animation_instance& instance = animator.m_instance;
        instance.m_global_time += _dt * instance.m_anim_speed;
        float new_time = AnimationUtil::rollover_modulus(instance.m_global_time, animator_duration);
        instance.m_paused = (!instance.m_loop && new_time != instance.m_global_time);
        instance.m_global_time = (float)(!instance.m_paused) * new_time + (float)(instance.m_paused) * animator_duration;
    }
}

void SkeletonAnimatorManager::impl_clear()
{
    m_entity_animator_data.clear();
}

bool SkeletonAnimatorManager::impl_create(Entity _e)
{
    animator_data animator;
    animator.m_instance.m_anim_speed = 1.0f;
    animator.m_instance.m_global_time = 0.0f;
    animator.m_instance.m_paused = true;
    animator.m_instance.m_loop = true;
    m_entity_animator_data.emplace(_e, std::move(animator));
    return true;
}

void SkeletonAnimatorManager::impl_destroy(Entity const* _entities, unsigned int _count)
{
    for (unsigned int i = 0; i < _count; ++i)
        m_entity_animator_data.erase(_entities[i]);

}

bool SkeletonAnimatorManager::impl_component_owned_by_entity(Entity _entity) const
{
    return m_entity_animator_data.find(_entity) != m_entity_animator_data.end();
}

void SkeletonAnimatorManager::impl_edit_component(Entity _entity)
{
    animator_data & animator = get_entity_animator(_entity);
    auto & res_mgr = Singleton<ResourceManager>();

    animation_instance& instance = animator.m_instance;
    bool    is_paused = instance.m_paused,
            is_looping = instance.m_loop;
    float   timer = animator.m_instance.m_global_time;

    if (ImGui::Checkbox("Paused", &is_paused))
        instance.m_paused = is_paused;
    if (ImGui::Checkbox("Loop", &is_looping))
        instance.m_loop = is_looping;

    float animation_duration = 0.0f;
    if(animator.m_blendtree_root_node)
        animation_duration = animator.m_blendtree_root_node->duration();

    char format_buffer[64];
    snprintf(format_buffer, sizeof(format_buffer), "%s / %.2f", "%.2f", animation_duration);
    if (ImGui::SliderFloat("Time", &timer, 0.0f, animation_duration, format_buffer))
        instance.m_global_time = timer;
    float anim_speed = instance.m_anim_speed;
    if (ImGui::SliderFloat("Animation Speed", &anim_speed, 0.001f, 10.0f, "%.2f"))
        instance.m_anim_speed = anim_speed;

    ImGui::Checkbox("Use SLERP", &m_use_slerp);

    if (ImGui::Button("Edit Blend Tree"))
    {
        m_edit_tree = _entity;
        m_blendtree_editor_open = true;
    }
    if (m_blendtree_editor_open && !m_edit_tree.Alive())
        m_blendtree_editor_open = false;
    if (m_blendtree_editor_open)
    {
        if (ImGui::Begin("Blend Tree Editor", &m_blendtree_editor_open, ImGuiWindowFlags_MenuBar))
        {
            window_edit_blendtree(animator.m_blendtree_root_node);
        }
        ImGui::End();
    }
}

void SkeletonAnimatorManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
{
}

SkeletonAnimatorManager::animator_data& SkeletonAnimatorManager::get_entity_animator(Entity _e)
{
    return m_entity_animator_data.at(_e);
}

void SkeletonAnimatorManager::update_joint_transform_components(
    Component::Transform const * _components, 
    Engine::Math::transform3D const* _transforms, 
    unsigned int _joint_count
)
{
    for (unsigned int i = 0; i < _joint_count; ++i)
    {
        Component::Transform edit_transform = _components[i];
        edit_transform.SetLocalTransform(_transforms[i]);
    }
}

void SkeletonAnimatorManager::window_edit_blendtree(std::unique_ptr<animation_tree_node>& _tree_root)
{
    animator_data& animator = get_entity_animator(m_edit_tree);
    if (ImGui::IsWindowAppearing())
    {
        s_p_edit_blend_mask = nullptr;
        m_editing_tree_node = animator.m_blendtree_root_node.get();
    }

    bool open_savefile_popup = false;
    bool open_loadfile_popup = false;
    bool open_blendmask_edit_popup = false;
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Files"))
        {
            if (ImGui::MenuItem("Save"))
                open_savefile_popup = true;
            if (ImGui::MenuItem("Load"))
                open_loadfile_popup = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if(open_savefile_popup)
        ImGui::OpenPopup("Save Blend Tree File");
    if(open_loadfile_popup)
        ImGui::OpenPopup("Load Blend Tree File");
    if (s_open_blendmask_edit_popup)
        ImGui::OpenPopup("Blend Mask Editor");
        

    glm::uvec2 const window_size = Singleton<Engine::sdl_manager>().get_window_size();
    ImVec2 const center = ImVec2((float)window_size.x / 2.0f,  (float)window_size.y / 2.0f);
    if (open_savefile_popup | open_loadfile_popup | s_open_blendmask_edit_popup)
    {
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5, 0.5));
    }
    if (ImGui::BeginPopupModal("Save Blend Tree File", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        bool enter_pressed = ImGui::InputText(
            "File Name",
            blend_tree_file_name_buffer, sizeof(blend_tree_file_name_buffer),
            ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue
        );
        ImGui::SetItemDefaultFocus();
        if(enter_pressed)
        {
            std::filesystem::path file_name(blend_tree_file_name_buffer);
            if (!file_name.empty())
            {
                file_name.replace_extension("blent");
                std::filesystem::path const file_path = s_blendtree_dir / file_name;
                std::filesystem::create_directories(s_blendtree_dir);
                std::ofstream out(file_path);
                try
                {
                    auto const & tree_root = get_entity_animator(m_edit_tree).m_blendtree_root_node;
                    out << std::setw(4) << tree_root->serialize();
                }
                catch (nlohmann::json::exception e)
                {
                    Engine::Utils::print_error("[Blend Tree Editor] Failed to serialize Blend Tree node:\n %s", e.what());
                }
                out.close();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("Load Blend Tree File", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        if (ImGui::BeginListBox("###Files", ImVec2(window_size.x * 0.25f, window_size.y * 0.75f)))
        {
            using namespace std::filesystem;
            // TODO: Support recursive directory traversal.
            for (auto const& file : directory_iterator(s_blendtree_dir))
            {
                if (file.is_directory())
                    continue;
                path const file_path = file.path();
                path const file_extension = file_path.extension();
                if (file_extension != ".blent")
                    continue;
                if (ImGui::Selectable(file_path.string().c_str()))
                {
                    ImGui::CloseCurrentPopup();
                    std::ifstream in(file_path);
                    nlohmann::json j;
                    in >> j;
                    try
                    {
                        auto& tree_root = get_entity_animator(m_edit_tree).m_blendtree_root_node;
                        tree_root = animation_tree_node::create(j);
                        m_editing_tree_node = tree_root.get();
                    }
                    catch (nlohmann::json::exception e)
                    {
                        Engine::Utils::print_error("[Blend Tree Editor] Failed to deserialize Blend Tree node:\n %s", e.what());
                    }
                    in.close();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndListBox();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("Blend Mask Editor"))
    {
        s_open_blendmask_edit_popup = false;

        Component::Skin const skin_comp = m_edit_tree.GetComponent<Component::Skin>();

        static float new_factor = 1.0f;
        if (ImGui::IsWindowAppearing())
            new_factor = 1.0f;

        bool mask_enabled = !s_p_edit_blend_mask->m_joint_blend_masks.empty();

        if (ImGui::Checkbox("Blending Mask Enabled", &mask_enabled))
        {
            if (mask_enabled)
            {
                s_p_edit_blend_mask->m_joint_blend_masks.resize(
                    get_entity_animator(m_edit_tree).m_bind_pose.m_joint_transforms.size(), 1.0f
                );
            }
            else
            {
                s_p_edit_blend_mask->m_joint_blend_masks.clear();
            }
        }

        if (mask_enabled)
        {
            bool apply_global_mask = ImGui::Button("Apply");
            ImGui::SameLine();
            ImGui::InputFloat("Factor for all joints", &new_factor, 0.0f, 2.0f, "%.3f");
            if (ImGui::Button("Reset all factors to Default"))
                s_p_edit_blend_mask->m_joint_blend_masks.clear();

            if (apply_global_mask)
            {
                apply_global_mask = false;
                if (s_p_edit_blend_mask->m_joint_blend_masks.empty())
                    s_p_edit_blend_mask->m_joint_blend_masks.resize(skin_comp.GetSkeletonInstanceNodes().size());
                for (unsigned int i = 0; i < s_p_edit_blend_mask->m_joint_blend_masks.size(); ++i)
                    s_p_edit_blend_mask->m_joint_blend_masks[i] = new_factor;
            }

            bool table_began = ImGui::BeginTable(
                "Skin Joints", 2, 
                ImGuiTableFlags_BordersH | ImGuiTableFlags_ScrollY, 
                glm::vec2(ImGui::GetContentRegionAvailWidth(), window_size.y * 0.3f)
            );
            if (table_began)
            {
                ImGui::TableSetupColumn("Joint", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Mask Factor");
                ImGui::TableHeadersRow();

                float imgui_alpha = ImGui::GetStyleColorVec4(ImGuiCol_Button).w;
                auto const & skeleton_joints = skin_comp.GetSkeletonInstanceNodes();
                for (unsigned int i = 0; i < skeleton_joints.size(); ++i)
                {
                    ImGui::PushID(i);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text(skeleton_joints[i].Owner().GetName());
                    ImGui::TableNextColumn();
                    
                    float value = (*s_p_edit_blend_mask)[i];
                    bool use_left = (value >= 0.5f);
                    const char* mask_side;
                    if (use_left)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, glm::vec4(0.0f, 1.0f, 0.0f, imgui_alpha));
                        mask_side = "Use Left";
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, glm::vec4(1.0f, 0.0f, 0.0f, imgui_alpha));
                        mask_side = "Use Right";
                    }

                    if (ImGui::Button(mask_side))
                    {
                        s_p_edit_blend_mask->m_joint_blend_masks[i] = use_left ? 0.0f : 1.0f;
                    }
                    ImGui::PopStyleColor();

                    //if (ImGui::InputFloat("", &value, 0.0f, 2.0f, "%.3f"))
                    //{
                    //    if (s_p_edit_blend_mask->m_joint_blend_masks.empty())
                    //        s_p_edit_blend_mask->m_joint_blend_masks.resize(skeleton_joints.size(), 1.0f);
                    //    s_p_edit_blend_mask->m_joint_blend_masks[i] = value;
                    //}
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::EndPopup();
    }
    else
        s_p_edit_blend_mask = nullptr;

    float window_width = ImGui::GetWindowContentRegionWidth();
    if(ImGui::BeginChild("Tree Viewer", ImVec2(window_width * 0.3f, -1.0f), true))
    {
        if (animator.m_blendtree_root_node)
        {
            gui_node_context context;
            context.node_index = 0;
            context.selected_node_ptr = &m_editing_tree_node;
            int return_value = animator.m_blendtree_root_node->gui_node(context);
            if (return_value == -1)
            {
                animator.m_blendtree_root_node.reset();
                m_editing_tree_node = nullptr;
            }

        }
        else
        {
            ImGui::TextWrapped("Tree is empty.");
            if (ImGui::BeginPopupContextWindow())
            {
                auto result = show_node_creation_menu(
                    ECreatableNodeFlags::eLeafNode |
                    ECreatableNodeFlags::eBlendNode1D |
                    ECreatableNodeFlags::eBlendNode2D
                );
                if (result)
                {
                    animator.m_blendtree_root_node = std::move(result);
                    m_editing_tree_node = animator.m_blendtree_root_node.get();
                }
                ImGui::EndPopup();
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("Node Editor", ImVec2(0.0f, -1.0f), true))
    {
        if (m_editing_tree_node)
            m_editing_tree_node->gui_edit();
        else
            ImGui::TextWrapped("No tree node selected.");
    }
    ImGui::EndChild();

}

bool SkeletonAnimator::IsPaused() const
{
    return GetManager().m_entity_animator_data.at(m_owner).m_instance.m_paused;
}

void SkeletonAnimator::SetPaused(bool _paused)
{
    GetManager().m_entity_animator_data.at(m_owner).m_instance.m_paused = _paused;
}

void SkeletonAnimator::SetAnimation(animation_leaf_node _animation)
{
    GetManager().get_entity_animator(Owner()).m_blendtree_root_node = std::make_unique<animation_leaf_node>(_animation);
}

void SkeletonAnimator::LoadBlendTree(std::string _filename)
{
    auto& root_node = GetManager().get_entity_animator(Owner()).m_blendtree_root_node;
    std::filesystem::path path = s_blendtree_dir / _filename;
    if (std::filesystem::exists(path) && path.extension() == ".blent")
    {
        std::ifstream in(path);
        nlohmann::json j;
        in >> j;
        root_node = animation_tree_node::create(j);
    }

}

void SkeletonAnimator::Deserialize(nlohmann::json const& _j)
{
    GetManager().get_entity_animator(Owner()) = _j;
}

void SkeletonAnimator::SetBindPose(animation_pose _bind_pose)
{
    GetManager().get_entity_animator(Owner()).m_bind_pose = _bind_pose;
}

std::unique_ptr<animation_tree_node>& SkeletonAnimator::GetBlendTreeRootNode()
{
    return GetManager().get_entity_animator(Owner()).m_blendtree_root_node;
}

void to_json(nlohmann::json& _j, animation_blend_mask const& _mask)
{
    _j = _mask.m_joint_blend_masks;
}

void from_json(nlohmann::json const& _j, animation_blend_mask& _mask)
{
    _mask.m_joint_blend_masks = _j.get<decltype(_mask.m_joint_blend_masks)>();
}

void to_json(nlohmann::json& _j, animation_instance const& _instance)
{
    _j["animation_speed"] = _instance.m_anim_speed;
    _j["loop"] = _instance.m_loop;
    _j["paused"] = _instance.m_paused;
    _j["global_time"] = _instance.m_global_time;
}

void from_json(nlohmann::json const& _j, animation_instance& _instance)
{
    _instance.m_anim_speed = _j.value("animation_speed", 1.0f);
    _instance.m_loop = _j.value("loop", true);
    _instance.m_paused = _j.value("paused", false);
    _instance.m_global_time = _j.value("global_time", 0.0f);
}

void to_json(nlohmann::json& _j, animation_pose const& _instance)
{
    _j["joint_transforms"] = _instance.m_joint_transforms;
}

void from_json(nlohmann::json const& _j, animation_pose& _instance)
{
    _instance.m_joint_transforms = _j["joint_transforms"].get<std::vector<Engine::Math::transform3D>>();
}

void to_json(nlohmann::json& _j, SkeletonAnimatorManager::animator_data const& _animator)
{
    _j["instance_data"] = _animator.m_instance;
    _j["bind_pose"] = _animator.m_bind_pose;
}

void from_json(nlohmann::json const& _j, SkeletonAnimatorManager::animator_data& _animator)
{
    _animator.m_instance = _j.value("instance_data", animation_instance());
    _animator.m_bind_pose = _j.value("bind_pose", animation_pose());
}

}
