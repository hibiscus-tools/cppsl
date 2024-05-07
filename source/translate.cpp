#include <cassert>
#include <map>
#include <set>
#include <variant>

#include <fmt/format.h>

#include "fmt.hpp"
#include "gir.hpp"
#include "translate.hpp"

static const std::string LAYOUT_INPUT_PREFIX = "_lin";
static const std::string LAYOUT_OUTPUT_PREFIX = "_lout";
static const std::string PUSH_CONSTANTS_PREFIX = "_pc";
static const std::string PUSH_CONSTANTS_MEMBER_PREFIX = "m";

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
struct translator {
	// Full graph
	gcir_graph graph;

	// State data
	int generator;
	int T;

	// Cached translations
	std::map <int, statement_list> cache;

	// Construction from graph only
	translator(const gcir_graph &gcir) : graph(gcir), generator(0), T(0), cache() {}

	using refs = std::vector <int>;
	using handler = std::function <statement_list (const refs &)>;

	statement_list handle_none(const refs &R) {
		statement_list statements;
		for (int C : R) {
			auto [sC, _] = cached_translation(C);
			statements.insert(statements.end(), sC.begin(), sC.end());
		}

		return statements;
	}

	statement_list handle_layout_input(const refs &R) {
		gloa type = std::get <gloa> (graph.data[R[0]]);
		int binding = std::get <int> (graph.data[R[1]]);
		return { statement::from(type, fmt::format("{}{}", LAYOUT_INPUT_PREFIX, binding), generator) };
	}

	statement_list handle_push_constants(const refs &R) {
		gloa type = std::get <gloa> (graph.data[R[0]]);
		int member = std::get <int> (graph.data[R[1]]);
		return { statement::from(type, fmt::format("{}.{}{}", PUSH_CONSTANTS_PREFIX, PUSH_CONSTANTS_MEMBER_PREFIX, member), generator) };
	}

	statement_list handle_layout_output(const refs &R) {
		int binding = std::get <int> (graph.data[R[0]]);
		auto [statements, last] = cached_translation(R[1]);
		statement after = statement::builtin_from(fmt::format("{}{}", LAYOUT_OUTPUT_PREFIX, binding), last.full_loc());
		statements.push_back(after);
		return statements;
	}

	statement_list handle_gl_position(const refs &R) {
		auto [statements, last] = cached_translation(R[0]);
		statement after = statement::builtin_from("gl_Position", last.full_loc());
		statements.push_back(after);
		return statements;
	}

	// TODO: conglomerate handler for all vector types, scalar types... etc
	statement_list handle_construct_f32(const refs &R) {
		auto [statements, last] = cached_translation(R[1]);
		statement after = statement::from(eFloat32, last.full_loc(), generator);
		statements.push_back(after);
		return statements;
	}

	statement_list handle_construct_vec4(const refs &R) {
		int count = std::get <int> (graph.data[R[1]]);

		statement_list v;
		std::vector <std::string> args;
		for (int i = 0; i < count; i++) {
			auto [vi, last] = cached_translation(R[i + 2]);
			v.insert(v.end(), vi.begin(), vi.end());
			args.push_back(last.full_loc());
		}

		statement after = statement::from(eVec4, function_call("vec4", args), generator);
		v.push_back(after);
		return v;
	}

	statement_list handle_construct(const refs &R) {
		gloa type = std::get <gloa> (graph.data[R[0]]);

		// TODO: switch?
		if (type == eFloat32)
			return handle_construct_f32(R);
		if (type == eVec4)
			return handle_construct_vec4(R);

		throw fmt::system_error(1, "(cppsl) unknown type {}", type);
	}

	statement_list handle_component(const refs &R) {
		static const std::string postfixes[] { ".x", ".y", ".z", ".w" };
		int index = std::get <int> (graph.data[R[0]]);

		auto [statements, last] = cached_translation(R[1]);
		statement after = statement::from(scalar_type(last.loc.type), last.full_loc() + postfixes[index], generator);
		statements.push_back(after);
		return statements;
	}

	statement_list handle_binary_operation(const refs &R, gloa op) {
		static const std::unordered_map <gloa, std::string> OPERATION_MAP {
			{ eAdd, "+" }, { eMul, "*" }
		};

		assert(R.size() == 2);
		auto [s0, s0_last] = cached_translation(R[0]);
		auto [s1, s1_last] = cached_translation(R[1]);

		// TODO: check the type maps...
		gloa type = s1_last.loc.type;
		statement after = statement::from(type, fmt::format("{} {} {}", s0_last.full_loc(), OPERATION_MAP.at(op), s1_last.full_loc()), generator);

		std::vector <statement> statements;
		statements.insert(statements.end(), s0.begin(), s0.end());
		statements.insert(statements.end(), s1.begin(), s1.end());
		statements.push_back(after);
		return statements;
	}

