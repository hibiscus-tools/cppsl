#pragma once

#include <string>

#include <fmt/format.h>

#include "gir.hpp"
#include "core.hpp"

// Translate into GLSL source code
struct identifier {
	gloa type;
	std::string prefix;
	int id;
	std::string postfix; // Only for arrays, components, etc.

	std::string full_id() const {
		return prefix + std::to_string(id) + postfix;
	}

	static identifier from(gloa type, int &generator) {
		return { type, "_v", generator++ };
	}
};

struct statement {
	identifier loc;
	std::string source;

	std::string full_loc() const {
		return loc.full_id();
	}

	static statement from(gloa type, const std::string &source, int &generator) {
		return { identifier::from(type, generator), source };
	}
};

using statement_list = std::vector <statement>;

// Un-templated structures
struct unt_layout_output {
	gloa type;
	int binding;
	gir_tree gt;
};

// TODO: support multi output programs
namespace detail {

std::string translate_vertex_shader(const intrinsics::vertex &);
std::string translate_fragment_shader(const unt_layout_output &);

}

// TODO: template
inline std::string translate_vertex_shader(const intrinsics::vertex &vintr)
{
	return detail::translate_vertex_shader(vintr);
}

template <typename T, int N>
std::string translate_fragment_shader(const layout_output <T, N> &fout)
{
	return detail::translate_fragment_shader({ T::native_type, N, fout });
}
