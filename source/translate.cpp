#include <cassert>
#include <map>
#include <set>

#include <fmt/format.h>

#include "fmt.hpp"
#include "gir.hpp"
#include "translate.hpp"

static const std::string LAYOUT_INPUT_PREFIX = "_lin";
static const std::string LAYOUT_OUTPUT_PREFIX = "_lout";
static const std::string PUSH_CONSTANTS_PREFIX = "_pc";
static const std::string PUSH_CONSTANTS_MEMBER_PREFIX = "m";

std::vector <statement> translate(const gir_tree &, int &);

std::vector <statement> translate_variadic(const std::vector <gir_tree> &nodes, int &generator)
{
	std::vector <statement> statements;
	for (const auto &gt : nodes) {
		auto cstmts = translate(gt, generator);
		statements.insert(statements.end(), cstmts.begin(), cstmts.end());
	}

	return statements;
}

std::vector <statement> translate_layout_input(const std::vector <gir_tree> &nodes, int &generator)
{
	gloa type = std::get <gloa> (nodes[0].data);
	int binding = std::get <int> (nodes[1].data);
	return { statement::from(type, fmt::format("{}{}", LAYOUT_INPUT_PREFIX, binding), generator) };
}

std::vector <statement> translate_push_constants(const std::vector <gir_tree> &nodes, int &generator)
{
	gloa type = std::get <gloa> (nodes[0].data);
	int member = std::get <int> (nodes[1].data);
	return { statement::from(type, fmt::format("{}.{}{}", PUSH_CONSTANTS_PREFIX, PUSH_CONSTANTS_MEMBER_PREFIX, member), generator) };
}

std::vector <statement> translate_layout_output(const std::vector <gir_tree> &nodes, int &generator)
{
	int binding = std::get <int> (nodes[0].data);
	auto statements = translate(nodes[1], generator);
	statement after = statement::builtin_from(fmt::format("{}{}", LAYOUT_OUTPUT_PREFIX, binding),
		 statements.back().full_loc());
	statements.push_back(after);
	return statements;
}

std::vector <statement> translate_gl_position(const std::vector <gir_tree> &nodes, int &generator)
{
	auto statements = translate(nodes[0], generator);
	statement after = statement::builtin_from("gl_Position", statements.back().full_loc());
	statements.push_back(after);
	return statements;
}

std::vector <statement> translate_binary_operation(gloa op, const std::vector <gir_tree> &nodes, int &generator)
{
	static const std::unordered_map <gloa, std::string> OPERATION_MAP {
		{ eAdd, "+" }, { eMul, "*" }
	};

	assert(nodes.size() == 2);
	auto s0 = translate(nodes[0], generator);
	auto s1 = translate(nodes[1], generator);

	statement s0_back = s0.back();
	statement s1_back = s1.back();

	// TODO: check the type maps...
	gloa type = s1_back.loc.type;
	statement after = statement::from(type, fmt::format("{} {} {}", s0_back.full_loc(), OPERATION_MAP.at(op), s1_back.full_loc()), generator);

	std::vector <statement> statements;
	statements.insert(statements.end(), s0.begin(), s0.end());
	statements.insert(statements.end(), s1.begin(), s1.end());
	statements.push_back(after);
	return statements;
}

std::string function_call(const std::string &ftn, const std::vector <std::string> &args)
{
	std::string out = ftn + "(";
	for (size_t i = 0; i < args.size(); i++) {
		out += args[i];
		if (i + 1 < args.size())
			out += ", ";
	}

	return out + ")";
}

