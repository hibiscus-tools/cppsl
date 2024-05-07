#pragma once

#include <stack>

#include "gir.hpp"
#include "translate.hpp"

#include <fmt/format.h>

// TODO: replace the constructor translator/handler with this as a table
inline std::string gloa_type_string(gloa x)
{
	// TODO: unordered map?
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
	case eMat3:
		return "mat3";
	case eMat4:
		return "mat4";
	default:
		break;
	}

	throw fmt::system_error(1, "(cppsl) could not translate type {}", GLOA_STRINGS[x]);
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

		std::string operator()(const std::string &x) {
			return x;
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
	if (std::holds_alternative <std::string> (gt.data))
		variant = "string";

	std::string out = fmt::format("{}({:>7s}: {}: {})", tab, variant, gt.data, gt.cexpr);
	for (const auto &cgt : gt.children)
		out += "\n" + format_as(cgt, indent + 4);
	return out;
}

inline std::string format_as(const gcir_graph &gcir)
{
	std::stack <std::pair <int, int>> refs;
	refs.push({ 0, 0 });

	std::string out = "";
	while (!refs.empty()) {
		auto [r, t] = refs.top();
		refs.pop();

		const gir_t &data = gcir.data[r];

		std::string variant = "int";
		if (std::holds_alternative <float> (data))
			variant = "float";
		if (std::holds_alternative <gloa> (data))
			variant = "gloa";
		if (std::holds_alternative <std::string> (data))
			variant = "string";

		std::string rs = "";
		for (size_t i = 0; i < gcir.refs[r].size(); i++) {
			rs += std::to_string(gcir.refs[r][i]);
			if (i + 1 < gcir.refs[r].size())
				rs += ", ";
		}

		if (rs.empty())
			rs = "X";

		std::string tab(t, ' ');
		// out += fmt::format("{:>3}: {}[{:>7s} | {} | {}]\n", r, tab, variant, data, rs);
		out += fmt::format("{:>3}: {}[{} | {}]\n", r, tab, data, rs);

		// Add child reference in reverse order
		for (auto rc = gcir.refs[r].rbegin(); rc != gcir.refs[r].rend(); rc++)
			refs.push({ *rc, t + 4 });
	}

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
