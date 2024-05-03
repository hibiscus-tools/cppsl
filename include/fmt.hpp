#pragma once

#include "gir.hpp"
#include "translate.hpp"

#include <fmt/format.h>

inline std::string gloa_type_string(gloa x)
{
	switch (x) {
	case eNone:
		return "";
	case eFloat32:
		return "float";
	case eVec2:
		return "vec2";
	case eVec3:
		return "vec3";
	case eVec4:
		return "vec4";
	default:
		break;
	}

	return "<?>";
}

inline std::string format_as(const gir_t &v)
{
	struct visitor {
		std::string operator()(int x) {
			return std::to_string(x);
		}

		std::string operator()(float x) {
			return std::to_string(x);
		}

		std::string operator()(gloa x) {
			return GLOA_STRINGS[x];
		}
	};

	return std::visit(visitor {}, v);
}

inline std::string format_as(const gir_tree &gt, size_t indent = 0)
{
	std::string tab(indent, ' ');

	std::string variant = "int";
	if (std::holds_alternative <float> (gt.data))
		variant = "float";
	if (std::holds_alternative <gloa> (gt.data))
		variant = "gloa";

	std::string out = fmt::format("{}({:>5s}: {}: {})", tab, variant, gt.data, gt.cexpr);
	for (const auto &cgt : gt.children)
		out += "\n" + format_as(cgt, indent + 4);
	return out;
}


inline std::string format_as(const statement &s)
{
	std::string type = gloa_type_string(s.loc.type);
	if (type.empty())
		return s.loc.full_id() + " = " + s.source + ";";

	return gloa_type_string(s.loc.type) + " " + s.loc.full_id() + " = " + s.source + ";";
}

inline std::string format_as(const std::vector <statement> &statements)
{
	std::string out = "";
	for (const auto &s : statements)
		out += fmt::format("{}", s) + "\n";
	return out;
}