// TODO: inlining certain sources...
std::vector <statement> translate_construct(const std::vector <gir_tree> &nodes, int &generator)
{
	gloa type = std::get <gloa> (nodes[0].data);
	if (type == eFloat32) {
		auto v = translate(nodes[1], generator);
		statement vlast = v.back();
		statement after = statement::from(eFloat32, vlast.full_loc(), generator);
		v.push_back(after);
		return v;
	} else if (type == eVec3) {
		int count = std::get <int> (nodes[1].data);

		statement_list v;
		std::vector <std::string> args;
		for (int i = 0; i < count; i++) {
			auto vi = translate(nodes[i + 2], generator);
			v.insert(v.end(), vi.begin(), vi.end());
			args.push_back(vi.back().full_loc());
		}

		statement after = statement::from(eVec3, function_call("vec3", args), generator);
		v.push_back(after);
		return v;
	} else if (type == eVec4) {
		int count = std::get <int> (nodes[1].data);

		statement_list v;
		std::vector <std::string> args;
		for (int i = 0; i < count; i++) {
			auto vi = translate(nodes[i + 2], generator);
			v.insert(v.end(), vi.begin(), vi.end());
			args.push_back(vi.back().full_loc());
		}

		statement after = statement::from(eVec4, function_call("vec4", args), generator);
		v.push_back(after);
		return v;
	} else if (type == eMat4) {
		int count = std::get <int> (nodes[1].data);

		statement_list v;
		std::vector <std::string> args;
		for (int i = 0; i < count; i++) {
			auto vi = translate(nodes[i + 2], generator);
			v.insert(v.end(), vi.begin(), vi.end());
			args.push_back(vi.back().full_loc());
		}

		statement after = statement::from(eMat4, function_call("mat4", args), generator);
		v.push_back(after);
		return v;
	}

	throw fmt::system_error(1, "(cppsl) unknown type {}", type);
}

static constexpr gloa scalar_type(gloa vector_type)
{
	switch (vector_type) {
	case eVec2:
	case eVec3:
	case eVec4:
		return eFloat32;
	default:
		break;
	}

	return eNone;
}

std::vector <statement> translate_component(const std::vector <gir_tree> &nodes, int &generator)
{
	static const std::string postfixes[] { ".x", ".y", ".z", ".w" };
	int index = std::get <int> (nodes[0].data);
	auto statements = translate(nodes[1], generator);
	statement last = statements.back();
	statement after = statement::from(scalar_type(last.loc.type), last.full_loc() + postfixes[index], generator);
	statements.push_back(after);
	return statements;
}

std::vector <statement> translate(const gir_tree &gt, int &generator)
{
	struct visitor {
		gir_tree gt;
		int &generator;

		std::vector <statement> operator()(gloa x) {
			// TODO: table dispatcher to ftn pointers, check for non null
			switch (x) {
			case eNone:
				return translate_variadic(gt.children, generator);
			case eConstruct:
				return translate_construct(gt.children, generator);
			case eComponent:
				return translate_component(gt.children, generator);
			case eLayoutInput:
				return translate_layout_input(gt.children, generator);
			case eLayoutOutput:
				return translate_layout_output(gt.children, generator);
			case ePushConstants:
				return translate_push_constants(gt.children, generator);
			case eGlPosition:
				return translate_gl_position(gt.children, generator);
			// Binary operations
			case eAdd:
			case eMul:
				return translate_binary_operation(x, gt.children, generator);
			default:
				break;
			}

			throw fmt::system_error(1, "(cppsl) unsupported <gloa> of {}", x);
		}

		std::vector <statement> operator()(int x) {
			return { statement::from(eInt32, fmt::format("{}", x), generator) };
		}

		std::vector <statement> operator()(float x) {
			return { statement::from(eFloat32, fmt::format("{}", x), generator) };
		}
	};

	return std::visit(visitor { gt, generator }, gt.data);
}

// TODO: template these two
std::set <std::pair <gloa, int>> used_layout_input_bindings(const gir_tree &gt)
{
	if (std::holds_alternative <gloa> (gt.data)) {
		gloa x = std::get <gloa> (gt.data);
		if (x == eLayoutInput) {
			gloa type = std::get <gloa> (gt.children[0].data);
			int binding = std::get <int> (gt.children[1].data);
			return { { type, binding } };
		}
	}

	std::set <std::pair <gloa, int>> used;
	for (const gir_tree &cgt : gt.children) {
		auto cused = used_layout_input_bindings(cgt);
		used.insert(cused.begin(), cused.end());
	}

	return used;
}

