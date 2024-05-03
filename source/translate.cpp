#include <cassert>
#include <set>

#include <fmt/format.h>

#include "fmt.hpp"
#include "gir.hpp"
#include "translate.hpp"

static const std::string LAYOUT_INPUT_PREFIX = "_lin";
static const std::string LAYOUT_OUTPUT_PREFIX = "_lout";

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
	}

	return {};
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
			case eGlPosition:
				return translate_gl_position(gt.children, generator);
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

// TODO: support multi output programs
namespace detail {

// TODO: separate optimization stage

std::string translate_gir_tree(const gir_tree &unified, const std::vector <unt_layout_output> &louts)
{
	fmt::println("\nunified tree for shader:\n{}", unified);

	// TODO: create a gir_link_tree

	// Grab all used bindings
	auto used_lib = used_layout_input_bindings(unified);
	auto used_lob = used_layout_output_bindings(unified);

	// TODO: also all output bindings

	// Fill in the rest of the program
	std::string code = "#version 450\n";

	for (auto [type, binding] : used_lib) {
		code += fmt::format("layout (location = {}) in {} {}{};\n",
			binding, gloa_type_string(type), LAYOUT_INPUT_PREFIX, binding);
	}

	for (auto binding : used_lob) {
		gloa type = louts[binding].type;
		code += fmt::format("layout (location = {}) out {} {}{};\n",
			binding, gloa_type_string(type), LAYOUT_OUTPUT_PREFIX, binding);
	}

	code += "void main() {\n";

	int generator = 0;
	auto statements = translate(unified, generator);
	for (const statement &s : statements)
		code += fmt::format("  {}\n", s);

	code += "}\n";

	return code;
}

// std::string translate_vertex_shader(const intrinsics::vertex &vin, const std::vector <unt_layout_output> &louts)
// {
// 	int generator = 0;
//
// 	// Preprocessing; unify all outputs into a single tree
// 	std::vector <gir_tree> outputs;
//
// 	bool cexpr = true;
//
// 	cexpr &= vin.gl_Position.cexpr;
// 	outputs.push_back(gir_tree::from(eGlPosition, vin.gl_Position.cexpr, { vin.gl_Position }));
//
// 	for (const unt_layout_output &lout : louts) {
// 		cexpr &= lout.gt.cexpr;
// 		outputs.push_back(gir_tree::from(eLayoutOutput, lout.gt.cexpr, {
// 			gir_tree::cfrom(lout.binding),
// 			lout.gt
// 		}));
// 	}
//
// 	gir_tree unified = gir_tree::from(eNone, cexpr, outputs);
//
// 	fmt::println("\nunified tree for vertex shader:\n{}", unified);
//
// 	// TODO: combine all requirements into a single tree, then optimze the whole thing in one go
//
// 	// TODO: create a gir_link_tree
//
// 	// Grab all used bindings
// 	auto used_lib = used_layout_input_bindings(unified);
// 	auto used_lob = used_layout_output_bindings(unified);
//
// 	// TODO: also all output bindings
//
// 	// Fill in the rest of the program
// 	std::string code = "#version 450\n";
//
// 	for (auto [type, binding] : used_lib) {
// 		code += fmt::format("layout (location = {}) in {} {}{};\n",
// 			binding, gloa_type_string(type), LAYOUT_INPUT_PREFIX, binding);
// 	}
//
// 	for (auto binding : used_lob) {
// 		gloa type = louts[binding].type;
// 		code += fmt::format("layout (location = {}) out {} {}{};\n",
// 			binding, gloa_type_string(type), LAYOUT_OUTPUT_PREFIX, binding);
// 	}
//
// 	code += "void main() {\n";
//
// 	auto statements = translate(unified, generator);
// 	for (const statement &s : statements)
// 		code += fmt::format("  {}\n", s);
//
// 	code += "}\n";
//
// 	return code;
// }
//
// std::string translate_fragment_shader(const std::vector <unt_layout_output> &louts)
// {
// 	int generator = 0;
//
// 	// Preprocessing; unify all outputs into a single tree
// 	std::vector <gir_tree> outputs;
//
// 	bool cexpr = true;
//
// 	for (const unt_layout_output &lout : louts) {
// 		cexpr &= lout.gt.cexpr;
// 		outputs.push_back(gir_tree::from(eLayoutOutput, lout.gt.cexpr, {
// 			gir_tree::cfrom(lout.binding),
// 			lout.gt
// 		}));
// 	}
//
// 	gir_tree unified = gir_tree::from(eNone, cexpr, outputs);
//
// 	fmt::println("\nunified tree for fragment shader:\n{}", unified);
//
// 	// Grab all used bindings
// 	auto used_lib = used_layout_input_bindings(unified);
// 	auto used_lob = used_layout_output_bindings(unified);
//
// 	// Fill in the rest of the program
// 	// TODO: this is a common function
// 	std::string code = "#version 450\n";
//
// 	for (auto [type, binding] : used_lib) {
// 		code += fmt::format("layout (location = {}) in {} {}{};\n",
// 			binding, gloa_type_string(type), LAYOUT_INPUT_PREFIX, binding);
// 	}
//
// 	for (auto binding : used_lob) {
// 		gloa type = louts[binding].type;
// 		code += fmt::format("layout (location = {}) out {} {}{};\n",
// 			binding, gloa_type_string(type), LAYOUT_OUTPUT_PREFIX, binding);
// 	}
//
// 	code += "void main() {\n";
//
// 	auto statements = translate(unified, generator);
// 	for (const statement &s : statements)
// 		code += fmt::format("  {}\n", s);
//
// 	code += "}\n";
//
// 	return code;
// }

}
