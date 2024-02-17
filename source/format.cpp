#include <fmt/format.h>

#include "format.hpp"

namespace cppsl {

// Formatting
auto format_as(const iop t)
{
	switch (t) {
	case iop::fetch:
		return "fetch";
	case iop::store:
		return "store";
	}

	return "?";
}

auto format_as(const glsl t)
{
	switch (t) {
	case tFloat:
		return "float";
	case tVec2:
		return "vec2";
	case tVec3:
		return "vec3";
	case tVec4:
		return "vec4";
	case tMat3:
		return "mat3";
	case tMat4:
		return "mat4";
	}

	return "?";
}

auto format_as(const primitive_operation &primop)
{
	switch (primop) {
	case primitive_operation::add:
		return "add";
	case primitive_operation::sub:
		return "sub";
	case primitive_operation::mul:
		return "mul";
	case primitive_operation::div:
		return "div";
	case primitive_operation::uneg:
		return "uneg";
	case primitive_operation::cmp_gt:
		return "gt";
	case primitive_operation::cmp_lt:
		return "lt";
	}

	return "?";
}

std::string format_as(const instruction &I)
{
	if (auto value = I.grab <primitive_operation> ()) {
		return format_as(*value);
	}

	if (auto value = I.grab <shader_layout> ()) {
		return fmt::format("shader_layout <{}, {}, {}>", value->type, value->binding, value->fs);
	}
	
	if (auto value = I.grab <stash_intrinsic> ()) {
		return fmt::format("stash_instrinsic");
	}
	
	if (auto value = I.grab <fetch_push_constants> ()) {
		return fmt::format("fetch_push_constants <{}, {}>", value->type, value->location);
	}
	
	if (auto value = I.grab <constructor> ()) {
		return fmt::format("constructor <{}, {}>", value->type, value->nargs);
	}
	
	if (auto value = I.grab <static_indexing> ()) {
		return fmt::format("index (static) <{}, {}, {}>", value->original, value->index, value->fs);
	}
	
	if (auto value = I.grab <branch_trigger> ()) {
		switch (value->type) {
		case branch_trigger::eBegin:
			return "branch trigger (begin)";
		case branch_trigger::eCondition:
			return "branch trigger (cond)";
		case branch_trigger::eEnd:
			return "branch trigger (end)";
		default:
			break;
		}

		return "branch trigger (?)";
	}
	
	if (auto value = I.grab <float> ()) {
		return fmt::to_string(*value);
	}

	return "<?>";
}

std::string format_as(const stream &s)
{
	std::string str = "";
	for (uint32_t i = 0; i < s.size(); i++) {
		str += fmt::format("    {}", s[i]);
		if (i + 1 < s.size())
			str += "\n";
	}

	return "\n{\n" + str + "\n}";
}

}