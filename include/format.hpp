#pragma once

#include <string>

#include "stream.hpp"
#include "instruction.hpp"

namespace cppsl {

// Formatting
auto format_as(const iop);
auto format_as(const glsl);
auto format_as(const primitive_operation &);

std::string format_as(const instruction &);
std::string format_as(const stream &);

template <typename T>
requires streamable <T>
auto format_as(const T &t)
{
	return format_as(stream(t));
}

}