#pragma once

#include <iostream>
#include <set>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/mpl.hpp>
#include <boost/mpl/range_c.hpp>
#include <glm/vec3.hpp>

#include <pivot/ecs/Core/Component/array.hxx>
#include <pivot/ecs/Core/Component/description_helpers.hxx>
#include <pivot/ecs/Core/Component/error.hxx>
#include <pivot/ecs/Core/Component/index.hxx>

namespace pivot::ecs::component::helpers
{

namespace
{
}    // namespace

template <typename T>
constexpr const char *component_name = nullptr;

template <typename T>
concept ComponentBaseType = data::basic_type_representation<T>
!= std::nullopt;

template <typename T>
concept ComponentComplexType =
    (data::basic_type_representation<T> == std::nullopt) && boost::fusion::traits::is_sequence<T>::value;

template <typename T>
concept ComponentType = ComponentBaseType<T> || ComponentComplexType<T>;

template <ComponentComplexType T>
struct Helpers<T> {
    static data::Type getType()
    {
        data::RecordType record;
        using Indices = boost::mpl::range_c<unsigned, 0, boost::fusion::result_of::size<T>::value>;
        boost::fusion::for_each(Indices(), [&](auto i) {
            std::string field_name = boost::fusion::extension::struct_member_name<T, decltype(i)::value>::call();
            using value_type = typename boost::fusion::result_of::value_at_c<T, decltype(i)::value>::type;
            data::Type type = Helpers<value_type>::getType();
            record.insert({field_name, type});
        });
        return {record};
    }

    static data::Value createValueFromType(const T &v)
    {
        data::Record record;
        using Indices = boost::mpl::range_c<unsigned, 0, boost::fusion::result_of::size<T>::value>;
        boost::fusion::for_each(Indices(), [&](auto i) {
            std::string field_name = boost::fusion::extension::struct_member_name<T, decltype(i)::value>::call();
            using value_type = typename boost::fusion::result_of::value_at_c<T, decltype(i)::value>::type;
            auto it = boost::fusion::advance_c<decltype(i)::value>(boost::fusion::begin(v));
            record.insert({field_name, Helpers<value_type>::createValueFromType(*it)});
        });
        return {record};
    }

    static void updateTypeWithValue(T &data, const data::Value &value)
    {
        auto &record = std::get<data::Record>(value);
        using Indices = boost::mpl::range_c<unsigned, 0, boost::fusion::result_of::size<T>::value>;
        boost::fusion::for_each(Indices(), [&](auto i) {
            std::string field_name = boost::fusion::extension::struct_member_name<T, decltype(i)::value>::call();
            using value_type = typename boost::fusion::result_of::value_at_c<T, decltype(i)::value>::type;
            auto it = boost::fusion::advance_c<decltype(i)::value>(boost::fusion::begin(data));
            value_type &member = *it;
            const data::Value &member_value = record.at(field_name);
            Helpers<value_type>::updateTypeWithValue(member, member_value);
        });
    }
};

template <ComponentBaseType T>
struct Helpers<T> {
    static data::Type getType() { return data::Type{data::basic_type_representation<T>.value()}; }
    static data::Value createValueFromType(const T &v) { return data::Value(v); }
    static void updateTypeWithValue(T &data, const data::Value &value) { data = std::get<T>(value); }
};

template <typename A>
std::unique_ptr<IComponentArray> createContainer(Description description)
{
    return std::make_unique<A>(description);
}

template <typename T, typename A>
Description build_component_description(const char *name, bool registerComponent = true)
{
    Description description{name, helpers::Helpers<T>::getType(), helpers::createContainer<A>};
    if (registerComponent) { GlobalIndex::getSingleton().registerComponentWithType<T>(description); }
    return description;
}
}    // namespace pivot::ecs::component::helpers

/// Registers a component
#define PIVOT_REGISTER_COMPONENT(component_type, array_type)                                                      \
    namespace pivot::ecs::component::helpers                                                                      \
    {                                                                                                             \
        template <>                                                                                               \
        constexpr const char *component_name<component_type> = #component_type;                                   \
                                                                                                                  \
        template struct Helpers<std::string>;                                                                     \
                                                                                                                  \
        static const auto description = build_component_description<component_type, array_type>(#component_type); \
    }
