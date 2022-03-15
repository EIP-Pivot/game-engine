#pragma once

#include "pivot/ecs/Core/Data/value.hxx"
#include "pivot/ecs/Core/Event/description.hxx"
#include "pivot/ecs/Core/Systems/manager.hxx"
#include "pivot/ecs/Core/types.hxx"

#include <any>
#include <vector>

namespace pivot::ecs::event
{

class Manager
{
public:
    Manager(systems::Manager &systemManager);

    void sendEvent(const Event &event);

private:
    systems::Manager &m_systemManager;
};

}    // namespace pivot::ecs::event