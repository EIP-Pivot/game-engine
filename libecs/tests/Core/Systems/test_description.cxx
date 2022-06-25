#include "pivot/ecs/Components/Gravity.hxx"
#include "pivot/ecs/Components/RigidBody.hxx"
#include "pivot/ecs/Components/Tag.hxx"
#include <pivot/ecs/Core/Systems/description.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

#include <iostream>

using namespace pivot::ecs;

std::vector<event::Event> test_description(const systems::Description &, component::ArrayCombination &,
                                           const event::EventWithComponent &)
{
    return {};
}

TEST_CASE("valid system description", "[description]")
{
    event::Description event{
        .name = "Colid",
        .entities = {},
        .payload = pivot::ecs::data::BasicType::Number,
    };
    systems::Description description{
        .name = "Test Description",
        .systemComponents =
            {
                "RigidBody",
                "Tag",
            },
        .eventListener = event,
        .system = &test_description,
    };
    REQUIRE_NOTHROW(description.validate());
}

TEST_CASE("Empty system description", "[description]")
{
    systems::Description description;
    REQUIRE_THROWS_WITH(description.validate(), "Empty system name");
}

TEST_CASE("Empty args system description", "[description]")
{
    systems::Description description{
        .name = "Invalid",
    };
    REQUIRE_THROWS_WITH(description.validate(), "Empty system argument");
}