	statement_list operator()(gloa x) {
		const refs &R = graph.refs[T];
		switch (x) {
		case eNone:
			return handle_none(R);
		case eGlPosition:
			return handle_gl_position(R);
		case eConstruct:
			return handle_construct(R);
		case eComponent:
			return handle_component(R);
		case eLayoutInput:
			return handle_layout_input(R);
		case eLayoutOutput:
			return handle_layout_output(R);
		case ePushConstants:
			return handle_push_constants(R);
		case eAdd:
		case eMul:
			return handle_binary_operation(R, x);
		default:
			break;
		}

		throw fmt::system_error(1, "(cppsl) unexpected gloa of {}", x);
	}

	statement_list operator()(int x) {
		return { statement::from(eInt32, fmt::format("{}", x), generator) };
	}

	statement_list operator()(float x) {
		return { statement::from(eFloat32, fmt::format("{}", x), generator) };
	}

	statement_list translate(int t = 0) {
		T = t;
		auto statements = std::visit(*this, graph.data[T]);
		cache[t] = statements;
		return statements;
	}

	std::pair <statement_list, statement> cached_translation(int t) {
		if (cache.count(t)) {
			const auto &statements = cache[t];
			return std::make_pair(statement_list {}, statements.back());
		} else {
			auto statements = translate(t);
			return std::make_pair(statements, statements.back());
		}
	}
};

// Gathering shader input/output usage
struct shader_io {
	std::set <std::pair <gloa, int>> layout_inputs;
	std::set <int> layout_outputs;
	std::map <int, std::pair <gloa, int>> push_constants;
};

shader_io gather_shader_io(const gcir_graph &graph, int T = 0)
{
	const gir_t &data = graph.data[T];
	const auto &R = graph.refs[T];

	shader_io combined;
	if (std::holds_alternative <gloa> (data)) {
		gloa x = std::get <gloa> (data);
		if (x == eLayoutInput) {
			gloa type = std::get <gloa> (graph.data[R[0]]);
			int binding = std::get <int> (graph.data[R[1]]);
			combined.layout_inputs = { std::make_pair(type, binding) };
		} else if (x == eLayoutOutput) {
			int binding = std::get <int> (graph.data[R[0]]);
			combined.layout_outputs = { binding };
		} else if (x == ePushConstants) {
			gloa type = std::get <gloa> (graph.data[R[0]]);
			int member = std::get <int> (graph.data[R[1]]);
			int offset = std::get <int> (graph.data[R[2]]);
			combined.push_constants = { std::make_pair(member, std::make_pair(type, offset)) };
		}
	}

	for (int C : R) {
		auto io = gather_shader_io(graph, C);
		combined.layout_inputs.insert(io.layout_inputs.begin(), io.layout_inputs.end());
		combined.layout_outputs.insert(io.layout_outputs.begin(), io.layout_outputs.end());

		// Check for no conflicting members
		for (const auto &[member, info] : io.push_constants) {
			if (combined.push_constants.count(member))
				assert(combined.push_constants[member] == info);
			else
				combined.push_constants[member] = info;
		}
	}

	return combined;
}

namespace detail {

// TODO: separate optimization stage

// TODO: pass the gcir instead; compress before translation...
std::string translate(const gcir_graph &graph, const std::vector <unt_layout_output> &louts)
{
	fmt::println("\ncompressed graph:\n{}", graph);

	// Grab information of all shader inputs and outputs
	auto io = gather_shader_io(graph);

	// TODO: also all output bindings

	// Fill in the rest of the program
	std::string code = "#version 450\n";

	// Input layout bindings
	for (auto [type, binding] : io.layout_inputs) {
		code += fmt::format("layout (location = {}) in {} {}{};\n",
			binding, gloa_type_string(type), LAYOUT_INPUT_PREFIX, binding);
	}

	// Output layout bindings
	for (auto binding : io.layout_outputs) {
		gloa type = louts[binding].type;
		code += fmt::format("layout (location = {}) out {} {}{};\n",
			binding, gloa_type_string(type), LAYOUT_OUTPUT_PREFIX, binding);
	}

	// Push constants
	if (io.push_constants.size()) {
		// fmt::println("pc_struct:");
		// for (const auto &[member, info] : pc_struct)
		// 	fmt::println("  {}: {:<5} +{}", member, gloa_type_string(info.first), info.second);

		code += fmt::format("layout (push_constant) uniform PushConstants {{\n");
		// TODO: track the size for necessary paddings
		int offed = 0;
		for (const auto &[member, info] : io.push_constants) {
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

	auto tr = translator(graph);
	auto statements = tr.translate();
	for (const statement &s : statements)
		code += fmt::format("  {}\n", s);

	code += "}\n";

	return code;
}

}
