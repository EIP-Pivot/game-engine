#include "ImGuiCore/EntityModule.hxx"
#include "ImGuiCore/ImGuiTheme.hxx"

#include <misc/cpp/imgui_stdlib.h>

void EntityModule::create()
{
    PROFILE_FUNCTION();
    auto &componentManager = m_scene->getComponentManager();
    auto tagId = componentManager.GetComponentId("Tag").value();
    if (m_scene.id() != currentScene) {
        _hasSelected = false;
        entitySelected = -1;
    }
    currentScene = m_scene.id();
    ImGui::Begin(" Entity ");
    ImGuiTheme::setDefaultFramePadding();
    createPopUp();
    if (ImGui::Button("Add entity")) ImGui::OpenPopup("NewEntity");
    if (_hasSelected) {
        ImGui::SameLine();
        if (ImGui::Button("Remove entity")) removeEntity();
    }
    ImGui::Separator();
    for (auto const &[entity, _]: m_scene->getEntities()) {
        if (ImGui::Selectable(
                std::get<std::string>(
                    std::get<pivot::ecs::data::Record>(componentManager.GetComponent(entity, tagId).value()).at("name"))
                    .c_str(),
                entitySelected == entity)) {
            _hasSelected = true;
            entitySelected = entity;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            std::cout << std::get<std::string>(
                             std::get<pivot::ecs::data::Record>(componentManager.GetComponent(entity, tagId).value())
                                 .at("name"))
                             .c_str()
                      << std::endl;
        }
    }
    ImGuiTheme::unsetDefaultFramePadding();
    ImGui::End();
}

Entity EntityModule::addEntity()
{
    PROFILE_FUNCTION();
    Entity newEntity = m_scene->CreateEntity();
    entitySelected = newEntity;
    _hasSelected = true;
    return newEntity;
}

Entity EntityModule::addEntity(std::string name)
{
    PROFILE_FUNCTION();
    Entity newEntity = m_scene->CreateEntity(name);
    entitySelected = newEntity;
    _hasSelected = true;
    return newEntity;
}

void EntityModule::removeEntity()
{
    PROFILE_FUNCTION();
    m_scene->DestroyEntity(entitySelected);
    entitySelected = 0;
    _hasSelected = false;
}

Entity EntityModule::getEntitySelected() { return entitySelected; }

bool EntityModule::hasSelected() { return _hasSelected; }

void EntityModule::createPopUp()
{
    PROFILE_FUNCTION();
    if (ImGui::BeginPopup("NewEntity")) {
        static std::string entityName;
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##", &entityName, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (entityName.empty())
                addEntity();
            else
                addEntity(entityName);
            entityName.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
