#include "pivot/ecs/Components/Gravity.hxx"
#include "pivot/ecs/Components/RigidBody.hxx"
#include "pivot/ecs/Components/Tag.hxx"
#include <catch2/catch.hpp>
#include <pivot/ecs/Core/Systems/description.hxx>

#include <iostream>

using namespace pivot::ecs;

void test_description(const systems::Description::availableEntities &, const systems::Description &,
                      const systems::Description::systemArgs &, const event::Event &)
{
}

TEST_CASE("valid system description", "[description]")
{
    systems::Description description {
        .name = "Test Description",
        .components = {
            "RigidBody",
            "Tag",
        },
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

TEST_CASE("Invalid args system description", "[description]")
{
    systems::Description description{
        .name = "Invalid",
        .components = {"NOT REGISTERED"},
    };
    REQUIRE_THROWS_WITH(description.validate(), Catch::Matchers::Contains("Component ") && Catch::Matchers::Contains(" his not registered."));
}