std::set <int> used_layout_output_bindings(const gir_tree &gt)
{
	if (std::holds_alternative <gloa> (gt.data)) {
		gloa x = std::get <gloa> (gt.data);
		if (x == eLayoutOutput) {
			int binding = std::get <int> (gt.children[0].data);
			return { binding };
		}
	}

	std::set <int> used;
	for (const gir_tree &cgt : gt.children) {
		auto cused = used_layout_output_bindings(cgt);
		used.insert(cused.begin(), cused.end());
	}

	return used;
}

std::map <int, std::pair <gloa, int>> push_constants_structure(const gir_tree &gt)
{
	if (std::holds_alternative <gloa> (gt.data)) {
		gloa x = std::get <gloa> (gt.data);
		if (x == ePushConstants) {
			gloa type = std::get <gloa> (gt.children[0].data);
			int member = std::get <int> (gt.children[1].data);
			int offset = std::get <int> (gt.children[2].data);
			return { { member, { type, offset } } };
		}
	}

	std::map <int, std::pair <gloa, int>> structure;
	for (const gir_tree &cgt : gt.children) {
		auto cstruct = push_constants_structure(cgt);

		// Check for no conflicting members
		for (const auto &[member, info] : cstruct) {
			if (structure.count(member))
				assert(structure[member] == info);
			else
				structure[member] = info;
		}
	}

	return structure;
}

namespace detail {

// TODO: separate optimization stage

std::string translate_gir_tree(const gir_tree &unified, const std::vector <unt_layout_output> &louts)
{
	fmt::println("\nunified tree for shader:\n{}", unified);

	// TODO: create a gir_link_tree

	// Grab all used bindings
	auto used_lib = used_layout_input_bindings(unified);
	auto used_lob = used_layout_output_bindings(unified);
	auto pc_struct = push_constants_structure(unified);

	// TODO: also all output bindings

	// Fill in the rest of the program
	std::string code = "#version 450\n";

	// Input layout bindings
	for (auto [type, binding] : used_lib) {
		code += fmt::format("layout (location = {}) in {} {}{};\n",
			binding, gloa_type_string(type), LAYOUT_INPUT_PREFIX, binding);
	}

	// Output layout bindings
	for (auto binding : used_lob) {
		gloa type = louts[binding].type;
		code += fmt::format("layout (location = {}) out {} {}{};\n",
			binding, gloa_type_string(type), LAYOUT_OUTPUT_PREFIX, binding);
	}

	// Push constants
	if (pc_struct.size()) {
		// fmt::println("pc_struct:");
		// for (const auto &[member, info] : pc_struct)
		// 	fmt::println("  {}: {:<5} +{}", member, gloa_type_string(info.first), info.second);

		code += fmt::format("layout (push_constant) uniform PushConstants {{\n");
		// TODO: track the size for necessary paddings
		int offed = 0;
		for (const auto &[member, info] : pc_struct) {
			assert(offed <= info.second);
			if (offed < info.second) {
				int size = (info.second - offed)/sizeof(float);
				code += fmt::format("  float _off{}[{}];\n", offed, size);
			}

			code += fmt::format("  {} {}{};\n", gloa_type_string(info.first), PUSH_CONSTANTS_MEMBER_PREFIX, member);
			offed += gloa_type_offset(info.first);
		}

		code += fmt::format("}} {};\n", PUSH_CONSTANTS_PREFIX);
	}

	code += "void main() {\n";

	int generator = 0;
	auto statements = translate(unified, generator);
	for (const statement &s : statements)
		code += fmt::format("  {}\n", s);

	code += "}\n";

	return code;
}

}
