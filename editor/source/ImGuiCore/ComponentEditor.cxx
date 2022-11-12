#include "ImGuiCore/ComponentEditor.hxx"
#include "ImGuiCore/CustomWidget.hxx"
#include "ImGuiCore/ImGuiTheme.hxx"
#include "ImGuiCore/ValueInput.hxx"
#include <magic_enum.hpp>

#include <imgui.h>

#include <pivot/builtins/components/Camera.hxx>

using namespace pivot::ecs;
using namespace pivot::ecs::component;
using namespace pivot::ecs::data;

void ComponentEditor::create(Entity entity)
{
    PROFILE_FUNCTION();
    currentEntity = entity;
    ImGui::Begin(" Component editor ");
    ImGuiTheme::setDefaultFramePadding();
    createPopUp();
    displayComponent();
    if (CustomWidget::ButtonCenteredOnLine("Add Component")) { ImGui::OpenPopup("AddComponent"); }
    ImGuiTheme::unsetDefaultFramePadding();
    ImGui::End();
}

void ComponentEditor::create()
{
    PROFILE_FUNCTION();
    ImGui::Begin(" Component editor ");
    ImGui::Text("No entity selected.");
    ImGui::End();
}

void ComponentEditor::createPopUp()
{
    PROFILE_FUNCTION();
    auto &cm = m_scene->getComponentManager();
    if (ImGui::BeginPopup("AddComponent")) {
        for (const auto &[name, description]: m_index) {
            auto id = cm.GetComponentId(name);
            if (!id || cm.GetComponent(currentEntity, *id) == std::nullopt) {
                if (ImGui::MenuItem(name.c_str())) {
                    if (!id) { cm.RegisterComponent(description); }
                    addComponent(description);
                }
            }
        }
        ImGui::EndPopup();
    }
}

void ComponentEditor::displayComponent()
{
    PROFILE_FUNCTION();
    auto &cm = m_scene->getComponentManager();
    displayName();
    for (ComponentRef ref: cm.GetAllComponents(currentEntity)) {
        if (ref.description().name == "Tag") continue;
        deleteComponent(ref);
        if (ImGui::TreeNode(ref.description().name.c_str())) {
            ImGui::TreePop();
            Value value = ref;
            ImGui::PushID(ref.description().name.c_str());
            m_value_input.drawInput(value, "oui");
            ImGui::PopID();
            ref = value;
            this->selectCamera(ref);
            ImGui::Separator();
        }
    }
}

void ComponentEditor::displayName()
{
    PROFILE_FUNCTION();
    auto &cm = m_scene->getComponentManager();
    auto tagId = cm.GetComponentId("Tag").value();
    auto &tagArray = cm.GetComponentArray(tagId);
    auto tag = ComponentRef(tagArray, currentEntity);
    Value value = tag;
    auto &name = std::get<std::string>(std::get<Record>(value).at("name"));
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##Tag", &name);
    ImGui::PopItemWidth();
    tag.set(value);
}

void ComponentEditor::deleteComponent(ComponentRef ref)
{
    PROFILE_FUNCTION();
    auto &cm = m_scene->getComponentManager();
    ImGuiIO &io = ImGui::GetIO();
    auto boldFont = io.Fonts->Fonts[0];
    float lineHeight = (GImGui->Font->FontSize * boldFont->Scale) + GImGui->Style.FramePadding.y * 2.f;
    ImVec2 buttonSize = {lineHeight + 3.f, lineHeight};
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.f});
    ImGui::PushFont(boldFont);
    ImGui::PushID((std::string("Delete") + ref.description().name).c_str());
    if (ImGui::Button("X", buttonSize)) {
        auto id = cm.GetComponentId(ref.description().name).value();
        cm.RemoveComponent(currentEntity, id);
    }
    ImGui::PopID();
    ImGui::PopFont();
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
}

void ComponentEditor::addComponent(const Description &description)
{
    PROFILE_FUNCTION();
    auto &cm = m_scene->getComponentManager();
    auto id = cm.GetComponentId(description.name).value();
    Value newComponent = description.defaultValue;
    cm.AddComponent(currentEntity, newComponent, id);
}

void ComponentEditor::selectCamera(pivot::ecs::component::ComponentRef ref)
{
    if (ref.description() == pivot::builtins::components::Camera::description) {
        ImGuiIO &io = ImGui::GetIO();
        auto boldFont = io.Fonts->Fonts[0];
        float lineHeight = (GImGui->Font->FontSize * boldFont->Scale) + GImGui->Style.FramePadding.y * 2.f;
        if (ImGui::Button("Select camera")) { m_engine.setCurrentCamera(ref.entity()); }
    }
}