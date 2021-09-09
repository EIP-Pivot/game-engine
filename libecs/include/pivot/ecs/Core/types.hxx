#pragma once

#include <bitset>

constexpr std::uint32_t fnv1a_32(char const* s, std::size_t count)
{
	return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u; // NOLINT (hicpp-signed-bitwise)
}

constexpr std::uint32_t operator "" _hash(char const* s, std::size_t count)
{
	return fnv1a_32(s, count);
}

using Entity = std::uint32_t;
const Entity MAX_ENTITIES = 5000;

using ComponentType = std::uint8_t;
const ComponentType MAX_COMPONENTS = 32;

using Signature = std::bitset<MAX_COMPONENTS>;

using EventId = std::uint32_t;
using ParamId = std::uint32_t;

enum InputButtons: std::size_t
{
	W,
	A,
	S,
	D,
	SPACE,
	LEFT_SHIFT
};

namespace Events::Window {
const EventId INPUT = "Events::Window::INPUT"_hash;
}

namespace Events::Window::Input {
const ParamId INPUT = "Events::Window::Input::INPUT"_hash;
}