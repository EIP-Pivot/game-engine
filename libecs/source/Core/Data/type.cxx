#include <magic_enum.hpp>
#include <sstream>

#include <pivot/ecs/Core/Data/type.hxx>

namespace pivot::ecs::data
{

std::ostream &operator<<(std::ostream &stream, const BasicType &type)
{
    return stream << "BasicType::" << magic_enum::enum_name(type);
}

std::ostream &operator<<(std::ostream &stream, const RecordType &type)
{
    bool first = true;
    stream << "Record {";
    for (const auto &[key, value]: type) {
        if (!first) { stream << ", "; }
        stream << "{\"" << key << "\", " << value << "}";
        first = false;
    }
    return stream << "}";
}

std::ostream &operator<<(std::ostream &stream, const OptionalType &type)
{
    return stream << "Optional {" << type.type << "}";
}

std::ostream &operator<<(std::ostream &stream, const Type &type)
{
    stream << "Type { ";
    // FIXME: Remove the static_cast when GCC 12 releases
    std::visit([&](const auto &value) { stream << value; }, static_cast<const Type::variant &>(type));
    return stream << " }";
}

std::string Type::toString() const
{
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}
}    // namespace pivot::ecs::data
