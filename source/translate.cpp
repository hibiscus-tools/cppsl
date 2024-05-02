#include <cassert>
#include <set>

#include "translate.hpp"
#include "fmt.hpp"

static const std::string LAYOUT_INPUT_PREFIX = "_lin";
static const std::string LAYOUT_OUTPUT_PREFIX = "_lout";

std::vector <statement> translate(const gir_tree &, int &);

std::vector <statement> translate_layout_input(const std::vector <gir_tree> &nodes, int &generator)
{
	gloa type = std::get <gloa> (nodes[0].data);
	int binding = std::get <int> (nodes[1].data);
	return { statement::from(type, fmt::format("{}{}", LAYOUT_INPUT_PREFIX, binding), generator) };
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

// TODO: return the location (identifier) of the stored result, can choose to inline it
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

std::vector <statement> translate(const gir_tree &gt, int &generator)
{
	struct visitor {
		gir_tree gt;
		int &generator;

		std::vector <statement> operator()(gloa x) {
			switch (x) {
			case eConstruct:
				return translate_construct(gt.children, generator);
			case eLayoutInput:
				return translate_layout_input(gt.children, generator);
			default:
				break;
			}

			assert(false);
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

// TODO: support multi output programs
namespace detail {

std::string translate_vertex_shader(const intrinsics::vertex &vin)
{
	int generator = 0;
	auto statements = translate(vin.gl_Position, generator);

	// Grab all used bindings
	auto used_lib = used_layout_input_bindings(vin.gl_Position);

	// Fill in the rest of the program
	std::string code = "#version 450\n";
	for (auto [type, binding] : used_lib) {
		code += fmt::format("layout (location = {}) in {} {}{};\n",
			binding, gloa_type_string(type), LAYOUT_INPUT_PREFIX, binding);
	}

	code += "void main() {\n";
	for (const statement &s : statements)
		code += fmt::format("  {}\n", s);

	code += fmt::format("  gl_Position = {};\n", statements.back().full_loc());
	code += "}\n";

	return code;
}

std::string translate_fragment_shader(const unt_layout_output &fout)
{
	int generator = 0;
	auto statements = translate(fout.gt, generator);

	// Fill in the rest of the program
	std::string code = "#version 450\n";
	code += "layout (location = 0) out " + gloa_type_string(fout.type) + " " + LAYOUT_OUTPUT_PREFIX + "0;\n";
	code += "void main() {\n";
	for (const statement &s : statements)
		code += fmt::format("  {}\n", s);
	// TODO: type checking for the sake of sanity
	code += "  " + LAYOUT_OUTPUT_PREFIX + "0 = " + statements.back().full_loc() + ";\n";
	code += "}\n";

	return code;
}

}
