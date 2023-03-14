// Copyright 2022 Robotec.AI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "RGLServerPluginManager.hh"

#include <ignition/gazebo/components/SystemPluginInfo.hh>
#include <ignition/plugin/Register.hh>

IGNITION_ADD_PLUGIN(
        rgl::RGLServerPluginManager,
        ignition::gazebo::System,
        rgl::RGLServerPluginManager::ISystemConfigure,
        rgl::RGLServerPluginManager::ISystemPostUpdate
)

using namespace rgl;
using namespace std::placeholders;

RGLServerPluginManager::RGLServerPluginManager() = default;

RGLServerPluginManager::~RGLServerPluginManager() = default;

void RGLServerPluginManager::Configure(
        const ignition::gazebo::Entity& entity,
        const std::shared_ptr<const sdf::Element>&,
        ignition::gazebo::EntityComponentManager& ecm,
        ignition::gazebo::EventManager& evm) {

    checkSameRGLVersion();
    RGL_CHECK(rgl_configure_logging(RGL_LOG_LEVEL_ERROR, nullptr, true));
}

void RGLServerPluginManager::PostUpdate(
        const ignition::gazebo::UpdateInfo& info,
        const ignition::gazebo::EntityComponentManager& ecm) {

    ecm.EachNew<>([&](const ignition::gazebo::Entity& entity)-> bool {
        return RegisterNewLidarsCb(entity, ecm);});

    ecm.EachNew<ignition::gazebo::components::Visual, ignition::gazebo::components::Geometry>
            (std::bind(&RGLServerPluginManager::LoadEntityToRGLCb, this, _1, _2, _3));

    ecm.EachRemoved<>([&](const ignition::gazebo::Entity& entity)-> bool {
        return CheckRemovedLidarsCb(entity, ecm);});

    ecm.EachRemoved<ignition::gazebo::components::Visual, ignition::gazebo::components::Geometry>
            (std::bind(&RGLServerPluginManager::RemoveEntityFromRGLCb, this, _1, _2, _3));

    UpdateRGLEntityPose(ecm);
}
